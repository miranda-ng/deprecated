#include "stdafx.h"

LIST<CEmLanProto> CEmLanProto::Accounts(1, CEmLanProto::CompareAccounts);

int CEmLanProto::CompareAccounts(const CEmLanProto *p1, const CEmLanProto *p2)
{
	return mir_tstrcmp(p1->m_tszUserName, p2->m_tszUserName);
}

CEmLanProto* CEmLanProto::InitAccount(const char *protoName, const wchar_t *userName)
{
	CEmLanProto *proto = new CEmLanProto(protoName, userName);
	Accounts.insert(proto);
	return proto;
}

int CEmLanProto::UninitAccount(CEmLanProto *proto)
{
	Accounts.remove(proto);
	delete proto;
	return 0;
}

//CEmLanProto* CEmLanProto::GetContactAccount(MCONTACT hContact)
//{
//	for (int i = 0; i < Accounts.getCount(); i++)
//		if (mir_strcmpi(GetContactProto(hContact), Accounts[i]->m_szModuleName) == 0)
//			return Accounts[i];
//	return NULL;
//}
//
//int CEmLanProto::OnAccountLoaded(WPARAM, LPARAM)
//{
//	HookProtoEvent(ME_OPT_INITIALISE, &CToxProto::OnOptionsInit);
//	HookProtoEvent(ME_USERINFO_INITIALISE, &CToxProto::OnUserInfoInit);
//	HookProtoEvent(ME_MSG_PRECREATEEVENT, &CToxProto::OnPreCreateMessage);
//
//	return 0;
//}
//
//int CToxProto::OnAccountRenamed(WPARAM, LPARAM)
//{
//	std::tstring newPath = GetToxProfilePath();
//	TCHAR oldPath[MAX_PATH];
//	mir_sntprintf(oldPath, MAX_PATH, _T("%s\\%s.tox"), VARST(_T("%miranda_userdata%")), accountName);
//	_trename(oldPath, newPath.c_str());
//	mir_free(accountName);
//	accountName = mir_tstrdup(m_tszUserName);
//
//	return 0;
//}

INT_PTR CEmLanProto::OnAccountManagerInit(WPARAM, LPARAM lParam)
{
	return (INT_PTR)(CEmLanOptionsMain::CreateAccountManagerPage(this, (HWND)lParam))->GetHwnd();
}