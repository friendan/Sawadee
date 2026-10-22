#include "StdAfx.h"
#include "duilib.h"
#include "resource.h"
#include "ImageWnd.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <shobjidl.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace {

int ColorLuma(DWORD dw)
{
	const int r = (int)DuiColorR(dw);
	const int g = (int)DuiColorG(dw);
	const int b = (int)DuiColorB(dw);
	return (r * 299 + g * 587 + b * 114) / 1000;
}

int ClampExportSize(int v)
{
	if( v < 1 ) return 1;
	if( v > 4096 ) return 4096;
	return v;
}

const UINT kMsgBgReject = WM_APP + 42;

ID2D1Factory* SharedD2D()
{
	static ID2D1Factory* factory = NULL;
	if( factory == NULL )
		D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory);
	return factory;
}

IDWriteFactory* SharedDW()
{
	static IDWriteFactory* factory = NULL;
	if( factory == NULL ) {
		IDWriteFactory* created = NULL;
		if( SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&created))) )
			factory = created;
	}
	return factory;
}

IWICImagingFactory* SharedWic()
{
	static IWICImagingFactory* factory = NULL;
	if( factory == NULL )
		::CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
	return factory;
}

struct GlyphStyle
{
	const wchar_t* family;
	float sizePx;
	bool bold;
	bool italic;
	bool underline;
	bool strike;
};

void CenterInk(std::vector<BYTE>& px, int w, int h);

bool RenderTextPremul(const wchar_t* text, int w, int h, DWORD color, const GlyphStyle& style, std::vector<BYTE>& out, float* usedPx)
{
	out.clear();
	if( text == NULL || text[0] == 0 || w < 1 || h < 1 ) return false;
	ID2D1Factory* d2d = SharedD2D();
	IDWriteFactory* dw = SharedDW();
	IWICImagingFactory* wic = SharedWic();
	if( d2d == NULL || dw == NULL || wic == NULL ) return false;

	const wchar_t* family = (style.family != NULL && style.family[0] != 0) ? style.family : L"TH Sarabun New";
	IDWriteTextFormat* format = NULL;
	HRESULT hr = dw->CreateTextFormat(family, NULL,
		style.bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL,
		style.italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		64.0f, L"", &format);
	if( FAILED(hr) || format == NULL ) return false;
	format->SetWordWrapping(DWRITE_WORD_WRAPPING_EMERGENCY_BREAK);
	format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
	format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

	const UINT32 len = (UINT32)wcslen(text);
	IDWriteTextLayout* layout = NULL;
	hr = dw->CreateTextLayout(text, len, format, (FLOAT)w, (FLOAT)h, &layout);
	format->Release();
	if( FAILED(hr) || layout == NULL ) return false;

	const DWRITE_TEXT_RANGE range = { 0, len };
	float used = style.sizePx;
	if( style.sizePx > 1.0f ) {
		layout->SetFontSize(style.sizePx, range);
	}
	else {
		float lo = 4.0f;
		float hi = (float)((w < h) ? w : h);
		float fitted = lo;
		for( int i = 0; i < 16; ++i ) {
			const float mid = (lo + hi) * 0.5f;
			layout->SetFontSize(mid, range);
			DWRITE_TEXT_METRICS metrics = {};
			layout->GetMetrics(&metrics);
			if( metrics.width <= (float)w * 0.88f && metrics.height <= (float)h * 0.88f ) {
				fitted = mid;
				lo = mid;
			}
			else {
				hi = mid;
			}
		}
		layout->SetFontSize(fitted, range);
		used = fitted;
	}
	if( style.underline ) layout->SetUnderline(TRUE, range);
	if( style.strike ) layout->SetStrikethrough(TRUE, range);

	DWRITE_TEXT_METRICS metrics = {};
	layout->GetMetrics(&metrics);
	const float overhangX = metrics.left < 0 ? -metrics.left : 0;
	const float overhangY = metrics.top < 0 ? -metrics.top : 0;
	int needW = (int)(metrics.left + metrics.width + overhangX + 8.0f);
	int needH = (int)(metrics.top + metrics.height + overhangY + 8.0f);
	if( needW < w ) needW = w;
	if( needH < h ) needH = h;
	if( needW > 8192 ) needW = 8192;
	if( needH > 8192 ) needH = 8192;
	if( needW != w ) layout->SetMaxWidth((FLOAT)needW);
	if( needH != h ) layout->SetMaxHeight((FLOAT)needH);

	IWICBitmap* bmp = NULL;
	hr = wic->CreateBitmap((UINT)needW, (UINT)needH, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bmp);
	if( FAILED(hr) || bmp == NULL ) {
		layout->Release();
		return false;
	}
	ID2D1RenderTarget* rt = NULL;
	hr = d2d->CreateWicBitmapRenderTarget(bmp,
		D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
		&rt);
	if( FAILED(hr) || rt == NULL ) {
		layout->Release();
		bmp->Release();
		return false;
	}
	rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

	ID2D1SolidColorBrush* brush = NULL;
	hr = rt->CreateSolidColorBrush(
		D2D1::ColorF(DuiColorR(color) / 255.0f, DuiColorG(color) / 255.0f, DuiColorB(color) / 255.0f, DuiColorA(color) / 255.0f),
		&brush);
	if( FAILED(hr) || brush == NULL ) {
		layout->Release();
		rt->Release();
		bmp->Release();
		return false;
	}

	rt->BeginDraw();
	rt->Clear(D2D1::ColorF(0, 0.0f));
	rt->DrawTextLayout(D2D1::Point2F(overhangX, overhangY), layout, brush);
	hr = rt->EndDraw();
	brush->Release();
	layout->Release();
	rt->Release();
	if( FAILED(hr) ) {
		bmp->Release();
		return false;
	}

	IWICBitmapLock* lock = NULL;
	WICRect rc = { 0, 0, needW, needH };
	hr = bmp->Lock(&rc, WICBitmapLockRead, &lock);
	if( FAILED(hr) || lock == NULL ) {
		bmp->Release();
		return false;
	}
	UINT stride = 0;
	BYTE* data = NULL;
	UINT cb = 0;
	lock->GetStride(&stride);
	lock->GetDataPointer(&cb, &data);
	if( data == NULL || stride < (UINT)needW * 4 ) {
		lock->Release();
		bmp->Release();
		return false;
	}
	std::vector<BYTE> drawn((size_t)needW * (size_t)needH * 4);
	for( int y = 0; y < needH; ++y )
		memcpy(drawn.data() + (size_t)y * (size_t)needW * 4, data + (size_t)y * stride, (size_t)needW * 4);
	lock->Release();
	bmp->Release();
	CenterInk(drawn, needW, needH);

	out.assign((size_t)w * (size_t)h * 4, 0);
	const int ox = (needW - w) / 2;
	const int oy = (needH - h) / 2;
	for( int y = 0; y < h; ++y ) {
		const int sy = y + oy;
		if( sy < 0 || sy >= needH ) continue;
		int sx = ox;
		int dx = 0;
		int count = w;
		if( sx < 0 ) {
			count += sx;
			dx -= sx;
			sx = 0;
		}
		if( sx + count > needW ) count = needW - sx;
		if( count <= 0 ) continue;
		memcpy(out.data() + ((size_t)y * (size_t)w + (size_t)dx) * 4,
			drawn.data() + ((size_t)sy * (size_t)needW + (size_t)sx) * 4,
			(size_t)count * 4);
	}
	if( usedPx != NULL ) *usedPx = used;
	return true;
}

void CenterInk(std::vector<BYTE>& px, int w, int h)
{
	int left = w;
	int top = h;
	int right = 0;
	int bottom = 0;
	for( int y = 0; y < h; ++y ) {
		const BYTE* row = px.data() + (size_t)y * (size_t)w * 4;
		for( int x = 0; x < w; ++x ) {
			if( row[x * 4 + 3] == 0 ) continue;
			if( x < left ) left = x;
			if( y < top ) top = y;
			if( x + 1 > right ) right = x + 1;
			if( y + 1 > bottom ) bottom = y + 1;
		}
	}
	if( right <= left || bottom <= top ) return;
	const int dx = (w - (right - left)) / 2 - left;
	const int dy = (h - (bottom - top)) / 2 - top;
	if( dx == 0 && dy == 0 ) return;

	std::vector<BYTE> dst((size_t)w * (size_t)h * 4, 0);
	for( int y = top; y < bottom; ++y ) {
		const int ny = y + dy;
		if( ny < 0 || ny >= h ) continue;
		int count = right - left;
		int sx = left;
		int nx = left + dx;
		if( nx < 0 ) {
			count += nx;
			sx -= nx;
			nx = 0;
		}
		if( nx + count > w ) count = w - nx;
		if( count <= 0 ) continue;
		memcpy(dst.data() + ((size_t)ny * (size_t)w + (size_t)nx) * 4,
			px.data() + ((size_t)y * (size_t)w + (size_t)sx) * 4,
			(size_t)count * 4);
	}
	px.swap(dst);
}

struct InkBox
{
	int left;
	int top;
	int right;
	int bottom;
	float usedPx;
	bool ok;
};

struct PlacedText
{
	std::wstring text;
	std::wstring family;
	int userPx;
	bool bold;
	bool italic;
	bool underline;
	bool strike;
	DWORD color;
	float cx;
	float cy;
};

int ShiftPx(float center, int span)
{
	const float d = (center - 0.5f) * (float)span;
	if( d >= 0 ) return (int)(d + 0.5f);
	return (int)(d - 0.5f);
}

void BlendShift(std::vector<BYTE>& dst, int w, int h, const std::vector<BYTE>& src, int dx, int dy)
{
	for( int y = 0; y < h; ++y ) {
		const int sy = y - dy;
		if( sy < 0 || sy >= h ) continue;
		for( int x = 0; x < w; ++x ) {
			const int sx = x - dx;
			if( sx < 0 || sx >= w ) continue;
			const size_t si = ((size_t)sy * (size_t)w + (size_t)sx) * 4;
			const int sa = src[si + 3];
			if( sa == 0 ) continue;
			const size_t di = ((size_t)y * (size_t)w + (size_t)x) * 4;
			if( sa >= 255 ) {
				dst[di + 0] = src[si + 0];
				dst[di + 1] = src[si + 1];
				dst[di + 2] = src[si + 2];
				dst[di + 3] = 255;
				continue;
			}
			const int inv = 255 - sa;
			dst[di + 0] = (BYTE)(src[si + 0] + dst[di + 0] * inv / 255);
			dst[di + 1] = (BYTE)(src[si + 1] + dst[di + 1] * inv / 255);
			dst[di + 2] = (BYTE)(src[si + 2] + dst[di + 2] * inv / 255);
			dst[di + 3] = (BYTE)(sa + dst[di + 3] * inv / 255);
		}
	}
}

bool FindInk(const std::vector<BYTE>& px, int w, int h, int& left, int& top, int& right, int& bottom)
{
	left = w;
	top = h;
	right = 0;
	bottom = 0;
	for( int y = 0; y < h; ++y ) {
		const BYTE* row = px.data() + (size_t)y * (size_t)w * 4;
		for( int x = 0; x < w; ++x ) {
			if( row[x * 4 + 3] == 0 ) continue;
			if( x < left ) left = x;
			if( y < top ) top = y;
			if( x + 1 > right ) right = x + 1;
			if( y + 1 > bottom ) bottom = y + 1;
		}
	}
	return right > left && bottom > top;
}

