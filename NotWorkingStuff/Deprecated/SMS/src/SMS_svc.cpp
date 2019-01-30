#include "stdafx.h"

int LoadServices(void)
{
	char szServiceFunction[MAX_PATH], *pszServiceFunctionName;

	memcpy(szServiceFunction, MODULENAME, PROTOCOL_NAME_SIZE);
	pszServiceFunctionName = szServiceFunction + PROTOCOL_NAME_LEN;

	// Service creation
	for (auto &it : siPluginServices) {
		memcpy(pszServiceFunctionName, it.lpszName, (mir_strlen(it.lpszName) + 1));
		CreateServiceFunction(szServiceFunction, (MIRANDASERVICE)it.lpFunc);
	}
	return 0;
}


int LoadModules(void)
{
	char szServiceFunction[MAX_PATH];
	mir_snprintf(szServiceFunction, "%s%s", MODULENAME, SMS_SEND);

	CMenuItem mi(&g_plugin);

	SET_UID(mi, 0x3ce387db, 0xbaac, 0x490f, 0xac, 0xab, 0x8c, 0xf7, 0xe9, 0xcd, 0x86, 0xa1);
	mi.position = 300050000;
	mi.hIcolibItem = Skin_LoadIcon(SKINICON_OTHER_SMS);
	mi.name.w = SMS_SEND_STR;
	mi.pszService = szServiceFunction;
	mi.flags = CMIF_UNICODE;
	Menu_AddMainMenuItem(&mi);

	SET_UID(mi, 0x736e4cff, 0x769e, 0x45dc, 0x8b, 0x78, 0x83, 0xf9, 0xe4, 0xbb, 0x81, 0x9e);
	mi.position = -2000070000;
	mi.hIcolibItem = Skin_LoadIcon(SKINICON_OTHER_SMS);
	mi.name.w = SMS_SEND_CM_STR;
	mi.pszService = szServiceFunction;
	mi.flags = CMIF_UNICODE;
	ssSMSSettings.hContactMenuItems[0] = Menu_AddContactMenuItem(&mi);

	g_plugin.addSound("RecvSMSMsg", PROTOCOL_NAMEW, LPGENW("Incoming SMS Message"));
	g_plugin.addSound("RecvSMSConfirmation", PROTOCOL_NAMEW, LPGENW("Incoming SMS Confirmation"));

	RefreshAccountList(NULL, NULL);
	return 0;
}

int SmsRebuildContactMenu(WPARAM wParam, LPARAM)
{
	Menu_ShowItem(ssSMSSettings.hContactMenuItems[0], GetContactPhonesCount(wParam) != 0);
	return 0;
}

//This function called when user clicked Menu.
int SendSMSMenuCommand(WPARAM wParam, LPARAM)
{
	HWND hwndSendSms;

	// user clicked on the "SMS Message" on one of the users
	if (wParam) {
		hwndSendSms = SendSMSWindowIsOtherInstanceHContact(wParam);
		if (hwndSendSms)
			SetFocus(hwndSendSms);
		else
			hwndSendSms = SendSMSWindowAdd(wParam);
	}
	// user clicked on the "SMS Send" in the Main Menu
	else {
		hwndSendSms = SendSMSWindowAdd(NULL);
		EnableWindow(GetDlgItem(hwndSendSms, IDC_NAME), TRUE);
		EnableWindow(GetDlgItem(hwndSendSms, IDC_SAVENUMBER), FALSE);

		for (auto &hContact : Contacts()) {
			if (GetContactPhonesCount(hContact)) {
				SendDlgItemMessage(hwndSendSms, IDC_NAME, CB_ADDSTRING, 0, (LPARAM)Clist_GetContactDisplayName(hContact));
				SendSMSWindowSMSContactAdd(hwndSendSms, hContact);
			}
		}
	}
	return 0;
}

// This function used to popup a read SMS window after the user clicked on the received SMS message.
int ReadMsgSMS(WPARAM, LPARAM lParam)
{
	CLISTEVENT *cle = (CLISTEVENT*)lParam;

	DBEVENTINFO dbei = {};
	if ((dbei.cbBlob = db_event_getBlobSize(((CLISTEVENT*)lParam)->hDbEvent)) == -1)
		return 1;
	dbei.pBlob = (PBYTE)_alloca(dbei.cbBlob);

	if (db_event_get(cle->hDbEvent, &dbei) == 0)
		if (dbei.eventType == ICQEVENTTYPE_SMS || dbei.eventType == ICQEVENTTYPE_SMSCONFIRMATION)
			if (dbei.cbBlob > MIN_SMS_DBEVENT_LEN) {
				if (RecvSMSWindowAdd(cle->hContact, ICQEVENTTYPE_SMS, nullptr, 0, (LPSTR)dbei.pBlob, dbei.cbBlob)) {
					db_event_markRead(cle->hContact, cle->hDbEvent);
					return 0;
				}
			}
	return 1;
}

// This function used to popup a read SMS window after the user clicked on the received SMS confirmation.
int ReadAckSMS(WPARAM, LPARAM lParam)
{
	CLISTEVENT *cle = (CLISTEVENT*)lParam;

	DBEVENTINFO dbei = {};
	if ((dbei.cbBlob = db_event_getBlobSize(cle->hDbEvent)) == -1)
		return 1;
	dbei.pBlob = (PBYTE)_alloca(dbei.cbBlob);

	if (db_event_get(cle->hDbEvent, &dbei) == 0)
	if (dbei.eventType == ICQEVENTTYPE_SMS || dbei.eventType == ICQEVENTTYPE_SMSCONFIRMATION)
	if (dbei.cbBlob > MIN_SMS_DBEVENT_LEN) {
		if (RecvSMSWindowAdd(cle->hContact, ICQEVENTTYPE_SMSCONFIRMATION, nullptr, 0, (LPSTR)dbei.pBlob, dbei.cbBlob)) {
			db_event_delete(cle->hContact, cle->hDbEvent);
			return 0;
		}
	}
	return 1;
}
