// ---------------------------------------------------------------------------80
//                ICQ plugin for Miranda Instant Messenger
//                ________________________________________
// 
// Copyright © 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
// Copyright © 2001-2002 Jon Keating, Richard Hughes
// Copyright © 2002-2004 Martin Öberg, Sam Kothari, Robert Rainwater
// Copyright © 2004-2009 Joe Kucera
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
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// -----------------------------------------------------------------------------
//  DESCRIPTION:
//
//  Helper functions for Oscar TLV chains
// -----------------------------------------------------------------------------

#include "stdafx.h"

/* set maxTlvs<=0 to get all TLVs in length, or a positive integer to get at most the first n */
oscar_tlv_chain* readIntoTLVChain(BYTE **buf, size_t wLen, int maxTlvs)
{
	oscar_tlv_chain *now, *last = nullptr, *chain = nullptr;
	WORD now_tlv_len;

	if (!buf || !wLen)
		return nullptr;

	intptr_t len = wLen;
	while (len > 0) { /* don't use unsigned variable for this check */
		now = (oscar_tlv_chain *)SAFE_MALLOC(sizeof(oscar_tlv_chain));

		if (!now) {
			disposeChain(&chain);
			return nullptr;
		}

		unpackWord(buf, &(now->tlv.wType));
		unpackWord(buf, &now_tlv_len);
		now->tlv.wLen = now_tlv_len;
		len -= 4;

		if (now_tlv_len < 1)
			now->tlv.pData = nullptr;
		else if (now_tlv_len <= len) {
			now->tlv.pData = (BYTE *)SAFE_MALLOC(now_tlv_len);
			if (now->tlv.pData)
				memcpy(now->tlv.pData, *buf, now_tlv_len);
		}
		else { // the packet is shorter than it should be
			SAFE_FREE((void**)&now);
			return chain; // give at least the rest of chain
		}

		if (chain) // keep the original order
			last->next = now;
		else
			chain = now;

		last = now;

		len -= now_tlv_len;
		*buf += now_tlv_len;

		if (--maxTlvs == 0)
			break;
	}

	return chain;
}

// Returns a pointer to the TLV with type wType and number wIndex in the chain
// If wIndex = 1, the first matching TLV will be returned, if wIndex = 2,
// the second matching one will be returned.
// wIndex must be > 0
oscar_tlv* oscar_tlv_chain::getTLV(WORD wType, WORD wIndex)
{
	int i = 0;
	oscar_tlv_chain *list = this;

	while (list) {
		if (list->tlv.wType == wType)
			i++;
		if (i >= wIndex)
			return &list->tlv;
		list = list->next;
	}

	return nullptr;
}

WORD oscar_tlv_chain::getChainLength()
{
	int len = 0;
	oscar_tlv_chain *list = this;

	while (list) {
		len += list->tlv.wLen + 4;
		list = list->next;
	}
	return len;
}

oscar_tlv* oscar_tlv_chain::putTLV(WORD wType, size_t wLen, BYTE *pData, BOOL bReplace)
{
	oscar_tlv *pTLV = getTLV(wType, 1);

	if (pTLV && bReplace)
		SAFE_FREE((void**)&pTLV->pData);
	else {
		oscar_tlv_chain *last = this;

		while (last && last->next)
			last = last->next;

		if (last) {
			last->next = (oscar_tlv_chain*)SAFE_MALLOC(sizeof(oscar_tlv_chain));
			pTLV = &last->next->tlv;
			pTLV->wType = wType;
		}
	}
	if (pTLV) {
		pTLV->wLen = WORD(wLen);
		pTLV->pData = (PBYTE)SAFE_MALLOC(wLen);
		memcpy(pTLV->pData, pData, wLen);
	}
	return pTLV;
}

oscar_tlv_chain* oscar_tlv_chain::removeTLV(oscar_tlv *pTLV)
{
	oscar_tlv_chain *list = this, *prev = nullptr, *chain = this;
	while (list) {
		if (&list->tlv == pTLV) {
			if (prev) // relink
				prev->next = list->next;
			else if (list->next) { // move second item's pTLV to the first item
				list->tlv = list->next->tlv;
				list = list->next;
			}
			else // result is an empty chain (NULL)
				chain = nullptr;
			// release chain item memory
			SAFE_FREE((void**)&list->tlv.pData);
			SAFE_FREE((void**)&list);
		}
		prev = list;
		list = list->next;
	}

	return chain;
}

WORD oscar_tlv_chain::getLength(WORD wType, WORD wIndex)
{
	oscar_tlv *pTLV = getTLV(wType, wIndex);
	if (pTLV)
		return pTLV->wLen;

	return 0;
}

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
/* Values are returned in MSB format */
/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

