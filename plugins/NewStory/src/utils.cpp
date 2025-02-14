/*
Copyright (c) 2005 Victor Pavlychko (nullbyte@sotline.net.ua)
Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2
of the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

#include <msapi/comptr.h>

Bitmap* LoadImageFromResource(HINSTANCE hInst, int resourceId, const wchar_t *pwszType)
{
	if (HRSRC hrsrc = FindResourceW(hInst, MAKEINTRESOURCE(resourceId), pwszType)) {
		if (DWORD dwSize = SizeofResource(hInst, hrsrc)) {
			if (HGLOBAL hRes = LoadResource(hInst, hrsrc)) {
				void *pImage = LockResource(hRes);

				if (HGLOBAL hGlobal = ::GlobalAlloc(GHND, dwSize)) {
					void *pBuffer = ::GlobalLock(hGlobal);
					if (pBuffer) {
						memcpy(pBuffer, pImage, dwSize);

						CComPtr<IStream> pStream;
						HRESULT hr = CreateStreamOnHGlobal(hGlobal, TRUE, &pStream);
						if (SUCCEEDED(hr))
							return new Gdiplus::Bitmap(pStream);
					}

					GlobalFree(hGlobal); // free memory only if the function fails
				}
			}
		}
	}

	return nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////

static void SmartSendEventWorker(MWindowList wndList, int iEventType, MCONTACT cc1, MCONTACT cc2, MEVENT hEvent)
{
	if (HWND hwnd = WindowList_Find(wndList, cc1))
		PostMessage(hwnd, iEventType, cc1, hEvent);

	if (cc2 != INVALID_CONTACT_ID)
		if (HWND hwnd = WindowList_Find(wndList, cc2))
			PostMessage(hwnd, iEventType, cc2, hEvent);
}

int SmartSendEvent(int iEventType, MCONTACT cc1, MEVENT hEvent)
{
	MCONTACT cc2 = INVALID_CONTACT_ID;

	// Send a message to a real contact too
	if (db_mc_isMeta(cc1)) {
		MCONTACT cc = db_event_getContact(hEvent);
		if (cc != cc1)
			cc2 = cc;
	}

	SmartSendEventWorker(g_hNewstoryLogs, iEventType, cc1, cc2, hEvent);
	SmartSendEventWorker(g_hNewstoryHistLogs, iEventType, cc1, cc2, hEvent);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

uint32_t toggleBit(uint32_t dw, uint32_t bit)
{
	if (dw & bit)
		return dw & ~bit;
	return dw | bit;
}

bool CheckFilter(wchar_t *buf, wchar_t *filter)
{
	//	MessageBox(0, buf, filter, MB_OK);
	int l1 = (int)mir_wstrlen(buf);
	int l2 = (int)mir_wstrlen(filter);
	for (int i = 0; i < l1 - l2 + 1; i++)
		if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, buf + i, l2, filter, l2) == CSTR_EQUAL)
			return true;
	return false;
}

int GetFontHeight(const LOGFONTA &lf)
{
	return 2 * abs(lf.lfHeight) * 74 / g_iPixelY;
}

/////////////////////////////////////////////////////////////////////////////////////////

struct
{
	wchar_t *pStart, *pEnd;
	size_t cbStart, cbEnd;
}
static bbcodes[] = 
{
	{ L"[b]",      nullptr },
	{ L"[/b]",     nullptr },
	{ L"[i]",      nullptr },
	{ L"[/i]",     nullptr },
	{ L"[u]",      nullptr },
	{ L"[/u]",     nullptr },
	{ L"[s]",      nullptr },
	{ L"[/s]",     nullptr },

	{ L"[color=", L"]"     },
	{ L"[/color]", nullptr },

	{ L"[c0]",     nullptr },
	{ L"[c1]",     nullptr },
	{ L"[c2]",     nullptr },
	{ L"[c3]",     nullptr },
	{ L"[c4]",     nullptr },
	{ L"[c5]",     nullptr },
	{ L"[c6]",     nullptr },

	{ L"[$hicon=", L"$]"   },

	{ L"[url]", L"[/url]"  },
	{ L"[url=", L"]",      },
	{ L"[img]", L"[/img]"  },
	{ L"[img=", L"]"       },
};

void RemoveBbcodes(CMStringW &wszText)
{
	if (wszText.IsEmpty())
		return;

	if (bbcodes[0].cbStart == 0)
		for (auto &it : bbcodes) {
			it.cbStart = wcslen(it.pStart);
			if (it.pEnd)
				it.cbEnd = wcslen(it.pEnd);
		}

	for (int idx = wszText.Find('[', 0); idx != -1; idx = wszText.Find('[', idx)) {
		bool bFound = false;
		for (auto &it : bbcodes) {
			if (wcsncmp(wszText.c_str() + idx, it.pStart, it.cbStart))
				continue;

			wszText.Delete(idx, (int)it.cbStart);

			if (it.pEnd) {
				int idx2 = wszText.Find(it.pEnd, idx);
				if (idx2 != -1) {
					wszText.Delete(idx, idx2 - idx);
					wszText.Delete(idx, (int)it.cbEnd);
				}
			}

			bFound = true;
			break;
		}

		// just an occasional square bracket? skip it
		if (!bFound)
			idx++;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

static int countNoWhitespace(const wchar_t *str)
{
	int c;
	for (c = 0; *str != '\n' && *str != '\r' && *str != '\t' && *str != ' ' && *str != '\0'; str++, c++);
	return c;
}

static int DetectUrl(const wchar_t *text)
{
	int i;
	for (i = 0; text[i] != '\0'; i++)
		if (!((text[i] >= '0' && text[i] <= '9') || iswalpha(text[i])))
			break;

	if (i <= 0 || wcsncmp(text + i, L"://", 3))
		return 0;

	i += countNoWhitespace(text + i);
	for (; i > 0; i--)
		if ((text[i - 1] >= '0' && text[i - 1] <= '9') || iswalpha(text[i - 1]) || text[i - 1] == '/')
			break;

	return i;
}

void UrlAutodetect(CMStringW &str)
{
	if (str.IsEmpty())
		return;

	int level = 0;

	for (auto *p = str.c_str(); *p; p++) {
		if (!wcsncmp(p, L"[img=", 5) || !wcsncmp(p, L"[img]", 5) || !wcsncmp(p, L"[url]", 5) || !wcsncmp(p, L"[url=", 5)) {
			p += 4;
			level++;
		}
		if (!wcsncmp(p, L"[/img]", 6) || !wcsncmp(p, L"[/url]", 6)) {
			p += 5;
			level--;
		}
		else if (int len = DetectUrl(p)) {
			if (level == 0) {
				int pos = p - str.c_str();
				CMStringW url = str.Mid(pos, len);
				str.Insert(pos + len, L"[/url]");
				str.Insert(pos, L"[url]");
				p = str.c_str() + pos + len + 11;
			}
		}
	}
}
