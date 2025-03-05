/*

Chat module interface for Miranda NG

Copyright (c) 2014-25 George Hazan

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

#pragma once

#ifndef M_CHAT_INT_H__
#define M_CHAT_INT_H__ 1

#pragma warning(disable:4512 4275)

#include <time.h>

#ifndef M_CORE_H__
#include <m_core.h>
#endif

#include <m_string.h>
#include <m_chat.h>
#include <m_gui.h>
#include <m_utils.h>

#define OPTIONS_FONTCOUNT 20
#define STATUSICONCOUNT 6

#define CHAT_MODULE "Chat"
#define CHATFONT_MODULE "ChatFonts"

#define GCW_TABROOM            10
#define GCW_TABPRIVMSG         11

#define GC_EVENT_HIGHLIGHT     0x1000
#define STATE_TALK             0x0001

#define ICON_ACTION            0
#define ICON_ADDSTATUS         1
#define ICON_HIGHLIGHT         2
#define ICON_INFO              3
#define ICON_JOIN              4
#define ICON_KICK              5
#define ICON_MESSAGE           6
#define ICON_MESSAGEOUT        7
#define ICON_NICK              8
#define ICON_NOTICE            9
#define ICON_PART             10
#define ICON_QUIT             11
#define ICON_REMSTATUS        12
#define ICON_TOPIC            13

#define CHATMODE_NORMAL        0
#define CHATMODE_MUTE          1
#define CHATMODE_UNMUTE        2

//structs

struct SESSION_INFO;
struct MODULEINFO;

class CMsgDialog;

struct USERINFO : public MZeroedObject, public MNonCopyable
{
	~USERINFO() {
		iSignature = 0;
	}

	wchar_t* pszUID;
	wchar_t* pszNick;
	uint32_t iSignature = 0xDEADBEEF;
	uint16_t Status;
	uint16_t ContactStatus;
	int      iStatusEx;

	bool isValid() const {
		return iSignature == 0xDEADBEEF;
	}
};

struct MIR_APP_EXPORT GCModuleInfoBase : public MZeroedObject, public MNonCopyable
{
	GCModuleInfoBase();
	~GCModuleInfoBase();

	char*    pszModule;
	wchar_t* ptszModDispName;
	
	bool     bChanMgr, bAckMsg, bDatabase, bPersistent;
	
	int      iMaxText;
};

struct FONTINFO
{
	LOGFONT  lf;
	COLORREF color;
};

struct MIR_APP_EXPORT LOGINFO : public MZeroedObject, public MNonCopyable
{
	LOGINFO(const GCEVENT*);
	~LOGINFO();

	int getIcon() const;
	int getIndex() const;

	void write(struct RtfChatLogStreamData *streamData, bool simpleMode, CMStringA &str, const wchar_t *line) const;

	ptrW     ptszText;
	ptrW     ptszNick;
	ptrW     ptszUID;
	ptrW     ptszStatus;
	ptrW     ptszUserInfo;
	bool     bIsMe;
	bool     bIsHighlighted;
	bool     bSimple;
	time_t   time;
	MEVENT   hEvent;
	int      iType;
};

struct STATUSINFO
{
	wchar_t    *pszGroup;
	int         iIconIndex;
	int         iStatus;
	STATUSINFO *next;
};

struct MIR_APP_EXPORT SESSION_INFO : public MZeroedObject, public MNonCopyable
{
	SESSION_INFO();
	~SESSION_INFO();

	MCONTACT    hContact;

	bool        bInitDone;
	bool        bHasToolTip;
	bool        bHasNicklist;
	bool        bIsDirty;

	char*       pszModule;
	wchar_t*    ptszID;
	wchar_t*    ptszName;
	wchar_t*    ptszStatusbarText;
	wchar_t*    ptszTopic;

	int         iType;
	int         iStatusCount, iLastEvent;
	int         iTrayFlags, iPopupFlags;
	int         currentHovered;

	uint16_t    wStatus;
	uint16_t    wState;
	uint16_t    wCommandsNum;
	void*       pItemData;
	time_t      LastTime;
	mir_cs      csLock;

	CMsgDialog *pDlg;
	USERINFO   *pMe;
	STATUSINFO *pStatuses;
	MODULEINFO *pMI;
	SESSION_INFO *pParent;

	OBJLIST<USERINFO> arUsers;
	OBJLIST<LOGINFO> arEvents;

	wchar_t pszLogFileName[MAX_PATH];

	__forceinline USERINFO* getMe() const {
		return (pParent != nullptr) ? pParent->pMe : pMe;
	}

	__forceinline STATUSINFO* getStatuses() const {
		return (pParent != nullptr) ? pParent->pStatuses : pStatuses;
	}

	__forceinline OBJLIST<USERINFO>& getUserList() {
		return (pParent != nullptr) ? pParent->arUsers : arUsers;
	}

	const char* getSoundName(int iEventType) const;
	void markRead(bool bForce = false);
};

struct GlobalLogSettingsBase
{
	bool     bShowTime;
	bool     bShowTimeIfChanged;
	bool     bLoggingEnabled;
	bool     bHighlightEnabled;
	bool     bLogIndentEnabled;
	bool     bStripFormat;
	bool     bSoundsFocus;
	bool     bPopupInactiveOnly;
	bool     bTrayIconInactiveOnly;
	bool     bLogLimitNames;
	bool     bTimeStampEventColour;
	bool		bShowContactStatus;
	bool		bContactStatusFirst;
	uint32_t dwIconFlags;
	int      LogIconSize;
	int      LogTextIndent;
	int      LoggingLimit;
	int      iEventLimit;
	int      iPopupStyle;
	int      iPopupTimeout;
	int      iWidth;
	int      iHeight;
	wchar_t *pszTimeStamp;
	wchar_t *pszTimeStampLog;
	wchar_t *pszIncomingNick;
	wchar_t *pszOutgoingNick;
	wchar_t *pszHighlightWords;
	wchar_t *pszLogDir;
	HFONT    UserListFont;
	HFONT    UserListHeadingsFont;
	HFONT    NameFont;
	COLORREF crLogBackground;
	COLORREF crUserListColor;
	COLORREF crUserListBGColor;
	COLORREF crUserListSelectedBGColor;
	COLORREF crUserListHeadingsColor;
	COLORREF crPUTextColour;
	COLORREF crPUBkgColour;
};

/////////////////////////////////////////////////////////////////////////////////////////

#ifndef SRMM_OWN_STRUCTURES
struct MODULEINFO : public GCModuleInfoBase {};
#endif

/////////////////////////////////////////////////////////////////////////////////////////

#define FONTMODE_USE   1
#define FONTMODE_SKIP  2
#define FONTMODE_ALTER 3

struct CHAT_MANAGER_INITDATA
{
	GlobalLogSettingsBase *pSettings;
	int cbModuleInfo;
	wchar_t *szFontGroup;
	int iFontMode;
	HPLUGIN pPlugin;
};

typedef BOOL (*pfnDoTrayIcon)(SESSION_INFO *si, GCEVENT *gce);
typedef BOOL (*pfnDoPopup)(SESSION_INFO *si, GCEVENT *gce);

struct CHAT_MANAGER
{
	CHAT_MANAGER();

	HICON         (*SM_GetStatusIcon)(SESSION_INFO *si, USERINFO * ui);
	int           (*SM_GetCount)(const char *pszModule);
	SESSION_INFO* (*SM_FindSessionByIndex)(const char *pszModule, int iItem);
	void          (*SM_InvalidateLogDirectories)(void);

	MODULEINFO*   (*MM_CreateModule)(void);
	void          (*MM_IconsChanged)(void);
	BOOL          (*MM_RemoveAll)(void);

	BOOL          (*TM_RemoveAll)(STATUSINFO** pStatusList);

	int           (*UM_CompareItem)(const USERINFO *u1, const USERINFO *u2);
	USERINFO*     (*UM_AddUser)(SESSION_INFO *si, const wchar_t *pszUID, const wchar_t *pszNick, uint16_t wStatus);
	USERINFO*     (*UM_FindUser)(SESSION_INFO *si, const wchar_t *pszUID);
	USERINFO*     (*UM_GiveStatus)(SESSION_INFO *si, const wchar_t *pszUID, uint16_t status);
	BOOL          (*UM_RemoveAll)(SESSION_INFO *si);
	USERINFO*     (*UM_SetContactStatus)(SESSION_INFO *si, const wchar_t *pszUID, uint16_t status);
	USERINFO*     (*UM_TakeStatus)(SESSION_INFO *si, const wchar_t *pszUID, uint16_t status);
	wchar_t*      (*UM_FindUserAutoComplete)(SESSION_INFO *si, const wchar_t* pszOriginal, const wchar_t* pszCurrent);

	void          (*LoadMsgDlgFont)(int i, LOGFONT *lf, COLORREF *color);
	wchar_t*      (*MakeTimeStamp)(wchar_t *pszStamp, time_t time);

	BOOL          (*DoSoundsFlashPopupTrayStuff)(SESSION_INFO *si, GCEVENT *gce, BOOL bHighlight, int bManyFix);
	BOOL          (*DoTrayIcon)(SESSION_INFO *si, GCEVENT *gce);
	BOOL          (*DoPopup)(SESSION_INFO *si, GCEVENT *gce);
	int           (*ShowPopup)(MCONTACT hContact, SESSION_INFO *si, HICON hIcon, char* pszProtoName, wchar_t* pszRoomName, COLORREF crBkg, const wchar_t* fmt, ...);
	wchar_t*      (*GetChatLogsFilename)(SESSION_INFO *si, time_t tTime);
	char*         (*Log_SetStyle)(int style);

	bool          (*IsHighlighted)(SESSION_INFO *si, GCEVENT *pszText);
	void          (*ReloadSettings)(void);

	void          (*CreateNick)(const SESSION_INFO *si, const LOGINFO *lin, CMStringW &dest);

	int logPixelSY, logPixelSX;
	HICON  hStatusIcons[STATUSICONCOUNT];
	HBRUSH hListBkgBrush, hListSelectedBkgBrush;
	HANDLE hevWinPopup, hevPreCreate;
	FONTINFO aFonts[OPTIONS_FONTCOUNT];
	LIST<SESSION_INFO> &arSessions;
	char **pLogIconBmpBits;
	CMOption<bool> bRightClickFilter;

	// public API
	MIR_APP_DLL(HICON) getIcon(int iEventType) const;

	// user-defined custom callbacks
	bool (*DbEventIsShown)(const DB::EventInfo &dbei);

	void (*OnCreateModule)(MODULEINFO*);
	void (*OnDestroyModule)(MODULEINFO*);

	void (*OnCreateSession)(SESSION_INFO*, MODULEINFO*);
	void (*OnOfflineSession)(SESSION_INFO*);
	void (*OnReplaceSession)(SESSION_INFO*);

	void (*ShowRoom)(SESSION_INFO*);
	void (*OnEventBroadcast)(SESSION_INFO *si, GCEVENT *gce);
	
	void (*OnSetTopic)(SESSION_INFO*);

	void (*OnAddUser)(SESSION_INFO*, USERINFO*);
	void (*OnNewUser)(SESSION_INFO*, USERINFO*);
	void (*OnRemoveUser)(SESSION_INFO*, USERINFO*);
	void (*OnChangeNick)(SESSION_INFO*);

	void (*OnAddStatus)(SESSION_INFO*, STATUSINFO*);
	void (*OnSetStatus)(SESSION_INFO*, int);

	void (*OnLoadSettings)(void);
	void (*OnFlashWindow)(SESSION_INFO*, int bInactive);
	void (*OnFlashHighlight)(SESSION_INFO*, int bInactive);
};

extern MIR_APP_EXPORT CHAT_MANAGER g_chatApi;

MIR_APP_DLL(CHAT_MANAGER*) Chat_CustomizeApi(const CHAT_MANAGER_INITDATA *pData);

/////////////////////////////////////////////////////////////////////////////////////////

// updates options for all windows
MIR_APP_DLL(void) Chat_UpdateOptions();

// runs ME_GC_EVENT with the parameters passed
MIR_APP_DLL(BOOL) Chat_DoEventHook(SESSION_INFO *si, int iType, const USERINFO *pUser, const wchar_t* pszText, INT_PTR dwItem);

// wipes chat's history
MIR_APP_DLL(void) Chat_EmptyHistory(SESSION_INFO *si);

// creates custom popup menu for a group chat
MIR_APP_DLL(void) Chat_CreateMenu(HMENU hMenu, SESSION_INFO *si, const wchar_t *pszUID);

// retrieves user's role/status
MIR_APP_DLL(STATUSINFO*) Chat_GetStatus(STATUSINFO *pStatuses, const USERINFO *pUser);

// creates a default description of a group chat event
// returns true if lin->ptszText is already utilized, you need to add it manually then otherwise
MIR_APP_DLL(bool) Chat_GetDefaultEventDescr(const SESSION_INFO *si, const LOGINFO *lin, CMStringW &res);

// calculates width or height of a string
MIR_APP_DLL(int) Chat_GetTextPixelSize(const wchar_t *pszText, HFONT hFont, bool bWidth);

// sets mute mode for a group chat
MIR_APP_DLL(int) Chat_IsMuted(MCONTACT hContact);
MIR_APP_DLL(void) Chat_Mute(MCONTACT hContact, int mode);

// resets filters for all session
MIR_APP_DLL(void) Chat_ReconfigureFilters();

// sets filters for a session
MIR_APP_DLL(void) Chat_SetFilters(SESSION_INFO *si);

/////////////////////////////////////////////////////////////////////////////////////////
// common settings

namespace Chat
{
	extern MIR_APP_EXPORT CMOption<bool>
		bShowNicklist,
		bFilterEnabled,
		bTopicOnClist,
		bPopupOnJoin,
		bUseGroup,
		bDoubleClick4Privat,
		bShowContactStatus,
		bContactStatusFirst,
		bShowTime,
		bShowTimeIfChanged,
		bTimeStampEventColour,
		bFlashWindow,
		bFlashWindowHighlight,
		bDoubleClick4Privat,
		bLogIndentEnabled,
		bLogLimitNames,
		bStripFormat,
		bEnableCustomLogs;

	extern MIR_APP_EXPORT CMOption<uint32_t>
		iPopupFlags,
		iSoundFlags,
		iFilterFlags,
		iDiskLogFlags,
		iTrayIconFlags;
};

#endif // M_CHAT_INT_H__
