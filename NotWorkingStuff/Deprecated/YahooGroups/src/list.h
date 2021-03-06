/*
YahooGroups plugin for Miranda IM

Copyright � 2007 Cristian Libotean

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

#ifndef M_YAHOOGROUPS_LIST_H
#define M_YAHOOGROUPS_LIST_H

#include "stdafx.h"

class CGroupsList
{
	protected:
		char **groups;
		int size;
		int count;
		
		void Enlarge(int amount);
		void EnsureCapacity();
		
	public:
		CGroupsList();
		~CGroupsList();
		
		void Add(char *group);
		void Clear();
		
		int Count();
		
		int Contains(const char *group);
		int Index(const char *group);
		
		char *operator [](int index);
};

extern CGroupsList availableGroups;

#endif //M_VIUPLOADER_LIST_H