/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

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

MIR_CORE_DLL(char *) newStr(const char *src)
{
	if (!src)
		return nullptr;

	return strcpy(new char[strlen(src) + 1], src);
}

MIR_CORE_DLL(wchar_t *) newStrW(const wchar_t *src)
{
	if (!src)
		return nullptr;

	return wcscpy(new wchar_t[wcslen(src) + 1], src);
}

MIR_CORE_DLL(char*) replaceStr(char* &dest, const char *src)
{
	if (dest != nullptr)
		mir_free(dest);

	return dest = (src != nullptr) ? mir_strdup(src) : nullptr;
}

MIR_CORE_DLL(wchar_t*) replaceStrW(wchar_t* &dest, const wchar_t *src)
{
	if (dest != nullptr)
		mir_free(dest);

	return dest = (src != nullptr) ? mir_wstrdup(src) : nullptr;
}

MIR_CORE_DLL(char*) rtrim(char *str)
{
	if (str == nullptr)
		return nullptr;

	char* p = strchr(str, 0);
	while (--p >= str) {
		switch (*p) {
		case ' ': case '\t': case '\n': case '\r':
			*p = 0; break;
		default:
			return str;
		}
	}
	return str;
}

MIR_CORE_DLL(wchar_t*) rtrimw(wchar_t *str)
{
	if (str == nullptr)
		return nullptr;

	wchar_t *p = wcschr(str, 0);
	while (--p >= str) {
		switch (*p) {
		case ' ': case '\t': case '\n': case '\r':
			*p = 0; break;
		default:
			return str;
		}
	}
	return str;
}

MIR_CORE_DLL(char*) ltrim(char *str)
{
	if (str == nullptr)
		return nullptr;

	char* p = str;
	for (;;) {
		switch (*p) {
		case ' ': case '\t': case '\n': case '\r':
			++p; break;
		default:
			memmove(str, p, strlen(p) + 1);
			return str;
		}
	}
}

MIR_CORE_DLL(wchar_t*) ltrimw(wchar_t *str)
{
	if (str == nullptr)
		return nullptr;

	wchar_t *p = str;
	for (;;) {
		switch (*p) {
		case ' ': case '\t': case '\n': case '\r':
			++p; break;
		default:
			memmove(str, p, sizeof(wchar_t)*(wcslen(p) + 1));
			return str;
		}
	}
}

MIR_CORE_DLL(char*) ltrimp(char *str)
{
	if (str == nullptr)
		return nullptr;

	char *p = str;
	for (;;) {
		switch (*p) {
		case ' ': case '\t': case '\n': case '\r':
			++p; break;
		default:
			return p;
		}
	}
}

MIR_CORE_DLL(wchar_t*) ltrimpw(wchar_t *str)
{
	if (str == nullptr)
		return nullptr;

	wchar_t *p = str;
	for (;;) {
		switch (*p) {
		case ' ': case '\t': case '\n': case '\r':
			++p; break;
		default:
			return p;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

MIR_CORE_DLL(char*) strdel(char *str, size_t len)
{
	char* p;
	for (p = str + len; *p != 0; p++)
		*(p - len) = *p;

	*(p - len) = '\0';
	return str;
}

MIR_CORE_DLL(wchar_t*) strdelw(wchar_t *str, size_t len)
{
	wchar_t* p;
	for (p = str + len; *p != 0; p++)
		*(p - len) = *p;

	*(p - len) = '\0';
	return str;
}

/////////////////////////////////////////////////////////////////////////////////////////

MIR_CORE_DLL(int) wildcmp(const char *name, const char *mask)
{
	if (name == nullptr || mask == nullptr)
		return false;

	const char *last = nullptr;
	for (;; mask++, name++) {
		if (*mask != '?' && *mask != *name) break;
		if (*name == '\0') return ((BOOL)!*mask);
	}
	if (*mask != '*') return FALSE;
	for (;; mask++, name++) {
		while (*mask == '*') {
			last = mask++;
			if (*mask == '\0') return ((BOOL)!*mask);   /* true */
		}
		if (*name == '\0') return ((BOOL)!*mask);      /* *mask == EOS */
		if (*mask != '?' && *mask != *name) name -= (size_t)(mask - last) - 1, mask = last;
	}
}

MIR_CORE_DLL(int) wildcmpw(const wchar_t *name, const wchar_t *mask)
{
	if (name == nullptr || mask == nullptr)
		return false;

	const wchar_t* last = nullptr;
	for (;; mask++, name++) {
		if (*mask != '?' && *mask != *name) break;
		if (*name == '\0') return ((BOOL)!*mask);
	}
	if (*mask != '*') return FALSE;
	for (;; mask++, name++) {
		while (*mask == '*') {
			last = mask++;
			if (*mask == '\0') return ((BOOL)!*mask);   /* true */
		}
		if (*name == '\0') return ((BOOL)!*mask);      /* *mask == EOS */
		if (*mask != '?' && *mask != *name) name -= (size_t)(mask - last) - 1, mask = last;
	}
}

#define _qtoupper(_c) (((_c) >= 'a' && (_c) <= 'z')?((_c)-'a'+'A'):(_c))

MIR_CORE_DLL(int) wildcmpi(const char *name, const char *mask)
{
	if (name == nullptr || mask == nullptr)
		return false;

	const char *last = nullptr;
	for (;; mask++, name++) {
		if (*mask != '?' && _qtoupper(*mask) != _qtoupper(*name)) break;
		if (*name == '\0') return ((BOOL)!*mask);
	}
	if (*mask != '*') return FALSE;
	for (;; mask++, name++) {
		while (*mask == '*') {
			last = mask++;
			if (*mask == '\0') return ((BOOL)!*mask);   /* true */
		}
		if (*name == '\0') return ((BOOL)!*mask);      /* *mask == EOS */
		if (*mask != '?' && _qtoupper(*mask) != _qtoupper(*name)) name -= (size_t)(mask - last) - 1, mask = last;
	}
}

MIR_CORE_DLL(int) wildcmpiw(const wchar_t *name, const wchar_t *mask)
{
	if (name == nullptr || mask == nullptr)
		return false;

	const wchar_t* last = nullptr;
	for (;; mask++, name++) {
		if (*mask != '?' && _qtoupper(*mask) != _qtoupper(*name)) break;
		if (*name == '\0') return ((BOOL)!*mask);
	}
	if (*mask != '*') return FALSE;
	for (;; mask++, name++) {
		while (*mask == '*') {
			last = mask++;
			if (*mask == '\0') return ((BOOL)!*mask);   /* true */
		}
		if (*name == '\0') return ((BOOL)!*mask);      /* *mask == EOS */
		if (*mask != '?' && _qtoupper(*mask) != _qtoupper(*name)) name -= (size_t)(mask - last) - 1, mask = last;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

static char szHexTable[] = "0123456789abcdef";

MIR_CORE_DLL(char*) bin2hex(const void *pData, size_t len, char *dest)
{
	const uint8_t *p = (const uint8_t*)pData;
	char *d = dest;

	for (size_t i = 0; i < len; i++, p++) {
		*d++ = szHexTable[*p >> 4];
		*d++ = szHexTable[*p & 0x0F];
	}
	*d = 0;

	return dest;
}

MIR_CORE_DLL(wchar_t*) bin2hexW(const void *pData, size_t len, wchar_t *dest)
{
	const uint8_t *p = (const uint8_t*)pData;
	wchar_t *d = dest;

	for (size_t i = 0; i < len; i++, p++) {
		*d++ = szHexTable[*p >> 4];
		*d++ = szHexTable[*p & 0x0F];
	}
	*d = 0;

	return dest;
}

static int hex2dec(int iHex)
{
	if (iHex >= '0' && iHex <= '9')
		return iHex - '0';
	if (iHex >= 'a' && iHex <= 'f')
		return iHex - 'a' + 10;
	if (iHex >= 'A' && iHex <= 'F')
		return iHex - 'A' + 10;
	return 0;
}

MIR_CORE_DLL(bool) hex2bin(const char *pSrc, void *pData, size_t len)
{
	if (pSrc == nullptr || pData == nullptr || len == 0)
		return false;

	size_t bufLen = strlen(pSrc)/2;
	if (pSrc[bufLen*2] != 0 || bufLen > len)
		return false;

	uint8_t *pDest = (uint8_t*)pData;
	const char *p = (const char *)pSrc;
	for (size_t i = 0; i < bufLen; i++, p += 2)
		pDest[i] = hex2dec(p[0]) * 16 + hex2dec(p[1]);

	if (bufLen < len)
		memset(pDest + bufLen, 0, len - bufLen);
	return true;
}

MIR_CORE_DLL(bool) hex2binW(const wchar_t *pSrc, void *pData, size_t len)
{
	if (pSrc == nullptr || pData == nullptr || len == 0)
		return false;

	size_t bufLen = wcslen(pSrc)/2;
	if (pSrc[bufLen * 2] != 0 || bufLen > len)
		return false;

	uint8_t *pDest = (uint8_t*)pData;
	const wchar_t *p = (const wchar_t *)pSrc;
	for (size_t i = 0; i < bufLen; i++, p += 2)
		pDest[i] = hex2dec(p[0]) * 16 + hex2dec(p[1]);

	if (bufLen < len)
		memset(pDest+bufLen, 0, len - bufLen);
	return true;
}


/////////////////////////////////////////////////////////////////////////////////////////

#pragma intrinsic(strlen, strcpy, strcat, strcmp, wcslen, wcscpy, wcscat, wcscmp)

MIR_CORE_DLL(size_t) mir_strlen(const char *p)
{
	return (p) ? strlen(p) : 0;
}

MIR_CORE_DLL(size_t) mir_wstrlen(const wchar_t *p)
{
	return (p) ? wcslen(p) : 0;
}

MIR_CORE_DLL(char*) mir_strcpy(char *dest, const char *src)
{
	if (dest == nullptr)
		return nullptr;

	if (src == nullptr) {
		*dest = 0;
		return dest;
	}

	return strcpy(dest, src);
}

MIR_CORE_DLL(wchar_t*) mir_wstrcpy(wchar_t *dest, const wchar_t *src)
{
	if (dest == nullptr)
		return nullptr;

	if (src == nullptr) {
		*dest = 0;
		return dest;
	}

	return wcscpy(dest, src);
}

MIR_CORE_DLL(char*) mir_strncpy(char *dest, const char *src, size_t len)
{
	if (dest == nullptr)
		return nullptr;

	if (src == nullptr)
		*dest = 0;
	else
		strncpy_s(dest, len, src, _TRUNCATE);
	return dest;
}

MIR_CORE_DLL(wchar_t*) mir_wstrncpy(wchar_t *dest, const wchar_t *src, size_t len)
{
	if (dest == nullptr)
		return nullptr;

	if (src == nullptr)
		*dest = 0;
	else
		wcsncpy_s(dest, len, src, _TRUNCATE);
	return dest;
}

MIR_CORE_DLL(char*) mir_strcat(char *dest, const char *src)
{
	if (dest == nullptr)
		return nullptr;

	if (src == nullptr) {
		*dest = 0;
		return dest;
	}

	return strcat(dest, src);
}

MIR_CORE_DLL(wchar_t*) mir_wstrcat(wchar_t *dest, const wchar_t *src)
{
	if (dest == nullptr)
		return nullptr;

	if (src == nullptr) {
		*dest = 0;
		return dest;
	}

	return wcscat(dest, src);
}

MIR_CORE_DLL(char*) mir_strncat(char *dest, const char *src, size_t len)
{
	if (dest == nullptr)
		return nullptr;

	if (src == nullptr)
		*dest = 0;
	else
		strncat_s(dest, len, src, _TRUNCATE);
	return dest;
}

MIR_CORE_DLL(wchar_t*) mir_wstrncat(wchar_t *dest, const wchar_t *src, size_t len)
{
	if (dest == nullptr)
		return nullptr;

	if (src == nullptr)
		*dest = 0;
	else
		wcsncat_s(dest, len, src, _TRUNCATE);
	return dest;
}

MIR_CORE_DLL(int) mir_strcmp(const char *p1, const char *p2)
{
	if (p1 == nullptr)
		return (p2 == nullptr) ? 0 : -1;
	if (p2 == nullptr)
		return 1;
	return strcmp(p1, p2);
}

MIR_CORE_DLL(int) mir_wstrcmp(const wchar_t *p1, const wchar_t *p2)
{
	if (p1 == nullptr)
		return (p2 == nullptr) ? 0 : -1;
	if (p2 == nullptr)
		return 1;
	return wcscmp(p1, p2);
}

MIR_CORE_DLL(int) mir_strcmpi(const char *p1, const char *p2)
{
	if (p1 == nullptr)
		return (p2 == nullptr) ? 0 : -1;
	if (p2 == nullptr)
		return 1;
	return stricmp(p1, p2);
}

MIR_CORE_DLL(int) mir_wstrcmpi(const wchar_t *p1, const wchar_t *p2)
{
	if (p1 == nullptr)
		return (p2 == nullptr) ? 0 : -1;
	if (p2 == nullptr)
		return 1;
	return wcsicmp(p1, p2);
}

MIR_CORE_DLL(int) mir_strncmp(const char *p1, const char *p2, size_t n)
{
	if (p1 == nullptr)
		return (p2 == nullptr) ? 0 : -1;
	if (p2 == nullptr)
		return 1;
	return strncmp(p1, p2, n);
}

MIR_CORE_DLL(int) mir_wstrncmp(const wchar_t *p1, const wchar_t *p2, size_t n)
{
	if (p1 == nullptr)
		return (p2 == nullptr) ? 0 : -1;
	if (p2 == nullptr)
		return 1;
	return wcsncmp(p1, p2, n);
}

MIR_CORE_DLL(int) mir_strncmpi(const char *p1, const char *p2, size_t n)
{
	if (p1 == nullptr)
		return (p2 == nullptr) ? 0 : -1;
	if (p2 == nullptr)
		return 1;
	return strnicmp(p1, p2, n);
}

MIR_CORE_DLL(int) mir_wstrncmpi(const wchar_t *p1, const wchar_t *p2, size_t n)
{
	if (p1 == nullptr)
		return (p2 == nullptr) ? 0 : -1;
	if (p2 == nullptr)
		return 1;
	return wcsnicmp(p1, p2, n);
}

#ifdef _MSC_VER
MIR_CORE_DLL(const wchar_t*) mir_wstrstri(const wchar_t *s1, const wchar_t *s2)
{
	for (int i = 0; s1[i]; i++)
		for (int j = i, k = 0; towlower(s1[j]) == towlower(s2[k]); j++, k++)
			if (!s2[k + 1])
				return s1 + i;

	return nullptr;
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
	PGENRANDOM pfnRtlGenRandom;
#endif

MIR_CORE_DLL(void) Utils_GetRandom(void *pszDest, size_t cbLen)
{
	if (pszDest == nullptr || cbLen == 0)
		return;

	#ifdef _MSC_VER
		if (pfnRtlGenRandom != nullptr) {
			pfnRtlGenRandom(pszDest, (uint32_t)cbLen);
			return;
		}
	#endif
		
	srand(time(0));
	uint8_t *p = (uint8_t*)pszDest;
	for (size_t i = 0; i < cbLen; i++)
		p[i] = rand() & 0xFF;
}

MIR_CORE_DLL(bool) Utils_IsRtl(const wchar_t *pszwText)
{
   #ifdef _MSC_VER
      size_t iLen = mir_wstrlen(pszwText);
      mir_ptr<uint16_t> infoTypeC2((uint16_t*)mir_calloc(sizeof(uint16_t) * (iLen + 2)));
      GetStringTypeW(CT_CTYPE2, pszwText, (int)iLen, infoTypeC2);

      for (size_t i = 0; i < iLen; i++)
         if (infoTypeC2[i] == C2_RIGHTTOLEFT)
            return true;
   #endif

	return false;
}

MIR_CORE_DLL(time_t) Utils_IsoToUnixTime(const char *stamp)
{
	wchar_t date[9];
	int i, y;

	if (stamp == nullptr)
		return 0;

	auto *p = stamp;

	// Get the date part
	for (i = 0; *p != '\0' && i < 8 && isdigit(*p); p++, i++)
		date[i] = *p;

	// Parse year
	if (i == 6) {
		// 2-digit year (1970-2069)
		y = (date[0] - '0') * 10 + (date[1] - '0');
		if (y < 70) y += 100;
	}
	else if (i == 8) {
		// 4-digit year
		y = (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 + date[3] - '0';
		y -= 1900;
	}
	else return 0;

	struct tm timestamp;
	timestamp.tm_year = y;

	// Parse month
	timestamp.tm_mon = (date[i - 4] - '0') * 10 + date[i - 3] - '0' - 1;

	// Parse date
	timestamp.tm_mday = (date[i - 2] - '0') * 10 + date[i - 1] - '0';

	// Skip any date/time delimiter
	for (; *p != '\0' && !isdigit(*p); p++);

	// Parse time
	if (sscanf(p, "%d:%d:%d", &timestamp.tm_hour, &timestamp.tm_min, &timestamp.tm_sec) != 3)
		return (time_t)0;

	timestamp.tm_isdst = 0;	// DST is already present in _timezone below
	time_t t = mktime(&timestamp);

	_tzset();
	t -= _timezone;
	return (t >= 0) ? t : 0;
}
