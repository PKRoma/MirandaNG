/*
Scriver

Copyright (c) 2000-12 Miranda ICQ/IM project,

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

HCURSOR  hDragCursor;
HGENMENU hMsgMenuItem;
HMODULE hMsftEdit;

#define SCRIVER_DB_GETEVENTTEXT "Scriver/GetText"

static int SRMMStatusToPf2(int status)
{
	switch (status) {
	case ID_STATUS_ONLINE:
		return PF2_ONLINE;
	case ID_STATUS_AWAY:
		return PF2_SHORTAWAY;
	case ID_STATUS_DND:
		return PF2_HEAVYDND;
	case ID_STATUS_NA:
		return PF2_LONGAWAY;
	case ID_STATUS_OCCUPIED:
		return PF2_LIGHTDND;
	case ID_STATUS_FREECHAT:
		return PF2_FREECHAT;
	case ID_STATUS_INVISIBLE:
		return PF2_INVISIBLE;
	case ID_STATUS_OFFLINE:
		return MODEF_OFFLINE;
	}
	return 0;
}

int IsAutoPopup(MCONTACT hContact) {
	if (g_dat.flags.bAutoPopup) {
		char *szProto = Proto_GetBaseAccountName(hContact);

		hContact = db_mc_getSrmmSub(hContact);
		if (hContact != 0)
			szProto = Proto_GetBaseAccountName(hContact);

		if (szProto && (g_dat.openFlags & SRMMStatusToPf2(Proto_GetStatus(szProto))))
			return 1;
	}
	return 0;
}

static INT_PTR ReadMessageCommand(WPARAM, LPARAM lParam)
{
	CLISTEVENT *pcle = (CLISTEVENT*)lParam;
	MCONTACT hContact = db_mc_tryMeta(pcle->hContact);

	auto *pDlg = Srmm_FindDialog(hContact);
	if (pDlg == nullptr)
		(new CMsgDialog(hContact, false))->Show();
	else
		pDlg->PopupWindow();
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

struct CAutoPpopup : public MAsyncObject
{
	MCONTACT hContact;
	MEVENT hDbEvent;

	CAutoPpopup(MCONTACT _1, MEVENT _2) :
		hContact(_1),
		hDbEvent(_2)
	{}

	void Invoke() override
	{
		/* does a window for the contact exist? */
		HWND hwnd = Srmm_FindWindow(hContact);
		if (hwnd == nullptr)
			hwnd = Srmm_FindWindow(db_event_getContact(hDbEvent));

		if (hwnd == nullptr) {
			/* new message */
			Skin_PlaySound("AlertMsg");
			if (IsAutoPopup(hContact)) {
				(new CMsgDialog(hContact, true))->Show();
				return;
			}
		}

		if (hwnd == nullptr || !IsWindowVisible(GetParent(hwnd)))
			Srmm_AddEvent(hContact, hDbEvent);
	}
};

