#pragma once
#include <DuiLib/UIlib.h>

#include "SearchFileNotification.h"

class CProgtessUI :
	public WindowImplBase
{
public:
	CProgtessUI();
	~CProgtessUI();

	virtual void InitWindow();
	virtual void OnFinalMessage(HWND hWnd);
	virtual void Notify(TNotifyUI& msg);
	DUI_DECLARE_MESSAGE_MAP();

	void ReceiveSearchFile(SearchFileNotification* pNf);

	void ShowProgress();

private:
	int			m_CountFile;
	int				m_Searchfile_count;
	int			m_finish;
	bool			m_InitNotify;

protected:
	virtual LPCTSTR GetWindowClassName() const;
	virtual CDuiString GetSkinFolder();
	virtual CDuiString GetSkinFile();
};