bool ComposePlaced(const std::vector<PlacedText>& items, int w, int h, int refSide, std::vector<BYTE>& out, int selected, InkBox* box, bool (*yield)(void*), void* yieldUser)
{
	out.assign((size_t)w * (size_t)h * 4, 0);
	if( box != NULL ) box->ok = false;
	if( w < 1 || h < 1 ) return false;
	const int side = (w < h) ? w : h;
	const int ref = refSide > 0 ? refSide : side;
	bool any = false;
	for( size_t i = 0; i < items.size(); ++i ) {
		if( yield != NULL && !yield(yieldUser) ) return false;
		const PlacedText& item = items[i];
		if( item.text.empty() ) continue;
		float sizePx = 0;
		if( item.userPx > 0 && ref > 0 )
			sizePx = (float)item.userPx * (float)side / (float)ref;
		GlyphStyle style = {};
		style.family = item.family.empty() ? L"TH Sarabun New" : item.family.c_str();
		style.sizePx = sizePx;
		style.bold = item.bold;
		style.italic = item.italic;
		style.underline = item.underline;
		style.strike = item.strike;
		std::vector<BYTE> layer;
		float used = 0;
		if( !RenderTextPremul(item.text.c_str(), w, h, item.color, style, layer, &used) )
			continue;
		const int dx = ShiftPx(item.cx, w);
		const int dy = ShiftPx(item.cy, h);
		if( box != NULL && (int)i == selected ) {
			int l = 0, t = 0, r = 0, b = 0;
			if( FindInk(layer, w, h, l, t, r, b) ) {
				box->left = l + dx;
				box->top = t + dy;
				box->right = r + dx;
				box->bottom = b + dy;
				box->usedPx = used;
				box->ok = true;
			}
		}
		BlendShift(out, w, h, layer, dx, dy);
		any = true;
	}
	return any;
}

bool SamePlaced(const std::vector<PlacedText>& a, const std::vector<PlacedText>& b)
{
	if( a.size() != b.size() ) return false;
	for( size_t i = 0; i < a.size(); ++i ) {
		if( a[i].text != b[i].text || a[i].family != b[i].family || a[i].userPx != b[i].userPx
			|| a[i].bold != b[i].bold || a[i].italic != b[i].italic
			|| a[i].underline != b[i].underline || a[i].strike != b[i].strike
			|| a[i].color != b[i].color || a[i].cx != b[i].cx || a[i].cy != b[i].cy )
			return false;
	}
	return true;
}

void FlattenPremul(const std::vector<BYTE>& premul, int w, int h, bool whiteMatte, std::vector<BYTE>& straight)
{
	straight.resize((size_t)w * (size_t)h * 4);
	for( int i = 0; i < w * h; ++i ) {
		BYTE b = premul[(size_t)i * 4 + 0];
		BYTE g = premul[(size_t)i * 4 + 1];
		BYTE r = premul[(size_t)i * 4 + 2];
		BYTE a = premul[(size_t)i * 4 + 3];
		if( whiteMatte ) {
			int ib = (int)b + (255 - (int)a);
			int ig = (int)g + (255 - (int)a);
			int ir = (int)r + (255 - (int)a);
			if( ib > 255 ) ib = 255;
			if( ig > 255 ) ig = 255;
			if( ir > 255 ) ir = 255;
			b = (BYTE)ib;
			g = (BYTE)ig;
			r = (BYTE)ir;
			a = 255;
		}
		else if( a > 0 && a < 255 ) {
			b = (BYTE)(((int)b * 255) / (int)a);
			g = (BYTE)(((int)g * 255) / (int)a);
			r = (BYTE)(((int)r * 255) / (int)a);
		}
		else if( a == 0 ) {
			b = g = r = 0;
		}
		straight[(size_t)i * 4 + 0] = b;
		straight[(size_t)i * 4 + 1] = g;
		straight[(size_t)i * 4 + 2] = r;
		straight[(size_t)i * 4 + 3] = a;
	}
}

bool EncodeBitmap(IWICBitmap* bmp, LPCTSTR path, const GUID& container, bool jpeg, int jpegQuality)
{
	IWICImagingFactory* wic = SharedWic();
	if( wic == NULL || bmp == NULL || path == NULL ) return false;
	IWICStream* stream = NULL;
	IWICBitmapEncoder* encoder = NULL;
	IWICBitmapFrameEncode* frame = NULL;
	IPropertyBag2* bag = NULL;
	HRESULT hr = wic->CreateStream(&stream);
	if( SUCCEEDED(hr) ) hr = stream->InitializeFromFilename(path, GENERIC_WRITE);
	if( SUCCEEDED(hr) ) hr = wic->CreateEncoder(container, NULL, &encoder);
	if( SUCCEEDED(hr) ) hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
	if( SUCCEEDED(hr) ) hr = encoder->CreateNewFrame(&frame, &bag);
	if( SUCCEEDED(hr) && jpeg && bag != NULL ) {
		PROPBAG2 opt = {};
		opt.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
		VARIANT val;
		::VariantInit(&val);
		val.vt = VT_R4;
		val.fltVal = (jpegQuality < 1) ? 0.9f : ((jpegQuality > 100) ? 1.0f : jpegQuality / 100.0f);
		bag->Write(1, &opt, &val);
		::VariantClear(&val);
	}
	if( SUCCEEDED(hr) ) hr = frame->Initialize(bag);
	UINT bw = 0, bh = 0;
	if( SUCCEEDED(hr) ) hr = bmp->GetSize(&bw, &bh);
	if( SUCCEEDED(hr) ) hr = frame->SetSize(bw, bh);
	WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
	if( SUCCEEDED(hr) ) hr = bmp->GetPixelFormat(&fmt);
	if( SUCCEEDED(hr) ) hr = frame->SetPixelFormat(&fmt);
	if( SUCCEEDED(hr) ) hr = frame->WriteSource(bmp, NULL);
	if( SUCCEEDED(hr) ) hr = frame->Commit();
	if( SUCCEEDED(hr) ) hr = encoder->Commit();
	if( bag ) bag->Release();
	if( frame ) frame->Release();
	if( encoder ) encoder->Release();
	if( stream ) stream->Release();
	return SUCCEEDED(hr);
}

