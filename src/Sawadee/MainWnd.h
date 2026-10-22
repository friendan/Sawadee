#pragma once

class CWriteGlyphUI;

class CMainWnd : public WindowImplBase, public IVirtualListCallback
{
public:
	CMainWnd();
	~CMainWnd();

	CDuiString GetSkinFile() override;
	CDuiString GetSkinFolder();
	LPCTSTR GetWindowClassName() const override;
	void InitWindow() override;
	void Notify(TNotifyUI& msg) override;
	LRESULT OnDestroy(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) override;
	LRESULT HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) override;

	DUI_DECLARE_MESSAGE_MAP()
	void OnClick(TNotifyUI& msg) override;
	void OnSelectChanged(TNotifyUI& msg);
	void OnContextMenu(TNotifyUI& msg);
	LPCTSTR GetItemText(CControlUI* pList, int iIndex) override;
	bool PaintItem(CControlUI* pList, IRenderContext& ctx, int iIndex, const RECT& rcItem, UINT uState) override;

private:
	void EnsureRootLayout();
	void ShowPage(int index);
	void SetStatusMessage(LPCTSTR text);
	struct ConsCell
	{
		CContainerUI* box;
		CWriteGlyphUI* letter;
		CLabelUI* index;
	};

	void BuildConsonantBoard();
	void BuildVowelBoard();
	void BuildToneBoard();
	void OnConsonantClick(int index);
	void OnVowelClick(int index);
	void OnToneClick(int index);
	void ApplyConsonantStyle(ConsCell& cell, bool highlighted);
	void CopyGlyphText(LPCTSTR text);
	void LoadWords();
	void ApplyWordFilter();
	void SetWordStatus();
	void ShowWordMenu(TNotifyUI& msg);
	bool WordAt(int shown, CDuiString& thai, CDuiString& zh) const;
	void LoadSentences();
	void ApplySentenceFilter();
	void SetSentenceStatus();
	void ShowSentenceMenu(TNotifyUI& msg);
	bool SentenceAt(int shown, CDuiString& thai, CDuiString& zh) const;

	struct WordItem
	{
		std::wstring thai;
		std::wstring zh;
	};

	CTabLayoutUI* m_pPages;
	CLabelUI* m_pStatusMessage;
	std::vector<ConsCell> m_consCells;
	std::vector<ConsCell> m_vowelCells;
	std::vector<ConsCell> m_toneCells;
	int m_selectedSound;
	int m_selectedVowel;
	int m_selectedTone;
	CVirtualListUI* m_pWords;
	CEditUI* m_pWordSearch;
	std::vector<WordItem> m_words;
	std::vector<int> m_wordShown;
	CVirtualListUI* m_pSentences;
	CEditUI* m_pSentenceSearch;
	std::vector<WordItem> m_sentences;
	std::vector<int> m_sentenceShown;
	CDuiString m_wordText;
	int m_fontIndex;
	int m_fontThai;
	int m_fontSentence;
	int m_fontZh;
};
