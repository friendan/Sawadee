#include "StdAfx.h"
#include "duilib.h"
#include "WriteGlyph.h"
#include "ImageWnd.h"

namespace {

	const UINT WM_SAWADEE_LAYOUT = WM_USER + 0x5301;

	struct NavPage
	{
		LPCTSTR name;
		int index;
		LPCTSTR status;
	};

	const NavPage kNavPages[] = {
		{ _T("nav_consonant"), 0, _T("辅音") },
		{ _T("nav_vowel"), 1, _T("元音") },
		{ _T("nav_tone"), 2, _T("声调") },
		{ _T("nav_lesson"), 4, _T("单词") },
		{ _T("nav_practice"), 5, _T("句子") },
	};

	// klass: 0 中辅音 1 高辅音 2 低辅音。sound 为现代声母读音分组，跨类别相同。
	struct ConsLetter
	{
		wchar_t ch;
		int klass;
		int sound;
		const wchar_t* reading;
	};

	const ConsLetter kCons[] = {
		{ L'ก', 0, 1, L"ko kai" },
		{ L'ข', 1, 2, L"kho khai" },
		{ L'ฃ', 1, 2, L"kho khuat" },
		{ L'ค', 2, 2, L"kho khwai" },
		{ L'ฅ', 2, 2, L"kho khon" },
		{ L'ฆ', 2, 2, L"kho ra-khang" },
		{ L'ง', 2, 3, L"ngo ngu" },
		{ L'จ', 0, 4, L"cho chan" },
		{ L'ฉ', 1, 5, L"cho ching" },
		{ L'ช', 2, 5, L"cho chang" },
		{ L'ซ', 2, 6, L"so so" },
		{ L'ฌ', 2, 5, L"cho choe" },
		{ L'ญ', 2, 7, L"yo ying" },
		{ L'ฎ', 0, 8, L"do cha-da" },
		{ L'ฏ', 0, 9, L"to pa-tak" },
		{ L'ฐ', 1, 10, L"tho than" },
		{ L'ฑ', 2, 10, L"tho montho" },
		{ L'ฒ', 2, 10, L"tho phu-thao" },
		{ L'ณ', 2, 11, L"no nen" },
		{ L'ด', 0, 8, L"do dek" },
		{ L'ต', 0, 9, L"to tao" },
		{ L'ถ', 1, 10, L"tho thung" },
		{ L'ท', 2, 10, L"tho thahan" },
		{ L'ธ', 2, 10, L"tho thong" },
		{ L'น', 2, 11, L"no nu" },
		{ L'บ', 0, 12, L"bo bai-mai" },
		{ L'ป', 0, 13, L"po pla" },
		{ L'ผ', 1, 14, L"pho phueng" },
		{ L'ฝ', 1, 15, L"fo fa" },
		{ L'พ', 2, 14, L"pho phan" },
		{ L'ฟ', 2, 15, L"fo fan" },
		{ L'ภ', 2, 14, L"pho sam-phao" },
		{ L'ม', 2, 16, L"mo ma" },
		{ L'ย', 2, 7, L"yo yak" },
		{ L'ร', 2, 17, L"ro ruea" },
		{ L'ล', 2, 18, L"lo ling" },
		{ L'ว', 2, 19, L"wo waen" },
		{ L'ศ', 1, 6, L"so sala" },
		{ L'ษ', 1, 6, L"so rue-si" },
		{ L'ส', 1, 6, L"so suea" },
		{ L'ห', 1, 20, L"ho hip" },
		{ L'ฬ', 2, 18, L"lo chu-la" },
		{ L'อ', 0, 21, L"o ang" },
		{ L'ฮ', 2, 20, L"ho nok-huk" },
	};

	const int kConsCount = (int)(sizeof(kCons) / sizeof(kCons[0]));

	const int kConsClasses[] = { 0, 1, 2 };

	// klass: 0 写在辅音前 1 写在上面 2 写在下面 3 写在后面 4 单独成字 5 复合元音。
	struct VowelLetter
	{
		const wchar_t* text;
		int klass;
		const wchar_t* reading;
	};

	const VowelLetter kVowels[] = {
		{ L"เ", 0, L"e" }, { L"แ", 0, L"ae" }, { L"โ", 0, L"o" }, { L"ใ", 0, L"ai" }, { L"ไ", 0, L"ai" },
		{ L"ั", 1, L"a" }, { L"ิ", 1, L"i" }, { L"ี", 1, L"ii" }, { L"ึ", 1, L"ue" }, { L"ื", 1, L"uee" },
		{ L"ุ", 2, L"u" }, { L"ู", 2, L"uu" },
		{ L"ะ", 3, L"a" }, { L"า", 3, L"aa" }, { L"ำ", 3, L"am" }, { L"ๅ", 3, L"aa" },
		{ L"ฤ", 4, L"rue" }, { L"ฦ", 4, L"lue" },
		{ L"เอียะ", 5, L"ia" }, { L"เอีย", 5, L"ia" }, { L"เอือะ", 5, L"uea" }, { L"เอือ", 5, L"uea" },
		{ L"อัวะ", 5, L"ua" }, { L"อัว", 5, L"ua" }, { L"ใอ", 5, L"ai" }, { L"ไอ", 5, L"ai" },
		{ L"เอา", 5, L"ao" }, { L"อำ", 5, L"am" }, { L"ฤ", 5, L"rue" }, { L"ฤๅ", 5, L"rue" },
		{ L"ฦ", 5, L"lue" }, { L"ฦๅ", 5, L"lue" },
	};

	const int kVowelCount = (int)(sizeof(kVowels) / sizeof(kVowels[0]));
	const int kVowelClasses[] = { 0, 1, 2, 3, 4, 5 };

