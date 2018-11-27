#include "stdafx.h"
#include "MraIcons.h"

HANDLE hXStatusAdvancedStatusIcons[MRA_XSTATUS_COUNT+4];

IconItem gdiMainIcon[1] =
{
	{ LPGEN("Main icon"),         "main",                 IDI_MRA }
};

IconItem gdiMenuItems[MAIN_MENU_ITEMS_COUNT] =
{
	{ MRA_GOTO_INBOX_STR,         MRA_GOTO_INBOX,         IDI_INBOX             },
	{ MRA_SHOW_INBOX_STATUS_STR,  MRA_SHOW_INBOX_STATUS,  IDI_MAIL_NOTIFY       },
	{ MRA_EDIT_PROFILE_STR,       MRA_EDIT_PROFILE,       IDI_PROFILE           },
	{ MRA_MY_ALBUM_STR,           MRA_VIEW_ALBUM,         IDI_MRA_PHOTO         },
	{ MRA_MY_BLOGSTATUS_STR,      MRA_REPLY_BLOG_STATUS,  IDI_BLOGSTATUS        },
	{ MRA_MY_VIDEO_STR,           MRA_VIEW_VIDEO,         IDI_MRA_VIDEO         },
	{ MRA_MY_ANSWERS_STR,         MRA_ANSWERS,            IDI_MRA_ANSWERS       },
	{ MRA_MY_WORLD_STR,           MRA_WORLD,              IDI_MRA_WORLD         },
	{ MRA_WEB_SEARCH_STR,         MRA_WEB_SEARCH,         IDI_MRA_WEB_SEARCH    },
	{ MRA_UPD_ALL_USERS_INFO_STR, MRA_UPD_ALL_USERS_INFO, IDI_PROFILE           },
	{ MRA_CHK_USERS_AVATARS_STR,  MRA_CHK_USERS_AVATARS,  IDI_PROFILE           },
	{ MRA_REQ_AUTH_FOR_ALL_STR,   MRA_REQ_AUTH_FOR_ALL,   IDI_AUTHRUGUEST       }
};

IconItem gdiContactMenuItems[CONTACT_MENU_ITEMS_COUNT] =
{
	{ MRA_REQ_AUTH_STR,           MRA_REQ_AUTH,           IDI_AUTHRUGUEST       },
	{ MRA_GRANT_AUTH_STR,         MRA_GRANT_AUTH,         IDI_AUTHGRANT         },
	{ MRA_SEND_EMAIL_STR,         MRA_SEND_EMAIL,         IDI_INBOX             },
	{ MRA_SEND_POSTCARD_STR,      MRA_SEND_POSTCARD,      IDI_MRA_POSTCARD      },
	{ MRA_VIEW_ALBUM_STR,         MRA_VIEW_ALBUM,         IDI_MRA_PHOTO         },
	{ MRA_REPLY_BLOG_STATUS_STR,  MRA_REPLY_BLOG_STATUS,  IDI_BLOGSTATUS        },
	{ MRA_VIEW_VIDEO_STR,         MRA_VIEW_VIDEO,         IDI_MRA_VIDEO         },
	{ MRA_ANSWERS_STR,            MRA_ANSWERS,            IDI_MRA_ANSWERS       },
	{ MRA_WORLD_STR,              MRA_WORLD,              IDI_MRA_WORLD         },
	{ MRA_SENDNUDGE_STR,          PS_SEND_NUDGE,          IDI_MRA_ALARM         }
};

IconItem gdiExtraStatusIconsItems[ADV_ICON_MAX] =
{
   { ADV_ICON_DELETED_STR,        ADV_ICON_DELETED_ID,        IDI_DELETED      },
   { ADV_ICON_NOT_ON_SERVER_STR,	 ADV_ICON_NOT_ON_SERVER_ID,  IDI_AUTHGRANT    },
   { ADV_ICON_NOT_AUTHORIZED_STR, ADV_ICON_NOT_AUTHORIZED_ID, IDI_AUTHRUGUEST  },
   { ADV_ICON_PHONE_STR,          ADV_ICON_PHONE_ID,          IDI_MRA_PHONE    },
   { ADV_ICON_BLOGSTATUS_STR,     ADV_ICON_BLOGSTATUS_ID,     IDI_BLOGSTATUS   }
};

//////////////////////////////////////////////////////////////////////////////////////

void CMraProto::OnBuildProtoMenu()
{
	CListCreateMenu(2000060000, 500085000, TRUE, gdiMenuItems, MAIN_MENU_ITEMS_COUNT, hMainMenuItems);
}