static int MessageEventAdded(WPARAM hContact, LPARAM hDbEvent)
{
	if (hContact == 0 || Contact::IsGroupChat(hContact))
		return 0;

	DB::EventInfo dbei(hDbEvent, false);
	if (!dbei)
		return 0;

	if (dbei.eventType == EVENTTYPE_MESSAGE && dbei.bRead)
		return 0;

	if (dbei.bSent || !dbei.isSrmm())
		return 0;

	Utils_InvokeAsync(new CAutoPpopup(hContact, hDbEvent));
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static INT_PTR SendMessageCommandWorker(MCONTACT hContact, wchar_t *pszMsg)
{
	hContact = db_mc_tryMeta(hContact);

	/* does the MCONTACT's protocol support IM messages? */
	char *szProto = Proto_GetBaseAccountName(hContact);
	if (szProto == nullptr)
		return 1; /* unknown contact */

	if (!(CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_IMSEND))
		return 1;

	auto *pDlg = Srmm_FindDialog(hContact);
	if (pDlg != nullptr) {
		if (pszMsg) {
			HWND hEdit = GetDlgItem(pDlg->GetHwnd(), IDC_SRMM_MESSAGE);
			SendMessage(hEdit, EM_SETSEL, -1, GetWindowTextLength(hEdit));
			SendMessage(hEdit, EM_REPLACESEL, FALSE, (LPARAM)pszMsg);
			mir_free(pszMsg);
		}
		pDlg->PopupWindow();
	}
	else {
		pDlg = new CMsgDialog(hContact, true);
		pDlg->m_wszInitialText = pszMsg;
		pDlg->Show();
	}

	return 0;
}

static INT_PTR SendMessageCommandW(WPARAM hContact, LPARAM lParam)
{
	return SendMessageCommandWorker(hContact, mir_wstrdup(LPCWSTR(lParam)));
}

static INT_PTR SendMessageCommand(WPARAM hContact, LPARAM lParam)
{
	return SendMessageCommandWorker(hContact, mir_a2u(LPCSTR(lParam)));
}

/////////////////////////////////////////////////////////////////////////////////////////

static INT_PTR TypingMessageCommand(WPARAM, LPARAM lParam)
{
	CLISTEVENT *cle = (CLISTEVENT*)lParam;
	if (cle)
		SendMessageCommand(cle->hContact, 0);
	return 0;
}

static int TypingMessage(WPARAM hContact, LPARAM lParam)
{
	if (!g_dat.flags2.bShowTyping)
		return 0;

	hContact = db_mc_tryMeta(hContact);

	Skin_PlaySound((lParam) ? "TNStart" : "TNStop");

	auto *pDlg = Srmm_FindDialog(hContact);
	if (pDlg != nullptr)
		pDlg->UserIsTyping(lParam);
	else if (lParam && g_dat.flags2.bShowTypingTray) {
		wchar_t szTip[256];
		mir_snwprintf(szTip, TranslateT("%s is typing a message"), Clist_GetContactDisplayName(hContact));
		if (g_dat.flags2.bShowTypingClist) {
			Clist_RemoveEvent(hContact, 1);

			CLISTEVENT cle = {};
			cle.hContact = hContact;
			cle.hDbEvent = 1;
			cle.flags = CLEF_ONLYAFEW | CLEF_UNICODE;
			cle.hIcon = g_plugin.getIcon(IDI_TYPING);
			cle.pszService = MS_MSG_TYPINGMESSAGE;
			cle.szTooltip.w = szTip;
			g_clistApi.pfnAddEvent(&cle);
		}
		else Clist_TrayNotifyW(nullptr, TranslateT("Typing notification"), szTip, NIIF_INFO, 1000 * 4);
	}
	return 0;
}

static int MessageSettingChanged(WPARAM hContact, LPARAM lParam)
{
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING *)lParam;
	char *szProto = Proto_GetBaseAccountName(hContact); // szProto maybe nullptr
	if (!strcmp(cws->szModule, "CList") || !mir_strcmp(cws->szModule, szProto))
		Srmm_Broadcast(DM_CLISTSETTINGSCHANGED, hContact, lParam);
	return 0;
}

