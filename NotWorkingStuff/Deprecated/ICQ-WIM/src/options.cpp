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

#include "stdafx.h"

class CIcqEnterLoginDlg : public CIcqDlgBase
{
	CCtrlEdit edtPass;
	CCtrlCheck chkSave;

public:
	CIcqEnterLoginDlg(CIcqProto *ppro) :
		CIcqDlgBase(ppro, IDD_LOGINPW),
		edtPass(this, IDC_PASSWORD),
		chkSave(this, IDC_SAVEPASS)
	{
	}

	bool OnInitDialog() override
	{
		m_proto->m_bDlgActive = true;
		chkSave.SetState(m_proto->getBool("RememberPass"));
		Window_SetIcon_IcoLib(m_hwnd, m_proto->m_hProtoIcon);
		return true;
	}

	bool OnApply() override
	{
		m_proto->setByte("RememberPass", m_proto->m_bRememberPwd = chkSave.GetState());
		m_proto->m_szPassword = ptrA(edtPass.GetTextU());
		EndModal(true);
		return true;
	}

	void OnDestroy() override
	{
		m_proto->m_bDlgActive = false;
	}
};

bool CIcqProto::RetrievePassword()
{
	// if we registered via phone (i.e., server holds the password), we don't need to enter it
	if (getByte(DB_KEY_PHONEREG))
		return true;

	if (!m_szPassword.IsEmpty() && m_bRememberPwd)
		return true;
		
	m_szPassword = getMStringA("Password");
	if (!m_szPassword.IsEmpty()) {
		m_bRememberPwd = true;
		return true;
	}

	if (m_bDlgActive)
		return false;

	return CIcqEnterLoginDlg(this).DoModal();
}

/////////////////////////////////////////////////////////////////////////////////////////

struct CIcqRegistrationDlg : public CIcqDlgBase
{
	CMStringA szTrans, szMsisdn;
	int iErrorCode = 0;

	CCtrlEdit edtPhone, edtCode;
	CCtrlButton btnSendSms;

	CIcqRegistrationDlg(CIcqProto *ppro) :
		CIcqDlgBase(ppro, IDD_REGISTER),
		edtPhone(this, IDC_PHONE),
		edtCode(this, IDC_CODE),
		btnSendSms(this, IDC_SENDSMS)
	{
		btnSendSms.OnClick = Callback(this, &CIcqRegistrationDlg::onClick_SendSms);
		edtPhone.OnChange = Callback(this, &CIcqRegistrationDlg::onChange_Phone);
	}

	bool OnApply() override
	{
		auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "https://u.icq.net/api/v60/smsreg/loginWithPhoneNumber.php", &CIcqProto::OnLoginViaPhone);
		pReq << CHAR_PARAM("locale", "en") << CHAR_PARAM("msisdn", szMsisdn) << CHAR_PARAM("trans_id", szTrans) << CHAR_PARAM("k", APP_ID)
			<< CHAR_PARAM("r", pReq->m_reqId) << CHAR_PARAM("f", "json") << WCHAR_PARAM("sms_code", ptrW(edtCode.GetText())) << INT_PARAM("create_account", 1);
		pReq->pUserInfo = this;
		
		SetCursor(LoadCursor(0, IDC_WAIT));
		m_proto->ExecuteRequest(pReq);
		SetCursor(LoadCursor(0, IDC_ARROW));

		if (iErrorCode != 200)
			return false;

		EndDialog(m_hwnd, 1);
		return true;
	}

	void onChange_Phone(CCtrlEdit*)
	{
		auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "https://u.icq.net/api/v60/smsapi/fcgi-bin/smsphoneinfo", &CIcqProto::OnCheckPhone);
		pReq << CHAR_PARAM("service", "icq_registration") << CHAR_PARAM("info", "typing_check,score,iso_country_code") << CHAR_PARAM("lang", "ru")
			<< WCHAR_PARAM("phone", ptrW(edtPhone.GetText())) << CHAR_PARAM("id", pReq->m_reqId);
		pReq->pUserInfo = this;
		m_proto->Push(pReq);
	}

	void onClick_SendSms(CCtrlButton*)
	{
		auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "https://u.icq.net/api/v60/smsreg/requestPhoneValidation.php", &CIcqProto::OnValidateSms);
		pReq << CHAR_PARAM("locale", "en") << CHAR_PARAM("msisdn", szMsisdn) << CHAR_PARAM("r", pReq->m_reqId)
			<< CHAR_PARAM("smsFormatType", "human") << CHAR_PARAM("k", APP_ID);
		pReq->pUserInfo = this;
		m_proto->Push(pReq);
	}
};

