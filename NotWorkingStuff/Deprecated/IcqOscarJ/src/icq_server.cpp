// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
//
// Copyright © 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
// Copyright © 2001-2002 Jon Keating, Richard Hughes
// Copyright © 2002-2004 Martin Öberg, Sam Kothari, Robert Rainwater
// Copyright © 2004-2010 Joe Kucera
// Copyright © 2012-2018 Miranda NG team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
// -----------------------------------------------------------------------------
//  DESCRIPTION:
//
//  Manages main server connection, low-level communication
// -----------------------------------------------------------------------------

#include "stdafx.h"

void icq_newConnectionReceived(HNETLIBCONN hNewConnection, DWORD dwRemoteIP, void *pExtra);

/////////////////////////////////////////////////////////////////////////////////////////
// ICQ Server thread

void __cdecl CIcqProto::ServerThread(serverthread_start_info *infoParam)
{
	Thread_SetName("ICQ: ServerThread");

	serverthread_info info = { 0 };
	info.isLoginServer = info.bReinitRecver = true;
	info.wAuthKeyLen = infoParam->wPassLen;
	null_strcpy((char*)info.szAuthKey, infoParam->szPass, info.wAuthKeyLen);
	// store server port
	info.wServerPort = infoParam->nloc.wPort;

	srand(time(0));

	ResetSettingsOnConnect();

	// Connect to the login server
	debugLogA("Authenticating to server");
	{
		NETLIBOPENCONNECTION nloc = infoParam->nloc;
		nloc.timeout = 6;
		if (m_bGatewayMode)
			nloc.flags |= NLOCF_HTTPGATEWAY;

		hServerConn = NetLib_OpenConnection(m_hNetlibUser, nullptr, &nloc);

		SAFE_FREE((void**)&nloc.szHost);
		SAFE_FREE((void**)&infoParam);

		if (hServerConn && m_bSecureConnection) {
			if (!Netlib_StartSsl(hServerConn, nullptr)) {
				icq_LogMessage(LOG_ERROR, LPGEN("Unable to connect to ICQ login server, SSL could not be negotiated"));
				SetCurrentStatus(ID_STATUS_OFFLINE);
				NetLib_CloseConnection(&hServerConn, TRUE);
				return;
			}
		}
	}

	// Login error
	if (hServerConn == nullptr) {
		DWORD dwError = GetLastError();

		SetCurrentStatus(ID_STATUS_OFFLINE);

		icq_LogUsingErrorCode(LOG_ERROR, dwError, LPGEN("Unable to connect to ICQ login server"));
		return;
	}

	// Initialize direct connection ports
	DWORD dwInternalIP;
	BYTE bConstInternalIP = getByte("ConstRealIP", 0);

	info.hDirectBoundPort = NetLib_BindPort(icq_newConnectionReceived, this, &wListenPort, &dwInternalIP);
	if (!info.hDirectBoundPort) {
		icq_LogUsingErrorCode(LOG_WARNING, GetLastError(), LPGEN("Miranda was unable to allocate a port to listen for direct peer-to-peer connections between clients. You will be able to use most of the ICQ network without problems but you may be unable to send or receive files.\n\nIf you have a firewall this may be blocking Miranda, in which case you should configure your firewall to leave some ports open and tell Miranda which ports to use in Options -> Network -> ICQ client-to-client connections."));
		wListenPort = 0;
		if (!bConstInternalIP)
			delSetting("RealIP");
	}
	else if (!bConstInternalIP)
		setDword("RealIP", dwInternalIP);

	// Initialize rate limiting queues
	{
		mir_cslock l(m_ratesMutex);
		m_ratesQueue_Request = new rates_queue(this, "request", RML_IDLE_30, RML_IDLE_50, 1);
		m_ratesQueue_Response = new rates_queue(this, "response", RML_IDLE_10, RML_IDLE_30, -1);
	}

	StartKeepAlive(&info);

	// This is the "infinite" loop that receives the packets from the ICQ server
	NETLIBPACKETRECVER packetRecv;
	info.hPacketRecver = Netlib_CreatePacketReceiver(hServerConn, 0x2400);

	while (serverThreadHandle) {
		if (info.bReinitRecver) { // we reconnected, reinit struct
			info.bReinitRecver = false;
			memset(&packetRecv, 0, sizeof(packetRecv));
			packetRecv.dwTimeout = 1000;
		}

		int recvResult = Netlib_GetMorePackets(info.hPacketRecver, &packetRecv);
		if (recvResult == 0) {
			debugLogA("Clean closure of server socket");
			break;
		}

		if (recvResult == SOCKET_ERROR) {
			DWORD dwError = GetLastError();
			if (dwError == ERROR_TIMEOUT) {
				if (m_iDesiredStatus == ID_STATUS_OFFLINE)
					break;

				CheckKeepAlive(&info);
				continue;
			}
			if (dwError == WSAESHUTDOWN) // ok, we're going offline
				break;

			debugLogA("Abortive closure of server socket, error: %d", dwError);
			break;
		}

		// Deal with the packet
		CheckKeepAlive(&info);
		packetRecv.bytesUsed = handleServerPackets(packetRecv.buffer, packetRecv.bytesAvailable, &info);
	}
	serverThreadHandle = nullptr;

	// Time to shutdown
	debugLogA("Closing server connections...");
	NetLib_CloseConnection(&hServerConn, TRUE);

	// Close the packet receiver (connection may still be open)
	NetLib_SafeCloseHandle(&info.hPacketRecver);

	// Close DC port
	Netlib_CloseHandle(info.hDirectBoundPort);
	info.hDirectBoundPort = nullptr;

	// disable auto info-update thread
	icq_EnableUserLookup(FALSE);

	if (m_iStatus != ID_STATUS_OFFLINE && m_iDesiredStatus != ID_STATUS_OFFLINE) {
		if (!info.bLoggedIn)
			icq_LogMessage(LOG_FATAL, LPGEN("Connection failed.\nLogin sequence failed for unknown reason.\nTry again later."));

		// set flag indicating we were kicked out
		m_bConnectionLost = true;

		SetCurrentStatus(ID_STATUS_OFFLINE);
	}

	// signal keep-alive thread to stop
	StopKeepAlive(&info);

	// Close all open DC connections
	CloseContactDirectConns(NULL);

	// Close avatar connection if any
	StopAvatarThread();

	// Offline all contacts
	for (auto &hContact : AccContacts()) {
		if (getContactStatus(hContact) == ID_STATUS_OFFLINE)
			continue;

		setWord(hContact, "Status", ID_STATUS_OFFLINE);

		DWORD dwUIN;
		uid_str szUID;
		if (!getContactUid(hContact, &dwUIN, &szUID)) {
			char tmp = 0;
			handleXStatusCaps(dwUIN, szUID, hContact, (BYTE*)&tmp, 0, &tmp, 0);
		}
	}

	setDword("LogonTS", 0); // clear logon time

	servlistPendingFlushOperations(); // clear pending operations list

	// release rates queues
	{
		mir_cslock l(m_ratesMutex);
		delete m_ratesQueue_Request; m_ratesQueue_Request = nullptr;
		delete m_ratesQueue_Response;	m_ratesQueue_Response = nullptr;
		delete m_rates; m_rates = nullptr;
	}

	FlushServerIDs();         // clear server IDs list

	debugLogA("%s thread ended.", "Server");
}

