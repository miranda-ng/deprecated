// -----------------------------------------------------------------------------
// ICQ plugin for Miranda NG
// -----------------------------------------------------------------------------
// Copyright © 2018-24 Miranda NG team
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
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
// -----------------------------------------------------------------------------
// Server permissions

#include "stdafx.h"

void CIcqProto::GetPermitDeny()
{
	Push(new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "/preference/getPermitDeny", &CIcqProto::OnGetPermitDeny) << AIMSID(this));
}

void CIcqProto::OnGetPermitDeny(MHttpResponse *pReply, AsyncHttpRequest*)
{
	JsonReply root(pReply);
	if (root.error() == 200)
		ProcessPermissions(root.data());
}

void CIcqProto::ProcessPermissions(const JSONNode &ev)
{
	{	mir_cslock lck(m_csCache);
		for (auto &it : m_arCache)
			it->m_iApparentMode = 0;
	}

	for (auto &it : ev["allows"]) {
		auto *p = FindUser(it.as_mstring());
		if (p)
			p->m_iApparentMode = ID_STATUS_ONLINE;
	}

	m_bIgnoreListEmpty = true;
	for (auto &it : ev["ignores"]) {
		CMStringW wszId(it.as_mstring());
		auto *p = FindUser(wszId);
		if (p == nullptr) {
			CreateContact(wszId, false);
			p = FindUser(wszId);
		}
		p->m_iApparentMode = ID_STATUS_OFFLINE;
		Contact::Hide(p->m_hContact);
		m_bIgnoreListEmpty = false;
	}
}

void CIcqProto::SetPermitDeny(const CMStringW &userId, bool bAllow)
{
	auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "/preference/setPermitDeny")
		<< AIMSID(this) << WCHAR_PARAM((bAllow) ? "pdIgnoreRemove" : "pdIgnore", userId);
	if (!m_bIgnoreListEmpty)
		pReq << CHAR_PARAM("pdMode", "denySome");
	Push(pReq);
}