static int ContactDeleted(WPARAM wParam, LPARAM)
{
	HWND hwnd = Srmm_FindWindow(wParam);
	if (hwnd)
		SendMessage(hwnd, WM_CLOSE, 0, 0);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

struct MSavedEvent
{
	MSavedEvent(MCONTACT _hContact, MEVENT _hEvent) :
		hContact(_hContact),
		hEvent(_hEvent)
	{
	}

	MEVENT   hEvent;
	MCONTACT hContact;
};

static void RestoreUnreadMessageAlerts(void)
{
	OBJLIST<MSavedEvent> events(10, NumericKeySortT);

	for (auto &hContact : Contacts()) {
		if (Contact::IsGroupChat(hContact) || !Proto_GetBaseAccountName(hContact))
			continue;

		for (MEVENT hDbEvent = db_event_firstUnread(hContact); hDbEvent; hDbEvent = db_event_next(hContact, hDbEvent)) {
			DB::EventInfo dbei(hDbEvent, false);
			if (!dbei)
				continue;
			if (dbei.bRead || !dbei.isSrmm() || !Proto_GetBaseAccountName(hContact))
				continue;

			int windowAlreadyExists = Srmm_FindWindow(hContact) != nullptr;
			if (windowAlreadyExists)
				continue;

			if (IsAutoPopup(hContact) && !windowAlreadyExists)
				(new CMsgDialog(hContact, true))->Show();
			else
				events.insert(new MSavedEvent(hContact, hDbEvent));
		}
	}

	for (auto &e : events)
		Srmm_AddEvent(e->hContact, e->hEvent);
}

void CMsgDialog::SetStatusText(const wchar_t *wszText, HICON hIcon)
{
	ParentWindowData *pDat = m_pParent;
	if (pDat != nullptr) {
		SendMessage(pDat->m_hwndStatus, SB_SETICON, 0, (LPARAM)hIcon);
		SendMessage(pDat->m_hwndStatus, SB_SETTEXT, 0, (LPARAM)(wszText == nullptr ? L"" : wszText));
	}
}

static int PrebuildContactMenu(WPARAM hContact, LPARAM)
{
	if (hContact == 0)
		return 0;

	bool bEnabled = false;
	char *szProto = Proto_GetBaseAccountName(hContact);
	if (szProto) {
		// leave this menu item hidden for chats
		if (!Contact::IsGroupChat(hContact, szProto))
			if (CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_IMSEND)
				bEnabled = true;
	}

	Menu_ShowItem(hMsgMenuItem, bEnabled);
	return 0;
}

static int AvatarChanged(WPARAM wParam, LPARAM lParam)
{
	if (wParam == 0)          // protocol picture has changed...
		Srmm_Broadcast(DM_AVATARCHANGED, wParam, lParam);
	else {
		HWND hwnd = Srmm_FindWindow(wParam);
		SendMessage(hwnd, DM_AVATARCHANGED, wParam, lParam);
	}
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// status icons processing

static void RegisterStatusIcons()
{
	StatusIconData sid = {};
	sid.szModule = SRMM_MODULE;
	sid.dwId = 1;
	sid.hIcon = g_plugin.getIcon(IDI_TYPING);
	sid.hIconDisabled = g_plugin.getIcon(IDI_TYPINGOFF);
	sid.flags = MBF_HIDDEN;
	Srmm_AddIcon(&sid, &g_plugin);
}

int StatusIconPressed(WPARAM wParam, LPARAM lParam)
{
	StatusIconClickData *sicd = (StatusIconClickData *) lParam;
	if (mir_strcmp(SRMM_MODULE, sicd->szModule))
		return 0;

	auto *pDlg = Srmm_FindDialog(wParam);
	if (pDlg != nullptr)
		pDlg->SwitchTyping();
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// toolbar icons processing

int RegisterToolbarIcons(WPARAM, LPARAM)
{
	BBButton bbd = {};
	bbd.pszModuleName = "SRMM";
	bbd.bbbFlags = BBBF_ISCHATBUTTON | BBBF_ISIMBUTTON | BBBF_CREATEBYID | BBBF_ISRSIDEBUTTON | BBBF_CANTBEHIDDEN | BBBF_NOREADONLY;
	bbd.dwButtonID = IDOK;
	bbd.dwDefPos = 5;
	bbd.hIcon = g_plugin.getIconHandle(IDI_SEND);
	bbd.pwszText = LPGENW("&OK");
	bbd.pwszTooltip = LPGENW("Send message");
	g_plugin.addButton(&bbd);
	
	bbd.bbbFlags = BBBF_ISIMBUTTON | BBBF_CREATEBYID | BBBF_NOREADONLY;
	bbd.dwButtonID = IDC_QUOTE;
	bbd.dwDefPos = 10;
	bbd.hIcon = g_plugin.getIconHandle(IDI_QUOTE);
	bbd.pwszText = LPGENW("&Quote");
	bbd.pwszTooltip = LPGENW("Quote");
	g_plugin.addButton(&bbd);

	bbd.bbbFlags = BBBF_ISIMBUTTON | BBBF_CREATEBYID | BBBF_ISRSIDEBUTTON;
	bbd.dwButtonID = IDC_ADD;
	bbd.dwDefPos = 20;
	bbd.hIcon = g_plugin.getIconHandle(IDI_ADDCONTACT);
	bbd.pwszText = LPGENW("&Add");
	bbd.pwszTooltip = LPGENW("Add contact permanently to list");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_USERMENU;
	bbd.dwDefPos = 30;
	bbd.hIcon = Skin_GetIconHandle(SKINICON_OTHER_DOWNARROW);
	bbd.pwszText = LPGENW("&User menu");
	bbd.pwszTooltip = LPGENW("User menu");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_DETAILS;
	bbd.dwDefPos = 40;
	bbd.hIcon = Skin_GetIconHandle(SKINICON_OTHER_USERDETAILS);
	bbd.pwszText = LPGENW("User &details");
	bbd.pwszTooltip = LPGENW("View user's details");
	g_plugin.addButton(&bbd);

	bbd.bbbFlags |= BBBF_ISCHATBUTTON | BBBF_ISRSIDEBUTTON;
	bbd.dwButtonID = IDC_SRMM_HISTORY;
	bbd.dwDefPos = 50;
	bbd.hIcon = g_plugin.getIconHandle(IDI_HISTORY);
	bbd.pwszText = LPGENW("&History");
	bbd.pwszTooltip = LPGENW("View user's history");
	g_plugin.addButton(&bbd);

	// format buttons
	bbd.bbbFlags = BBBF_ISPUSHBUTTON | BBBF_ISCHATBUTTON | BBBF_ISIMBUTTON | BBBF_CREATEBYID | BBBF_NOREADONLY;
	bbd.dwButtonID = IDC_SRMM_BOLD;
	bbd.dwDefPos = 10;
	bbd.hIcon = g_plugin.getIconHandle(IDI_BBOLD);
	bbd.pwszText = LPGENW("&Bold");
	bbd.pwszTooltip = LPGENW("Make the text bold");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_SRMM_ITALICS;
	bbd.dwDefPos = 15;
	bbd.hIcon = g_plugin.getIconHandle(IDI_BITALICS);
	bbd.pwszText = LPGENW("&Italic");
	bbd.pwszTooltip = LPGENW("Make the text italicized");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_SRMM_UNDERLINE;
	bbd.dwDefPos = 20;
	bbd.hIcon = g_plugin.getIconHandle(IDI_BUNDERLINE);
	bbd.pwszText = LPGENW("&Underline");
	bbd.pwszTooltip = LPGENW("Make the text underlined");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_SRMM_STRIKEOUT;
	bbd.dwDefPos = 25;
	bbd.hIcon = g_plugin.getIconHandle(IDI_STRIKEOUT);
	bbd.pwszText = LPGENW("&Strikethrough");
	bbd.pwszTooltip = LPGENW("Make the text strikethrough");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_SRMM_COLOR;
	bbd.dwDefPos = 30;
	bbd.hIcon = g_plugin.getIconHandle(IDI_COLOR);
	bbd.pwszText = LPGENW("&Color");
	bbd.pwszTooltip = LPGENW("Select text color");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_SRMM_BKGCOLOR;
	bbd.dwDefPos = 35;
	bbd.hIcon = g_plugin.getIconHandle(IDI_BKGCOLOR);
	bbd.pwszText = LPGENW("&Background color");
	bbd.pwszTooltip = LPGENW("Select background color");
	g_plugin.addButton(&bbd);

	// chat buttons
	bbd.bbbFlags = BBBF_ISCHATBUTTON | BBBF_ISRSIDEBUTTON | BBBF_CREATEBYID;
	bbd.dwButtonID = IDC_SRMM_CHANMGR;
	bbd.dwDefPos = 30;
	bbd.hIcon = g_plugin.getIconHandle(IDI_TOPICBUT);
	bbd.pwszText = LPGENW("&Room settings");
	bbd.pwszTooltip = LPGENW("Control this room");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_SRMM_SHOWNICKLIST;
	bbd.dwDefPos = 20;
	bbd.hIcon = g_plugin.getIconHandle(IDI_NICKLIST);
	bbd.pwszText = LPGENW("&Show/hide nick list");
	bbd.pwszTooltip = LPGENW("Show/hide the nick list");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_SRMM_FILTER;
	bbd.dwDefPos = 10;
	bbd.hIcon = g_plugin.getIconHandle(IDI_FILTER);
	bbd.pwszText = LPGENW("&Filter");
	bbd.pwszTooltip = LPGENW("Enable/disable the event filter");
	g_plugin.addButton(&bbd);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int ModuleLoad(WPARAM, LPARAM)
{
	g_dat.smileyAddInstalled = ServiceExists(MS_SMILEYADD_REPLACESMILEYS);
	return 0;
}

static int MetaContactChanged(WPARAM hMeta, LPARAM)
{
	if (hMeta) {
		auto *pDlg = Srmm_FindDialog(hMeta);
		if (pDlg != nullptr)
			pDlg->GetAvatar();
	}
	return 0;
}

static int OnModulesLoaded(WPARAM, LPARAM)
{
	ReloadGlobals();
	LoadGlobalIcons();
	LoadMsgLogIcons();
	ModuleLoad(0, 0);

	CMenuItem mi(&g_plugin);
	SET_UID(mi, 0x58d8dc1, 0x1c25, 0x49c0, 0xb8, 0x7c, 0xa3, 0x22, 0x2b, 0x3d, 0xf1, 0xd8);
	mi.position = -2000090000;
	mi.flags = CMIF_DEFAULT;
	mi.hIcolibItem = Skin_GetIconHandle(SKINICON_EVENT_MESSAGE);
	mi.name.a = LPGEN("&Message");
	mi.pszService = MS_MSG_SENDMESSAGE;
	hMsgMenuItem = Menu_AddContactMenuItem(&mi);
	IcoLib_ReleaseIcon((HICON)mi.hIcolibItem);

	HookEvent(ME_SMILEYADD_OPTIONSCHANGED, SmileySettingsChanged);
	HookEvent(ME_IEVIEW_OPTIONSCHANGED, SmileySettingsChanged);
	HookEvent(ME_AV_AVATARCHANGED, AvatarChanged);
	HookEvent(ME_FONT_RELOAD, FontServiceFontsChanged);
	HookEvent(ME_MSG_ICONPRESSED, StatusIconPressed);
	HookEvent(ME_MC_DEFAULTTCHANGED, MetaContactChanged);

	HookTemporaryEvent(ME_MSG_TOOLBARLOADED, RegisterToolbarIcons);

	RestoreUnreadMessageAlerts();
	RegisterStatusIcons();
	return 0;
}

int OnSystemPreshutdown(WPARAM, LPARAM)
{
	WindowList_Broadcast(g_dat.hParentWindowList, WM_CLOSE, 0, 0);
	return 0;
}

int OnUnloadModule(void)
{
	DestroyCursor(hDragCursor);

	ReleaseIcons();
	FreeMsgLogIcons();
	FreeGlobals();
	FreeLibrary(hMsftEdit);
	return 0;
}

int OnLoadModule(void)
{
	hMsftEdit = LoadLibrary(L"Msftedit.dll");
	if (hMsftEdit == nullptr) {
		if (IDYES != MessageBox(nullptr,
			TranslateT("Miranda could not load the built-in message module, Msftedit.dll is missing. If you are using WINE, please make sure you have Msftedit.dll installed. Press 'Yes' to continue loading Miranda."),
			TranslateT("Information"), MB_YESNO | MB_ICONINFORMATION))
			return 1;
		return 0;
	}

	InitGlobals();
	InitStatusIcons();
	RegisterIcons();
	RegisterFontServiceFonts();
	RegisterKeyBindings();

	HookEvent(ME_OPT_INITIALISE, OptInitialise);
	HookEvent(ME_DB_EVENT_ADDED, MessageEventAdded);
	HookEvent(ME_DB_EVENT_EDITED, MessageEventAdded);
	HookEvent(ME_DB_CONTACT_SETTINGCHANGED, MessageSettingChanged);
	HookEvent(ME_DB_CONTACT_DELETED, ContactDeleted);
	HookEvent(ME_SYSTEM_MODULESLOADED, OnModulesLoaded);
	HookEvent(ME_SKIN_ICONSCHANGED, IconsChanged);
	HookEvent(ME_PROTO_CONTACTISTYPING, TypingMessage);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, OnSystemPreshutdown);
	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, PrebuildContactMenu);
	HookEvent(ME_SYSTEM_MODULELOAD, ModuleLoad);
	HookEvent(ME_SYSTEM_MODULEUNLOAD, ModuleLoad);

	CreateServiceFunction(MS_MSG_READMESSAGE, ReadMessageCommand);
	CreateServiceFunction(MS_MSG_SENDMESSAGE, SendMessageCommand);
	CreateServiceFunction(MS_MSG_SENDMESSAGEW, SendMessageCommandW);
	CreateServiceFunction(MS_MSG_TYPINGMESSAGE, TypingMessageCommand);

	g_plugin.addSound("RecvMsgActive", LPGENW("Instant messages"), LPGENW("Incoming (focused window)"));
	g_plugin.addSound("RecvMsgInactive", LPGENW("Instant messages"), LPGENW("Incoming (unfocused window)"));
	g_plugin.addSound("AlertMsg", LPGENW("Instant messages"), LPGENW("Incoming (new session)"));
	g_plugin.addSound("SendMsg", LPGENW("Instant messages"), LPGENW("Outgoing"));
	g_plugin.addSound("TNStart", LPGENW("Instant messages"), LPGENW("Contact started typing"));
	g_plugin.addSound("TNStop", LPGENW("Instant messages"), LPGENW("Contact stopped typing"));

	hDragCursor = LoadCursor(g_plugin.getInst(), MAKEINTRESOURCE(IDC_DRAGCURSOR));

	Chat_Load();
	return 0;
}