void CIcqProto::icq_serverDisconnect()
{
	if (!hServerConn)
		return;

	debugLogA("Server shutdown requested");
	Netlib_Shutdown(hServerConn);

	debugLogA("Dropping server thread");
	if (serverThreadHandle) {
		debugLogA("Closing server thread handle: %08p", serverThreadHandle);
		CloseHandle(serverThreadHandle);
		serverThreadHandle = nullptr;
	}

	SetCurrentStatus(ID_STATUS_OFFLINE);
}

int CIcqProto::handleServerPackets(BYTE *buf, int len, serverthread_info *info)
{
	BYTE channel;
	WORD sequence;
	WORD datalen;
	int bytesUsed = 0;

	while (len > 0) {
		if (info->bReinitRecver)
			break;

		// All FLAPS begin with 0x2a
		if (*buf++ != FLAP_MARKER)
			break;

		if (len < 6)
			break;

		unpackByte(&buf, &channel);
		unpackWord(&buf, &sequence);
		unpackWord(&buf, &datalen);

		if (len < 6 + datalen)
			break;

		debugLogA("Server FLAP: Channel %u, Seq %u, Length %u bytes", channel, sequence, datalen);

		switch (channel) {
		case ICQ_LOGIN_CHAN:
			handleLoginChannel(buf, datalen, info);
			break;

		case ICQ_DATA_CHAN:
			handleDataChannel(buf, datalen, info);
			break;

		case ICQ_ERROR_CHAN:
			handleErrorChannel(buf, datalen);
			break;

		case ICQ_CLOSE_CHAN:
			handleCloseChannel(buf, datalen, info);
			break; // we need this for walking thru proxy

		case ICQ_PING_CHAN:
			handlePingChannel(buf, datalen);
			break;

		default:
			debugLogA("Warning: Unhandled Server FLAP Channel: Channel %u, Seq %u, Length %u bytes", channel, sequence, datalen);
			break;
		}

		/* Increase pointers so we can check for more FLAPs */
		buf += datalen;
		len -= (datalen + 6);
		bytesUsed += (datalen + 6);
	}

	return bytesUsed;
}


