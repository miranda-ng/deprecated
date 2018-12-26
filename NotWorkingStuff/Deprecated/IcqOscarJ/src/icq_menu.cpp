// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
//
// Copyright © 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
// Copyright © 2001-2002 Jon Keating, Richard Hughes
// Copyright © 2002-2004 Martin Öberg, Sam Kothari, Robert Rainwater
// Copyright © 2004-2008 Joe Kucera, Bio
// Copyright © 2012-2018 Miranda NG team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// -----------------------------------------------------------------------------

#include "stdafx.h"

#include <m_skin.h>

HGENMENU g_hContactMenuItems[3];
HANDLE   g_hContactMenuSvc[3];

static INT_PTR IcqMenuHandleAddServContact(WPARAM wParam, LPARAM lParam)
{
	CIcqProto* ppro = CMPlugin::getInstance(wParam);
	return (ppro) ? ppro->AddServerContact(wParam, lParam) : 0;
}

static INT_PTR IcqMenuHandleXStatusDetails(WPARAM wParam, LPARAM lParam)
{
	CIcqProto* ppro = CMPlugin::getInstance(wParam);
	return (ppro) ? ppro->ShowXStatusDetails(wParam, lParam) : 0;
}

static INT_PTR IcqMenuHandleOpenProfile(WPARAM wParam, LPARAM lParam)
{
	CIcqProto* ppro = CMPlugin::getInstance(wParam);
	return (ppro) ? ppro->OpenWebProfile(wParam, lParam) : 0;
}

static int IcqPrebuildContactMenu(WPARAM wParam, LPARAM lParam)
{
	Menu_ShowItem(g_hContactMenuItems[ICMI_ADD_TO_SERVLIST], FALSE);
	Menu_ShowItem(g_hContactMenuItems[ICMI_XSTATUS_DETAILS], FALSE);
	Menu_ShowItem(g_hContactMenuItems[ICMI_OPEN_PROFILE], FALSE);

	CIcqProto* ppro = CMPlugin::getInstance(wParam);
	return (ppro) ? ppro->OnPreBuildContactMenu(wParam, lParam) : 0;
}

void g_MenuInit(void)
{
	///////////////
	// Contact menu

	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, IcqPrebuildContactMenu);

	// Contact menu initialization

	char str[MAXMODULELABELLENGTH], *pszDest = str + 3;
	mir_strcpy(str, "ICQ");

	CMenuItem mi(&g_plugin);
	mi.pszService = str;

	// "Add to server list"
	mir_strcpy(pszDest, MS_ICQ_ADDSERVCONTACT); CreateServiceFunction(str, IcqMenuHandleAddServContact);
	SET_UID(mi, 0x3928ba10, 0x69bc, 0x4ec9, 0x96, 0x48, 0xa4, 0x1b, 0xbe, 0x58, 0x4a, 0x7e);
	mi.name.a = LPGEN("Add to server list");
	mi.position = -2049999999;
	mi.hIcolibItem = Skin_GetIconHandle(SKINICON_AUTH_ADD);
	g_hContactMenuItems[ICMI_ADD_TO_SERVLIST] = Menu_AddContactMenuItem(&mi);

	// "Show custom status details"
	mir_strcpy(pszDest, MS_XSTATUS_SHOWDETAILS); CreateServiceFunction(str, IcqMenuHandleXStatusDetails);
	SET_UID(mi, 0x4767918b, 0x898b, 0x4cb6, 0x9c, 0x54, 0x8c, 0x56, 0x6a, 0xc4, 0xed, 0x42);
	mi.name.a = LPGEN("Show custom status details");
	mi.position = -2000004999;
	mi.hIcolibItem = nullptr;
	g_hContactMenuItems[ICMI_XSTATUS_DETAILS] = Menu_AddContactMenuItem(&mi);

	// "Open ICQ profile"
	mir_strcpy(pszDest, MS_OPEN_PROFILE); CreateServiceFunction(str, IcqMenuHandleOpenProfile);
	SET_UID(mi, 0x4f006492, 0x9fe5, 0x4d10, 0x88, 0xce, 0x47, 0x53, 0xba, 0x27, 0xe9, 0xc9);
	mi.name.a = LPGEN("Open ICQ profile");
	mi.position = 1000029997;
	g_hContactMenuItems[ICMI_OPEN_PROFILE] = Menu_AddContactMenuItem(&mi);
}

void g_MenuUninit(void)
{
	Menu_RemoveItem(g_hContactMenuItems[ICMI_ADD_TO_SERVLIST]);
	Menu_RemoveItem(g_hContactMenuItems[ICMI_XSTATUS_DETAILS]);
	Menu_RemoveItem(g_hContactMenuItems[ICMI_OPEN_PROFILE]);
}

INT_PTR CIcqProto::OpenWebProfile(WPARAM hContact, LPARAM)
{
	Utils_OpenUrl(CMStringA(FORMAT, "http://www.icq.com/people/%d", getContactUin(hContact)));
	return 0;
}

int CIcqProto::OnPreBuildContactMenu(WPARAM hContact, LPARAM)
{
	if (hContact == NULL)
		return 0;

	if (icqOnline()) {
		BOOL bCtrlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

		DWORD dwUin = getContactUin(hContact);

		Menu_ShowItem(m_hmiReqAuth, dwUin && (bCtrlPressed || (getByte(hContact, "Auth", 0) && getWord(hContact, DBSETTING_SERVLIST_ID, 0))));
		Menu_ShowItem(m_hmiGrantAuth, dwUin && (bCtrlPressed || getByte(hContact, "Grant", 0)));
		Menu_ShowItem(m_hmiRevokeAuth, dwUin && (bCtrlPressed || (getByte("PrivacyItems", 0) && !getByte(hContact, "Grant", 0))));

		Menu_ShowItem(g_hContactMenuItems[ICMI_ADD_TO_SERVLIST],
			m_bSsiEnabled && !getWord(hContact, DBSETTING_SERVLIST_ID, 0) &&
			!getWord(hContact, DBSETTING_SERVLIST_IGNORE, 0) &&
			!db_get_b(hContact, "CList", "NotOnList", 0));
	}

	Menu_ShowItem(g_hContactMenuItems[ICMI_OPEN_PROFILE], getContactUin(hContact) != 0);
	BYTE bXStatus = getContactXStatus(hContact);

	Menu_ShowItem(g_hContactMenuItems[ICMI_XSTATUS_DETAILS], m_bHideXStatusUI ? 0 : bXStatus != 0);
	if (bXStatus && !m_bHideXStatusUI) {
		if (bXStatus > 0 && bXStatus <= XSTATUS_COUNT)
			Menu_ModifyItem(g_hContactMenuItems[ICMI_XSTATUS_DETAILS], nullptr, getXStatusIcon(bXStatus, LR_SHARED));
		else
			Menu_ModifyItem(g_hContactMenuItems[ICMI_XSTATUS_DETAILS], nullptr, Skin_LoadIcon(SKINICON_OTHER_SMALLDOT));
	}

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// OnPreBuildStatusMenu event

int CIcqProto::OnPreBuildStatusMenu(WPARAM, LPARAM)
{
	InitXStatusItems(TRUE);
	return 0;
}
