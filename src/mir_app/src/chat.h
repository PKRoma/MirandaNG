/*

Chat module plugin for Miranda IM

Copyright (C) 2003 Jörgen Persson

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

#include <m_smileyadd.h>
#include <m_popup.h>
#include <m_fontservice.h>

#define GC_FAKE_EVENT MEVENT(0xBABABEDA)

#define STREAMSTAGE_HEADER  0
#define STREAMSTAGE_EVENTS  1
#define STREAMSTAGE_TAIL    2
#define STREAMSTAGE_STOP    3

#define SRMM_HK_BOLD         40001
#define SRMM_HK_ITALIC       40002
#define SRMM_HK_UNDERLINE    40003
#define SRMM_HK_BKCOLOR      40004
#define SRMM_HK_COLOR        40005
#define SRMM_HK_CLEAR        40006
#define SRMM_HK_HISTORY      40007
#define SRMM_HK_CHANNELMGR   40008
#define SRMM_HK_FILTERTOGGLE 40009
#define SRMM_HK_LISTTOGGLE   40010

#define N_CUSTOM_BBCODES    3
extern wchar_t *wszBbcodes[N_CUSTOM_BBCODES];

#define DM_OPTIONSAPPLIED (WM_USER+14)

void Srmm_CreateToolbarIcons(CSrmmBaseDialog *pDlg, int flags);

void CheckChatCompatibility();

class CLogWindow : public CSrmmLogWindow {};

extern HPLUGIN  g_pChatPlugin;
extern int      g_cbModuleInfo, g_iFontMode;
extern wchar_t *g_szFontGroup;
extern mir_cs   csChat;

extern HICON    g_hChatIcons[20];
extern HCURSOR  g_hCurHyperlinkHand;
extern char*    pLogIconBmpBits[14];
extern HANDLE   hevSendEvent, hevBuildMenuEvent;

extern MWindowList g_hWindowList;
extern LIST<SESSION_INFO> g_arSessions;
extern GlobalLogSettingsBase *g_Settings;

extern CMOption<bool> g_bChatTrayInactive, g_bChatPopupInactive;

extern const char *g_pszHotkeySection;

// log.c
void          LoadMsgLogBitmaps(void);
void          FreeMsgLogBitmaps(void);
void          ValidateFilename (wchar_t *filename);
wchar_t*      MakeTimeStamp(wchar_t *pszStamp, time_t time);
wchar_t*      GetChatLogsFilename(SESSION_INFO *si, time_t tTime);
char*         Log_SetStyle(int style);

// chat_manager.cpp
MODULEINFO*   MM_AddModule(const char *pszModule);
MODULEINFO*   MM_FindModule(const char *pszModule);

LOGINFO*      SM_AddEvent(SESSION_INFO *si, GCEVENT *gce, bool bIsHighlighted);
BOOL          SM_ChangeNick(SESSION_INFO *si, GCEVENT *gce);
void          SM_FreeSession(SESSION_INFO *si);
BOOL          SM_GiveStatus(SESSION_INFO *si, const wchar_t *pszUID, const wchar_t *pszStatus);
BOOL          SM_AssignStatus(SESSION_INFO *si, const wchar_t *pszUID, const wchar_t *pszStatus);
void          SM_RemoveAll(void);
BOOL          SM_RemoveUser(SESSION_INFO *si, const wchar_t *pszUID);
BOOL          SM_SetContactStatus(SESSION_INFO *si, const wchar_t *pszUID, uint16_t wStatus);
BOOL          SM_SetOffline(const char *pszModule, SESSION_INFO *si);
BOOL          SM_SetStatus(const char *pszModule, SESSION_INFO *si, int wStatus);
BOOL          SM_TakeStatus(SESSION_INFO *si, const wchar_t *pszUID, const wchar_t *pszStatus);
BOOL          SM_UserTyping(GCEVENT* gce);

SESSION_INFO* SM_FindSessionByContact(MCONTACT hContact);
SESSION_INFO* SM_FindSessionByIndex(const char *pszModule, int iItem);

STATUSINFO*   TM_AddStatus(STATUSINFO **ppStatusList, const wchar_t *pszStatus, int *iCount);
STATUSINFO*   TM_FindStatus(STATUSINFO *pStatusList, const wchar_t *pszStatus);
uint16_t      TM_StringToWord(STATUSINFO *pStatusList, const wchar_t *pszStatus);
wchar_t*      TM_WordToString(STATUSINFO *pStatusList, uint16_t Status);

USERINFO*     UM_AddUser(SESSION_INFO *si, const wchar_t *pszUID, const wchar_t *pszNick, uint16_t wStatus);
BOOL          UM_RemoveAll(SESSION_INFO *si);
BOOL          UM_SetStatusEx(SESSION_INFO *si, const wchar_t* pszText, int flags);
void          UM_SortKeys(SESSION_INFO *si);

// clist.c
MCONTACT      AddRoom(const char *pszModule, const wchar_t *pszRoom, const wchar_t *pszDisplayName, int iType);
BOOL          SetAllOffline(const char *pszModule);
BOOL          SetOffline(MCONTACT hContact);
		        
int           RoomDoubleclicked(WPARAM wParam,LPARAM lParam);

// options.c
void          ChatOptionsInit(WPARAM wParam);
void          SrmmLogOptionsInit(WPARAM wParam);

int           OptionsInit(void);
int           OptionsUnInit(void);
void          LoadMsgDlgFont(int i, LOGFONT * lf, COLORREF * colour);
void          LoadGlobalSettings(void);
HICON         LoadIconEx(char* pszIcoLibName, bool big);
void          LoadLogFonts(void);
void          SetIndentSize(void);
void          RegisterFonts(void);

// services.c
void          LoadChatIcons(void);
int           LoadChatModule(void);
void          UnloadChatModule(void);

// tools.c
int           DoRtfToTags(CMStringW &pszText, int iNumColors, COLORREF *pColors);
BOOL          DoSoundsFlashPopupTrayStuff(SESSION_INFO *si, GCEVENT *gce, BOOL bHighlight, int bManyFix);
int           GetRichTextLength(HWND hwnd);
bool          IsHighlighted(SESSION_INFO *si, GCEVENT *pszText);
BOOL          IsEventSupported(int eventType);
BOOL          LogToFile(SESSION_INFO *si, GCEVENT *gce);
BOOL          DoTrayIcon(SESSION_INFO *si, GCEVENT *gce);
BOOL          DoPopup(SESSION_INFO *si, GCEVENT *gce);
int           ShowPopup(MCONTACT hContact, SESSION_INFO *si, HICON hIcon, char* pszProtoName, wchar_t* pszRoomName, COLORREF crBkg, const wchar_t* fmt, ...);

void          Chat_EventToGC(SESSION_INFO *si, MEVENT hDbEvent);
void          Chat_RemoveContact(MCONTACT hContact);

CMStringW     Chat_GetFolderName(SESSION_INFO *si = nullptr);
void          Chat_Serialize(SESSION_INFO *si);
bool          Chat_Unserialize(SESSION_INFO *si);

EXTERN_C MIR_APP_DLL(HANDLE) Srmm_AddButton(const BBButton *bbdi, HPLUGIN _hLang);

#pragma comment(lib,"comctl32.lib")
