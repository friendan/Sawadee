#include "StdAfx.h"
#include "duilib.h"
#include "resource.h"
#include "WriteGlyph.h"

namespace {

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

	bool ExtractPacked(const CDuiString& name, BYTE*& data, DWORD& size)
	{
		data = NULL;
		size = 0;
		CZipFile* zip = reinterpret_cast<CZipFile*>(CPaintManagerUI::GetResourceZipHandle());
		if( zip == NULL ) return false;
		if( zip->ExtractMemory(name.GetData(), &data, &size) && data != NULL ) return true;
		const CDuiString alt = SlashSwap(name);
		if( alt == name ) return false;
		return zip->ExtractMemory(alt.GetData(), &data, &size) && data != NULL;
	}

	bool WriteTempFont(const BYTE* data, DWORD size, const CDuiString& fileName, CDuiString& outPath)
	{
		TCHAR temp[MAX_PATH] = { 0 };
		if( ::GetTempPath(MAX_PATH, temp) == 0 ) return false;
		CDuiString dir = temp;
		dir += _T("Sawadee");
		::CreateDirectory(dir.GetData(), NULL);
		dir += _T("\\font");
		::CreateDirectory(dir.GetData(), NULL);
		outPath = dir + _T("\\") + fileName;
		HANDLE file = ::CreateFile(outPath.GetData(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if( file == INVALID_HANDLE_VALUE ) return false;
		DWORD written = 0;
		const BOOL ok = ::WriteFile(file, data, size, &written, NULL);
		::CloseHandle(file);
		if( !ok || written != size ) {
			::DeleteFile(outPath.GetData());
			return false;
		}
		return true;
	}

	bool InstallFont(const CDuiString& path, bool temporary)
	{
		if( ::AddFontResource(path.GetData()) > 0 ) return true;
		if( temporary ) ::DeleteFile(path.GetData());
		return false;
	}

	void LoadThaiFonts()
	{
		const TCHAR* files[] = {
			_T("THSarabunNew.ttf"),
			_T("THSarabunNew Bold.ttf"),
			_T("THSarabunNew Italic.ttf"),
			_T("THSarabunNew BoldItalic.ttf"),
		};
		bool installed = false;
		CDuiString diskDir = CPaintManagerUI::GetInstancePath();
		diskDir += _T("skin\\Sawadee\\font\\THSarabunNew\\");
		for( int i = 0; i < 4; ++i ) {
			const CDuiString disk = diskDir + files[i];
			if( ::GetFileAttributes(disk.GetData()) != INVALID_FILE_ATTRIBUTES ) {
				if( InstallFont(disk, false) ) installed = true;
				continue;
			}
			BYTE* data = NULL;
			DWORD size = 0;
			CDuiString packed = _T("font/THSarabunNew/");
			packed += files[i];
			if( !ExtractPacked(packed, data, size) ) continue;
			CDuiString tempPath;
			const bool wrote = WriteTempFont(data, size, files[i], tempPath);
			delete[] data;
			if( wrote && InstallFont(tempPath, true) ) installed = true;
		}
		if( installed )
			::PostMessage(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
	}

	void LoadPackedSkin()
	{
		HINSTANCE instance = CPaintManagerUI::GetInstance();
		HRSRC resource = ::FindResource(instance, MAKEINTRESOURCE(IDR_SAWADEE_SKIN), _T("ZIPRES"));
		if( resource == NULL ) return;
		HGLOBAL global = ::LoadResource(instance, resource);
		const DWORD size = ::SizeofResource(instance, resource);
		if( global == NULL || size == 0 ) return;
		CPaintManagerUI::SetResourceZip((LPBYTE)::LockResource(global), size);
	}

} // namespace

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
	HRESULT hr = ::CoInitialize(NULL);
	if( FAILED(hr) ) return 0;
	::OleInitialize(NULL);

	CPaintManagerUI::SetInstance(hInstance);
	CPaintManagerUI::SetResourceType(UILIB_ZIPRESOURCE);

	CDuiString path = CPaintManagerUI::GetInstancePath();
	path += _T("skin\\Sawadee\\");
	CPaintManagerUI::SetResourcePath(path.GetData());
	LoadPackedSkin();
	LoadThaiFonts();
	const int scored = RunWriteGlyphCommand();
	if( scored >= 0 ) {
		::OleUninitialize();
		::CoUninitialize();
		return scored;
	}

	if( CThemeManager* tm = CThemeManager::GetInstance() ) {
		tm->SetDefaultThemeId(_T("dark"));
		tm->ApplyTheme(_T("dark"));
	}

	CMainWnd* pMain = new CMainWnd();
	pMain->Create(NULL, _T("Sawadee"), UI_WNDSTYLE_FRAME, 0L, 0, 0, 1280, 800);
	pMain->ShowWindow(true);

	CPaintManagerUI::MessageLoop();

	delete pMain;
	::OleUninitialize();
	::CoUninitialize();
	return 0;
}