void CIcqProto::sendServPacket(icq_packet *pPacket)
{
	// make sure to have the connection handle
	mir_cslockfull l(connectionHandleMutex);

	if (hServerConn) {
		// This critsec makes sure that the sequence order doesn't get screwed up
		int nSendResult;
		{
			mir_cslock slck(localSeqMutex);

			// :IMPORTANT:
			// The FLAP sequence must be a WORD. When it reaches 0xFFFF it should wrap to
			// 0x0000, otherwise we'll get kicked by server.
			wLocalSequence++;

			// Pack sequence number
			pPacket->pData[2] = ((wLocalSequence & 0xff00) >> 8);
			pPacket->pData[3] = (wLocalSequence & 0x00ff);

			nSendResult = Netlib_Send(hServerConn, (const char *)pPacket->pData, pPacket->wLen, 0);
		}
		l.unlock();

		// Send error
		if (nSendResult == SOCKET_ERROR) {
			DWORD dwErrorCode = GetLastError();
			if (dwErrorCode != WSAESHUTDOWN)
				icq_LogUsingErrorCode(LOG_ERROR, GetLastError(), LPGEN("Your connection with the ICQ server was abortively closed"));
			icq_serverDisconnect();
		}
		else { // Rates management
			mir_cslock r(m_ratesMutex);
			m_rates->packetSent(pPacket);
		}
	}
	else debugLogA("Error: Failed to send packet (no connection)");

	SAFE_FREE((void**)&pPacket->pData);
}


void __cdecl CIcqProto::SendPacketAsyncThread(icq_packet* pkt)
{
	Thread_SetName("ICQ: SendPacketAsyncThread");
	sendServPacket(pkt);
	SAFE_FREE((void**)&pkt);
}


void CIcqProto::sendServPacketAsync(icq_packet *packet)
{
	icq_packet *pPacket;

	pPacket = (icq_packet*)SAFE_MALLOC(sizeof(icq_packet)); // This will be freed in the new thread
	memcpy(pPacket, packet, sizeof(icq_packet));

	ForkThread((MyThreadFunc)&CIcqProto::SendPacketAsyncThread, pPacket);
}


int CIcqProto::IsServerOverRate(WORD wFamily, WORD wCommand, int nLevel)
{
	mir_cslock l(m_ratesMutex);

	if (m_rates) {
		WORD wGroup = m_rates->getGroupFromSNAC(wFamily, wCommand);

		// check if the rate is not over specified level
		if (m_rates->getNextRateLevel(wGroup) < m_rates->getLimitLevel(wGroup, nLevel))
			return TRUE;
	}

	return FALSE;
}


/////////////////////////////////////////////////////////////////////////////////////////
// ICQ Server thread

void CIcqProto::icq_login(const char* szPassword)
{
	DWORD dwUin = getContactUin(NULL);
	serverthread_start_info* stsi = (serverthread_start_info*)SAFE_MALLOC(sizeof(serverthread_start_info));

	// Server host name
	char szServer[MAX_PATH];
	if (getSettingStringStatic(NULL, "OscarServer", szServer, MAX_PATH))
		stsi->nloc.szHost = null_strdup(m_bSecureConnection ? DEFAULT_SERVER_HOST_SSL : DEFAULT_SERVER_HOST);
	else
		stsi->nloc.szHost = null_strdup(szServer);

	// Server port
	stsi->nloc.wPort = getWord("OscarPort", m_bSecureConnection ? DEFAULT_SERVER_PORT_SSL : DEFAULT_SERVER_PORT);
	if (stsi->nloc.wPort == 0)
		stsi->nloc.wPort = RandRange(1024, 65535);

	// User password
	stsi->wPassLen = mir_strlen(szPassword);
	null_strcpy(stsi->szPass, szPassword, stsi->wPassLen);

	// Randomize sequence
	wLocalSequence = generate_flap_sequence();

	m_dwLocalUIN = dwUin;

	// Initialize members
	m_avatarsConnectionPending = TRUE;

	serverThreadHandle = ForkThreadEx((MyThreadFunc)&CIcqProto::ServerThread, stsi, &serverThreadId);
}