void CIcqProto::OnCheckPhone(MHttpResponse *pReply, AsyncHttpRequest *pReq)
{
	if (pReply == nullptr || pReply->resultCode != 200)
		return;

	CIcqRegistrationDlg *pDlg = (CIcqRegistrationDlg*)pReq->pUserInfo;
	pDlg->btnSendSms.Disable();
	pDlg->edtCode.Disable();

	JSONROOT root(pReply->body);
	CMStringW wszStatus((*root)["status"].as_mstring());
	if (wszStatus != L"OK") {
		pDlg->edtCode.SetText((*root)["printable"].as_mstring());
		return;
	}

	CMStringA szPhoneNumber((*root)["typing_check"]["modified_phone_number"].as_mstring());
	CMStringA szPrefix((*root)["modified_prefix"].as_mstring());

	auto *pNew = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, "https://www.icq.com/smsreg/normalizePhoneNumber.php", &CIcqProto::OnNormalizePhone);
	pNew << CHAR_PARAM("countryCode", szPrefix) << CHAR_PARAM("phoneNumber", szPhoneNumber.c_str() + szPrefix.GetLength())
		<< CHAR_PARAM("k", APP_ID) << CHAR_PARAM("r", pReq->m_reqId);
	pNew->pUserInfo = pDlg;
	Push(pNew);
}

void CIcqProto::OnNormalizePhone(MHttpResponse *pReply, AsyncHttpRequest *pReq)
{
	CIcqRegistrationDlg *pDlg = (CIcqRegistrationDlg*)pReq->pUserInfo;

	JsonReply root(pReply);
	pDlg->iErrorCode = root.error();
	if (root.error() != 200)
		return;

	const JSONNode &data = root.data();
	pDlg->szMsisdn = data["msisdn"].as_mstring();
	pDlg->btnSendSms.Enable();
}

void CIcqProto::OnValidateSms(MHttpResponse *pReply, AsyncHttpRequest *pReq)
{
	JsonReply root(pReply);
	if (root.error() != 200)
		return;

	CIcqRegistrationDlg *pDlg = (CIcqRegistrationDlg*)pReq->pUserInfo;
	const JSONNode &data = root.data();
	pDlg->szTrans = data["trans_id"].as_mstring();

	pDlg->edtCode.Enable();
	pDlg->edtCode.SetText(L"");
}

void CIcqProto::OnLoginViaPhone(MHttpResponse *pReply, AsyncHttpRequest *pReq)
{
	CIcqRegistrationDlg *pDlg = (CIcqRegistrationDlg*)pReq->pUserInfo;

	JsonReply root(pReply);
	pDlg->iErrorCode = root.error();
	if (root.error() != 200)
		return;

	const JSONNode &data = root.data();
	m_szAToken = data["token"]["a"].as_mstring();
	mir_urlDecode(m_szAToken.GetBuffer());
	setString(DB_KEY_ATOKEN, m_szAToken);

	int srvTS = data["hostTime"].as_int();
	m_iTimeShift = (srvTS) ? time(0) - srvTS : 0;

	m_szSessionKey = data["sessionKey"].as_mstring();
	setString(DB_KEY_SESSIONKEY, m_szSessionKey);

	SetOwnId(data["loginId"].as_mstring());
	setByte(DB_KEY_PHONEREG, 1);
}

/////////////////////////////////////////////////////////////////////////////////////////

class COptionsDlg : public CIcqDlgBase
{
	CCtrlEdit edtUin, edtPassword;
	CCtrlCheck chkHideChats, chkTrayIcon, chkLaunchMailbox, chkShowErrorPopups;
	CCtrlButton btnCreate;
	CMStringW wszOldPass;

public:
	COptionsDlg(CIcqProto *ppro, int iDlgID, bool bFullDlg) :
		CIcqDlgBase(ppro, iDlgID),
		edtUin(this, IDC_UIN),
		btnCreate(this, IDC_REGISTER),
		edtPassword(this, IDC_PASSWORD),
		chkTrayIcon(this, IDC_USETRAYICON),
		chkHideChats(this, IDC_HIDECHATS),
		chkLaunchMailbox(this, IDC_LAUNCH_MAILBOX),
		chkShowErrorPopups(this, IDC_SHOWERRORPOPUPS)
	{
		btnCreate.OnClick = Callback(this, &COptionsDlg::onClick_Register);

		CreateLink(edtUin, ppro->m_szOwnId);
		if (bFullDlg) {
			CreateLink(chkHideChats, ppro->m_bHideGroupchats);
			CreateLink(chkTrayIcon, ppro->m_bUseTrayIcon);
			CreateLink(chkLaunchMailbox, ppro->m_bLaunchMailbox);
			CreateLink(chkShowErrorPopups, ppro->m_bErrorPopups);

			chkTrayIcon.OnChange = Callback(this, &COptionsDlg::onChange_Tray);
		}
	}

	bool OnInitDialog() override
	{
		btnCreate.Hide();

		wszOldPass = m_proto->getMStringW("Password");
		edtPassword.SetText(wszOldPass);
		return true;
	}

	bool OnApply() override
	{
		ptrW wszPass(edtPassword.GetText());
		if (wszPass)
			m_proto->setWString("Password", wszPass);
		else
			m_proto->delSetting("Password");

		if (wszOldPass != wszPass) {
			m_proto->delSetting(DB_KEY_ATOKEN);
			m_proto->delSetting(DB_KEY_SESSIONKEY);
			m_proto->delSetting(DB_KEY_PHONEREG);
		}

		if (mir_wstrlen(wszPass)) {
			m_proto->m_szPassword = T2Utf(wszPass).get();
			m_proto->m_bRememberPwd = true;
		}
		else m_proto->m_bRememberPwd = m_proto->getByte("RememberPass");

		return true;
	}

