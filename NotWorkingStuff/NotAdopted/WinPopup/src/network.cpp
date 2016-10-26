/*

WinPopup Protocol plugin for Miranda IM.

Copyright (C) 2004-2010 Nikolay Raspopov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"

// ��������, ��� ������ ������ �� IPv4-����� � ������� xx.xx.xx.xx
bool IsValidIP (LPCTSTR name)
{
	int dots = 0;
	bool digit = false;
	for ( ; *name; name++ )
	{
		if ( *name == _T('.') )
		{
			if ( ! digit )
				// ��� ����� ������
				return false;

			dots++;
			digit = false;
		}
		else if ( *name < _T('0') || *name > _T('9') )
		{
			// ��� �� �����
			return false;
		}
		else
		{
			digit = true;
		}
	}
	return dots == 3 && digit;
}

// ��������� IP �� ����� ����� � ��������� ��������� ����� �����
ip ResolveToIP (CString& name)
{
	// ��������� ����� �� IP-������
	ip addr = IsValidIP (name) ? ::inet_addr ( CT2A( name ) ) : INADDR_NONE;
	if (addr != INADDR_NONE) {
		struct hostent *hp = ::gethostbyaddr ((const char*) &addr, sizeof (addr), AF_INET);
		if (hp) {
			// ����������� ���
			name = hp->h_name;
			int n = name.Find ('.');
			if (n != -1)
				name = name.Left (n);
		}
	}
	// ��������� IP-������ �� �����
	if (addr == INADDR_NONE) {
		struct hostent *hp = ::gethostbyname ( CT2A( name ) );
		if (hp) {
			addr = ((struct in_addr*)hp->h_addr)->s_addr;
			// ����������� ���
			name = hp->h_name;
			int n = name.Find ('.');
			if (n != -1)
				name = name.Left (n);
		}
	}
	return ntohl (addr);
}
