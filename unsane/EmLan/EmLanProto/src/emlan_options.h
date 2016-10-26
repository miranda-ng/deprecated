#ifndef _EMLAN_OPTIONS_H_
#define _EMLAN_OPTIONS_H_

class CEmLanOptionsMain : public CEmLanDlgBase
{
private:
	CCtrlListBox m_ipAddress;
	CCtrlEdit m_nickname;
	CCtrlEdit m_group;

protected:
	void OnInitDialog();

	void OnApply();

public:
	CEmLanOptionsMain(CEmLanProto *proto, int idDialog);

	static CDlgBase *CreateAccountManagerPage(void *param, HWND owner)
	{
		CEmLanOptionsMain *page = new CEmLanOptionsMain((CEmLanProto*)param, IDD_ACCOUNT_MANAGER);
		page->SetParent(owner);
		page->Show();
		return page;
	}

	static CDlgBase *CreateOptionsPage(void *param) { return new CEmLanOptionsMain((CEmLanProto*)param, IDD_OPTIONS_MAIN); }
};

#endif //_EMLAN_OPTIONS_H_