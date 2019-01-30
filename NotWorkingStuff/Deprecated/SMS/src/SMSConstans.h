#if !defined(AFX_SMS_CONSTANS_H__F58D13FF_F6F2_476C_B8F0_7B9E9357CF48__INCLUDED_)
#define AFX_SMS_CONSTANS_H__F58D13FF_F6F2_476C_B8F0_7B9E9357CF48__INCLUDED_

#define MODULENAME			"SMSPlugin"
#define PROTOCOL_NAMEW			L"SMSPlugin"
#define PROTOCOL_NAME_LEN		(sizeof(MODULENAME)-1)
#define PROTOCOL_NAME_SIZE		sizeof(MODULENAME)
#define PROTOCOL_DISPLAY_NAME_ORIG	"SMS"

#define TIMEOUT_MSGSEND			60000			// send sms timeout
#define MIN_SMS_DBEVENT_LEN		4				// для фильтрации событий авторизации от джабер плагина
#define MAX_PHONE_LEN			MAX_PATH
#define PHONES_MIN_COUNT		4 //internal	// количество элементов обязательно проверяемых при извлечении email адреса из инфы юзера

#define SMS_DEFAULT_
#define SMS_DEFAULT_SIGNATUREPOS	FALSE
#define SMS_DEFAULT_SHOWACK		TRUE
#define SMS_DEFAULT_USESIGNATURE	TRUE
#define SMS_DEFAULT_AUTOPOP		FALSE
#define SMS_DEFAULT_SAVEWINPOS		FALSE



//
#define ICQEVENTTYPE_SMSCONFIRMATION 3001


//Fonts definitions
//#define SRMMMOD 			"SRMM"
#define SRMMMOD 			"TabSRMM_Fonts"

#define FONTF_BOLD   1
#define FONTF_ITALIC 2
struct FontOptionsList
{
	COLORREF defColour;
	wchar_t*   szDefFace;
	BYTE     defStyle;
	char     defSize;
}

static fontOptionsList[] = {
	{ RGB(106, 106, 106), L"Arial", 0, -12},
};


#define MSGFONTID_MYMSG			0
#define MSGFONTID_YOURMSG		2
#define MSGFONTID_MESSAGEAREA		16


#define SRMSGSET_BKGCOLOUR		"BkgColour"
#define SRMSGSET_INPBKGCOLOUR		"inputbg"
#define SRMSGDEFSET_BKGCOLOUR		GetSysColor(COLOR_WINDOW)
#define FONTF_BOLD   1
#define FONTF_ITALIC 2



//Decleration of functions that used when user hit miranda for new message/confirmation
int ReadAckSMS(WPARAM wParam,LPARAM lParam);
int ReadMsgSMS(WPARAM wParam,LPARAM lParam);
int SendSMSMenuCommand(WPARAM wParam,LPARAM lParam);



#define SMS_READ			"/ReadSms"
#define SMS_READ_ACK			"/ReadSmsAck"
#define SMS_SEND			"/SendSMS"
#define SMS_SEND_STR			LPGENW("Send &SMS...")
#define SMS_SEND_CM_STR			LPGENW("&SMS Message")

// структура содержащая информацию о сервисах/функциях
struct SERVICE_ITEM
{
	LPSTR	lpszName;		// имя сервиса, оно же имя в иколибе
	LPVOID	lpFunc;			// функция вызываемая
};

static const SERVICE_ITEM siPluginServices[] =
{
	{ SMS_READ,     ReadMsgSMS },
	{ SMS_READ_ACK, ReadAckSMS },
	{ SMS_SEND,     SendSMSMenuCommand },
};

#endif // !defined(AFX_SMS_CONSTANS_H__F58D13FF_F6F2_476C_B8F0_7B9E9357CF48__INCLUDED_)
