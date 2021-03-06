/*
 *  xfirelib - C++ Library for the xfire protocol.
 *  Copyright (C) 2006 by
 *          Beat Wolf <asraniel@fryx.ch> / http://gfire.sf.net
 *          Herbert Poul <herbert.poul@gmail.com> / http://goim.us
 *			dufte <dufte@justmail.de>
 *
 *    http://xfirelib.sphene.net
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "stdafx.h"
#include "sendsidpacket.h"
#include <string.h>
#include <iostream>

/*
	whois packet von xfire, f�r z.b. friends of friends
*/

namespace xfirelib
{
	int SendSidPacket::getPacketContent(char *packet)
	{
		int index = 0;

		XERROR("Send Sid Packet!\n");

		packet[index++] = 0x03;
		packet[index++] = 's';
		packet[index++] = 'i';
		packet[index++] = 'd';
		packet[index++] = 4;
		packet[index++] = 3;
		XDEBUG2("Sids: %d\n", sids->size());
		packet[index++] = sids->size();
		packet[index++] = 0;

		for (uint i = 0; i < sids->size(); i++) {
			XDEBUG2("Sid%d:", i);
			char* sid = sids->at(i);
			for (int u = 0; u < 16; u++) {
				XDEBUG2("%x,", sid[u]);
				packet[index++] = sid[u];
			}
			XDEBUG("\n");
		}

		length = index;
		return index;
	}

	int SendSidPacket::getPacketAttributeCount()
	{
		return 1;
	}
}