bool SaveRaster(const std::vector<BYTE>& premul, int w, int h, LPCTSTR path, LPCTSTR ext)
{
	IWICImagingFactory* wic = SharedWic();
	if( wic == NULL ) return false;
	const bool jpg = (ext != NULL && _tcsicmp(ext, _T("jpg")) == 0);
	const bool bmp = (ext != NULL && _tcsicmp(ext, _T("bmp")) == 0);
	std::vector<BYTE> straight;
	FlattenPremul(premul, w, h, jpg || bmp, straight);
	IWICBitmap* bmpSrc = NULL;
	HRESULT hr = wic->CreateBitmapFromMemory((UINT)w, (UINT)h, GUID_WICPixelFormat32bppBGRA,
		(UINT)w * 4, (UINT)straight.size(), straight.data(), &bmpSrc);
	if( FAILED(hr) || bmpSrc == NULL ) return false;

	IWICBitmapSource* source = bmpSrc;
	IWICFormatConverter* conv = NULL;
	if( jpg || bmp ) {
		hr = wic->CreateFormatConverter(&conv);
		if( SUCCEEDED(hr) )
			hr = conv->Initialize(bmpSrc, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
		if( FAILED(hr) ) {
			if( conv ) conv->Release();
			bmpSrc->Release();
			return false;
		}
		source = conv;
	}

	GUID container = GUID_ContainerFormatPng;
	if( jpg ) container = GUID_ContainerFormatJpeg;
	else if( bmp ) container = GUID_ContainerFormatBmp;

	IWICBitmap* encoded = NULL;
	hr = wic->CreateBitmapFromSource(source, WICBitmapCacheOnLoad, &encoded);
	bool ok = false;
	if( SUCCEEDED(hr) && encoded != NULL )
		ok = EncodeBitmap(encoded, path, container, jpg, 90);
	if( encoded ) encoded->Release();
	if( conv ) conv->Release();
	bmpSrc->Release();
	return ok;
}

bool SavePngMemory(const std::vector<BYTE>& premul, int w, int h, std::vector<BYTE>& png)
{
	png.clear();
	IWICImagingFactory* wic = SharedWic();
	if( wic == NULL ) return false;
	std::vector<BYTE> straight;
	FlattenPremul(premul, w, h, false, straight);
	IWICBitmap* bmp = NULL;
	HRESULT hr = wic->CreateBitmapFromMemory((UINT)w, (UINT)h, GUID_WICPixelFormat32bppBGRA,
		(UINT)w * 4, (UINT)straight.size(), straight.data(), &bmp);
	if( FAILED(hr) || bmp == NULL ) return false;

	IStream* stm = NULL;
	hr = ::CreateStreamOnHGlobal(NULL, TRUE, &stm);
	if( FAILED(hr) || stm == NULL ) {
		bmp->Release();
		return false;
	}
	IWICBitmapEncoder* encoder = NULL;
	IWICBitmapFrameEncode* frame = NULL;
	hr = wic->CreateEncoder(GUID_ContainerFormatPng, NULL, &encoder);
	if( SUCCEEDED(hr) ) hr = encoder->Initialize(stm, WICBitmapEncoderNoCache);
	if( SUCCEEDED(hr) ) hr = encoder->CreateNewFrame(&frame, NULL);
	if( SUCCEEDED(hr) ) hr = frame->Initialize(NULL);
	if( SUCCEEDED(hr) ) hr = frame->SetSize((UINT)w, (UINT)h);
	WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
	if( SUCCEEDED(hr) ) hr = frame->SetPixelFormat(&fmt);
	if( SUCCEEDED(hr) ) hr = frame->WriteSource(bmp, NULL);
	if( SUCCEEDED(hr) ) hr = frame->Commit();
	if( SUCCEEDED(hr) ) hr = encoder->Commit();
	if( frame ) frame->Release();
	if( encoder ) encoder->Release();
	bmp->Release();
	if( FAILED(hr) ) {
		stm->Release();
		return false;
	}
	HGLOBAL mem = NULL;
	if( FAILED(::GetHGlobalFromStream(stm, &mem)) || mem == NULL ) {
		stm->Release();
		return false;
	}
	const SIZE_T n = ::GlobalSize(mem);
	const void* p = ::GlobalLock(mem);
	if( p == NULL || n == 0 ) {
		if( p ) ::GlobalUnlock(mem);
		stm->Release();
		return false;
	}
	png.resize((size_t)n);
	memcpy(png.data(), p, (size_t)n);
	::GlobalUnlock(mem);
	stm->Release();
	return !png.empty();
}

bool WriteIco(LPCTSTR path, const std::vector<std::vector<BYTE> >& pngs, const std::vector<int>& sizes)
{
	if( path == NULL || pngs.empty() || pngs.size() != sizes.size() ) return false;
	const WORD nCount = (WORD)pngs.size();
#pragma pack(push, 1)
	struct ICONDIR { WORD reserved; WORD type; WORD count; };
	struct ICONDIRENTRY {
		BYTE width; BYTE height; BYTE colors; BYTE reserved;
		WORD planes; WORD bitCount; DWORD bytes; DWORD offset;
	};
#pragma pack(pop)

	HANDLE file = ::CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if( file == INVALID_HANDLE_VALUE ) return false;
	ICONDIR dir = { 0, 1, nCount };
	DWORD written = 0;
	if( !::WriteFile(file, &dir, sizeof(dir), &written, NULL) ) {
		::CloseHandle(file);
		::DeleteFile(path);
		return false;
	}
	DWORD offset = (DWORD)(sizeof(ICONDIR) + sizeof(ICONDIRENTRY) * nCount);
	std::vector<ICONDIRENTRY> ents(nCount);
	for( WORD i = 0; i < nCount; ++i ) {
		const int s = sizes[i];
		ICONDIRENTRY& ent = ents[i];
		::ZeroMemory(&ent, sizeof(ent));
		ent.width = (s >= 256) ? 0 : (BYTE)s;
		ent.height = (s >= 256) ? 0 : (BYTE)s;
		ent.planes = 1;
		ent.bitCount = 32;
		ent.bytes = (DWORD)pngs[i].size();
		ent.offset = offset;
		offset += ent.bytes;
	}
	for( WORD i = 0; i < nCount; ++i ) {
		if( !::WriteFile(file, &ents[i], sizeof(ICONDIRENTRY), &written, NULL) ) {
			::CloseHandle(file);
			::DeleteFile(path);
			return false;
		}
	}
	for( WORD i = 0; i < nCount; ++i ) {
		if( pngs[i].empty() || !::WriteFile(file, pngs[i].data(), (DWORD)pngs[i].size(), &written, NULL) ) {
			::CloseHandle(file);
			::DeleteFile(path);
			return false;
		}
	}
	::CloseHandle(file);
	return true;
}

CDuiString FileBaseName(const CDuiString& text)
{
	CDuiString name;
	const int n = text.GetLength();
	for( int i = 0; i < n && name.GetLength() < 16; ++i ) {
		const TCHAR ch = text.GetAt(i);
		if( ch < 32 ) continue;
		if( _tcschr(_T("\\/:*?\"<>|"), ch) != NULL ) continue;
		name += ch;
	}
	if( name.IsEmpty() ) name = _T("image");
	return name;
}

bool PickSavePath(HWND owner, const CDuiString& base, LPCTSTR filter, LPCTSTR defExt, CDuiString& path)
{
	IFileSaveDialog* dlg = NULL;
	HRESULT hr = ::CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
	if( FAILED(hr) || dlg == NULL ) return false;

	DWORD opts = 0;
	if( SUCCEEDED(dlg->GetOptions(&opts)) ) {
		opts |= FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM;
		opts &= ~FOS_OVERWRITEPROMPT;
		dlg->SetOptions(opts);
	}

	const wchar_t* specName = filter;
	const wchar_t* specPat = filter + wcslen(filter) + 1;
	COMDLG_FILTERSPEC types[1];
	types[0].pszName = specName;
	types[0].pszSpec = (specPat != NULL && specPat[0] != 0) ? specPat : L"*.*";
	dlg->SetFileTypes(1, types);
	dlg->SetFileTypeIndex(1);
	dlg->SetDefaultExtension(defExt);

	CDuiString initial;
	initial.Format(_T("%s.%s"), base.GetData(), defExt);
	dlg->SetFileName(initial.GetData());

	for( ;; ) {
		hr = dlg->Show(owner);
		if( FAILED(hr) ) {
			dlg->Release();
			return false;
		}
		IShellItem* item = NULL;
		hr = dlg->GetResult(&item);
		if( FAILED(hr) || item == NULL ) {
			dlg->Release();
			return false;
		}
		PWSTR full = NULL;
		hr = item->GetDisplayName(SIGDN_FILESYSPATH, &full);
		item->Release();
		if( FAILED(hr) || full == NULL ) {
			dlg->Release();
			return false;
		}
		path = full;
		::CoTaskMemFree(full);
		if( ::GetFileAttributes(path.GetData()) == INVALID_FILE_ATTRIBUTES )
			break;
		const int answer = CMessageBox::Show(owner, _T("导出"), _T("文件已存在，是否替换？"),
			CModalOptions().ShowCancel(true).OkText(_T("替换")).CancelText(_T("取消")).Kind(CONTROLKIND_WARNING));
		if( answer == MESSAGEBOX_OK ) break;
		const wchar_t* slash = wcsrchr(path.GetData(), L'\\');
		dlg->SetFileName((slash != NULL) ? slash + 1 : path.GetData());
	}
	dlg->Release();
	return !path.IsEmpty();
}

unsigned short ReadBe16(const BYTE* p)
{
	return (unsigned short)((p[0] << 8) | p[1]);
}

unsigned int ReadBe32(const BYTE* p)
{
	return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

bool FamilyFromSfnt(const BYTE* data, size_t size, std::wstring& family)
{
	if( data == NULL || size < 12 ) return false;
	const unsigned short tables = ReadBe16(data + 4);
	if( 12 + (size_t)tables * 16 > size ) return false;
	const BYTE* nameTable = NULL;
	unsigned int nameLen = 0;
	for( unsigned short i = 0; i < tables; ++i ) {
		const BYTE* rec = data + 12 + (size_t)i * 16;
		if( rec[0] != 'n' || rec[1] != 'a' || rec[2] != 'm' || rec[3] != 'e' ) continue;
		const unsigned int off = ReadBe32(rec + 8);
		nameLen = ReadBe32(rec + 12);
		if( (size_t)off + nameLen > size || nameLen < 6 ) return false;
		nameTable = data + off;
		break;
	}
	if( nameTable == NULL ) return false;
	const unsigned short count = ReadBe16(nameTable + 2);
	const unsigned short stringOffset = ReadBe16(nameTable + 4);
	if( 6 + (size_t)count * 12 > nameLen ) return false;

	const BYTE* best = NULL;
	unsigned short bestLen = 0;
	int bestScore = -1;
	for( unsigned short i = 0; i < count; ++i ) {
		const BYTE* rec = nameTable + 6 + (size_t)i * 12;
		const unsigned short platform = ReadBe16(rec);
		const unsigned short encoding = ReadBe16(rec + 2);
		const unsigned short nameId = ReadBe16(rec + 6);
		const unsigned short len = ReadBe16(rec + 8);
		const unsigned short off = ReadBe16(rec + 10);
		if( nameId != 16 && nameId != 1 ) continue;
		const bool utf16 = (platform == 3 && encoding == 1) || platform == 0;
		if( !utf16 || len < 2 || (len % 2) != 0 ) continue;
		if( (size_t)stringOffset + off + len > nameLen ) continue;
		const int score = (nameId == 16 ? 2 : 0) + (platform == 3 ? 1 : 0);
		if( score <= bestScore ) continue;
		bestScore = score;
		best = nameTable + stringOffset + off;
		bestLen = len;
	}
	if( best == NULL ) return false;
	family.clear();
	for( unsigned short i = 0; i + 1 < bestLen; i += 2 ) {
		const wchar_t ch = (wchar_t)((best[i] << 8) | best[i + 1]);
		if( ch == 0 ) break;
		family.push_back(ch);
	}
	return !family.empty();
}

bool FamilyFromFontFile(const std::wstring& path, std::wstring& family)
{
	HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( file == INVALID_HANDLE_VALUE ) return false;
	const DWORD size = ::GetFileSize(file, NULL);
	if( size < 12 || size > 16 * 1024 * 1024 ) {
		::CloseHandle(file);
		return false;
	}
	std::vector<BYTE> buf(size);
	DWORD read = 0;
	const BOOL ok = ::ReadFile(file, buf.data(), size, &read, NULL);
	::CloseHandle(file);
	if( !ok || read != size ) return false;
	if( buf.size() >= 12 && buf[0] == 't' && buf[1] == 't' && buf[2] == 'c' && buf[3] == 'f' ) {
		const unsigned int faces = ReadBe32(buf.data() + 8);
		if( faces < 1 ) return false;
		const unsigned int off = ReadBe32(buf.data() + 12);
		if( (size_t)off >= buf.size() ) return false;
		return FamilyFromSfnt(buf.data() + off, buf.size() - off, family);
	}
	return FamilyFromSfnt(buf.data(), buf.size(), family);
}

bool IsFontExt(const wchar_t* name)
{
	const wchar_t* dot = wcsrchr(name, L'.');
	if( dot == NULL ) return false;
	return _wcsicmp(dot, L".ttf") == 0 || _wcsicmp(dot, L".otf") == 0 || _wcsicmp(dot, L".ttc") == 0;
}

void SortUnique(std::vector<std::wstring>& names)
{
	std::sort(names.begin(), names.end(), [](const std::wstring& a, const std::wstring& b) {
		return _wcsicmp(a.c_str(), b.c_str()) < 0;
	});
	names.erase(std::unique(names.begin(), names.end(), [](const std::wstring& a, const std::wstring& b) {
		return _wcsicmp(a.c_str(), b.c_str()) == 0;
	}), names.end());
}

void CollectBuiltInFamilies(std::vector<std::wstring>& names)
{
	names.clear();
	CDuiString root = CPaintManagerUI::GetInstancePath();
	root += _T("skin\\Sawadee\\font");
	std::vector<std::wstring> dirs;
	dirs.push_back(root.GetData());
	while( !dirs.empty() ) {
		const std::wstring dir = dirs.back();
		dirs.pop_back();
		WIN32_FIND_DATAW fd;
		HANDLE found = ::FindFirstFileW((dir + L"\\*").c_str(), &fd);
		if( found == INVALID_HANDLE_VALUE ) continue;
		do {
			if( fd.cFileName[0] == L'.' ) continue;
			const std::wstring path = dir + L"\\" + fd.cFileName;
			if( (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ) {
				dirs.push_back(path);
				continue;
			}
			if( !IsFontExt(fd.cFileName) ) continue;
			std::wstring family;
			if( FamilyFromFontFile(path, family) )
				names.push_back(family);
		} while( ::FindNextFileW(found, &fd) );
		::FindClose(found);
	}
	SortUnique(names);
}

int CALLBACK EnumFamilyProc(const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM param)
{
	if( lf == NULL || lf->lfFaceName[0] == 0 || lf->lfFaceName[0] == L'@' ) return 1;
	reinterpret_cast<std::vector<std::wstring>*>(param)->push_back(lf->lfFaceName);
	return 1;
}

void CollectSystemFamilies(std::vector<std::wstring>& names)
{
	names.clear();
	LOGFONTW lf = {};
	lf.lfCharSet = DEFAULT_CHARSET;
	HDC dc = ::GetDC(NULL);
	if( dc == NULL ) return;
	::EnumFontFamiliesExW(dc, &lf, EnumFamilyProc, (LPARAM)&names, 0);
	::ReleaseDC(NULL, dc);
	SortUnique(names);
}

bool SameFamily(const std::wstring& a, const std::wstring& b)
{
	return _wcsicmp(a.c_str(), b.c_str()) == 0;
}

CDuiString SlashSwap(const CDuiString& path)
{
	CDuiString out;
	const int n = path.GetLength();
	for( int i = 0; i < n; ++i ) {
		const TCHAR ch = path.GetAt(i);
		if( ch == _T('/') ) out += _T('\\');
		else if( ch == _T('\\') ) out += _T('/');
		else out += ch;
	}
	return out;
}

bool ExtractSkinBytes(const wchar_t* name, BYTE*& data, DWORD& size)
{
	data = NULL;
	size = 0;
	if( name == NULL || name[0] == 0 ) return false;
	CZipFile* zip = reinterpret_cast<CZipFile*>(CPaintManagerUI::GetResourceZipHandle());
	if( zip == NULL ) return false;
	if( zip->ExtractMemory(name, &data, &size) && data != NULL ) return true;
	const CDuiString alt = SlashSwap(name);
	if( alt == name ) return false;
	return zip->ExtractMemory(alt.GetData(), &data, &size) && data != NULL;
}

bool CoverFromDecoder(IWICImagingFactory* wic, IWICBitmapDecoder* decoder, int w, int h, std::vector<BYTE>& straight)
{
	IWICBitmapFrameDecode* frame = NULL;
	HRESULT hr = decoder->GetFrame(0, &frame);
	if( FAILED(hr) || frame == NULL ) return false;
	UINT srcW = 0, srcH = 0;
	frame->GetSize(&srcW, &srcH);
	if( srcW < 1 || srcH < 1 ) {
		frame->Release();
		return false;
	}

	IWICBitmapScaler* scaler = NULL;
	IWICFormatConverter* conv = NULL;
	hr = wic->CreateBitmapScaler(&scaler);
	if( SUCCEEDED(hr) ) hr = scaler->Initialize(frame, (UINT)w, (UINT)h, WICBitmapInterpolationModeFant);
	if( SUCCEEDED(hr) ) hr = wic->CreateFormatConverter(&conv);
	if( SUCCEEDED(hr) )
		hr = conv->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
	if( SUCCEEDED(hr) ) {
		straight.resize((size_t)w * (size_t)h * 4);
		hr = conv->CopyPixels(NULL, (UINT)w * 4, (UINT)straight.size(), straight.data());
	}
	if( conv ) conv->Release();
	if( scaler ) scaler->Release();
	frame->Release();
	if( FAILED(hr) ) straight.clear();
	return SUCCEEDED(hr) && !straight.empty();
}

bool LoadCoverStraight(const wchar_t* path, int w, int h, std::vector<BYTE>& straight)
{
	straight.clear();
	if( path == NULL || path[0] == 0 || w < 1 || h < 1 ) return false;
	IWICImagingFactory* wic = SharedWic();
	if( wic == NULL ) return false;

	if( ::GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES ) {
		IWICBitmapDecoder* decoder = NULL;
		const HRESULT hr = wic->CreateDecoderFromFilename(path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
		if( FAILED(hr) || decoder == NULL ) return false;
		const bool ok = CoverFromDecoder(wic, decoder, w, h, straight);
		decoder->Release();
		return ok;
	}

	BYTE* data = NULL;
	DWORD size = 0;
	if( !ExtractSkinBytes(path, data, size) ) return false;
	IWICStream* stream = NULL;
	HRESULT hr = wic->CreateStream(&stream);
	if( SUCCEEDED(hr) ) hr = stream->InitializeFromMemory(data, size);
	IWICBitmapDecoder* decoder = NULL;
	if( SUCCEEDED(hr) )
		hr = wic->CreateDecoderFromStream(stream, NULL, WICDecodeMetadataCacheOnDemand, &decoder);
	bool ok = false;
	if( SUCCEEDED(hr) && decoder != NULL )
		ok = CoverFromDecoder(wic, decoder, w, h, straight);
	if( decoder ) decoder->Release();
	if( stream ) stream->Release();
	delete[] data;
	return ok;
}

void CompositeBackground(std::vector<BYTE>& textPremul, int w, int h, const wchar_t* path)
{
	if( path == NULL || path[0] == 0 || w < 1 || h < 1 ) return;
	std::vector<BYTE> bg;
	if( !LoadCoverStraight(path, w, h, bg) || bg.size() != textPremul.size() ) return;
	const int count = w * h;
	for( int i = 0; i < count; ++i ) {
		const int alpha = textPremul[(size_t)i * 4 + 3];
		const int inv = 255 - alpha;
		for( int c = 0; c < 3; ++c ) {
			int value = (int)textPremul[(size_t)i * 4 + c] + (int)bg[(size_t)i * 4 + c] * inv / 255;
			if( value > 255 ) value = 255;
			textPremul[(size_t)i * 4 + c] = (BYTE)value;
		}
		textPremul[(size_t)i * 4 + 3] = 255;
	}
}

} // namespace

class CCenteredPreviewUI : public CControlUI
{
public:
	CCenteredPreviewUI();
	~CCenteredPreviewUI();
	void SetPlaced(const std::vector<PlacedText>& items, int refSide, int exportW, int exportH, const CDuiString& background, int selected);
	bool GetContentRect(RECT& rc) const;
	bool GetSelectionBox(RECT& rc) const;
	float GetUsedPx() const;
	bool DoPaint(IRenderContext& ctx, const RECT& rcPaint, CControlUI* pStopControl) override;

private:
	void ReleaseBitmap();
	void EnsureBitmap();
	void CanvasSize(int viewW, int viewH, int& bw, int& bh) const;
	void PaintSelection(IRenderContext& ctx) const;

	std::vector<PlacedText> m_items;
	int m_refSide;
	int m_exportW;
	int m_exportH;
	int m_sel;
	CDuiString m_background;
	RECT m_box;
	float m_usedPx;
	bool m_boxOk;
	HBITMAP m_bitmap;
	int m_bw;
	int m_bh;
	bool m_dirty;
};

CCenteredPreviewUI::CCenteredPreviewUI()
	: m_refSide(1080)
	, m_exportW(1920)
	, m_exportH(1080)
	, m_sel(-1)
	, m_box()
	, m_usedPx(0)
	, m_boxOk(false)
	, m_bitmap(NULL)
	, m_bw(0)
	, m_bh(0)
	, m_dirty(true)
{
	SetMouseEnabled(false);
}

CCenteredPreviewUI::~CCenteredPreviewUI()
{
	ReleaseBitmap();
}

void CCenteredPreviewUI::ReleaseBitmap()
{
	if( m_bitmap != NULL ) {
		::DeleteObject(m_bitmap);
		m_bitmap = NULL;
	}
	m_bw = 0;
	m_bh = 0;
}

void CCenteredPreviewUI::SetPlaced(const std::vector<PlacedText>& items, int refSide, int exportW, int exportH, const CDuiString& background, int selected)
{
	if( exportW < 1 ) exportW = 1;
	if( exportH < 1 ) exportH = 1;
	if( !m_dirty && m_sel == selected && m_refSide == refSide
		&& m_exportW == exportW && m_exportH == exportH
		&& m_background == background && SamePlaced(m_items, items) )
		return;
	m_items = items;
	m_refSide = refSide;
	m_exportW = exportW;
	m_exportH = exportH;
	m_sel = selected;
	m_background = background;
	m_dirty = true;
	Invalidate();
}

void CCenteredPreviewUI::CanvasSize(int viewW, int viewH, int& bw, int& bh) const
{
	if( viewW < 1 ) viewW = 1;
	if( viewH < 1 ) viewH = 1;
	const int ew = m_exportW > 0 ? m_exportW : viewW;
	const int eh = m_exportH > 0 ? m_exportH : viewH;
	const double sx = (double)viewW / (double)ew;
	const double sy = (double)viewH / (double)eh;
	const double scale = sx < sy ? sx : sy;
	bw = (int)((double)ew * scale + 0.5);
	bh = (int)((double)eh * scale + 0.5);
	if( bw < 1 ) bw = 1;
	if( bh < 1 ) bh = 1;
	if( bw > viewW ) bw = viewW;
	if( bh > viewH ) bh = viewH;
}

bool CCenteredPreviewUI::GetContentRect(RECT& rc) const
{
	const int viewW = (int)(m_rcItem.right - m_rcItem.left);
	const int viewH = (int)(m_rcItem.bottom - m_rcItem.top);
	int bw = 1, bh = 1;
	CanvasSize(viewW, viewH, bw, bh);
	rc.left = m_rcItem.left + (viewW - bw) / 2;
	rc.top = m_rcItem.top + (viewH - bh) / 2;
	rc.right = rc.left + bw;
	rc.bottom = rc.top + bh;
	return viewW > 0 && viewH > 0;
}

bool CCenteredPreviewUI::GetSelectionBox(RECT& rc) const
{
	if( !m_boxOk || m_bw < 1 || m_bh < 1 ) return false;
	RECT content = {};
	if( !GetContentRect(content) ) return false;
	const float sx = (float)(content.right - content.left) / (float)m_bw;
	const float sy = (float)(content.bottom - content.top) / (float)m_bh;
	rc.left = content.left + (int)(m_box.left * sx);
	rc.top = content.top + (int)(m_box.top * sy);
	rc.right = content.left + (int)(m_box.right * sx);
	rc.bottom = content.top + (int)(m_box.bottom * sy);
	return true;
}

float CCenteredPreviewUI::GetUsedPx() const
{
	return m_usedPx;
}

void CCenteredPreviewUI::EnsureBitmap()
{
	const int viewW = (int)(m_rcItem.right - m_rcItem.left);
	const int viewH = (int)(m_rcItem.bottom - m_rcItem.top);
	int w = 1, h = 1;
	CanvasSize(viewW, viewH, w, h);
	if( w < 1 || h < 1 ) return;
	if( !m_dirty && m_bitmap != NULL && m_bw == w && m_bh == h ) return;

	ReleaseBitmap();
	m_dirty = false;
	if( m_items.empty() && m_background.IsEmpty() ) return;

	std::vector<BYTE> px;
	InkBox box = {};
	ComposePlaced(m_items, w, h, m_refSide, px, m_sel, &box, NULL, NULL);
	m_boxOk = box.ok;
	m_usedPx = box.usedPx;
	if( box.ok ) {
		m_box.left = box.left;
		m_box.top = box.top;
		m_box.right = box.right;
		m_box.bottom = box.bottom;
	}
	CompositeBackground(px, w, h, m_background.GetData());

	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = w;
	bi.bmiHeader.biHeight = -h;
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32;
	bi.bmiHeader.biCompression = BI_RGB;
	void* bits = NULL;
	m_bitmap = ::CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
	if( m_bitmap == NULL || bits == NULL ) {
		ReleaseBitmap();
		return;
	}
	memcpy(bits, px.data(), px.size());
	m_bw = w;
	m_bh = h;
}

bool CCenteredPreviewUI::DoPaint(IRenderContext& ctx, const RECT& rcPaint, CControlUI* pStopControl)
{
	if( !CControlUI::DoPaint(ctx, rcPaint, pStopControl) ) return false;
	EnsureBitmap();
	if( m_bitmap == NULL ) return true;
	RECT content = {};
	if( !GetContentRect(content) ) return true;
	RECT part = { 0, 0, m_bw, m_bh };
	RECT corners = { 0, 0, 0, 0 };
	ctx.DrawImage(m_bitmap, content, rcPaint, part, corners, true);
	PaintSelection(ctx);
	return true;
}

void CCenteredPreviewUI::PaintSelection(IRenderContext& ctx) const
{
	if( !m_boxOk || m_sel < 0 ) return;
	RECT frame = {};
	if( !GetSelectionBox(frame) ) return;
	::InflateRect(&frame, 6, 6);
	ctx.DrawRect(frame, 1, 0x0D6EFDFF);
	const int hs = 7;
	const int xs[3] = { frame.left, (frame.left + frame.right) / 2, frame.right };
	const int ys[3] = { frame.top, (frame.top + frame.bottom) / 2, frame.bottom };
	for( int iy = 0; iy < 3; ++iy ) {
		for( int ix = 0; ix < 3; ++ix ) {
			if( ix == 1 && iy == 1 ) continue;
			RECT grip = { xs[ix] - hs / 2, ys[iy] - hs / 2, xs[ix] - hs / 2 + hs, ys[iy] - hs / 2 + hs };
			ctx.DrawColor(grip, 0xFFFFFFFF);
			ctx.DrawRect(grip, 1, 0x0D6EFDFF);
		}
	}
}

DUI_BEGIN_MESSAGE_MAP(CImageWnd, WindowImplBase)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_CLICK, CImageWnd::OnClick)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_MENU, CImageWnd::OnContextMenu)
DUI_END_MESSAGE_MAP()

CImageWnd::CImageWnd()
	: m_pEdit(NULL)
	, m_pFont(NULL)
	, m_pFontSize(NULL)
	, m_pBold(NULL)
	, m_pItalic(NULL)
	, m_pUnderline(NULL)
	, m_pStrike(NULL)
	, m_pPreview(NULL)
	, m_pPlate(NULL)
	, m_pPalette(NULL)
	, m_pColorLabel(NULL)
	, m_pFormatHint(NULL)
	, m_pFormat(NULL)
	, m_pEditW(NULL)
	, m_pEditH(NULL)
	, m_pColorRow(NULL)
	, m_pTextList(NULL)
	, m_pBgList(NULL)
	, m_sel(-1)
	, m_drag(false)
	, m_dragX(0)
	, m_dragY(0)
	, m_dragCx(0.5f)
	, m_dragCy(0.5f)
	, m_dragMode(0)
	, m_dragUserPx(0)
	, m_anchorX(0)
	, m_anchorY(0)
	, m_dwColor(0x000000FF)
	, m_syncing(false)
	, m_exporting(false)
	, m_closeAfterExport(false)
	, m_yieldBroken(false)
	, m_job(0)
	, m_jobW(1920)
	, m_jobH(1080)
	, m_jobOk(false)
{
}

CImageWnd::~CImageWnd()
{
}

void CImageWnd::Open(HWND owner, LPCTSTR text)
{
	if( owner == NULL || !::IsWindow(owner) ) return;

	RECT rcOwner = {};
	if( !::GetWindowRect(owner, &rcOwner) ) return;
	const int width = rcOwner.right - rcOwner.left;
	const int height = rcOwner.bottom - rcOwner.top;
	if( width < 1 || height < 1 ) return;

	CImageWnd* wnd = new CImageWnd();
	if( text != NULL ) wnd->m_seed = text;
	wnd->Create(owner, _T("图片"),
		WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		0, rcOwner.left, rcOwner.top, width, height);
	if( wnd->GetHWND() == NULL ) {
		delete wnd;
		return;
	}

	::SetWindowPos(wnd->GetHWND(), NULL, rcOwner.left, rcOwner.top, width, height,
		SWP_NOZORDER | SWP_NOACTIVATE);
	wnd->SetSyncOwnerMove(true);
	wnd->SetSyncOwnerSize(true);
	wnd->ShowModal();
}

void CImageWnd::OnFinalMessage(HWND hWnd)
{
	WindowImplBase::OnFinalMessage(hWnd);
	delete this;
}

CDuiString CImageWnd::GetSkinFile()
{
	return _T("image.html");
}

CDuiString CImageWnd::GetSkinFolder()
{
	return _T("");
}

LPCTSTR CImageWnd::GetWindowClassName() const
{
	return _T("SawadeeImageWnd");
}

void CImageWnd::BuildColorSwatches()
{
	if( m_pColorRow == NULL ) return;
	m_pColorRow->RemoveAll();

	struct Swatch { LPCTSTR name; DWORD color; };
	const Swatch items[] = {
		{ _T("黑"), 0x000000FF },
		{ _T("白"), 0xFFFFFFFF },
		{ _T("灰"), 0x6C757DFF },
		{ _T("红"), 0xDC3545FF },
		{ _T("绿"), 0x198754FF },
		{ _T("蓝"), 0x0D6EFDFF },
		{ _T("橙"), 0xFD7E14FF },
		{ _T("紫"), 0x722ED1FF },
	};
	for( int i = 0; i < (int)(sizeof(items) / sizeof(items[0])); ++i ) {
		CFontIconUI* p = new CFontIconUI;
		p->SetSizePreset(28);
		p->SetShape(CFontIconUI::ShapeRounded);
		CDuiString name;
		name.Format(_T("swatch_%d"), i);
		p->SetName(name.GetData());
		p->SetClickable(true);
		p->SetToolTip(items[i].name);
		CDuiString bk;
		bk.Format(_T("#%08X"), items[i].color);
		p->SetAttribute(_T("background-color"), bk.GetData());
		p->SetAttribute(_T("border"), _T("1px solid"));
		p->SetAttribute(_T("border-color"), _T("var(--color-border)"));
		CDuiString tag;
		tag.Format(_T("%08X"), items[i].color);
		p->SetUserData(tag.GetData());
		m_pColorRow->Add(p);
	}

	CButtonUI* custom = new CButtonUI;
	custom->SetName(_T("btn_custom_color"));
	custom->SetText(_T("自定义"));
	custom->SetAttribute(_T("kind"), _T("default"));
	custom->SetFixedWidth(80);
	custom->SetFixedHeight(32);
	m_pColorRow->Add(custom);

	int rowWidth = 0;
	const int count = m_pColorRow->GetCount();
	const int gap = m_pColorRow->GetGap();
	for( int i = 0; i < count; ++i ) {
		CControlUI* child = m_pColorRow->GetItemAt(i);
		if( child == NULL ) continue;
		rowWidth += child->GetFixedWidth();
		if( i + 1 < count ) rowWidth += gap;
	}
	m_pColorRow->SetFixedWidth(rowWidth);
}

void CImageWnd::ApplyColor(DWORD dwColor)
{
	m_dwColor = dwColor;
	if( m_pColorLabel != NULL ) {
		CDuiString s;
		s.Format(_T("#%06X"), (dwColor >> 8) & 0xFFFFFF);
		m_pColorLabel->SetText(s.GetData());
	}
	if( m_pPalette != NULL )
		m_pPalette->SetSelectColor(m_dwColor);
	if( !m_syncing )
		PushStyle();
	SyncPreviewFrame();
	SyncPreview();
}

void CImageWnd::ReadFont(CDuiString& family, int& userPx, bool& bold, bool& italic, bool& underline, bool& strike) const
{
	family = _T("TH Sarabun New");
	if( m_pFont != NULL ) {
		const CDuiString picked = m_pFont->GetText();
		if( !picked.IsEmpty() ) family = picked;
	}
	userPx = 0;
	if( m_pFontSize != NULL ) userPx = _ttoi(m_pFontSize->GetText().GetData());
	if( userPx < 0 ) userPx = 0;
	if( userPx > 4096 ) userPx = 4096;
	bold = m_pBold != NULL && m_pBold->IsSelected();
	italic = m_pItalic != NULL && m_pItalic->IsSelected();
	underline = m_pUnderline != NULL && m_pUnderline->IsSelected();
	strike = m_pStrike != NULL && m_pStrike->IsSelected();
}

void CImageWnd::SelectFamily(const CDuiString& family)
{
	if( m_pFont == NULL ) return;
	int found = 0;
	const int count = m_pFont->GetCount();
	for( int i = 0; i < count; ++i ) {
		CControlUI* item = m_pFont->GetItemAt(i);
		if( item != NULL && _tcsicmp(item->GetText().GetData(), family.GetData()) == 0 ) {
			found = i;
			break;
		}
	}
	m_pFont->SelectItem(found);
}

void CImageWnd::PushStyle()
{
	if( m_syncing ) return;
	CDuiString family;
	int userPx = 0;
	bool bold = false, italic = false, underline = false, strike = false;
	ReadFont(family, userPx, bold, italic, underline, strike);
	const int begin = (m_sel >= 0 && m_sel < (int)m_items.size()) ? m_sel : 0;
	const int end = (m_sel >= 0 && m_sel < (int)m_items.size()) ? m_sel + 1 : (int)m_items.size();
	for( int i = begin; i < end; ++i ) {
		m_items[i].family = family;
		m_items[i].userPx = userPx;
		m_items[i].bold = bold;
		m_items[i].italic = italic;
		m_items[i].underline = underline;
		m_items[i].strike = strike;
		m_items[i].color = m_dwColor;
	}
}

void CImageWnd::AppendPieceButton(int index)
{
	if( m_pTextList == NULL ) return;
	CButtonUI* button = new CButtonUI;
	CDuiString name;
	name.Format(_T("piece_%d"), index);
	button->SetName(name.GetData());
	CDuiString label;
	label.Format(_T("%d"), index + 1);
	button->SetText(label.GetData());
	button->SetFixedHeight(36);
	button->SetAttribute(_T("kind"), _T("default"));
	button->SetAttribute(_T("text-align"), _T("center"));
	m_pTextList->Add(button);
}

void CImageWnd::UpdatePieceSelection()
{
	if( m_pTextList == NULL ) return;
	const int count = m_pTextList->GetCount();
	for( int i = 0; i < count; ++i ) {
		CControlUI* item = m_pTextList->GetItemAt(i);
		if( item == NULL ) continue;
		const bool on = (i == m_sel);
		item->SetAttribute(_T("border"), on ? _T("2px solid") : _T("1px solid"));
		item->SetAttribute(_T("border-color"), on ? _T("var(--color-primary)") : _T("var(--color-border)"));
	}
}

void CImageWnd::AddPiece()
{
	if( m_pEdit == NULL ) return;
	const CDuiString text = m_pEdit->GetText();
	if( text.IsEmpty() ) return;
	TextPiece item;
	item.text = text;
	ReadFont(item.family, item.userPx, item.bold, item.italic, item.underline, item.strike);
	item.color = m_dwColor;
	const int n = (int)m_items.size();
	static const float kShift[] = { 0.f, 0.14f, -0.14f, 0.28f, -0.28f, 0.42f, -0.42f };
	item.cx = 0.5f;
	item.cy = 0.5f + kShift[n % 7];
	m_items.push_back(item);
	m_sel = n;
	AppendPieceButton(n);
	UpdatePieceSelection();
	SyncPreview();
}

void CImageWnd::ShowPiece(int index)
{
	if( index < 0 || index >= (int)m_items.size() ) return;
	const TextPiece& item = m_items[index];
	m_syncing = true;
	if( m_pEdit != NULL )
		m_pEdit->SetText(item.text.GetData());
	if( m_pFontSize != NULL ) {
		CDuiString size;
		if( item.userPx > 0 ) size.Format(_T("%d"), item.userPx);
		m_pFontSize->SetText(size.GetData());
	}
	SelectFamily(item.family);
	if( m_pBold != NULL ) m_pBold->Selected(item.bold, false);
	if( m_pItalic != NULL ) m_pItalic->Selected(item.italic, false);
	if( m_pUnderline != NULL ) m_pUnderline->Selected(item.underline, false);
	if( m_pStrike != NULL ) m_pStrike->Selected(item.strike, false);
	m_dwColor = item.color;
	if( m_pColorLabel != NULL ) {
		CDuiString s;
		s.Format(_T("#%06X"), (item.color >> 8) & 0xFFFFFF);
		m_pColorLabel->SetText(s.GetData());
	}
	if( m_pPalette != NULL )
		m_pPalette->SetSelectColor(item.color);
	m_syncing = false;
	ApplyInputFont();
	SyncPreviewFrame();
	SyncPreview();
}

int CImageWnd::HitPiece(int x, int y) const
{
	if( m_pPreview == NULL || m_sel < 0 ) return 0;
	RECT ink = {};
	if( !m_pPreview->GetSelectionBox(ink) ) return 0;
	RECT frame = ink;
	::InflateRect(&frame, 6, 6);
	const int grip = 8;
	const int xs[3] = { frame.left, (frame.left + frame.right) / 2, frame.right };
	const int ys[3] = { frame.top, (frame.top + frame.bottom) / 2, frame.bottom };
	const int codes[3][3] = {
		{ 6, 2, 7 },
		{ 4, 1, 5 },
		{ 8, 3, 9 },
	};
	for( int iy = 0; iy < 3; ++iy ) {
		for( int ix = 0; ix < 3; ++ix ) {
			if( ix == 1 && iy == 1 ) continue;
			if( abs(x - xs[ix]) <= grip && abs(y - ys[iy]) <= grip )
				return codes[iy][ix];
		}
	}
	RECT outer = frame;
	::InflateRect(&outer, 4, 4);
	POINT pt = { x, y };
	if( !::PtInRect(&outer, pt) ) return 0;
	RECT inner = frame;
	::InflateRect(&inner, -8, -8);
	if( inner.left >= inner.right || inner.top >= inner.bottom || !::PtInRect(&inner, pt) ) {
		if( abs(y - frame.top) <= grip || abs(y - frame.bottom) <= grip )
			return abs(y - frame.top) <= abs(y - frame.bottom) ? 2 : 3;
		return abs(x - frame.left) <= abs(x - frame.right) ? 4 : 5;
	}
	return 1;
}

bool CImageWnd::PieceCursor()
{
	LPCTSTR cursor = NULL;
	if( m_drag )
		cursor = (m_dragMode == 1) ? IDC_SIZENWSE : IDC_SIZEALL;
	else {
		POINT pt = {};
		::GetCursorPos(&pt);
		::ScreenToClient(m_hWnd, &pt);
		const int hit = HitPiece(pt.x, pt.y);
		if( hit == 1 ) cursor = IDC_SIZEALL;
		else if( hit == 2 || hit == 3 ) cursor = IDC_SIZENS;
		else if( hit == 4 || hit == 5 ) cursor = IDC_SIZEWE;
		else if( hit >= 6 ) cursor = IDC_SIZENWSE;
	}
	if( cursor == NULL ) return false;
	::SetCursor(::LoadCursor(NULL, cursor));
	return true;
}

bool CImageWnd::TrackPiece(UINT uMsg, LPARAM lParam)
{
	if( m_pPreview == NULL ) return false;
	const int x = (int)(short)LOWORD(lParam);
	const int y = (int)(short)HIWORD(lParam);
	if( uMsg == WM_LBUTTONDOWN ) {
		if( m_sel < 0 || m_sel >= (int)m_items.size() ) return false;
		const int hit = HitPiece(x, y);
		if( hit == 0 ) return false;
		m_drag = true;
		m_dragMode = (hit == 1) ? 0 : 1;
		m_dragX = x;
		m_dragY = y;
		m_dragCx = m_items[m_sel].cx;
		m_dragCy = m_items[m_sel].cy;
		RECT content = {};
		if( !m_pPreview->GetContentRect(content) ) content = m_pPreview->GetPos();
		const int w = content.right - content.left;
		const int h = content.bottom - content.top;
		m_anchorX = content.left + (int)(m_items[m_sel].cx * (float)w + 0.5f);
		m_anchorY = content.top + (int)(m_items[m_sel].cy * (float)h + 0.5f);
		int userPx = m_items[m_sel].userPx;
		if( userPx <= 0 ) {
			int exportW = 1920, exportH = 1080;
			ReadExportSize(exportW, exportH);
			const int ref = (exportW < exportH) ? exportW : exportH;
			const int side = (w < h) ? w : h;
			const float used = m_pPreview->GetUsedPx();
			if( side > 0 && used > 0 )
				userPx = (int)(used * (float)ref / (float)side + 0.5f);
		}
		if( userPx < 1 ) userPx = 1;
		m_dragUserPx = userPx;
		::SetCapture(m_hWnd);
		return true;
	}
	if( uMsg == WM_MOUSEMOVE ) {
		if( !m_drag || m_sel < 0 || m_sel >= (int)m_items.size() ) return false;
		RECT content = {};
		if( !m_pPreview->GetContentRect(content) ) content = m_pPreview->GetPos();
		const int w = content.right - content.left;
		const int h = content.bottom - content.top;
		if( w < 1 || h < 1 ) return true;
		if( m_dragMode == 0 ) {
			float cx = m_dragCx + (float)(x - m_dragX) / (float)w;
			float cy = m_dragCy + (float)(y - m_dragY) / (float)h;
			if( cx < 0 ) cx = 0;
			if( cx > 1 ) cx = 1;
			if( cy < 0 ) cy = 0;
			if( cy > 1 ) cy = 1;
			m_items[m_sel].cx = cx;
			m_items[m_sel].cy = cy;
		}
		else {
			const float dx0 = (float)(m_dragX - m_anchorX);
			const float dy0 = (float)(m_dragY - m_anchorY);
			const float dx1 = (float)(x - m_anchorX);
			const float dy1 = (float)(y - m_anchorY);
			float d0 = sqrtf(dx0 * dx0 + dy0 * dy0);
			float d1 = sqrtf(dx1 * dx1 + dy1 * dy1);
			if( d0 < 8.0f ) d0 = 8.0f;
			int px = (int)((float)m_dragUserPx * d1 / d0 + 0.5f);
			if( px < 1 ) px = 1;
			if( px > 4096 ) px = 4096;
			m_items[m_sel].userPx = px;
			if( m_pFontSize != NULL ) {
				CDuiString size;
				size.Format(_T("%d"), px);
				m_syncing = true;
				m_pFontSize->SetText(size.GetData());
				m_syncing = false;
			}
		}
		SyncPreview();
		return true;
	}
	if( uMsg == WM_LBUTTONUP ) {
		if( !m_drag ) return false;
		m_drag = false;
		::ReleaseCapture();
		return true;
	}
	if( uMsg == WM_CAPTURECHANGED )
		m_drag = false;
	return false;
}

void CImageWnd::ApplyInputFont()
{
	if( m_pEdit == NULL ) return;
	CDuiString family;
	int userPx = 0;
	bool bold = false, italic = false, underline = false, strike = false;
	ReadFont(family, userPx, bold, italic, underline, strike);
	m_pEdit->SetAttribute(_T("font-family"), family.GetData());
	m_pEdit->SetAttribute(_T("font-weight"), bold ? _T("bold") : _T("normal"));
	m_pEdit->SetAttribute(_T("font-style"), italic ? _T("italic") : _T("normal"));
	if( underline && strike ) m_pEdit->SetAttribute(_T("text-decoration"), _T("underline line-through"));
	else if( underline ) m_pEdit->SetAttribute(_T("text-decoration"), _T("underline"));
	else if( strike ) m_pEdit->SetAttribute(_T("text-decoration"), _T("line-through"));
	else m_pEdit->SetAttribute(_T("text-decoration"), _T("none"));
}

void CImageWnd::FillFontList()
{
	if( m_pFont == NULL ) return;
	std::vector<std::wstring> builtIn;
	std::vector<std::wstring> system;
	CollectBuiltInFamilies(builtIn);
	CollectSystemFamilies(system);
	if( builtIn.empty() ) builtIn.push_back(L"TH Sarabun New");

	m_pFont->RemoveAll();
	int selected = 0;
	int index = 0;
	for( size_t i = 0; i < builtIn.size(); ++i ) {
		CListLabelElementUI* item = new CListLabelElementUI;
		item->SetText(builtIn[i].c_str());
		item->SetFixedHeight(30);
		m_pFont->Add(item);
		if( _wcsicmp(builtIn[i].c_str(), L"TH Sarabun New") == 0 ) selected = index;
		++index;
	}
	for( size_t i = 0; i < system.size(); ++i ) {
		bool skip = false;
		for( size_t j = 0; j < builtIn.size(); ++j ) {
			if( SameFamily(system[i], builtIn[j]) ) {
				skip = true;
				break;
			}
		}
		if( skip ) continue;
		CListLabelElementUI* item = new CListLabelElementUI;
		item->SetText(system[i].c_str());
		item->SetFixedHeight(30);
		m_pFont->Add(item);
		++index;
	}
	m_pFont->SelectItem(selected);
}

void CImageWnd::SyncPreview()
{
	if( m_exporting || m_syncing || m_pPreview == NULL ) return;
	std::vector<PlacedText> placed;
	placed.reserve(m_items.size());
	for( size_t i = 0; i < m_items.size(); ++i ) {
		PlacedText one;
		one.text = m_items[i].text.GetData();
		one.family = m_items[i].family.GetData();
		one.userPx = m_items[i].userPx;
		one.bold = m_items[i].bold;
		one.italic = m_items[i].italic;
		one.underline = m_items[i].underline;
		one.strike = m_items[i].strike;
		one.color = m_items[i].color;
		one.cx = m_items[i].cx;
		one.cy = m_items[i].cy;
		placed.push_back(one);
	}
	int exportW = 1920, exportH = 1080;
	ReadExportSize(exportW, exportH);
	const int ref = (exportW < exportH) ? exportW : exportH;
	m_pPreview->SetPlaced(placed, ref, exportW, exportH, m_bgPath, m_sel);
}

void CImageWnd::SyncFormatHint()
{
	if( m_pFormatHint == NULL ) return;
	const CDuiString ext = GetFormatExt();
	if( ext == _T("jpg") )
		m_pFormatHint->SetText(_T("白底"));
	else if( ext == _T("bmp") )
		m_pFormatHint->SetText(_T("白底"));
	else
		m_pFormatHint->SetText(_T("透明"));
}

bool CImageWnd::HasPieceText() const
{
	for( size_t i = 0; i < m_items.size(); ++i ) {
		if( !m_items[i].text.IsEmpty() ) return true;
	}
	return false;
}

CDuiString CImageWnd::FirstPieceText() const
{
	for( size_t i = 0; i < m_items.size(); ++i ) {
		if( !m_items[i].text.IsEmpty() ) return m_items[i].text;
	}
	return _T("");
}

bool CImageWnd::RenderCanvas(int w, int h, std::vector<BYTE>& premul)
{
	std::vector<PlacedText> placed;
	placed.reserve(m_items.size());
	for( size_t i = 0; i < m_items.size(); ++i ) {
		PlacedText one;
		one.text = m_items[i].text.GetData();
		one.family = m_items[i].family.GetData();
		one.userPx = m_items[i].userPx;
		one.bold = m_items[i].bold;
		one.italic = m_items[i].italic;
		one.underline = m_items[i].underline;
		one.strike = m_items[i].strike;
		one.color = m_items[i].color;
		one.cx = m_items[i].cx;
		one.cy = m_items[i].cy;
		placed.push_back(one);
	}
	int exportW = 1920, exportH = 1080;
	ReadExportSize(exportW, exportH);
	const int ref = (exportW < exportH) ? exportW : exportH;
	const CDuiString background = m_bgPath;
	const bool any = ComposePlaced(placed, w, h, ref, premul, -1, NULL, &CImageWnd::YieldExport, this);
	if( !any ) return false;
	CompositeBackground(premul, w, h, background.GetData());
	return true;
}

CDuiString CImageWnd::GetFormatExt() const
{
	if( m_pFormat == NULL ) return _T("png");
	LPCTSTR v = m_pFormat->GetSelectedValue();
	if( v == NULL || *v == _T('\0') ) return _T("png");
	return v;
}

bool CImageWnd::ReadExportSize(int& w, int& h) const
{
	w = 1920;
	h = 1080;
	if( m_pEditW != NULL ) w = _ttoi(m_pEditW->GetText().GetData());
	if( m_pEditH != NULL ) h = _ttoi(m_pEditH->GetText().GetData());
	if( w <= 0 ) w = 1920;
	if( h <= 0 ) h = 1080;
	w = ClampExportSize(w);
	h = ClampExportSize(h);
	return true;
}

void CImageWnd::PasteClipboard()
{
	if( m_pEdit == NULL ) return;
	if( !::OpenClipboard(m_hWnd) ) return;
	CDuiString text;
	HANDLE data = ::GetClipboardData(CF_UNICODETEXT);
	if( data != NULL ) {
		const wchar_t* p = (const wchar_t*)::GlobalLock(data);
		if( p != NULL ) {
			text = p;
			::GlobalUnlock(data);
		}
	}
	::CloseClipboard();
	if( text.IsEmpty() ) return;
	m_pEdit->SetText(text.GetData());
	if( m_sel >= 0 && m_sel < (int)m_items.size() ) {
		m_items[m_sel].text = text;
		SyncPreview();
	}
}

class CExportWaitWnd : public WindowImplBase
{
public:
	explicit CExportWaitWnd(CImageWnd* owner)
		: m_owner(owner)
		, m_done(false)
	{
	}

	CDuiString GetSkinFile() override { return _T("export_wait.html"); }
	CDuiString GetSkinFolder() { return _T(""); }
	LPCTSTR GetWindowClassName() const override { return _T("SawadeeExportWait"); }
	void InitWindow() override { ::PostMessage(m_hWnd, WM_APP + 41, 0, 0); }
	void OnFinalMessage(HWND hWnd) override
	{
		WindowImplBase::OnFinalMessage(hWnd);
		delete this;
	}

	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override
	{
		if( uMsg == WM_APP + 41 ) {
			::UpdateWindow(m_hWnd);
			if( m_owner != NULL ) m_owner->RunQueuedExport();
			m_done = true;
			Close(IDOK);
			return 0;
		}
		if( !m_done && (uMsg == WM_CLOSE || (uMsg == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_CLOSE)) ) {
			HWND owner = ::GetWindow(m_hWnd, GW_OWNER);
			if( owner != NULL ) ::EnableWindow(owner, FALSE);
			return 0;
		}
		return WindowImplBase::HandleMessage(uMsg, wParam, lParam);
	}

private:
	CImageWnd* m_owner;
	bool m_done;
};

HWND CImageWnd::ShowToast(LPCTSTR text, ControlKind kind, int duration)
{
	return CToast::Show(text, CToastOptions()
		.Owner(m_hWnd)
		.Align(ToastAlign_WindowTopCenter)
		.Kind(kind)
		.Duration(duration));
}

HWND CImageWnd::ShowToast(LPCTSTR title, LPCTSTR text, ControlKind kind, int duration)
{
	return CToast::Show(title, text, CToastOptions()
		.Owner(m_hWnd)
		.Align(ToastAlign_WindowTopCenter)
		.Kind(kind)
		.Duration(duration));
}

bool CImageWnd::YieldExport(void* self)
{
	return static_cast<CImageWnd*>(self)->PumpExport();
}

bool CImageWnd::PumpExport()
{
	for( int n = 0; n < 32; ++n ) {
		MSG msg;
		if( !::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) ) break;
		if( msg.message == WM_QUIT ) {
			::PostQuitMessage((int)msg.wParam);
			m_yieldBroken = true;
			return false;
		}
		if( msg.hwnd == m_hWnd && (msg.message == WM_CLOSE || msg.message == WM_DESTROY) ) {
			m_closeAfterExport = true;
			continue;
		}
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
		if( !::IsWindow(m_hWnd) ) {
			m_yieldBroken = true;
			return false;
		}
	}
	return ::IsWindow(m_hWnd);
}