	const wchar_t kTones[] = { L'่', L'้', L'๊', L'๋' };
	const int kToneCount = (int)(sizeof(kTones) / sizeof(kTones[0]));

	int CountConsonantSound(int sound)
	{
		int n = 0;
		for( int i = 0; i < kConsCount; ++i ) {
			if( kCons[i].sound == sound )
				++n;
		}
		return n;
	}

} // namespace

DUI_BEGIN_MESSAGE_MAP(CMainWnd, WindowImplBase)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_CLICK, CMainWnd::OnClick)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_SELECTCHANGED, CMainWnd::OnSelectChanged)
	DUI_ON_MSGTYPE(DUI_MSGTYPE_MENU, CMainWnd::OnContextMenu)
DUI_END_MESSAGE_MAP()

CMainWnd::CMainWnd()
	: m_pPages(NULL)
	, m_pStatusMessage(NULL)
	, m_selectedSound(-1)
	, m_selectedVowel(-1)
	, m_selectedTone(-1)
	, m_pWords(NULL)
	, m_pWordSearch(NULL)
	, m_pSentences(NULL)
	, m_pSentenceSearch(NULL)
	, m_fontIndex(-1)
	, m_fontThai(-1)
	, m_fontSentence(-1)
	, m_fontZh(-1)
{
}

CMainWnd::~CMainWnd()
{
}

CDuiString CMainWnd::GetSkinFile()
{
	return _T("main.html");
}

CDuiString CMainWnd::GetSkinFolder()
{
	return _T("");
}

LPCTSTR CMainWnd::GetWindowClassName() const
{
	return _T("SawadeeMainWnd");
}

LRESULT CMainWnd::OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const LRESULT result = WindowImplBase::OnDestroy(uMsg, wParam, lParam, bHandled);
	::PostQuitMessage(0);
	return result;
}

void CMainWnd::InitWindow()
{
	SetIcon(IDI_SAWADEE);

	m_pPages = static_cast<CTabLayoutUI*>(m_pm.FindControl(_T("pages")));
	m_pStatusMessage = static_cast<CLabelUI*>(m_pm.FindControl(_T("status_message")));
	BuildConsonantBoard();
	BuildVowelBoard();
	BuildToneBoard();
	m_pWords = static_cast<CVirtualListUI*>(m_pm.FindControl(_T("word_list")));
	m_pWordSearch = static_cast<CEditUI*>(m_pm.FindControl(_T("edt_word")));
	m_pSentences = static_cast<CVirtualListUI*>(m_pm.FindControl(_T("sentence_list")));
	m_pSentenceSearch = static_cast<CEditUI*>(m_pm.FindControl(_T("edt_sentence")));
	m_fontIndex = m_pm.EnsureFont(_T("Microsoft YaHei UI"), 15, false, false, false, false);
	m_fontThai = m_pm.EnsureFont(_T("TH Sarabun New"), 48, false, false, false, false);
	m_fontSentence = m_pm.EnsureFont(_T("TH Sarabun New"), 36, false, false, false, false);
	m_fontZh = m_pm.EnsureFont(_T("Microsoft YaHei UI"), 20, false, false, false, false);
	LoadWords();
	LoadSentences();
	if( m_pWords != NULL ) {
		m_pWords->SetItemHeight(88);
		m_pWords->SetCallback(this);
		ApplyWordFilter();
	}
	if( m_pSentences != NULL ) {
		m_pSentences->SetItemHeight(88);
		m_pSentences->SetCallback(this);
		ApplySentenceFilter();
	}
	ShowPage(0);

	HWND hWnd = GetHWND();
	if( hWnd != NULL )
		::PostMessage(hWnd, WM_SAWADEE_LAYOUT, 0, 0);
}

void CMainWnd::Notify(TNotifyUI& msg)
{
	if( msg.sType == DUI_MSGTYPE_TEXTCHANGED && msg.pSender == m_pWordSearch ) {
		ApplyWordFilter();
		return;
	}
	if( msg.sType == DUI_MSGTYPE_TEXTCHANGED && msg.pSender == m_pSentenceSearch ) {
		ApplySentenceFilter();
		return;
	}
	if( msg.sType == DUI_MSGTYPE_ITEMRCLICK && msg.pSender == m_pWords ) {
		ShowWordMenu(msg);
		return;
	}
	if( msg.sType == DUI_MSGTYPE_ITEMRCLICK && msg.pSender == m_pSentences ) {
		ShowSentenceMenu(msg);
		return;
	}
	WindowImplBase::Notify(msg);
}

void CMainWnd::OnClick(TNotifyUI& msg)
{
	if( msg.pSender == NULL ) {
		WindowImplBase::OnClick(msg);
		return;
	}
	const CDuiString name = msg.pSender->GetName();
	if( name == _T("nav_menu") ) {
		SetStatusMessage(_T("功能"));
		return;
	}
	if( name == _T("btn_image") ) {
		CImageWnd::Open(GetHWND());
		return;
	}
	if( name.Find(_T("cons_")) == 0 ) {
		OnConsonantClick(_ttoi(name.GetData() + 5));
		return;
	}
	if( name.Find(_T("vowel_")) == 0 ) {
		OnVowelClick(_ttoi(name.GetData() + 6));
		return;
	}
	if( name.Find(_T("tone_")) == 0 ) {
		OnToneClick(_ttoi(name.GetData() + 5));
		return;
	}
	if( name == _T("btn_nav_toggle") ) {
		CControlUI* pNav = m_pm.FindControl(_T("nav_panel"));
		if( pNav != NULL ) {
			const bool show = !pNav->IsVisible();
			pNav->SetVisible(show);
			msg.pSender->SetToolTip(show ? _T("隐藏功能菜单") : _T("显示功能菜单"));
		}
		return;
	}
	WindowImplBase::OnClick(msg);
}

