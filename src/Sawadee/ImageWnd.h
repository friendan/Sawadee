#pragma once

#include <vector>

class CCenteredPreviewUI;
class CExportWaitWnd;

struct TextPiece
{
	CDuiString text;
	CDuiString family;
	int userPx;
	bool bold;
	bool italic;
	bool underline;
	bool strike;
	DWORD color;
	float cx;
	float cy;
};

class CImageWnd : public WindowImplBase
{
public:
	CImageWnd();
	~CImageWnd();

	static void Open(HWND owner, LPCTSTR text = NULL);

	void OnFinalMessage(HWND hWnd) override;
	CDuiString GetSkinFile() override;
	CDuiString GetSkinFolder();
	LPCTSTR GetWindowClassName() const override;
	void InitWindow() override;
	void Notify(TNotifyUI& msg) override;
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	DUI_DECLARE_MESSAGE_MAP()
	void OnClick(TNotifyUI& msg) override;
	void OnContextMenu(TNotifyUI& msg);
	LRESULT HandleCustomMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled) override;

private:
	void BuildColorSwatches();
	void BuildBackgrounds();
	void UpdateBackgroundSelection();
	void SyncPreviewFrame();
	void FillFontList();
	void ApplyInputFont();
	void ReadFont(CDuiString& family, int& userPx, bool& bold, bool& italic, bool& underline, bool& strike) const;
	void SelectFamily(const CDuiString& family);
	void ApplyColor(DWORD dwColor);
	void PushStyle();
	void AddPiece();
	void ShowPiece(int index);
	void AppendPieceButton(int index);
	void UpdatePieceSelection();
	int HitPiece(int x, int y) const;
	bool PieceCursor();
	bool TrackPiece(UINT uMsg, LPARAM lParam);
	void SyncPreview();
	void SyncFormatHint();
	void PasteClipboard();
	void PickBackground();
	bool ReadExportSize(int& w, int& h) const;
	bool HasPieceText() const;
	CDuiString FirstPieceText() const;
	bool RenderCanvas(int w, int h, std::vector<BYTE>& premul);
	CDuiString GetFormatExt() const;
	HWND ShowToast(LPCTSTR text, ControlKind kind, int duration);
	HWND ShowToast(LPCTSTR title, LPCTSTR text, ControlKind kind, int duration);
	bool PumpExport();
	static bool YieldExport(void* self);
	void FinishExport(HWND busy);
	void RunExportModal();
	void RunQueuedExport();
	bool ExportImage();
	bool ExportIcon();

	friend class CExportWaitWnd;

	CEditUI* m_pEdit;
	CComboUI* m_pFont;
	CEditUI* m_pFontSize;
	CCheckBoxUI* m_pBold;
	CCheckBoxUI* m_pItalic;
	CCheckBoxUI* m_pUnderline;
	CCheckBoxUI* m_pStrike;
	CCenteredPreviewUI* m_pPreview;
	CControlUI* m_pPlate;
	CColorPaletteUI* m_pPalette;
	CLabelUI* m_pColorLabel;
	CLabelUI* m_pFormatHint;
	CSegmentedUI* m_pFormat;
	CEditUI* m_pEditW;
	CEditUI* m_pEditH;
	CHorizontalLayoutUI* m_pColorRow;
	CVerticalLayoutUI* m_pTextList;
	CVerticalLayoutUI* m_pBgList;
	std::vector<TextPiece> m_items;
	int m_sel;
	bool m_drag;
	int m_dragX;
	int m_dragY;
	float m_dragCx;
	float m_dragCy;
	int m_dragMode;
	int m_dragUserPx;
	int m_anchorX;
	int m_anchorY;
	CDuiString m_bgPath;
	DWORD m_dwColor;
	bool m_syncing;
	bool m_exporting;
	bool m_closeAfterExport;
	bool m_yieldBroken;
	int m_job;
	int m_jobW;
	int m_jobH;
	bool m_jobOk;
	CDuiString m_jobPath;
	CDuiString m_jobExt;
	CDuiString m_seed;
};