void CImageWnd::FinishExport(HWND busy)
{
	if( busy != NULL ) CToast::Dismiss(busy);
	m_exporting = false;
	if( m_closeAfterExport && ::IsWindow(m_hWnd) ) {
		m_closeAfterExport = false;
		::PostMessage(m_hWnd, WM_CLOSE, 0, 0);
		return;
	}
	SyncPreview();
}

void CImageWnd::RunQueuedExport()
{
	if( m_job == 2 ) {
		static const int kSizes[] = { 16, 24, 32, 48, 64, 128, 256 };
		std::vector<std::vector<BYTE> > pngs;
		std::vector<int> sizes;
		bool alive = true;
		for( int i = 0; alive && i < (int)(sizeof(kSizes) / sizeof(kSizes[0])); ++i ) {
			if( !PumpExport() ) {
				alive = false;
				break;
			}
			std::vector<BYTE> premul;
			std::vector<BYTE> png;
			if( !RenderCanvas(kSizes[i], kSizes[i], premul) ) {
				if( m_yieldBroken || !::IsWindow(m_hWnd) ) alive = false;
				continue;
			}
			if( !SavePngMemory(premul, kSizes[i], kSizes[i], png) )
				continue;
			sizes.push_back(kSizes[i]);
			pngs.push_back(std::move(png));
		}
		m_jobOk = alive && !m_yieldBroken && !pngs.empty() && WriteIco(m_jobPath.GetData(), pngs, sizes);
		return;
	}

	std::vector<BYTE> premul;
	m_jobOk = !m_yieldBroken && ::IsWindow(m_hWnd)
		&& RenderCanvas(m_jobW, m_jobH, premul)
		&& !m_yieldBroken
		&& SaveRaster(premul, m_jobW, m_jobH, m_jobPath.GetData(), m_jobExt.GetData());
}

