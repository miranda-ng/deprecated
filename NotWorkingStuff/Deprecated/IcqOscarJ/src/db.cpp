// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
// 
// Copyright © 2001-2004 Richard Hughes, Martin Öberg
// Copyright © 2004-2009 Joe Kucera, Bio
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
//  DESCRIPTION:
//
//  ChangeInfo Plugin stuff
// -----------------------------------------------------------------------------

#include "stdafx.h"

void ChangeInfoData::LoadSettingsFromDb(int keepChanged)
{
	for (int i = 0; i < settingCount; i++) {
		const SettingItem &si = setting[i];
		if (si.displayType == LI_DIVIDER)
			continue;

		SettingItemData &sid = settingData[i];
		if (keepChanged && sid.changed)
			continue;

		if (si.dbType == DBVT_ASCIIZ || si.dbType == DBVT_UTF8)
			SAFE_FREE((void**)(char**)&sid.value);
		else if (!keepChanged)
			sid.value = 0;

		sid.changed = 0;

		if (si.displayType & LIF_PASSWORD)
			continue;

		DBVARIANT dbv = { DBVT_DELETED };
		if (!ppro->getSetting(NULL, si.szDbSetting, &dbv)) {
			switch (dbv.type) {
			case DBVT_ASCIIZ:
				sid.value = (LPARAM)ppro->getSettingStringUtf(NULL, si.szDbSetting, nullptr);
				break;

			case DBVT_UTF8:
				sid.value = (LPARAM)null_strdup(dbv.pszVal);
				break;

			case DBVT_WORD:
				if (si.displayType & LIF_SIGNED)
					sid.value = dbv.sVal;
				else
					sid.value = dbv.wVal;
				break;

			case DBVT_BYTE:
				if (si.displayType & LIF_SIGNED)
					sid.value = dbv.cVal;
				else
					sid.value = dbv.bVal;
				break;

#ifdef _DEBUG
			default:
				MessageBoxA(nullptr, "That's not supposed to happen either", "Huh?", MB_OK);
				break;
#endif
			}
			db_free(&dbv);
		}

		char buf[MAX_PATH];
		wchar_t tbuf[MAX_PATH];

		if (make_unicode_string_static(GetItemSettingText(i, buf, _countof(buf)), tbuf, _countof(tbuf)))
			ListView_SetItemText(hwndList, i, 1, tbuf);
	}
}

void ChangeInfoData::FreeStoredDbSettings(void)
{
	for (int i = 0; i < settingCount; i++)
		if (setting[i].dbType == DBVT_ASCIIZ || setting[i].dbType == DBVT_UTF8)
			SAFE_FREE((void**)&settingData[i].value);
}

int ChangeInfoData::ChangesMade(void)
{
	for (int i = 0; i < settingCount; i++)
		if (settingData[i].changed)
			return 1;
	return 0;
}

void ChangeInfoData::ClearChangeFlags(void)
{
	for (int i = 0; i < settingCount; i++)
		settingData[i].changed = 0;
}

int ChangeInfoData::SaveSettingsToDb()
{
	int ret = 1;

	for (int i = 0; i < settingCount; i++) {
		SettingItemData &sid = settingData[i];
		if (!sid.changed)
			continue;

		const SettingItem &si = setting[i];
		if (!(si.displayType & LIF_ZEROISVALID) && sid.value == 0) {
			ppro->delSetting(si.szDbSetting);
			continue;
		}
		switch (si.dbType) {
		case DBVT_ASCIIZ:
			if (*(char*)sid.value)
				db_set_utf(NULL, ppro->m_szModuleName, si.szDbSetting, (char*)sid.value);
			else
				ppro->delSetting(si.szDbSetting);
			break;

		case DBVT_UTF8:
			if (*(char*)sid.value)
				db_set_utf(NULL, ppro->m_szModuleName, si.szDbSetting, (char*)sid.value);
			else
				ppro->delSetting(si.szDbSetting);
			break;

		case DBVT_WORD:
			ppro->setWord(si.szDbSetting, (WORD)sid.value);
			break;

		case DBVT_BYTE:
			ppro->setByte(si.szDbSetting, (BYTE)sid.value);
			break;
		}
	}
	return ret;
}
