#ifndef _TIMEZONE_H
#define _TIMEZONE_H

#define TZREG _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Time Zones")
#define TZREG_9X _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Time Zones")
//#define TZREG2 _T("SYSTEM\\CurrentControlSet\\Control\\TimeZoneInformation")
#define MAX_SIZE 512

#include "common.h"

struct REG_TZI {
	LONG Bias;
	LONG StandardBias;
	LONG DaylightBias;
	SYSTEMTIME StandardDate; 
	SYSTEMTIME DaylightDate;
};


struct LS_TZREG {
	TCHAR tcName[MAX_SIZE];
	TCHAR tcDisp[MAX_SIZE];
	TCHAR tcDLT[MAX_SIZE];
	TCHAR tcSTD[MAX_SIZE];
	TCHAR MapID[MAX_SIZE];
	DWORD Index;
	DWORD ActiveTimeBias;
	//TIME_ZONE_INFORMATION TZI;
	REG_TZI TZI;

	unsigned int list_index;

	static int compare(const LS_TZREG *p1, const LS_TZREG *p2);
};

typedef OBJLIST< LS_TZREG > TimeList;

extern TimeList timezone_list, geo_timezone_list;
bool build_timezone_list();

void convert_regdata_to_tzi(int vector_index);

#endif