void CImageWnd::RunExportModal()
{
	m_exporting = true;
	m_yieldBroken = false;
	m_jobOk = false;
	CExportWaitWnd* wnd = new CExportWaitWnd(this);
	wnd->Create(m_hWnd, _T("导出"),
		WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, WS_EX_TOOLWINDOW, 0, 0, 360, 150);
	if( wnd->GetHWND() == NULL ) {
		delete wnd;
		m_exporting = false;
		ShowToast(_T("导出失败"), CONTROLKIND_DANGER, 4000);
		return;
	}
	wnd->CenterWindow();
	wnd->ShowModal();
	const bool closing = m_closeAfterExport;
	m_exporting = false;
	if( !::IsWindow(m_hWnd) || m_yieldBroken ) return;
	if( closing ) {
		m_closeAfterExport = false;
		::PostMessage(m_hWnd, WM_CLOSE, 0, 0);
		return;
	}
	SyncPreview();
	if( !m_jobOk ) {
		ShowToast(_T("导出失败"), CONTROLKIND_DANGER, 4000);
		return;
	}
	if( m_job == 2 ) {
		ShowToast(_T("已导出"), m_jobPath.GetData(), CONTROLKIND_SUCCESS, 4000);
		return;
	}
	CDuiString tip;
	tip.Format(_T("%s    %d × %d"), m_jobPath.GetData(), m_jobW, m_jobH);
	ShowToast(_T("已导出"), tip.GetData(), CONTROLKIND_SUCCESS, 4000);
}

