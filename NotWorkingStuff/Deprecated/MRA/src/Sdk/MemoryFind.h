/*
 * Copyright (c) 2003 Rozhuk Ivan <rozhuk.im@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


#if !defined(AFX_MEMORYFIND__H__INCLUDED_)
#define AFX_MEMORYFIND__H__INCLUDED_


#pragma once

__inline void *MemoryFind(size_t dwFrom,const void *pBuff,size_t dwBuffSize,const void *pWhatFind,size_t dwWhatFindSize)
{
	void *pRet = nullptr;

	if (pBuff && dwBuffSize && pWhatFind && dwWhatFindSize && (dwFrom+dwWhatFindSize)<=dwBuffSize)
	{
		if (dwWhatFindSize==1)
		{// MemoryFindByte
			pRet=(void*)memchr((const void*)(((size_t)pBuff)+dwFrom),(*((unsigned char*)pWhatFind)),(dwBuffSize-dwFrom));
		}else{
			void *pCurPos;

			pCurPos=(void*)(((size_t)pBuff)+dwFrom);

			if ((dwFrom+dwWhatFindSize)==dwBuffSize)
			{
				if (memcmp(pCurPos,pWhatFind,dwWhatFindSize)==0) pRet=pCurPos;
			}else{
				dwBuffSize-=(dwWhatFindSize-1);

				while(pCurPos)
				{
					pCurPos=memchr(pCurPos,(*((unsigned char*)pWhatFind)),(dwBuffSize-(((size_t)pCurPos)-((size_t)pBuff))));
					if (pCurPos)
					{
						if (memcmp(pCurPos,pWhatFind,dwWhatFindSize)==0)
						{
							pRet=pCurPos;
							break;
						}else{
							pCurPos=(void*)(((size_t)pCurPos)+1);
						}
					}
				}
			}
		}
	}
return(pRet);
}


#endif // !defined(AFX_MEMORYFIND__H__INCLUDED_)
