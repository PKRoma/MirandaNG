/*

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
#include "chat.h"

#include <m_NewStory.h>
#include <m_messagestate.h>

MIR_APP_EXPORT CMOption<uint8_t> Srmm::iHistoryMode(SRMM_MODULE, "LoadHistory", LOADHISTORY_COUNT);

HCURSOR g_hCurHyperlinkHand;
HANDLE hHookIconsChanged, hHookIconPressedEvt, hHookSrmmEvent;

static HGENMENU hmiEmpty;

void LoadSrmmToolbarModule();
void UnloadSrmmToolbarModule();

/////////////////////////////////////////////////////////////////////////////////////////

static bool g_bHasMessageState, g_bHasNewStory;

MIR_APP_DLL(void) Srmm_NotifyRemoteRead(MCONTACT hContact, MEVENT hEvent)
{
	if (g_bHasMessageState)
		CallService(MS_MESSAGESTATE_UPDATE, hContact, MRD_TYPE_READ);

	if (g_bHasNewStory)
		NS_NotifyRemoteRead(hContact, hEvent);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Empty history service for main menu

class CEmptyHistoryDlg : public CDlgBase
{
	MCONTACT m_hContact;
	CCtrlCheck chkDelHistory, chkForEveryone;

public:
	char *szProto;
	bool bDelHistory, bForEveryone = false;

	CEmptyHistoryDlg(MCONTACT hContact) :
		CDlgBase(g_plugin, IDD_EMPTYHISTORY),
		m_hContact(hContact),
		chkDelHistory(this, IDC_DELSERVERHISTORY),
		chkForEveryone(this, IDC_BOTH)
	{
		szProto = Proto_GetBaseAccountName(hContact);
		bDelHistory = Proto_CanDeleteHistory(szProto, hContact);
		bForEveryone = (CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_4, hContact) & PF4_DELETEFORALL) != 0;
	}

	bool OnInitDialog() override
	{
		chkDelHistory.SetState(false);
		chkDelHistory.Enable(bDelHistory);

		chkForEveryone.SetState(false);
		chkForEveryone.Enable(bDelHistory && bForEveryone);

		LOGFONT lf;
		HFONT hFont = (HFONT)SendDlgItemMessage(m_hwnd, IDOK, WM_GETFONT, 0, 0);
		GetObject(hFont, sizeof(lf), &lf);
		lf.lfWeight = FW_BOLD;
		SendDlgItemMessage(m_hwnd, IDC_TOPLINE, WM_SETFONT, (WPARAM)CreateFontIndirect(&lf), 0);

		if (m_hContact != 0) {
			wchar_t szFormat[256], szFinal[256];
			GetDlgItemText(m_hwnd, IDC_TOPLINE, szFormat, _countof(szFormat));
			mir_snwprintf(szFinal, szFormat, Clist_GetContactDisplayName(m_hContact));
			SetDlgItemText(m_hwnd, IDC_TOPLINE, szFinal);
		}
		else {
			SetDlgItemText(m_hwnd, IDC_TOPLINE, TranslateT("Are you sure to wipe the system history?"));
			ShowWindow(GetDlgItem(m_hwnd, IDC_SECONDLINE), SW_HIDE);
		}

		SetFocus(GetDlgItem(m_hwnd, IDNO));
		SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		return true;
	}

	bool OnApply() override
	{
		bDelHistory = chkDelHistory.IsChecked();
		bForEveryone = chkForEveryone.IsChecked();
		return true;
	}

	void OnDestroy() override
	{
		DeleteObject((HFONT)SendDlgItemMessage(m_hwnd, IDC_TOPLINE, WM_GETFONT, 0, 0));
	}
};

static INT_PTR svcEmptyHistory(WPARAM hContact, LPARAM lParam)
{
	CEmptyHistoryDlg dlg(hContact);
	if (lParam == 0)
		if (dlg.DoModal() != IDOK)
			return 1;

	DB::ECPTR pCursor(DB::Events(hContact));
	while (pCursor.FetchNext())
		pCursor.DeleteEvent();

	if (Contact::IsGroupChat(hContact)) {
		if (auto *si = SM_FindSessionByContact(hContact))
			Chat_EmptyHistory(si);

		if (auto *szProto = Proto_GetBaseAccountName(hContact))
			db_unset(hContact, szProto, "ApparentMode");
	}

	if (dlg.bDelHistory)
		CallContactService(hContact, PS_EMPTY_SRV_HISTORY, hContact, CDF_DEL_HISTORY | (dlg.bForEveryone ? CDF_FOR_EVERYONE : 0));
	return 0;
}

static int OnPrebuildContactMenu(WPARAM hContact, LPARAM)
{
	Menu_ShowItem(hmiEmpty, db_event_first(hContact) != 0);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int OnModuleLoaded(WPARAM, LPARAM)
{
	g_bHasMessageState = ServiceExists(MS_MESSAGESTATE_UPDATE);
	g_bHasNewStory = ServiceExists("NewStory/FileReady");
	return 0;
}

void SrmmModulesLoaded()
{
	HookEvent(ME_SYSTEM_MODULELOAD, OnModuleLoaded);
	HookEvent(ME_SYSTEM_MODULEUNLOAD, OnModuleLoaded);
	OnModuleLoaded(0, 0);

	// menu item
	CMenuItem mi(&g_plugin);
	SET_UID(mi, 0x0d4306aa, 0xe31e, 0x46ee, 0x89, 0x88, 0x3a, 0x2e, 0x05, 0xa6, 0xf3, 0xbc);
	mi.pszService = MS_HISTORY_EMPTY;
	mi.name.a = LPGEN("Empty history");
	mi.position = 1000090001;
	mi.hIcon = Skin_LoadIcon(SKINICON_OTHER_DELETE);
	hmiEmpty = Menu_AddContactMenuItem(&mi);

	// create menu item in main menu for empty system history
	SET_UID(mi, 0x633AD23C, 0x24B5, 0x4914, 0xB2, 0x40, 0xAD, 0x9F, 0xAC, 0xB5, 0x64, 0xED);
	mi.position = 500060002;
	mi.name.a = LPGEN("Empty system history");
	mi.pszService = MS_HISTORY_EMPTY;
	mi.hIcon = Skin_LoadIcon(SKINICON_OTHER_DELETE);
	Menu_AddMainMenuItem(&mi);

	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, OnPrebuildContactMenu);
}

/////////////////////////////////////////////////////////////////////////////////////////

static HOTKEYDESC srmmHotkeys[] = {
	{ "srmm_bold",     LPGEN("Toggle bold formatting"),      BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL, 'B'), 0, SRMM_HK_BOLD },
	{ "srmm_italic",   LPGEN("Toggle italic formatting"),    BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL, 'I'), 0, SRMM_HK_ITALIC },
	{ "srmm_under",    LPGEN("Toggle underline formatting"), BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL, 'U'), 0, SRMM_HK_UNDERLINE },
	{ "srmm_color",    LPGEN("Toggle text color"),           BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL, 'K'), 0, SRMM_HK_COLOR },
	{ "srmm_bkcolor",  LPGEN("Toggle background color"),     BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL, 'L'), 0, SRMM_HK_BKCOLOR },
	{ "srmm_clear",    LPGEN("Clear formatting"),            BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL, VK_SPACE), 0, SRMM_HK_CLEAR },
	{ "srmm_history",  LPGEN("Open history window"),         BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL, 'H'), 0, SRMM_HK_HISTORY},
	{ "srmm_filter",   LPGEN("Toggle filter"),               BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL, 'F'), 0, SRMM_HK_FILTERTOGGLE },
	{ "srmm_nicklist", LPGEN("Toggle nick list"),            BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL | HOTKEYF_SHIFT, 'N'), 0, SRMM_HK_LISTTOGGLE },
	{ "srmm_cmgr",     LPGEN("Channel manager"),             BB_HK_SECTION, nullptr, HOTKEYCODE(HOTKEYF_CONTROL | HOTKEYF_SHIFT, 'O'), 0, SRMM_HK_CHANNELMGR },
};

int LoadSrmmModule()
{
	for (auto &it : srmmHotkeys)
		g_plugin.addHotkey(&it);

	g_hCurHyperlinkHand = LoadCursor(nullptr, IDC_HAND);

	CheckLogOptions();
	LoadSrmmToolbarModule();

	CreateServiceFunction(MS_HISTORY_EMPTY, svcEmptyHistory);

	hHookSrmmEvent = CreateHookableEvent(ME_MSG_WINDOWEVENT);
	hHookIconsChanged = CreateHookableEvent(ME_MSG_ICONSCHANGED);
	hHookIconPressedEvt = CreateHookableEvent(ME_MSG_ICONPRESSED);
	return 0;
}

void UnloadSrmmModule()
{
	DestroyHookableEvent(hHookIconsChanged);
	DestroyHookableEvent(hHookSrmmEvent);
	DestroyHookableEvent(hHookIconPressedEvt);

	DestroyCursor(g_hCurHyperlinkHand);

	UnloadSrmmToolbarModule();
}