bool CImageWnd::ExportImage()
{
	if( m_exporting ) return false;
	if( !HasPieceText() ) {
		ShowToast(_T("请先添加要导出的文字"), CONTROLKIND_WARNING, 3000);
		return false;
	}
	int w = 1920, h = 1080;
	ReadExportSize(w, h);
	const CDuiString ext = GetFormatExt();
	LPCTSTR filter = _T("PNG (*.png)\0*.png\0");
	if( ext == _T("jpg") ) filter = _T("JPEG (*.jpg)\0*.jpg\0");
	else if( ext == _T("bmp") ) filter = _T("BMP (*.bmp)\0*.bmp\0");

	CDuiString path;
	if( !PickSavePath(m_hWnd, FileBaseName(FirstPieceText()), filter, ext.GetData(), path) )
		return false;

	m_job = 1;
	m_jobW = w;
	m_jobH = h;
	m_jobPath = path;
	m_jobExt = ext;
	RunExportModal();
	return m_jobOk;
}

bool CImageWnd::ExportIcon()
{
	if( m_exporting ) return false;
	if( !HasPieceText() ) {
		ShowToast(_T("请先添加要导出的文字"), CONTROLKIND_WARNING, 3000);
		return false;
	}
	CDuiString path;
	if( !PickSavePath(m_hWnd, FileBaseName(FirstPieceText()), _T("ICO (*.ico)\0*.ico\0"), _T("ico"), path) )
		return false;

	m_job = 2;
	m_jobPath = path;
	m_jobExt = _T("ico");
	RunExportModal();
	return m_jobOk;
}

