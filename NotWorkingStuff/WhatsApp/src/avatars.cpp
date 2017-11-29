#include "stdafx.h"

INT_PTR WhatsAppProto::GetAvatarInfo(WPARAM wParam, LPARAM lParam)
{
	PROTO_AVATAR_INFORMATION *pai = (PROTO_AVATAR_INFORMATION*)lParam;

	ptrA id(getStringA(pai->hContact, isChatRoom(pai->hContact) ? "ChatRoomID" : WHATSAPP_KEY_ID));
	if (id == NULL)
		return GAIR_NOAVATAR;

	std::wstring tszFileName = GetAvatarFileName(pai->hContact);
	wcsncpy_s(pai->filename, tszFileName.c_str(), _TRUNCATE);
	pai->format = PA_FORMAT_JPEG;

	ptrA szAvatarId(getStringA(pai->hContact, WHATSAPP_KEY_AVATAR_ID));
	if (szAvatarId == NULL || (wParam & GAIF_FORCE) != 0)
		if (pai->hContact != NULL && m_pConnection != nullptr) {
			m_pConnection->sendGetPicture(id, "image");
			return GAIR_WAITFOR;
		}

	debugLogA("No avatar");
	return GAIR_NOAVATAR;
}

INT_PTR WhatsAppProto::GetAvatarCaps(WPARAM wParam, LPARAM lParam)
{
	switch (wParam) {
	case AF_PROPORTION:
		return PIP_SQUARE;

	case AF_FORMATSUPPORTED: // Jabber supports avatars of virtually all formats
		return PA_FORMAT_JPEG;

	case AF_ENABLED:
		return TRUE;

	case AF_MAXSIZE:
		POINT *size = (POINT*)lParam;
		if (size)
			size->x = size->y = 640;
		return 0;
	}
	return -1;
}

std::wstring WhatsAppProto::GetAvatarFileName(MCONTACT hContact)
{
	std::wstring result = m_tszAvatarFolder + L"\\";

	std::string jid;
	if (hContact != NULL) {
		ptrA szId(getStringA(hContact, isChatRoom(hContact) ? "ChatRoomID" : WHATSAPP_KEY_ID));
		if (szId == NULL)
			return L"";

		jid = szId;
	}
	else jid = m_szJid;

	return result + std::wstring(_A2T(jid.c_str())) + L".jpg";
}

INT_PTR WhatsAppProto::GetMyAvatar(WPARAM wParam, LPARAM lParam)
{
	std::wstring tszOwnAvatar(m_tszAvatarFolder + L"\\myavatar.jpg");
	wcsncpy_s((wchar_t*)wParam, lParam, tszOwnAvatar.c_str(), _TRUNCATE);
	return 0;
}

static std::vector<unsigned char>* sttFileToMem(const wchar_t *ptszFileName)
{
	HANDLE hFile = CreateFile(ptszFileName, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return nullptr;

	DWORD upperSize, lowerSize = GetFileSize(hFile, &upperSize);
	std::vector<unsigned char> *result = new std::vector<unsigned char>(lowerSize);
	ReadFile(hFile, (void*)result->data(), lowerSize, &upperSize, nullptr);
	CloseHandle(hFile);
	return result;
}

int WhatsAppProto::InternalSetAvatar(MCONTACT hContact, const char *szJid, const wchar_t *ptszFileName)
{
	if (!isOnline() || ptszFileName == nullptr)
		return 1;

	if (_waccess(ptszFileName, 4) != 0)
		return errno;

	ResizeBitmap resize = { 0 };
	if ((resize.hBmp = Bitmap_Load(ptszFileName)) == nullptr)
		return 2;
	resize.size = sizeof(resize);
	resize.fit = RESIZEBITMAP_KEEP_PROPORTIONS;
	resize.max_height = resize.max_width = 96;

	HBITMAP hbmpPreview = (HBITMAP)CallService(MS_IMG_RESIZE, (LPARAM)&resize, 0);
	if (hbmpPreview == nullptr)
		return 3;

	wchar_t tszTempFile[MAX_PATH], tszMyFile[MAX_PATH];
	if (hContact == NULL) {
		mir_snwprintf(tszMyFile, L"%s\\myavatar.jpg", m_tszAvatarFolder.c_str());
		mir_snwprintf(tszTempFile, L"%s\\myavatar.preview.jpg", m_tszAvatarFolder.c_str());
	}
	else {
		std::wstring tszContactAva = GetAvatarFileName(hContact);
		wcsncpy_s(tszMyFile, tszContactAva.c_str(), _TRUNCATE);
		wcsncpy_s(tszTempFile, (tszContactAva + L".preview").c_str(), _TRUNCATE);
	}

	IMGSRVC_INFO saveInfo = { sizeof(saveInfo), nullptr };
	saveInfo.hbm = hbmpPreview;
	saveInfo.tszName = tszTempFile;
	saveInfo.dwMask = IMGI_HBITMAP;
	saveInfo.fif = FIF_JPEG;
	CallService(MS_IMG_SAVE, (WPARAM)&saveInfo, IMGL_WCHAR);

	if (hbmpPreview != resize.hBmp)
		DeleteObject(hbmpPreview);
	DeleteObject(resize.hBmp);

	CopyFile(ptszFileName, tszMyFile, FALSE);

	m_pConnection->sendSetPicture(szJid, sttFileToMem(ptszFileName), sttFileToMem(tszTempFile));
	return 0;
}

INT_PTR WhatsAppProto::SetMyAvatar(WPARAM, LPARAM lParam)
{
	return InternalSetAvatar(NULL, m_szJid.c_str(), (const wchar_t*)lParam);
}
