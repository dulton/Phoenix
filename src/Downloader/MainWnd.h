#pragma once
#include <DuiLib/UIlib.h>
#include "NICStatus.h"


class CMainWnd :
	public WindowImplBase,
	public NICStatusChanged
{
public:
	CMainWnd();
	~CMainWnd();

	virtual void OnFinalMessage(HWND hWnd);
	virtual void Notify(TNotifyUI& msg);
	virtual void InitWindow();
	virtual LRESULT OnNcActivate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
	DUI_DECLARE_MESSAGE_MAP();

protected:
	virtual LPCTSTR GetWindowClassName() const;
	virtual CDuiString GetSkinFolder();
	virtual CDuiString GetSkinFile();

	void OnMin(TNotifyUI& msg);
	void OnClose(TNotifyUI& msg);
	void OnAbout(TNotifyUI& msg);
	void OnSkinChanged(TNotifyUI& msg);
	void OnIPConfiguration(TNotifyUI& msg);

	void OnDownLoadWnd(TNotifyUI& msg);
	void OnLogWnd(TNotifyUI& msg);
	void OnOtherToolsWnd(TNotifyUI& msg);
	void OnVideoPlayWnd(TNotifyUI& msg);

	void ShowVersion();
	void BuildControlDDX();

	virtual void OnStatusChanged(NETWORK_STATUS st);

private:
	bool	m_IsMinWnd;
	bool	bMaxResolution;
//	StatusNotification	_sn;

	CControlUI*	_Network;
	CControlUI*	_un_use;
	CControlUI*	_DownloadUI;
	CControlUI*	_VideoUI;
	CControlUI*	_LogUI;
	CControlUI*	_ToolUI;
	CLabelUI* _Version;
};