	void onChange_Tray(CCtrlCheck*)
	{
		chkLaunchMailbox.Enable(chkTrayIcon.GetState());
	}

	void onClick_Register(CCtrlButton*)
	{
		CIcqRegistrationDlg dlg(m_proto);
		dlg.SetParent(m_hwnd);
		if (dlg.DoModal()) // force exit to avoid data corruption
			PostMessage(m_hwndParent, WM_COMMAND, MAKELONG(IDCANCEL, BN_CLICKED), 0);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// Advanced options

class CAdvOptionsDlg : public CIcqDlgBase
{
	CCtrlEdit edtDiff1, edtDiff2;
	CCtrlSpin spin1, spin2;
	CCtrlCombo cmbStatus1, cmbStatus2;

public:
	CAdvOptionsDlg(CIcqProto *ppro) :
		CIcqDlgBase(ppro, IDD_OPTIONS_ADV),
		spin1(this, IDC_SPIN1, 32000),
		spin2(this, IDC_SPIN2, 32000),
		edtDiff1(this, IDC_DIFF1),
		edtDiff2(this, IDC_DIFF2),
		cmbStatus1(this, IDC_STATUS1),
		cmbStatus2(this, IDC_STATUS2)
	{
		edtDiff1.OnChange = Callback(this, &CAdvOptionsDlg::onChange_Timeout1);
		edtDiff2.OnChange = Callback(this, &CAdvOptionsDlg::onChange_Timeout2);

		spin1.OnChange = Callback(this, &CAdvOptionsDlg::onChange_Spin1);
		spin2.OnChange = Callback(this, &CAdvOptionsDlg::onChange_Spin2);

		CreateLink(spin1, ppro->m_iTimeDiff1);
		CreateLink(spin2, ppro->m_iTimeDiff2);
	}

	bool OnInitDialog() override
	{
		for (uint32_t iStatus = ID_STATUS_OFFLINE; iStatus <= ID_STATUS_MAX; iStatus++) {
			int idx = cmbStatus1.AddString(Clist_GetStatusModeDescription(iStatus, 0), iStatus);
			if (iStatus == m_proto->m_iStatus1)
				cmbStatus1.SetCurSel(idx);

			idx = cmbStatus2.AddString(Clist_GetStatusModeDescription(iStatus, 0), iStatus);
			if (iStatus == m_proto->m_iStatus2)
				cmbStatus2.SetCurSel(idx);
		}

		return true;
	}

	bool OnApply() override
	{
		m_proto->m_iStatus1 = cmbStatus1.GetCurData();
		m_proto->m_iStatus2 = cmbStatus2.GetCurData();
		return true;
	}

	void onChange_Value1(int val)
	{
		bool bEnabled = val != 0;
		spin2.Enable(bEnabled);
		edtDiff2.Enable(bEnabled);
		cmbStatus1.Enable(bEnabled);
		cmbStatus2.Enable(bEnabled && spin2.GetPosition() != 0);
	}

	void onChange_Timeout1(CCtrlEdit *)
	{
		onChange_Value1(edtDiff1.GetInt());
	}

	void onChange_Spin1(CCtrlEdit *)
	{
		onChange_Value1(spin1.GetPosition());
	}

	void onChange_Timeout2(CCtrlEdit *)
	{
		cmbStatus2.Enable(edtDiff1.GetInt() != 0 && edtDiff2.GetInt() != 0);
	}

	void onChange_Spin2(CCtrlEdit *)
	{
		cmbStatus2.Enable(spin1.GetPosition() != 0 && spin2.GetPosition() != 0);
	}
};

/////////////////////////////////////////////////////////////////////////////////////////
// Services

MWindow CIcqProto::OnCreateAccMgrUI(MWindow hwndParent)
{
	COptionsDlg *pDlg = new COptionsDlg(this, IDD_OPTIONS_ACCMGR, false);
	pDlg->SetParent(hwndParent);
	pDlg->Create();
	return pDlg->GetHwnd();
}

int CIcqProto::OnOptionsInit(WPARAM wParam, LPARAM)
{
	OPTIONSDIALOGPAGE odp = {};
	odp.szTitle.w = m_tszUserName;
	odp.flags = ODPF_UNICODE | ODPF_BOLDGROUPS;
	odp.szGroup.w = LPGENW("Network");
	odp.position = 1;

	odp.szTab.w = LPGENW("General");
	odp.pDialog = new COptionsDlg(this, IDD_OPTIONS_FULL, true);
	g_plugin.addOptions(wParam, &odp);

	odp.szTab.w = LPGENW("Advanced");
	odp.pDialog = new CAdvOptionsDlg(this);
	g_plugin.addOptions(wParam, &odp);
	return 0;
}