DWORD oscar_tlv_chain::getDWord(WORD wType, WORD wIndex)
{
	DWORD dw = 0;

	oscar_tlv *pTLV = getTLV(wType, wIndex);
	if (pTLV && pTLV->wLen >= 4) {
		dw |= (*((pTLV->pData) + 0) << 24);
		dw |= (*((pTLV->pData) + 1) << 16);
		dw |= (*((pTLV->pData) + 2) << 8);
		dw |= (*((pTLV->pData) + 3));
	}

	return dw;
}

WORD oscar_tlv_chain::getWord(WORD wType, WORD wIndex)
{
	WORD w = 0;

	oscar_tlv *pTLV = getTLV(wType, wIndex);
	if (pTLV && pTLV->wLen >= 2) {
		w |= (*((pTLV->pData) + 0) << 8);
		w |= (*((pTLV->pData) + 1));
	}

	return w;
}

BYTE oscar_tlv_chain::getByte(WORD wType, WORD wIndex)
{
	BYTE b = 0;

	oscar_tlv *pTLV = getTLV(wType, wIndex);
	if (pTLV && pTLV->wLen)
		b = *(pTLV->pData);

	return b;
}

int oscar_tlv_chain::getNumber(WORD wType, WORD wIndex)
{
	oscar_tlv *pTLV = getTLV(wType, wIndex);
	if (pTLV) {
		if (pTLV->wLen == 1)
			return getByte(wType, wIndex);
		if (pTLV->wLen == 2)
			return getWord(wType, wIndex);
		if (pTLV->wLen == 4)
			return getDWord(wType, wIndex);
	}
	return 0;
}

double oscar_tlv_chain::getDouble(WORD wType, WORD wIndex)
{
	oscar_tlv *pTLV = getTLV(wType, wIndex);
	if (pTLV && pTLV->wLen == 8) {
		BYTE *buf = pTLV->pData;
		double d = 0;

		unpackQWord(&buf, (DWORD64*)&d);

		return d;
	}
	return 0;
}

char* oscar_tlv_chain::getString(WORD wType, WORD wIndex)
{
	oscar_tlv *pTLV = getTLV(wType, wIndex);
	if (pTLV) {
		char *str = (char*)SAFE_MALLOC(pTLV->wLen + 1); /* For \0 */
		if (!str)
			return nullptr;

		memcpy(str, pTLV->pData, pTLV->wLen);
		str[pTLV->wLen] = '\0';
		return str;
	}

	return nullptr;
}

void disposeChain(oscar_tlv_chain **chain)
{
	if (!chain || !*chain)
		return;

	oscar_tlv_chain *now = *chain;
	while (now) {
		oscar_tlv_chain *next = now->next;

		SAFE_FREE((void**)&now->tlv.pData);
		SAFE_FREE((void**)&now);
		now = next;
	}

	*chain = nullptr;
}

oscar_tlv_record_list* readIntoTLVRecordList(BYTE **buf, size_t wLen, int nCount)
{
	oscar_tlv_record_list *list = nullptr, *last = nullptr;

	while (wLen >= 2) {
		WORD wRecordSize;

		unpackWord(buf, &wRecordSize);
		wLen -= 2;
		if (wRecordSize && wRecordSize <= wLen) {
			oscar_tlv_record_list *pRecord = (oscar_tlv_record_list*)SAFE_MALLOC(sizeof(oscar_tlv_record_list));
			BYTE *pData = *buf;

			*buf += wRecordSize;
			wLen -= wRecordSize;

			pRecord->item = readIntoTLVChain(&pData, wRecordSize, 0);
			if (pRecord->item) { // keep the order
				if (list)
					last->next = pRecord;
				else
					list = pRecord;

				last = pRecord;
			}
			else SAFE_FREE((void**)&pRecord);
		}

		if (--nCount == 0)
			break;
	}
	return list;
}

void disposeRecordList(oscar_tlv_record_list** list)
{
	if (!list || !*list)
		return;

	oscar_tlv_record_list *now = *list;

	while (now) {
		oscar_tlv_record_list *next = now->next;

		disposeChain(&now->item);
		SAFE_FREE((void**)&now);
		now = next;
	}

	*list = nullptr;
}

oscar_tlv_chain* oscar_tlv_record_list::getRecordByTLV(WORD wType, int nValue)
{
	oscar_tlv_record_list *list = this;
	while (list) {
		if (list->item && list->item->getTLV(wType, 1) && list->item->getNumber(wType, 1) == nValue)
			return list->item;
		list = list->next;
	}

	return nullptr;
}
