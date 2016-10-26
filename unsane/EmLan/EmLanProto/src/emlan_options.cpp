#include "stdafx.h"

CEmLanOptionsMain::CEmLanOptionsMain(CEmLanProto *proto, int idDialog)
	: CEmLanDlgBase(proto, idDialog, false), m_ipAddress(this, IDC_LIST_IP),
	m_nickname(this, IDC_NAME), m_group(this, IDC_GROUP)
{
	char hostName[MAX_HOSTNAME_LEN];
	gethostname(hostName, MAX_HOSTNAME_LEN);
	CreateLink(m_nickname, "Nick", _A2T(hostName));
	CreateLink(m_group, "DefaultGroup", _T(MODULE));
}

void CEmLanOptionsMain::OnInitDialog()
{
	CEmLanDlgBase::OnInitDialog();

	int count = m_proto->GetHostAddrCount();
	in_addr caddr = m_proto->GetCurHostAddress();
	int cind = 0;
	for (int i = 0; i < count; i++)
	{
		in_addr addr = m_proto->GetHostAddress(i);
		char *ip = inet_ntoa(addr);
		if (addr.S_un.S_addr == caddr.S_un.S_addr)
			cind = i;
		m_ipAddress.AddString(_A2T(ip));
	}
	m_ipAddress.SetCurSel(cind);

	SendMessage(m_nickname.GetHwnd(), EM_LIMITTEXT, 32, 0);
	SendMessage(m_group.GetHwnd(), EM_LIMITTEXT, 64, 0);
}

void CEmLanOptionsMain::OnApply()
{
	ptrT group(m_group.GetText());
	if (mir_tstrlen(group) > 0 && Clist_GroupExists(group))
		Clist_CreateGroup(0, group);

	m_proto->setDword("ipaddr", m_ipAddress.GetCurSel());
}