//////////////////////////////////////////////////////////////////////////////////////

HICON IconLibGetIcon(HANDLE hIcon)
{
	return IconLibGetIconEx(hIcon, LR_SHARED);
}

HICON IconLibGetIconEx(HANDLE hIcon, DWORD dwFlags)
{
	HICON hiIcon = nullptr;
	if (hIcon) {
		hiIcon = IcoLib_GetIconByHandle(hIcon);
		if ((dwFlags & LR_SHARED) == 0)
			hiIcon = CopyIcon(hiIcon);
	}
	return hiIcon;
}

//////////////////////////////////////////////////////////////////////////////////////

void IconsLoad()
{
	g_plugin.registerIcon(LPGEN("Protocols") "/" LPGEN("MRA"), gdiMainIcon, "MRA_");
	g_plugin.registerIcon(LPGEN("Protocols") "/" LPGEN("MRA") "/" LPGEN("Main Menu"), gdiMenuItems, "MRA_");
	g_plugin.registerIcon(LPGEN("Protocols") "/" LPGEN("MRA") "/" LPGEN("Contact Menu"), gdiContactMenuItems, "MRA_");
	g_plugin.registerIcon(LPGEN("Protocols") "/" LPGEN("MRA") "/" LPGEN("Extra status"), gdiExtraStatusIconsItems, "MRA_");

	g_hMainIcon = IconLibGetIcon(gdiMainIcon[0].hIcolib);
}

void InitXStatusIcons()
{
	// load libs
	wchar_t szBuff[MAX_FILEPATH];
	if (GetModuleFileName(nullptr, szBuff, _countof(szBuff))) {
		LPWSTR lpwszFileName;
		g_dwMirWorkDirPathLen = GetFullPathName(szBuff, MAX_FILEPATH, g_szMirWorkDirPath, &lpwszFileName);
		if (g_dwMirWorkDirPathLen) {
			g_dwMirWorkDirPathLen -= mir_wstrlen(lpwszFileName);
			g_szMirWorkDirPath[g_dwMirWorkDirPathLen] = 0;

			// load xstatus icons lib
			DWORD dwBuffLen;
			DWORD dwErrorCode = FindFile(g_szMirWorkDirPath, (DWORD)g_dwMirWorkDirPathLen, L"xstatus_MRA.dll", -1, szBuff, _countof(szBuff), &dwBuffLen);
			if (dwErrorCode == NO_ERROR) {
				g_hDLLXStatusIcons = LoadLibraryEx(szBuff, nullptr, 0);
				if (g_hDLLXStatusIcons) {
					dwBuffLen = LoadString(g_hDLLXStatusIcons, IDS_IDENTIFY, szBuff, MAX_FILEPATH);
					if (dwBuffLen == 0 || wcsnicmp(L"# Custom Status Icons #", szBuff, 23)) {
						FreeLibrary(g_hDLLXStatusIcons);
						g_hDLLXStatusIcons = nullptr;
					}
				}
			}
		}
	}

	GetModuleFileName((g_hDLLXStatusIcons != nullptr) ? g_hDLLXStatusIcons : g_plugin.getInst(), szBuff, _countof(szBuff));

	SKINICONDESC sid = {};
	sid.section.w = LPGENW("Protocols")L"/" LPGENW("MRA") L"/" LPGENW("Custom Status");
	sid.defaultFile.w = szBuff;
	sid.flags = SIDF_ALL_UNICODE;

	hXStatusAdvancedStatusIcons[0] = nullptr;
	for (DWORD i = 1; i < MRA_XSTATUS_COUNT+1; i++) {
		char szIconName[MAX_PATH];
		mir_snprintf(szIconName, "mra_xstatus%ld", i);
		sid.pszName = szIconName;

		int iCurIndex = i+IDI_XSTATUS1-1;
		sid.description.w = (wchar_t*)lpcszXStatusNameDef[i];
		sid.iDefaultIndex = -iCurIndex;

		hXStatusAdvancedStatusIcons[i] = g_plugin.addIcon(&sid);
	}
}

void DestroyXStatusIcons()
{
	char szBuff[MAX_PATH];

	for (DWORD i = 1; i < MRA_XSTATUS_COUNT+1; i++) {
		mir_snprintf(szBuff, "mra_xstatus%ld", i);
		IcoLib_RemoveIcon(szBuff);
	}

	memset(hXStatusAdvancedStatusIcons, 0, sizeof(hXStatusAdvancedStatusIcons));
}
