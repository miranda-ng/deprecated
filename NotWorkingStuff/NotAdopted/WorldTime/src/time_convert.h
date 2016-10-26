#ifndef _TIME_CONVERT_H
#define _TIME_CONVERT_H

/*

  from http://cvs.sourceforge.net/viewcvs.py/tortoisecvs/TortoiseCVS/src/Utils/Attic/ClockRackCon.c?view=markup
  (GPL)

----------------------------------------------------------------
   ClockRackCon.c -- Time-zone time conversion for ClockRack 1.1
                     (c) Ziff Davis Media, Inc.
                     All rights reserved.

   First published in PC Magazine, US edition, September 1, 2000.

   Programmed by Charles Petzold.

   The entire function of this module is to implement the 
     MySystemTimeToTzSpecificLocalTime all because Windows 95 & 98
     do not implement the NT function SystemTimeToTzSpecificLocalTime.
  ---------------------------------------------------------------------


*/

BOOL LocalGreaterThanTransition (const SYSTEMTIME * pstLoc, const SYSTEMTIME * pstTran);

BOOL MySystemTimeToTzSpecificLocalTime(const TIME_ZONE_INFORMATION *pTZI, const SYSTEMTIME *st, SYSTEMTIME *other_st);

VOID MyGetSystemTime(LPSYSTEMTIME lpSystemTime);

#endif