void CImageWnd::BuildBackgrounds()
{
	if( m_pBgList == NULL ) return;
	m_pBgList->RemoveAll();

	struct BgFile { std::wstring name; std::wstring source; };
	std::vector<BgFile> files;
	CDuiString dir = CPaintManagerUI::GetInstancePath();
	dir += _T("skin\\Sawadee\\background\\");
	WIN32_FIND_DATAW found;
	HANDLE handle = ::FindFirstFileW((std::wstring(dir.GetData()) + L"*").c_str(), &found);
	if( handle != INVALID_HANDLE_VALUE ) {
		do {
			if( (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ) continue;
			const wchar_t* dot = wcsrchr(found.cFileName, L'.');
			if( dot == NULL ) continue;
			if( _wcsicmp(dot, L".jpg") != 0 && _wcsicmp(dot, L".jpeg") != 0
				&& _wcsicmp(dot, L".png") != 0 && _wcsicmp(dot, L".bmp") != 0 )
				continue;
			BgFile item;
			item.name = found.cFileName;
			item.source = (std::wstring)dir.GetData() + found.cFileName;
			files.push_back(item);
		} while( ::FindNextFileW(handle, &found) );
		::FindClose(handle);
	}

	CZipFile* zip = reinterpret_cast<CZipFile*>(CPaintManagerUI::GetResourceZipHandle());
	if( files.empty() && zip != NULL ) {
		const int count = zip->GetCount();
		for( int i = 0; i < count; ++i ) {
			TZipItem packed;
			if( !zip->GetItem(i, packed) || packed.IsDirectory() ) continue;
			std::wstring entry = packed.name.GetData();
			for( size_t k = 0; k < entry.size(); ++k ) {
				if( entry[k] == L'\\' ) entry[k] = L'/';
			}
			const std::wstring prefix = L"background/";
			if( entry.size() <= prefix.size() || _wcsnicmp(entry.c_str(), prefix.c_str(), prefix.size()) != 0 )
				continue;
			const std::wstring name = entry.substr(prefix.size());
			if( name.find(L'/') != std::wstring::npos ) continue;
			const wchar_t* dot = wcsrchr(name.c_str(), L'.');
			if( dot == NULL ) continue;
			if( _wcsicmp(dot, L".jpg") != 0 && _wcsicmp(dot, L".jpeg") != 0
				&& _wcsicmp(dot, L".png") != 0 && _wcsicmp(dot, L".bmp") != 0 )
				continue;
			bool exists = false;
			for( size_t j = 0; j < files.size(); ++j ) {
				if( _wcsicmp(files[j].name.c_str(), name.c_str()) == 0 ) {
					exists = true;
					break;
				}
			}
			if( exists ) continue;
			BgFile item;
			item.name = name;
			item.source = entry;
			files.push_back(item);
		}
	}
	std::sort(files.begin(), files.end(), [](const BgFile& a, const BgFile& b) {
		return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
	});

	for( size_t i = 0; i < files.size(); ++i ) {
		CDuiString full = files[i].source.c_str();
		CDuiString name;
		name.Format(_T("bg_%d"), (int)i);
		CDuiString num;
		num.Format(_T("%d"), (int)i + 1);

		CVerticalLayoutUI* tile = new CVerticalLayoutUI;
		tile->SetFixedWidth(96);
		tile->SetFixedHeight(96);

		CButtonUI* button = new CButtonUI;
		button->SetName(name.GetData());
		button->SetUserData(full.GetData());
		button->SetFixedWidth(96);
		button->SetFixedHeight(96);
		button->SetAttribute(_T("kind"), _T("none"));
		CDuiString image;
		image.Format(_T("file='background/%s' dest='0,0,96,96'"), files[i].name.c_str());
		button->SetAttribute(_T("background-image"), image.GetData());
		button->SetAttribute(_T("border"), _T("1px solid"));
		button->SetAttribute(_T("border-color"), _T("var(--color-border)"));
		button->SetAttribute(_T("border-radius"), _T("6"));
		button->SetAttribute(_T("cursor"), _T("pointer"));
		tile->Add(button);

		CLabelUI* badge = new CLabelUI;
		badge->SetText(num.GetData());
		badge->SetMouseEnabled(false);
		badge->SetFixedWidth(24);
		badge->SetFixedHeight(18);
		badge->SetAttribute(_T("position"), _T("absolute"));
		badge->SetAttribute(_T("position-align"), _T("left,top"));
		badge->SetAttribute(_T("margin-left"), _T("4"));
		badge->SetAttribute(_T("margin-top"), _T("4"));
		badge->SetAttribute(_T("text-align"), _T("center"));
		badge->SetAttribute(_T("valign"), _T("vcenter"));
		badge->SetAttribute(_T("font-size"), _T("12"));
		badge->SetAttribute(_T("color"), _T("#FFFFFFFF"));
		badge->SetAttribute(_T("background-color"), _T("#00000000"));
		badge->SetAttribute(_T("border-radius"), _T("4"));
		tile->Add(badge);
		m_pBgList->Add(tile);
	}
}

void CImageWnd::UpdateBackgroundSelection()
{
	if( m_pBgList == NULL ) return;
	const int count = m_pBgList->GetCount();
	for( int i = 0; i < count; ++i ) {
		CControlUI* item = m_pBgList->GetItemAt(i);
		if( item == NULL ) continue;
		CContainerUI* tile = static_cast<CContainerUI*>(item->GetInterface(DUI_CTR_CONTAINER));
		CControlUI* thumb = (tile != NULL && tile->GetCount() > 0) ? tile->GetItemAt(0) : item;
		if( thumb == NULL ) continue;
		const bool on = !m_bgPath.IsEmpty() && _tcsicmp(thumb->GetUserData().GetData(), m_bgPath.GetData()) == 0;
		thumb->SetAttribute(_T("border"), on ? _T("2px solid") : _T("1px solid"));
		thumb->SetAttribute(_T("border-color"), on ? _T("var(--color-primary)") : _T("var(--color-border)"));
	}
	SyncPreviewFrame();
}

void CImageWnd::SyncPreviewFrame()
{
	if( m_pPlate == NULL ) return;
	const bool hasBg = !m_bgPath.IsEmpty();
	if( hasBg ) {
		m_pPlate->SetAttribute(_T("padding"), _T("0,0,0,0"));
		m_pPlate->SetAttribute(_T("background-color"), _T("#00000000"));
		m_pPlate->SetAttribute(_T("border"), _T("0"));
	}
	else {
		m_pPlate->SetAttribute(_T("padding"), _T("16,16,16,16"));
		const DWORD plate = (ColorLuma(m_dwColor) >= 160) ? 0x3A3A3AFF : 0xF2F2F2FF;
		CDuiString bk;
		bk.Format(_T("#%08X"), plate);
		m_pPlate->SetAttribute(_T("background-color"), bk.GetData());
		m_pPlate->SetAttribute(_T("border"), _T("1px solid"));
		m_pPlate->SetAttribute(_T("border-color"), _T("var(--color-border)"));
	}
	m_pPlate->NeedParentUpdate();
}

void CImageWnd::InitWindow()
{
	SetIcon(IDI_SAWADEE);
	m_pEdit = static_cast<CEditUI*>(m_pm.FindControl(_T("edt_text")));
	m_pFont = static_cast<CComboUI*>(m_pm.FindControl(_T("cmb_font")));
	m_pFontSize = static_cast<CEditUI*>(m_pm.FindControl(_T("edt_font_size")));
	m_pBold = static_cast<CCheckBoxUI*>(m_pm.FindControl(_T("chk_bold")));
	m_pItalic = static_cast<CCheckBoxUI*>(m_pm.FindControl(_T("chk_italic")));
	m_pUnderline = static_cast<CCheckBoxUI*>(m_pm.FindControl(_T("chk_underline")));
	m_pStrike = static_cast<CCheckBoxUI*>(m_pm.FindControl(_T("chk_strike")));
	m_pPlate = m_pm.FindControl(_T("preview_plate"));
	CContainerUI* plate = static_cast<CContainerUI*>(m_pPlate);
	CControlUI* oldPreview = m_pm.FindControl(_T("preview"));
	if( plate != NULL && oldPreview != NULL )
		plate->Remove(oldPreview);
	m_pPreview = new CCenteredPreviewUI();
	m_pPreview->SetAttribute(_T("flexible"), _T("1"));
	if( plate != NULL )
		plate->Add(m_pPreview);
	m_pPalette = static_cast<CColorPaletteUI*>(m_pm.FindControl(_T("palette")));
	m_pColorLabel = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_color")));
	m_pFormatHint = static_cast<CLabelUI*>(m_pm.FindControl(_T("lbl_fmt_hint")));
	m_pFormat = static_cast<CSegmentedUI*>(m_pm.FindControl(_T("seg_format")));
	m_pEditW = static_cast<CEditUI*>(m_pm.FindControl(_T("edt_w")));
	m_pEditH = static_cast<CEditUI*>(m_pm.FindControl(_T("edt_h")));
	m_pColorRow = static_cast<CHorizontalLayoutUI*>(m_pm.FindControl(_T("color_row")));
	m_pTextList = static_cast<CVerticalLayoutUI*>(m_pm.FindControl(_T("text_list")));
	m_pBgList = static_cast<CVerticalLayoutUI*>(m_pm.FindControl(_T("bg_list")));
	BuildColorSwatches();
	BuildBackgrounds();
	FillFontList();
	ApplyInputFont();
	ApplyColor(m_dwColor);
	SyncFormatHint();
	if( !m_seed.IsEmpty() && m_pEdit != NULL ) {
		m_pEdit->SetText(m_seed.GetData());
		AddPiece();
	}
}

void CImageWnd::Notify(TNotifyUI& msg)
{
	if( m_syncing ) return;
	if( msg.sType == DUI_MSGTYPE_TEXTCHANGED && msg.pSender == m_pEdit ) {
		if( m_sel >= 0 && m_sel < (int)m_items.size() ) {
			m_items[m_sel].text = m_pEdit->GetText();
			SyncPreview();
		}
		return;
	}
	if( msg.sType == DUI_MSGTYPE_TEXTCHANGED && (msg.pSender == m_pFontSize || msg.pSender == m_pEditW || msg.pSender == m_pEditH) ) {
		if( msg.pSender == m_pFontSize )
			PushStyle();
		SyncPreview();
		return;
	}
	if( msg.sType == DUI_MSGTYPE_ITEMSELECT && msg.pSender == m_pFont ) {
		ApplyInputFont();
		PushStyle();
		SyncPreview();
		return;
	}
	if( msg.sType == DUI_MSGTYPE_SELECTCHANGED && msg.pSender == m_pFormat ) {
		SyncFormatHint();
		return;
	}
	if( msg.sType == DUI_MSGTYPE_SELECTCHANGED
		&& (msg.pSender == m_pBold || msg.pSender == m_pItalic || msg.pSender == m_pUnderline || msg.pSender == m_pStrike) ) {
		ApplyInputFont();
		PushStyle();
		SyncPreview();
		return;
	}
	if( (msg.sType == DUI_MSGTYPE_COLORCHANGING || msg.sType == DUI_MSGTYPE_COLORCHANGED)
		&& msg.pSender == m_pPalette ) {
		ApplyColor((DWORD)msg.wParam);
		return;
	}
	WindowImplBase::Notify(msg);
}

LRESULT CImageWnd::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if( uMsg == kMsgBgReject ) {
		ShowToast(_T("无法打开这张图片"), CONTROLKIND_WARNING, 3000);
		return 0;
	}
	if( uMsg == WM_SETCURSOR && PieceCursor() )
		return TRUE;
	if( (uMsg == WM_LBUTTONDOWN || uMsg == WM_MOUSEMOVE || uMsg == WM_LBUTTONUP || uMsg == WM_CAPTURECHANGED)
		&& TrackPiece(uMsg, lParam) )
		return 0;
	const LRESULT result = WindowImplBase::HandleMessage(uMsg, wParam, lParam);
	if( uMsg == WM_SIZE )
		SyncPreview();
	return result;
}

