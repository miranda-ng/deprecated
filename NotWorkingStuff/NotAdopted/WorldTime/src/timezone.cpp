#include "common.h"
#include "timezone.h"

TimeList timezone_list(10), geo_timezone_list(10, LS_TZREG::compare);

int LS_TZREG::compare(const LS_TZREG *p1, const LS_TZREG *p2) {
	//return Index < other.Index;
	return p1->TZI.Bias - p2->TZI.Bias;
}

bool build_timezone_list() {
	HKEY HKlmtz;
	HKEY KKtz;
	DWORD dwIndex = 0;
	TCHAR tcName[MAX_SIZE];
	DWORD dwcbName = MAX_SIZE;
	DWORD dwcbValue;
	DWORD dwcbSTD;
	DWORD dwcbDLT;
	LS_TZREG Temp;
	FILETIME ftLastWrite;
	bool win9x = false;

	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, TZREG, 0, KEY_ENUMERATE_SUB_KEYS, &HKlmtz) != ERROR_SUCCESS) {
		win9x = true;
		if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, TZREG_9X, 0, KEY_ENUMERATE_SUB_KEYS, &HKlmtz) != ERROR_SUCCESS)
			return false;
	}

	while(RegEnumKeyEx(HKlmtz, dwIndex, tcName, &dwcbName, NULL, NULL, NULL, &ftLastWrite) != ERROR_NO_MORE_ITEMS) {

		if(RegOpenKeyEx(HKlmtz, tcName, 0,KEY_QUERY_VALUE, &KKtz) != ERROR_SUCCESS) {
			RegCloseKey(HKlmtz);
			return false;
		}

		_tcsncpy(Temp.tcName, tcName, MAX_SIZE);
		dwcbValue = MAX_SIZE;
		RegQueryValueEx(KKtz, _T("Display"), NULL, NULL, (LPBYTE)Temp.tcDisp, &dwcbValue);
		dwcbDLT = MAX_SIZE;
		RegQueryValueEx(KKtz, _T("Dlt"), NULL, NULL, (LPBYTE)Temp.tcDLT, &dwcbDLT);
		dwcbSTD = MAX_SIZE;
		RegQueryValueEx(KKtz, _T("Std"), NULL, NULL, (LPBYTE)Temp.tcSTD, &dwcbSTD);
		dwcbValue = MAX_SIZE;
		RegQueryValueEx(KKtz, _T("MapID"), NULL, NULL, (LPBYTE)Temp.MapID, &dwcbValue);
		if(!win9x) {
			dwcbValue = sizeof(DWORD);
			RegQueryValueEx(KKtz, _T("Index"), NULL, NULL, (LPBYTE)&Temp.Index, &dwcbValue);
		}
		dwcbValue = sizeof(Temp.TZI);
		RegQueryValueEx(KKtz, _T("TZI"), NULL, NULL, (LPBYTE)&Temp.TZI, &dwcbValue);

		RegCloseKey(KKtz);

		Temp.list_index = dwIndex;
		timezone_list.insert(new LS_TZREG(Temp));
		geo_timezone_list.insert(new LS_TZREG(Temp));

		dwcbName = MAX_SIZE;
		dwIndex++;
	}

	RegCloseKey(HKlmtz);

	return true;
}

