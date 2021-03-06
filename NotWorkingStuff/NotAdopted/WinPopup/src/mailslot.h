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

class mailslot
{
public:
	mailslot();
	~mailslot();

	bool Create(LPCTSTR Name);
	void AskForDestroy();					// ��� ���������
	void Destroy();
	bool IsValid() const;
	bool SendMailslotMessage(HANDLE hContact, LPCTSTR msg, DWORD& err);

protected:
	CString					m_sName;		// ��� ���������
	HANDLE					m_hMailslot;	// ������� ���������
	CComAutoCriticalSection	m_cs;			// ������ ������
	HANDLE					m_MonitorTerm;	// ������� ��� ��������� Monitor
	HANDLE					m_Monitor;		// �����/�������� ��������� ����� ��������

	bool Receive(unsigned char* buf /* OEM */, DWORD size);
	static void MonitorThread(void* param);
	void Monitor();
};

extern mailslot pluginMailslot;				// �������� ��� ������ ���������
