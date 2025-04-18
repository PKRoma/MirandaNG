/////////////////////////////////////////////////////////////////////////////////////////
// Miranda NG: the free IM client for Microsoft* Windows*
//
// Copyright (C) 2012-25 Miranda NG team,
// Copyright (c) 2000-09 Miranda ICQ/IM project,
// all portions of this codebase are copyrighted to the people
// listed in contributors.txt.
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
// you should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// part of tabSRMM messaging plugin for Miranda.
//
// (C) 2005-2010 by silvercircle _at_ gmail _dot_ com and contributors
//
// wraps some parts of Miranda API
// Also, OS dependent stuff (visual styles api etc.)

#ifndef __MIM_H
#define __MIM_H

/*
 * Win32 API definitions of functions dynamically obtained via GetProcAddress()
 * - uxtheme
 * - dwmapi
 * - some GDI functions (AlphaBlend()..)
 */
//dwmapi
typedef HRESULT(WINAPI *DEFICA)(HWND hwnd, const MARGINS *margins);
typedef HRESULT(WINAPI *DICE)(BOOL *);
typedef HRESULT(WINAPI *DRT)(HWND, HWND, PHTHUMBNAIL);
typedef HRESULT(WINAPI *BBW)(HWND, DWM_BLURBEHIND *);
typedef HRESULT(WINAPI *DGC)(uint32_t *, BOOL *);
typedef HRESULT(WINAPI *DWMIIB)(HWND);
typedef HRESULT(WINAPI *DWMSWA)(HWND, uint32_t, LPCVOID, uint32_t);
typedef HRESULT(WINAPI *DWMUT)(HTHUMBNAIL, DWM_THUMBNAIL_PROPERTIES *);
typedef HRESULT(WINAPI *DURT)(HTHUMBNAIL);
typedef HRESULT(WINAPI *DSIT)(HWND, HBITMAP, uint32_t);
typedef HRESULT(WINAPI *DSILP)(HWND, HBITMAP, POINT *, uint32_t);
//uxtheme
typedef UINT(WINAPI *PDTTE)(HANDLE, HDC, int, int, LPCWSTR, int, uint32_t, RECT *, const DTTOPTS *);
typedef HANDLE(WINAPI *BBP)(HDC, RECT *, BP_BUFFERFORMAT, BP_PAINTPARAMS *, HDC *);
typedef HRESULT(WINAPI *EBP)(HANDLE, BOOL);
typedef HRESULT(WINAPI *BPI)(void);
typedef HRESULT(WINAPI *BPU)(void);
typedef HRESULT(WINAPI *BPSA)(HANDLE, const RECT *, uint8_t);
//kernel32
typedef int		(WINAPI *GLIX)(LPCWSTR, LCTYPE, LPCWSTR, int);

/*
 * used to encapsulate some parts of the Miranda API
 * constructor does early time initialization - do NOT put anything
 * here, except thngs that deal with the core and database API.
 *
 * it is UNSAFE to assume that any plugin provided services are available
 * when the object is instantiated.
 */

class CMimAPI
{
public:
	CMimAPI()
	{
		InitPaths();
		InitAPI();
		getAeroState();

		::QueryPerformanceFrequency((LARGE_INTEGER *)&m_tFreq);
		m_dFreq = (double)(1.0f / m_tFreq);
		m_hChatLogLock = INVALID_HANDLE_VALUE;
	}

	~CMimAPI() {
		if (m_haveBufferedPaint)
			m_pfnBufferedPaintUninit();
		if (m_hUxTheme != nullptr)
			FreeLibrary(m_hUxTheme);
		if (m_hDwmApi != nullptr)
			FreeLibrary(m_hDwmApi);

		if (m_hChatLogLock != INVALID_HANDLE_VALUE)
			CloseHandle(m_hChatLogLock);
	}

	/*
	 * database functions
	 */

	static uint32_t __forceinline GetDword(const char *szSetting, uint32_t uDefault)
	{	return db_get_dw(0, SRMSGMOD_T, szSetting, uDefault);
	}
	static uint32_t __forceinline GetDword(const MCONTACT hContact, const char *szSetting, uint32_t uDefault)
	{	return db_get_dw(hContact, SRMSGMOD_T, szSetting, uDefault);
	}

	static int __forceinline GetByte(const char *szSetting, int uDefault)
	{	return db_get_b(0, SRMSGMOD_T, szSetting, uDefault);
	}
	static int __forceinline GetByte(const MCONTACT hContact, const char *szSetting, int uDefault)
	{	return db_get_b(hContact, SRMSGMOD_T, szSetting, uDefault);
	}

	static bool __forceinline GetBool(const char *szSetting, bool bDefault)
	{	return db_get_b(0, SRMSGMOD_T, szSetting, bDefault) != 0;
	}
	static bool __forceinline GetBool(const char *szModule, const char *szSetting, bool bDefault)
	{	return db_get_b(0, szModule, szSetting, bDefault) != 0;
	}
	static bool __forceinline GetBool(const MCONTACT hContact, const char *szSetting, bool bDefault)
	{	return db_get_b(hContact, SRMSGMOD_T, szSetting, bDefault) != 0;
	}

	static bool __forceinline IsFavorite(const MCONTACT hContact)
	{	return db_get_b(hContact, SRMSGMOD_T, "isFavorite", false);
	}
	static void __forceinline SetFavorite(const MCONTACT hContact, bool bValue)
	{	db_set_b(hContact, SRMSGMOD_T, "isFavorite", bValue);
	}

	static uint32_t __forceinline IsRecent(const MCONTACT hContact)
	{	return db_get_dw(hContact, SRMSGMOD_T, "isRecent", 0);
	}
	static void __forceinline SetRecent(const MCONTACT hContact, uint32_t dwValue)
	{	db_set_dw(hContact, SRMSGMOD_T, "isRecent", dwValue);
	}

	/*
	 * path utilities
	 */

	__forceinline LPCTSTR getDataPath() const { return(m_szProfilePath); }
	__forceinline LPCTSTR getSkinPath() const { return(m_szSkinsPath); }
	__forceinline LPCTSTR getSavedAvatarPath() const { return(m_szSavedAvatarsPath); }
	__forceinline LPCTSTR getChatLogPath() const { return(m_szChatLogsPath); }

	LPCTSTR getUserDir();
	void    configureCustomFolders();
	INT_PTR foldersPathChanged();

	__forceinline __int64 getTimerStart() const { return m_tStart; }
	__forceinline __int64 getTimerStop() const { return m_tStop; }
	__forceinline __int64 getTicks() const { return m_tStop - m_tStart; }
	__forceinline double  getFreq() const { return m_dFreq; }
	__forceinline double  getMsec() const { return 1000 * ((double)(m_tStop - m_tStart) * m_dFreq); }

	//////////////////////////////////////////////////////////////////////////////////////
	// os dependent stuff (aero, visual styles etc.)

	// returns status of Windows Aero
	__forceinline bool isAero() const { return m_isAero; }

	__forceinline bool isDwmActive() const { return m_DwmActive; }

	// refresh Aero status.
	bool getAeroState();

	// returns status of visual styles theming engine (Windows XP+)
	__forceinline bool isVSThemed() { return m_isVsThemed; }

	// window lists
	static int FoldersPathChanged(WPARAM wParam, LPARAM lParam);  // hook subscriber for folders plugin
	static int TypingMessage(WPARAM wParam, LPARAM lParam);
	static int ProtoAck(WPARAM wParam, LPARAM lParam);
	static int PrebuildContactMenu(WPARAM wParam, LPARAM lParam);
	static int MessageEventAdded(WPARAM wParam, LPARAM lParam);

public:
	// various function pointers
	static PDTTE  m_pfnDrawThemeTextEx;
	static DEFICA m_pfnDwmExtendFrameIntoClientArea;
	static DICE   m_pfnDwmIsCompositionEnabled;
	static DRT    m_pfnDwmRegisterThumbnail;
	static BPI    m_pfnBufferedPaintInit;
	static BPU    m_pfnBufferedPaintUninit;
	static BBP    m_pfnBeginBufferedPaint;
	static EBP    m_pfnEndBufferedPaint;
	static BBW    m_pfnDwmBlurBehindWindow;
	static DGC    m_pfnDwmGetColorizationColor;
	static BPSA   m_pfnBufferedPaintSetAlpha;
	static DWMSWA m_pfnDwmSetWindowAttribute;
	static DWMIIB m_pfnDwmInvalidateIconicBitmaps;
	static DWMUT  m_pfnDwmUpdateThumbnailProperties;
	static DURT   m_pfnDwmUnregisterThumbnail;
	static DSIT   m_pfnDwmSetIconicThumbnail;
	static DSILP  m_pfnDwmSetIconicLivePreviewBitmap;
	static bool   m_shutDown, m_haveBufferedPaint;

private:
	wchar_t   m_szProfilePath[MAX_PATH + 2], m_szSkinsPath[MAX_PATH + 2], m_szSavedAvatarsPath[MAX_PATH + 2], m_szChatLogsPath[MAX_PATH + 2];
	HMODULE m_hUxTheme, m_hDwmApi;
	bool    m_isAero, m_DwmActive, m_isVsThemed;
	HANDLE  m_hDataPath, m_hSkinsPath, m_hAvatarsPath, m_hChatLogsPath;
	__int64 m_tStart, m_tStop, m_tFreq;
	double  m_dFreq;
	char    m_timerMsg[256];
	bool    m_hasFolders;
	HANDLE  m_hChatLogLock;

	void	InitAPI();
	void 	InitPaths();

private:
	static wchar_t m_userDir[MAX_PATH + 2];
};

extern  CMimAPI M;

#endif /* __MIM_H */
