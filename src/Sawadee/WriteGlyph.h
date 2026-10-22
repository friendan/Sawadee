#pragma once

// 选中辅音时，按书写顺序把字形逐渐涂上颜色。
class CWriteGlyphUI : public CLabelUI
{
public:
	CWriteGlyphUI();
	~CWriteGlyphUI();

	void StartWriting();
	void StopWriting();

	void DoEvent(TEventUI& event) override;
	void PaintText(IRenderContext& ctx) override;

private:
	static VOID CALLBACK TickProc(PVOID param, BOOLEAN fired);
	void StopTick();

	bool m_writing;
	DWORD m_startTick;
	HANDLE m_tick;
	HWND m_tickWnd;
	volatile LONG m_tickPending;
};

// 启动参数 --score / --ascii。不是这两个参数时返回 -1，调用方继续打开界面。
// --score 给笔顺打分（笔尖离墨、漏白、折返），--ascii 打印字形图。
// 字符可跟在参数后面，不写则处理笔顺表里的全部字符。退出码 0 表示打分全部通过。
int RunWriteGlyphCommand();
