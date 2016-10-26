#include "stdafx.h"

bool CEmLanProto::IsOnline()
{
	return m_iStatus > ID_STATUS_OFFLINE;
}