void CMainWnd::OnSelectChanged(TNotifyUI& msg)
{
	if( msg.pSender == NULL ) return;

	const CDuiString name = msg.pSender->GetName();
	for( int i = 0; i < (int)(sizeof(kNavPages) / sizeof(kNavPages[0])); ++i ) {
		if( name == kNavPages[i].name ) {
			ShowPage(kNavPages[i].index);
			return;
		}
	}
}

LRESULT CMainWnd::HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if( uMsg == WM_SAWADEE_LAYOUT ) {
		EnsureRootLayout();
		bHandled = TRUE;
		return 0;
	}
	if( uMsg == WM_MENUCLICK ) {
		MenuCmd* cmd = (MenuCmd*)wParam;
		if( cmd != NULL && _tcscmp(cmd->szName, _T("glyph_copy")) == 0 ) {
			CopyGlyphText(cmd->szUserData);
			m_pm.DeletePtr(cmd);
			bHandled = TRUE;
			return 0;
		}
		if( cmd != NULL && _tcscmp(cmd->szName, _T("word_copy")) == 0 ) {
			CDuiString thai;
			CDuiString zh;
			if( WordAt(_ttoi(cmd->szUserData), thai, zh) ) {
				CDuiString text;
				text.Format(_T("%s\r\n%s"), thai.GetData(), zh.GetData());
				CopyGlyphText(text.GetData());
			}
			m_pm.DeletePtr(cmd);
			bHandled = TRUE;
			return 0;
		}
		if( cmd != NULL && _tcscmp(cmd->szName, _T("word_export")) == 0 ) {
			CDuiString thai;
			CDuiString zh;
			const bool ok = WordAt(_ttoi(cmd->szUserData), thai, zh);
			m_pm.DeletePtr(cmd);
			bHandled = TRUE;
			if( ok )
				CImageWnd::Open(GetHWND(), thai.GetData());
			return 0;
		}
		if( cmd != NULL && _tcscmp(cmd->szName, _T("sentence_copy")) == 0 ) {
			CDuiString thai;
			CDuiString zh;
			if( SentenceAt(_ttoi(cmd->szUserData), thai, zh) ) {
				CDuiString text;
				text.Format(_T("%s\r\n%s"), thai.GetData(), zh.GetData());
				CopyGlyphText(text.GetData());
			}
			m_pm.DeletePtr(cmd);
			bHandled = TRUE;
			return 0;
		}
		if( cmd != NULL && _tcscmp(cmd->szName, _T("sentence_export")) == 0 ) {
			CDuiString thai;
			CDuiString zh;
			const bool ok = SentenceAt(_ttoi(cmd->szUserData), thai, zh);
			m_pm.DeletePtr(cmd);
			bHandled = TRUE;
			if( ok )
				CImageWnd::Open(GetHWND(), thai.GetData());
			return 0;
		}
		if( cmd != NULL && _tcscmp(cmd->szName, _T("glyph_export")) == 0 ) {
			const CDuiString text = cmd->szUserData;
			m_pm.DeletePtr(cmd);
			bHandled = TRUE;
			CImageWnd::Open(GetHWND(), text.GetData());
			return 0;
		}
	}
	bHandled = FALSE;
	return 0;
}

void CMainWnd::EnsureRootLayout()
{
	HWND hWnd = GetHWND();
	if( hWnd == NULL ) return;

	RECT rcClient = {};
	if( !::GetClientRect(hWnd, &rcClient) ) return;
	if( rcClient.right - rcClient.left < 8 || rcClient.bottom - rcClient.top < 8 )
		return;

	CControlUI* pRoot = m_pm.GetRoot();
	if( pRoot )
		pRoot->SetPos(rcClient);
}

void CMainWnd::ShowPage(int index)
{
	if( m_pPages != NULL && index >= 0 && index < m_pPages->GetCount() )
		m_pPages->SelectItem(index);

	LPCTSTR status = _T("");
	for( int i = 0; i < (int)(sizeof(kNavPages) / sizeof(kNavPages[0])); ++i ) {
		if( kNavPages[i].index == index ) {
			status = kNavPages[i].status;
			break;
		}
	}
	SetStatusMessage(status);
	if( index == 4 )
		SetWordStatus();
	else if( index == 5 )
		SetSentenceStatus();
}

void CMainWnd::SetStatusMessage(LPCTSTR text)
{
	if( m_pStatusMessage )
		m_pStatusMessage->SetText(text ? text : _T(""));
}

namespace {

std::wstring Utf8ToWide(const BYTE* data, DWORD size)
{
	if( data == NULL || size == 0 ) return std::wstring();
	if( size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF ) {
		data += 3;
		size -= 3;
	}
	if( size == 0 ) return std::wstring();
	const int n = ::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(data), (int)size, NULL, 0);
	if( n <= 0 ) return std::wstring();
	std::wstring text((size_t)n, L'\0');
	::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(data), (int)size, &text[0], n);
	return text;
}

struct RawWord
{
	std::wstring thai;
	std::wstring zh;
};

void ParseWordText(const std::wstring& text, std::vector<RawWord>& out)
{
	size_t i = 0;
	while( i < text.size() ) {
		size_t end = text.find(L'\n', i);
		if( end == std::wstring::npos ) end = text.size();
		std::wstring line = text.substr(i, end - i);
		if( !line.empty() && line.back() == L'\r' ) line.pop_back();
		i = end + 1;
		if( line.empty() || line[0] == L'#' ) continue;
		const size_t tab = line.find(L'\t');
		if( tab == std::wstring::npos || tab == 0 || tab + 1 >= line.size() ) continue;
		RawWord item;
		item.thai = line.substr(0, tab);
		item.zh = line.substr(tab + 1);
		while( !item.thai.empty() && item.thai.back() == L' ' ) item.thai.pop_back();
		while( !item.zh.empty() && (item.zh.front() == L' ' || item.zh.front() == L'\t') ) item.zh.erase(item.zh.begin());
		if( item.thai.empty() || item.zh.empty() ) continue;
		out.push_back(item);
	}
}

