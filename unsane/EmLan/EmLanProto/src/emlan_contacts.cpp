#include "stdafx.h"

WORD CEmLanProto::GetContactStatus(MCONTACT hContact)
{
	return getWord(hContact, "Status", ID_STATUS_OFFLINE);
}

void CEmLanProto::SetContactStatus(MCONTACT hContact, WORD status)
{
	WORD oldStatus = GetContactStatus(hContact);
	if (oldStatus != status)
	{
		setWord(hContact, "Status", status);
	}
}

void CEmLanProto::SetAllContactsStatus(WORD status)
{
	for (MCONTACT hContact = db_find_first(m_szModuleName); hContact; hContact = db_find_next(hContact, m_szModuleName))
	{
		SetContactStatus(hContact, status);
	}
}

MCONTACT CEmLanProto::GetContact(const in_addr &addr)
{
	MCONTACT hContact = NULL;
	for (hContact = db_find_first(m_szModuleName); hContact; hContact = db_find_next(hContact, m_szModuleName))
	{
		u_long caddr = getDword(hContact, "ipaddr", -1);
		if (caddr == addr.S_un.S_addr)
			break;
	}
	return hContact;
}

MCONTACT CEmLanProto::AddContact(const in_addr &addr, const TCHAR *nick, bool isTemporary)
{
	MCONTACT hContact = GetContact(addr);
	if (!hContact)
	{
		hContact = (MCONTACT)CallService(MS_DB_CONTACT_ADD, 0, 0);
		CallService(MS_PROTO_ADDTOCONTACT, hContact, (LPARAM)m_szModuleName);

		setDword(hContact, "ipaddr", addr.S_un.S_addr);
		setTString(hContact, "Nick", nick);

		DBVARIANT dbv;
		if (!getTString("DefaultGroup", &dbv))
		{
			db_set_ts(hContact, "CList", "Group", dbv.ptszVal);
			db_free(&dbv);
		}

		//setByte(hContact, "Auth", 1);
		//setByte(hContact, "Grant", 1);

		if (isTemporary)
			db_set_b(hContact, "CList", "NotOnList", 1);
	}
	return hContact;
}

void CEmLanProto::CheckContactsThread(void*)
{
	while (isCheckingStarted)
	{
		Sleep(MLAN_SLEEP);
		mir_cslock lock(checkContactsLock);

		TContact* cont = m_pRootContact;
		while (cont)
		{
			if (cont->m_status != ID_STATUS_OFFLINE)
			{
				if (cont->m_time)
					cont->m_time--;
				if (cont->m_time == MLAN_TIMEOUT)
					RequestStatus(true, cont->m_addr.S_un.S_addr);
				if (!cont->m_time)
				{
					cont->m_status = ID_STATUS_OFFLINE;
					if (MCONTACT hContact = GetContact(cont->m_addr))
						SetContactStatus(hContact, ID_STATUS_OFFLINE);
				}
			}
			cont = cont->m_prev;
		}
	}
}