void CImageWnd::OnClick(TNotifyUI& msg)
{
	if( msg.pSender == NULL ) {
		WindowImplBase::OnClick(msg);
		return;
	}
	const CDuiString name = msg.pSender->GetName();
	if( name == _T("btn_add") ) {
		AddPiece();
		return;
	}
	if( name == _T("btn_paste") ) {
		PasteClipboard();
		return;
	}
	if( name.Find(_T("piece_")) == 0 ) {
		const int index = _ttoi(name.GetData() + 6);
		if( index == m_sel )
			m_sel = -1;
		else {
			m_sel = index;
			ShowPiece(index);
		}
		UpdatePieceSelection();
		if( m_sel < 0 )
			SyncPreview();
		return;
	}
	if( name == _T("btn_custom_color") ) {
		if( m_pPalette != NULL ) {
			const bool show = !m_pPalette->IsVisible();
			m_pPalette->SetVisible(show);
			if( show )
				m_pPalette->SetSelectColor(m_dwColor);
		}
		return;
	}
	if( name == _T("btn_export") ) {
		ExportImage();
		return;
	}
	if( name == _T("btn_export_ico") ) {
		ExportIcon();
		return;
	}
	if( name.Find(_T("bg_")) == 0 ) {
		const CDuiString path = msg.pSender->GetUserData();
		if( !m_bgPath.IsEmpty() && _tcsicmp(m_bgPath.GetData(), path.GetData()) == 0 )
			m_bgPath.Empty();
		else
			m_bgPath = path;
		UpdateBackgroundSelection();
		SyncPreview();
		return;
	}
	if( name.Find(_T("swatch_")) == 0 ) {
		LPCTSTR ud = msg.pSender->GetUserData().GetData();
		DWORD c = 0;
		if( ud != NULL && _stscanf_s(ud, _T("%08X"), &c) == 1 )
			ApplyColor(c);
		if( m_pPalette != NULL )
			m_pPalette->SetVisible(false);
		return;
	}
	WindowImplBase::OnClick(msg);
}

void CImageWnd::OnContextMenu(TNotifyUI& msg)
{
	if( msg.pSender == NULL || msg.pSender->GetName() != _T("preview_plate") ) return;
	if( m_exporting ) return;

	static LPCTSTR kMenuShell =
		_T("<Window>")
		_T("<Menu icon-line=\"false\" border-width=\"1\" border-radius=\"4,4\" ")
		_T("padding=\"4,4,4,4\" item-padding=\"0,16,0,16\" item-vertical-align=\"vcenter\" />")
		_T("</Window>");

	CMenuWnd* menuWnd = CMenuWnd::CreateMenu(NULL, STRINGorID(kMenuShell), msg.ptScreen, &m_pm, NULL,
		eMenuAlignment_Left | eMenuAlignment_Top);
	if( menuWnd == NULL ) return;
	CMenuUI* menu = menuWnd->GetMenuUI();
	if( menu == NULL ) {
		menuWnd->Close();
		return;
	}
	menu->RemoveAll();
	CMenuElementUI* item = new CMenuElementUI;
	item->SetName(_T("bg_pick"));
	item->SetText(_T("选择背景图片"));
	item->SetFixedHeight(32);
	item->SetFixedWidth(168);
	menu->Add(item);
	menuWnd->ResizeMenu();
}

LRESULT CImageWnd::HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	(void)lParam;
	if( uMsg == WM_MENUCLICK ) {
		MenuCmd* cmd = (MenuCmd*)wParam;
		if( cmd != NULL && _tcscmp(cmd->szName, _T("bg_pick")) == 0 ) {
			m_pm.DeletePtr(cmd);
			bHandled = TRUE;
			PickBackground();
			return 0;
		}
	}
	bHandled = FALSE;
	return 0;
}

void CImageWnd::PickBackground()
{
	if( m_exporting ) return;
	IFileOpenDialog* dlg = NULL;
	HRESULT hr = ::CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
	if( FAILED(hr) || dlg == NULL ) {
		ShowToast(_T("无法打开文件选择"), CONTROLKIND_DANGER, 3000);
		return;
	}
	DWORD opts = 0;
	if( SUCCEEDED(dlg->GetOptions(&opts)) ) {
		opts |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR | FOS_FORCEFILESYSTEM;
		opts &= ~FOS_ALLOWMULTISELECT;
		dlg->SetOptions(opts);
	}
	const COMDLG_FILTERSPEC types[] = {
		{ L"图片", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff" },
		{ L"所有文件", L"*.*" },
	};
	dlg->SetFileTypes(2, types);
	dlg->SetFileTypeIndex(1);
	dlg->SetTitle(L"选择背景图片");
	hr = dlg->Show(m_hWnd);
	if( FAILED(hr) ) {
		dlg->Release();
		return;
	}
	IShellItem* item = NULL;
	hr = dlg->GetResult(&item);
	dlg->Release();
	if( FAILED(hr) || item == NULL ) return;
	PWSTR full = NULL;
	hr = item->GetDisplayName(SIGDN_FILESYSPATH, &full);
	item->Release();
	if( FAILED(hr) || full == NULL ) return;

	IWICImagingFactory* wic = SharedWic();
	IWICBitmapDecoder* decoder = NULL;
	if( wic != NULL )
		wic->CreateDecoderFromFilename(full, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
	IWICBitmapFrameDecode* frame = NULL;
	if( decoder != NULL )
		decoder->GetFrame(0, &frame);
	if( decoder != NULL ) decoder->Release();
	if( frame == NULL ) {
		::CoTaskMemFree(full);
		::PostMessage(m_hWnd, kMsgBgReject, 0, 0);
		return;
	}
	frame->Release();
	m_bgPath = full;
	::CoTaskMemFree(full);
	UpdateBackgroundSelection();
	SyncPreview();
}