bool ReadDiskTable(LPCTSTR relative, std::vector<RawWord>& out)
{
	CDuiString path = CPaintManagerUI::GetInstancePath();
	path += relative;
	HANDLE file = ::CreateFile(path.GetData(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if( file == INVALID_HANDLE_VALUE ) return false;
	const DWORD size = ::GetFileSize(file, NULL);
	if( size == INVALID_FILE_SIZE || size > 8 * 1024 * 1024 ) {
		::CloseHandle(file);
		return false;
	}
	std::vector<BYTE> data(size);
	DWORD read = 0;
	const BOOL ok = (size == 0) || ::ReadFile(file, data.data(), size, &read, NULL);
	::CloseHandle(file);
	if( !ok || read != size ) return false;
	ParseWordText(Utf8ToWide(data.data(), read), out);
	return !out.empty();
}

bool ReadPackedTable(LPCTSTR zipPath, LPCTSTR zipPathAlt, std::vector<RawWord>& out)
{
	CZipFile* zip = reinterpret_cast<CZipFile*>(CPaintManagerUI::GetResourceZipHandle());
	if( zip == NULL ) return false;
	BYTE* data = NULL;
	DWORD size = 0;
	if( !zip->ExtractMemory(zipPath, &data, &size) || data == NULL ) {
		if( !zip->ExtractMemory(zipPathAlt, &data, &size) || data == NULL )
			return false;
	}
	ParseWordText(Utf8ToWide(data, size), out);
	delete[] data;
	return !out.empty();
}

} // namespace

void CMainWnd::LoadWords()
{
	m_words.clear();
	std::vector<RawWord> raw;
	if( !ReadDiskTable(_T("skin\\Sawadee\\word\\words.txt"), raw) )
		ReadPackedTable(_T("word/words.txt"), _T("word\\words.txt"), raw);
	m_words.reserve(raw.size());
	for( size_t i = 0; i < raw.size(); ++i ) {
		WordItem item;
		item.thai = raw[i].thai;
		item.zh = raw[i].zh;
		m_words.push_back(item);
	}
}

bool CMainWnd::WordAt(int shown, CDuiString& thai, CDuiString& zh) const
{
	thai.Empty();
	zh.Empty();
	if( shown < 0 || shown >= (int)m_wordShown.size() ) return false;
	const WordItem& item = m_words[m_wordShown[shown]];
	thai = item.thai.c_str();
	zh = item.zh.c_str();
	return !thai.IsEmpty();
}

void CMainWnd::ShowWordMenu(TNotifyUI& msg)
{
	const int shown = (int)msg.wParam;
	CDuiString thai;
	CDuiString zh;
	if( !WordAt(shown, thai, zh) ) return;
	if( m_pWords != NULL )
		m_pWords->SelectItem(shown, false);

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
	CDuiString index;
	index.Format(_T("%d"), shown);
	menu->RemoveAll();
	CMenuElementUI* copyItem = new CMenuElementUI;
	copyItem->SetName(_T("word_copy"));
	copyItem->SetText(_T("复制"));
	copyItem->SetUserData(index.GetData());
	copyItem->SetFixedHeight(32);
	copyItem->SetFixedWidth(140);
	menu->Add(copyItem);
	CMenuElementUI* exportItem = new CMenuElementUI;
	exportItem->SetName(_T("word_export"));
	exportItem->SetText(_T("导出图片"));
	exportItem->SetUserData(index.GetData());
	exportItem->SetFixedHeight(32);
	exportItem->SetFixedWidth(140);
	menu->Add(exportItem);
	menuWnd->ResizeMenu();
}

void CMainWnd::LoadSentences()
{
	m_sentences.clear();
	std::vector<RawWord> raw;
	if( !ReadDiskTable(_T("skin\\Sawadee\\sentence\\sentences.txt"), raw) )
		ReadPackedTable(_T("sentence/sentences.txt"), _T("sentence\\sentences.txt"), raw);
	m_sentences.reserve(raw.size());
	for( size_t i = 0; i < raw.size(); ++i ) {
		WordItem item;
		item.thai = raw[i].thai;
		item.zh = raw[i].zh;
		m_sentences.push_back(item);
	}
}

bool CMainWnd::SentenceAt(int shown, CDuiString& thai, CDuiString& zh) const
{
	thai.Empty();
	zh.Empty();
	if( shown < 0 || shown >= (int)m_sentenceShown.size() ) return false;
	const WordItem& item = m_sentences[m_sentenceShown[shown]];
	thai = item.thai.c_str();
	zh = item.zh.c_str();
	return !thai.IsEmpty();
}

void CMainWnd::ShowSentenceMenu(TNotifyUI& msg)
{
	const int shown = (int)msg.wParam;
	CDuiString thai;
	CDuiString zh;
	if( !SentenceAt(shown, thai, zh) ) return;
	if( m_pSentences != NULL )
		m_pSentences->SelectItem(shown, false);

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
	CDuiString index;
	index.Format(_T("%d"), shown);
	menu->RemoveAll();
	CMenuElementUI* copyItem = new CMenuElementUI;
	copyItem->SetName(_T("sentence_copy"));
	copyItem->SetText(_T("复制"));
	copyItem->SetUserData(index.GetData());
	copyItem->SetFixedHeight(32);
	copyItem->SetFixedWidth(140);
	menu->Add(copyItem);
	CMenuElementUI* exportItem = new CMenuElementUI;
	exportItem->SetName(_T("sentence_export"));
	exportItem->SetText(_T("导出图片"));
	exportItem->SetUserData(index.GetData());
	exportItem->SetFixedHeight(32);
	exportItem->SetFixedWidth(140);
	menu->Add(exportItem);
	menuWnd->ResizeMenu();
}

void CMainWnd::SetSentenceStatus()
{
	CDuiString text;
	if( m_sentenceShown.size() == m_sentences.size() )
		text.Format(_T("句子    %d"), (int)m_sentences.size());
	else
		text.Format(_T("句子    %d / %d"), (int)m_sentenceShown.size(), (int)m_sentences.size());
	SetStatusMessage(text.GetData());
}

void CMainWnd::ApplySentenceFilter()
{
	std::wstring query;
	if( m_pSentenceSearch != NULL )
		query = m_pSentenceSearch->GetText().GetData();
	m_sentenceShown.clear();
	m_sentenceShown.reserve(m_sentences.size());
	for( int i = 0; i < (int)m_sentences.size(); ++i ) {
		if( query.empty()
			|| m_sentences[i].thai.find(query) != std::wstring::npos
			|| m_sentences[i].zh.find(query) != std::wstring::npos )
			m_sentenceShown.push_back(i);
	}
	if( m_pSentences != NULL ) {
		m_pSentences->SetItemCount((int)m_sentenceShown.size());
		const SIZE origin = { 0, 0 };
		m_pSentences->SetScrollPos(origin, false);
	}
	if( m_pPages != NULL && m_pPages->GetCurSel() == 5 )
		SetSentenceStatus();
}

void CMainWnd::SetWordStatus()
{
	CDuiString text;
	if( m_wordShown.size() == m_words.size() )
		text.Format(_T("单词    %d"), (int)m_words.size());
	else
		text.Format(_T("单词    %d / %d"), (int)m_wordShown.size(), (int)m_words.size());
	SetStatusMessage(text.GetData());
}

void CMainWnd::ApplyWordFilter()
{
	std::wstring query;
	if( m_pWordSearch != NULL )
		query = m_pWordSearch->GetText().GetData();
	m_wordShown.clear();
	m_wordShown.reserve(m_words.size());
	for( int i = 0; i < (int)m_words.size(); ++i ) {
		if( query.empty()
			|| m_words[i].thai.find(query) != std::wstring::npos
			|| m_words[i].zh.find(query) != std::wstring::npos )
			m_wordShown.push_back(i);
	}
	if( m_pWords != NULL ) {
		m_pWords->SetItemCount((int)m_wordShown.size());
		const SIZE origin = { 0, 0 };
		m_pWords->SetScrollPos(origin, false);
	}
	if( m_pPages != NULL && m_pPages->GetCurSel() == 4 )
		SetWordStatus();
}

LPCTSTR CMainWnd::GetItemText(CControlUI* pList, int iIndex)
{
	m_wordText.Empty();
	const bool sentence = (pList == m_pSentences);
	const std::vector<int>& shown = sentence ? m_sentenceShown : m_wordShown;
	const std::vector<WordItem>& items = sentence ? m_sentences : m_words;
	if( iIndex < 0 || iIndex >= (int)shown.size() ) return _T("");
	const WordItem& item = items[shown[iIndex]];
	m_wordText.Format(_T("%d  %s  %s"), shown[iIndex] + 1, item.thai.c_str(), item.zh.c_str());
	return m_wordText.GetData();
}

bool CMainWnd::PaintItem(CControlUI* pList, IRenderContext& ctx, int iIndex, const RECT& rcItem, UINT uState)
{
	const bool sentence = (pList == m_pSentences);
	const std::vector<int>& shown = sentence ? m_sentenceShown : m_wordShown;
	const std::vector<WordItem>& items = sentence ? m_sentences : m_words;
	if( pList == NULL || iIndex < 0 || iIndex >= (int)shown.size() ) return false;
	const WordItem& item = items[shown[iIndex]];
	CPaintManagerUI* pm = pList->GetManager();
	DWORD fontColor = (pm != NULL) ? pm->GetDefaultFontColor() : 0x333333FF;
	const int luma = ((int)DuiColorR(fontColor) * 299 + (int)DuiColorG(fontColor) * 587 + (int)DuiColorB(fontColor) * 114) / 1000;
	const bool dark = luma >= 160;
	DWORD bk = 0;
	LPCTSTR token = NULL;
	if( (uState & UISTATE_SELECTED) != 0 )
		token = _T("var(--color-selection)");
	else if( (uState & UISTATE_HOT) != 0 )
		token = _T("var(--color-bg-hover)");
	if( token != NULL && ::ParseColorString(token, bk) )
		ctx.DrawColor(rcItem, pList->GetAdjustColor(bk));

	const int scale = (pm != NULL && pm->GetDPIObj() != NULL) ? 1 : 1;
	(void)scale;
	auto scaled = [&](int v) -> int {
		if( pm == NULL || pm->GetDPIObj() == NULL ) return v;
		return pm->GetDPIObj()->Scale(v);
	};
	const UINT style = DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS;
	RECT rcNum = rcItem;
	rcNum.left += scaled(16);
	rcNum.right = rcNum.left + scaled(48);
	CDuiString num;
	num.Format(_T("%d"), shown[iIndex] + 1);
	const DWORD numColor = dark ? 0x88FFFFFF : 0xFF9AA0A6;
	ctx.DrawText(rcNum, num.GetData(), pList->GetAdjustColor(numColor), m_fontIndex, style | DT_RIGHT);

	RECT rcZh = rcItem;
	rcZh.right = rcItem.right - scaled(12);
	if( sentence )
		rcZh.left = rcZh.right - scaled(280);
	RECT rcThai = rcItem;
	rcThai.left = rcNum.right + scaled(16);
	if( sentence )
		rcThai.right = rcZh.left - scaled(16);
	else
		rcThai.right = rcThai.left + scaled(560);
	if( rcThai.right > rcItem.right - scaled(16) ) rcThai.right = rcItem.right - scaled(16);
	if( !sentence )
		rcZh.left = rcThai.right + scaled(16);
	const int thaiFont = sentence ? m_fontSentence : m_fontThai;
	ctx.DrawText(rcThai, item.thai.c_str(), pList->GetAdjustColor(fontColor), thaiFont, style | DT_LEFT);
	if( rcZh.left < rcZh.right )
		ctx.DrawText(rcZh, item.zh.c_str(), pList->GetAdjustColor(fontColor), m_fontZh, style | DT_LEFT);
	return true;
}

void CMainWnd::ApplyConsonantStyle(ConsCell& cell, bool highlighted)
{
	if( cell.box == NULL ) return;
	LPCTSTR textColor = highlighted ? _T("var(--color-primary-text)") : _T("var(--color-text)");
	LPCTSTR indexColor = highlighted ? _T("var(--color-primary-text)") : _T("var(--color-text-secondary)");
	if( cell.letter != NULL ) {
		cell.letter->SetAttribute(_T("color"), textColor);
		if( !highlighted )
			cell.letter->StopWriting();
	}
	if( cell.index != NULL )
		cell.index->SetAttribute(_T("color"), indexColor);
	if( highlighted ) {
		cell.box->SetAttribute(_T("background-color"), _T("var(--color-primary)"));
		cell.box->SetAttribute(_T("border-color"), _T("var(--color-primary)"));
	}
	else {
		cell.box->SetAttribute(_T("background-color"), _T("var(--color-bg)"));
		cell.box->SetAttribute(_T("border-color"), _T("var(--color-border)"));
	}
}

void CMainWnd::BuildConsonantBoard()
{
	CContainerUI* pBody = static_cast<CContainerUI*>(m_pm.FindControl(_T("consonant_body")));
	if( pBody == NULL ) return;

	m_consCells.assign(kConsCount, ConsCell());
	pBody->SetAttribute(_T("gap"), _T("8"));

	for( int c = 0; c < (int)(sizeof(kConsClasses) / sizeof(kConsClasses[0])); ++c ) {
		const int klass = kConsClasses[c];

		CFlowLayoutUI* pFlow = new CFlowLayoutUI;
		pFlow->SetAttribute(_T("gap"), _T("10"));
		pFlow->SetAttribute(_T("line-spacing"), _T("8"));

		int serial = 0;
		for( int i = 0; i < kConsCount; ++i ) {
			if( kCons[i].klass != klass ) continue;
			++serial;

			CVerticalLayoutUI* pCell = new CVerticalLayoutUI;
			pCell->SetAttribute(_T("width"), _T("88"));
			pCell->SetAttribute(_T("height"), _T("170"));
			pCell->SetAttribute(_T("padding"), _T("2,2,2,8"));
			pCell->SetAttribute(_T("border"), _T("1"));
			pCell->SetAttribute(_T("border-radius"), _T("8"));

			CWriteGlyphUI* pLetter = new CWriteGlyphUI;
			wchar_t text[2] = { kCons[i].ch, 0 };
			pLetter->SetText(text);
			pLetter->SetAttribute(_T("height"), _T("136"));
			pLetter->SetAttribute(_T("mouse"), _T("false"));
			pLetter->SetAttribute(_T("text-align"), _T("center"));
			pLetter->SetAttribute(_T("font-family"), _T("TH Sarabun New"));
			pLetter->SetAttribute(_T("font-size"), _T("88"));
			pCell->Add(pLetter);

			CLabelUI* pIndex = new CLabelUI;
			CDuiString indexText;
			indexText.Format(_T("%d"), serial);
			pIndex->SetText(indexText.GetData());
			pIndex->SetAttribute(_T("height"), _T("24"));
			pIndex->SetAttribute(_T("mouse"), _T("false"));
			pIndex->SetAttribute(_T("text-align"), _T("center"));
			pIndex->SetAttribute(_T("font-size"), _T("13"));
			pCell->Add(pIndex);

			CButtonUI* pHit = new CButtonUI;
			CDuiString cellName;
			cellName.Format(_T("cons_%d"), i);
			pHit->SetName(cellName.GetData());
			pHit->SetAttribute(_T("kind"), _T("none"));
			pHit->SetAttribute(_T("position"), _T("0,0,1,1"));
			pHit->SetAttribute(_T("cursor"), _T("pointer"));
			pHit->SetAttribute(_T("menu"), _T("true"));
			pCell->Add(pHit);

			ConsCell cell = {};
			cell.box = pCell;
			cell.letter = pLetter;
			cell.index = pIndex;
			ApplyConsonantStyle(cell, false);
			pFlow->Add(pCell);
			m_consCells[i] = cell;
		}

		pBody->Add(pFlow);
	}

	pBody->NeedUpdate();
}

void CMainWnd::BuildVowelBoard()
{
	CContainerUI* pBody = static_cast<CContainerUI*>(m_pm.FindControl(_T("vowel_body")));
	if( pBody == NULL ) return;

	pBody->SetAttribute(_T("gap"), _T("8"));
	m_vowelCells.assign(kVowelCount, ConsCell());

	for( int c = 0; c < (int)(sizeof(kVowelClasses) / sizeof(kVowelClasses[0])); ++c ) {
		const int klass = kVowelClasses[c];

		CFlowLayoutUI* pFlow = new CFlowLayoutUI;
		pFlow->SetAttribute(_T("gap"), _T("10"));
		pFlow->SetAttribute(_T("line-spacing"), _T("8"));

		int serial = 0;
		for( int i = 0; i < kVowelCount; ++i ) {
			if( kVowels[i].klass != klass ) continue;
			++serial;

			const bool compound = kVowels[i].klass == 5;
			CVerticalLayoutUI* pCell = new CVerticalLayoutUI;
			pCell->SetAttribute(_T("width"), compound ? _T("168") : _T("88"));
			pCell->SetAttribute(_T("height"), _T("170"));
			pCell->SetAttribute(_T("padding"), _T("2,2,2,8"));
			pCell->SetAttribute(_T("border"), _T("1"));
			pCell->SetAttribute(_T("border-radius"), _T("8"));
			const bool writable = (kVowels[i].klass == 0) || (kVowels[i].klass == 1 && serial <= 5) || (kVowels[i].klass == 2 && serial <= 2) || (kVowels[i].klass == 3 && serial <= 4) || (kVowels[i].klass == 4 && serial <= 2) || (kVowels[i].klass == 5 && serial <= 14);
			CLabelUI* pLetter = NULL;
			if( writable ) {
				CWriteGlyphUI* pGlyph = new CWriteGlyphUI;
				pGlyph->SetText(kVowels[i].text);
				pLetter = pGlyph;
			}
			else {
				pLetter = new CLabelUI;
				pLetter->SetText(kVowels[i].text);
			}
			pLetter->SetAttribute(_T("height"), _T("136"));
			pLetter->SetAttribute(_T("mouse"), _T("false"));
			pLetter->SetAttribute(_T("text-align"), _T("center"));
			pLetter->SetAttribute(_T("font-family"), _T("TH Sarabun New"));
			pLetter->SetAttribute(_T("font-size"), compound ? _T("56") : _T("88"));
			pCell->Add(pLetter);

			CLabelUI* pIndex = new CLabelUI;
			CDuiString indexText;
			indexText.Format(_T("%d"), serial);
			pIndex->SetText(indexText.GetData());
			pIndex->SetAttribute(_T("height"), _T("24"));
			pIndex->SetAttribute(_T("mouse"), _T("false"));
			pIndex->SetAttribute(_T("text-align"), _T("center"));
			pIndex->SetAttribute(_T("font-size"), _T("13"));
			pCell->Add(pIndex);

			CButtonUI* pHit = new CButtonUI;
			CDuiString cellName;
			cellName.Format(_T("vowel_%d"), i);
			pHit->SetName(cellName.GetData());
			pHit->SetAttribute(_T("kind"), _T("none"));
			pHit->SetAttribute(_T("position"), _T("0,0,1,1"));
			pHit->SetAttribute(_T("menu"), _T("true"));
			if( writable )
				pHit->SetAttribute(_T("cursor"), _T("pointer"));
			pCell->Add(pHit);

			if( writable ) {
				ConsCell cell = {};
				cell.box = pCell;
				cell.letter = static_cast<CWriteGlyphUI*>(pLetter);
				cell.index = pIndex;
				ApplyConsonantStyle(cell, false);
				m_vowelCells[i] = cell;
			}
			else {
				pCell->SetAttribute(_T("background-color"), _T("var(--color-bg)"));
				pCell->SetAttribute(_T("border-color"), _T("var(--color-border)"));
				pLetter->SetAttribute(_T("color"), _T("var(--color-text)"));
				pIndex->SetAttribute(_T("color"), _T("var(--color-text-secondary)"));
			}

			pFlow->Add(pCell);
		}

		pBody->Add(pFlow);
	}

	pBody->NeedUpdate();
}

void CMainWnd::BuildToneBoard()
{
	CContainerUI* pBody = static_cast<CContainerUI*>(m_pm.FindControl(_T("tone_body")));
	if( pBody == NULL ) return;

	pBody->SetAttribute(_T("gap"), _T("8"));
	m_toneCells.assign(kToneCount, ConsCell());

	CFlowLayoutUI* pFlow = new CFlowLayoutUI;
	pFlow->SetAttribute(_T("gap"), _T("10"));
	pFlow->SetAttribute(_T("line-spacing"), _T("8"));

	for( int i = 0; i < kToneCount; ++i ) {
		CVerticalLayoutUI* pCell = new CVerticalLayoutUI;
		pCell->SetAttribute(_T("width"), _T("88"));
		pCell->SetAttribute(_T("height"), _T("170"));
		pCell->SetAttribute(_T("padding"), _T("2,2,2,8"));
		pCell->SetAttribute(_T("border"), _T("1"));
		pCell->SetAttribute(_T("border-radius"), _T("8"));

		CWriteGlyphUI* pLetter = new CWriteGlyphUI;
		wchar_t text[2] = { kTones[i], 0 };
		pLetter->SetText(text);
		pLetter->SetAttribute(_T("height"), _T("136"));
		pLetter->SetAttribute(_T("mouse"), _T("false"));
		pLetter->SetAttribute(_T("text-align"), _T("center"));
		pLetter->SetAttribute(_T("font-family"), _T("TH Sarabun New"));
		pLetter->SetAttribute(_T("font-size"), _T("88"));
		pCell->Add(pLetter);

		CLabelUI* pIndex = new CLabelUI;
		CDuiString indexText;
		indexText.Format(_T("%d"), i + 1);
		pIndex->SetText(indexText.GetData());
		pIndex->SetAttribute(_T("height"), _T("24"));
		pIndex->SetAttribute(_T("mouse"), _T("false"));
		pIndex->SetAttribute(_T("text-align"), _T("center"));
		pIndex->SetAttribute(_T("font-size"), _T("13"));
		pCell->Add(pIndex);

		CButtonUI* pHit = new CButtonUI;
		CDuiString cellName;
		cellName.Format(_T("tone_%d"), i);
		pHit->SetName(cellName.GetData());
		pHit->SetAttribute(_T("kind"), _T("none"));
		pHit->SetAttribute(_T("position"), _T("0,0,1,1"));
		pHit->SetAttribute(_T("cursor"), _T("pointer"));
		pHit->SetAttribute(_T("menu"), _T("true"));
		pCell->Add(pHit);

		ConsCell cell = {};
		cell.box = pCell;
		cell.letter = pLetter;
		cell.index = pIndex;
		ApplyConsonantStyle(cell, false);
		pFlow->Add(pCell);
		m_toneCells[i] = cell;
	}

	pBody->Add(pFlow);
	pBody->NeedUpdate();
}

void CMainWnd::OnToneClick(int index)
{
	if( index < 0 || index >= kToneCount ) return;
	if( m_toneCells[index].letter == NULL ) return;

	const bool clear = (index == m_selectedTone);
	m_selectedTone = clear ? -1 : index;
	for( int i = 0; i < kToneCount; ++i ) {
		if( m_toneCells[i].box == NULL ) continue;
		ApplyConsonantStyle(m_toneCells[i], !clear && i == index);
	}
	if( !clear )
		m_toneCells[index].letter->StartWriting();
}

void CMainWnd::OnConsonantClick(int index)
{
	if( index < 0 || index >= kConsCount ) return;

	const int sound = kCons[index].sound;
	const bool clear = (sound == m_selectedSound);
	m_selectedSound = clear ? -1 : sound;
	for( int i = 0; i < kConsCount; ++i ) {
		if( m_consCells[i].box == NULL ) continue;
		ApplyConsonantStyle(m_consCells[i], !clear && kCons[i].sound == sound);
	}
	if( !clear && m_consCells[index].letter != NULL )
		m_consCells[index].letter->StartWriting();

	if( clear ) {
		SetStatusMessage(_T("辅音"));
		return;
	}

	wchar_t letter[2] = { kCons[index].ch, 0 };
	CDuiString msg;
	msg.Format(_T("%s    %s    和%s读音一样的共%d个辅音字符"), letter, kCons[index].reading, letter, CountConsonantSound(sound));
	SetStatusMessage(msg.GetData());
}

void CMainWnd::OnVowelClick(int index)
{
	if( index < 0 || index >= kVowelCount || kVowels[index].text == NULL ) return;

	if( m_vowelCells[index].letter == NULL ) {
		CDuiString msg;
		msg.Format(_T("%s    %s"), kVowels[index].text, kVowels[index].reading);
		SetStatusMessage(msg.GetData());
		return;
	}

	const bool clear = (index == m_selectedVowel);
	m_selectedVowel = clear ? -1 : index;
	for( int i = 0; i < kVowelCount; ++i ) {
		if( m_vowelCells[i].box == NULL ) continue;
		ApplyConsonantStyle(m_vowelCells[i], !clear && i == index);
	}
	if( clear ) {
		SetStatusMessage(_T("元音"));
		return;
	}
	m_vowelCells[index].letter->StartWriting();
	CDuiString msg;
	msg.Format(_T("%s    %s"), kVowels[index].text, kVowels[index].reading);
	SetStatusMessage(msg.GetData());
}

void CMainWnd::OnContextMenu(TNotifyUI& msg)
{
	if( msg.pSender == NULL ) return;
	const CDuiString name = msg.pSender->GetName();
	CDuiString text;
	if( name.Find(_T("cons_")) == 0 ) {
		const int index = _ttoi(name.GetData() + 5);
		if( index < 0 || index >= kConsCount ) return;
		wchar_t letter[2] = { kCons[index].ch, 0 };
		text = letter;
	}
	else if( name.Find(_T("vowel_")) == 0 ) {
		const int index = _ttoi(name.GetData() + 6);
		if( index < 0 || index >= kVowelCount || kVowels[index].text == NULL ) return;
		text = kVowels[index].text;
	}
	else if( name.Find(_T("tone_")) == 0 ) {
		const int index = _ttoi(name.GetData() + 5);
		if( index < 0 || index >= kToneCount ) return;
		wchar_t letter[2] = { kTones[index], 0 };
		text = letter;
	}
	else {
		return;
	}

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
	CMenuElementUI* copyItem = new CMenuElementUI;
	copyItem->SetName(_T("glyph_copy"));
	copyItem->SetText(_T("复制"));
	copyItem->SetUserData(text.GetData());
	copyItem->SetFixedHeight(32);
	copyItem->SetFixedWidth(140);
	menu->Add(copyItem);
	CMenuElementUI* exportItem = new CMenuElementUI;
	exportItem->SetName(_T("glyph_export"));
	exportItem->SetText(_T("导出图片"));
	exportItem->SetUserData(text.GetData());
	exportItem->SetFixedHeight(32);
	exportItem->SetFixedWidth(140);
	menu->Add(exportItem);
	menuWnd->ResizeMenu();
}

void CMainWnd::CopyGlyphText(LPCTSTR text)
{
	if( text == NULL || text[0] == 0 ) return;
	HWND hwnd = GetHWND();
	if( hwnd == NULL || !::OpenClipboard(hwnd) ) return;
	::EmptyClipboard();
	const SIZE_T bytes = (::_tcslen(text) + 1) * sizeof(TCHAR);
	HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
	if( mem != NULL ) {
		void* dst = ::GlobalLock(mem);
		if( dst != NULL ) {
			memcpy(dst, text, bytes);
			::GlobalUnlock(mem);
			if( ::SetClipboardData(CF_UNICODETEXT, mem) == NULL )
				::GlobalFree(mem);
		}
		else {
			::GlobalFree(mem);
		}
	}
	::CloseClipboard();
}
