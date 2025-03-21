/*

Copyright 2000-12 Miranda IM, 2012-25 Miranda NG team,
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
#include "statusicon.h"

static HGENMENU hMsgMenuItem;
static HMODULE hMsftEdit;

int OnCheckPlugins(WPARAM, LPARAM);

/////////////////////////////////////////////////////////////////////////////////////////

int SendMessageDirect(MCONTACT hContact, MEVENT hEvent, const wchar_t *szMsg)
{
	if (hContact == 0)
		return 0;

	int flags = 0;
	if (Utils_IsRtl(szMsg))
		flags |= DBEF_RTL;

	T2Utf sendBuffer(szMsg);
	if (!mir_strlen(sendBuffer))
		return 0;

	if (db_mc_isMeta(hContact))
		hContact = db_mc_getSrmmSub(hContact);

	int sendId = ProtoChainSend(hContact, PSS_MESSAGE, hEvent, (LPARAM)sendBuffer);
	msgQueue_add(hContact, hEvent, sendId, sendBuffer.detach(), flags);
	return sendId;
}

/////////////////////////////////////////////////////////////////////////////////////////

static int SRMMStatusToPf2(int status)
{
	switch (status) {
	case ID_STATUS_ONLINE:     return PF2_ONLINE;
	case ID_STATUS_AWAY:       return PF2_SHORTAWAY;
	case ID_STATUS_DND:        return PF2_HEAVYDND;
	case ID_STATUS_NA:         return PF2_LONGAWAY;
	case ID_STATUS_OCCUPIED:   return PF2_LIGHTDND;
	case ID_STATUS_FREECHAT:   return PF2_FREECHAT;
	case ID_STATUS_INVISIBLE:  return PF2_INVISIBLE;
	case ID_STATUS_OFFLINE:    return MODEF_OFFLINE;
	}
	return 0;
}

struct CAutoPopup : public MAsyncObject
{
	MCONTACT hContact;
	MEVENT hDbEvent;

	CAutoPopup(MCONTACT _1, MEVENT _2) :
		hContact(_1),
		hDbEvent(_2)
	{}

	void Invoke() override
	{
		bool bPopup = false;
		char *szProto = Proto_GetBaseAccountName(hContact);
		if (szProto && (g_plugin.popupFlags & SRMMStatusToPf2(Proto_GetStatus(szProto))))
			bPopup = true;

		/* does a window for the contact exist? */
		CTabbedWindow *pContainer = nullptr;
		auto *pDlg = Srmm_FindDialog(hContact);
		if (!pDlg) {
			if (bPopup) {
				pDlg = GetContainer()->AddPage(hContact, nullptr, true);
				pContainer = pDlg->getOwner();
			}
			else Srmm_AddEvent(hContact, hDbEvent);

			Skin_PlaySound("AlertMsg");			
		}
		else {
			pContainer = pDlg->getOwner();
			if (bPopup)
				ShowWindow(pContainer->GetHwnd(), SW_RESTORE);

			if (pContainer->CurrPage() != pDlg)
				Srmm_AddEvent(hContact, hDbEvent);
		}

		if (pContainer) {
			if (bPopup && g_Settings.bTabsEnable && GetForegroundWindow() != pContainer->GetHwnd())
				g_pTabDialog->m_tab.ActivatePage(g_pTabDialog->m_tab.GetDlgIndex(pDlg));

			if (!g_plugin.bDoNotStealFocus) {
				SetForegroundWindow(pContainer->GetHwnd());
				Skin_PlaySound("RecvMsgActive");
			}
			else {
				if (GetForegroundWindow() == GetParent(pContainer->GetHwnd()))
					Skin_PlaySound("RecvMsgActive");
				else
					Skin_PlaySound("RecvMsgInactive");
			}
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////////

static int MessageEventAdded(WPARAM hContact, LPARAM hDbEvent)
{
	if (hContact == 0 || Contact::IsGroupChat(hContact))
		return 0;

	DB::EventInfo dbei(hDbEvent, false);
	if (!dbei)
		return 0;

	if (dbei.markedRead() || !DbEventIsShown(dbei))
		return 0;

	Utils_InvokeAsync(new CAutoPopup(hContact, hDbEvent));
	return 0;
}

INT_PTR SendMessageCmd(MCONTACT hContact, wchar_t *pwszInitialText)
{
	/* does the MCONTACT's protocol support IM messages? */
	char *szProto = Proto_GetBaseAccountName(hContact);
	if (!szProto || !(CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_IMSEND))
		return 1;

	hContact = db_mc_tryMeta(hContact);

	HWND hwnd = Srmm_FindWindow(hContact);
	HWND hwndContainer;
	if (hwnd) {
		if (pwszInitialText) {
			SendDlgItemMessage(hwnd, IDC_SRMM_MESSAGE, EM_SETSEL, -1, SendDlgItemMessage(hwnd, IDC_SRMM_MESSAGE, WM_GETTEXTLENGTH, 0, 0));
			SendDlgItemMessageW(hwnd, IDC_SRMM_MESSAGE, EM_REPLACESEL, FALSE, (LPARAM)pwszInitialText);
			mir_free(pwszInitialText);
		}
		
		hwndContainer = GetParent(hwnd);
		if (!g_Settings.bTabsEnable) {
			SetWindowPos(hwndContainer, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
			SetForegroundWindow(hwndContainer);
		}
		else {
			CSrmmBaseDialog *pDlg = (CSrmmBaseDialog*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			g_pTabDialog->m_tab.ActivatePage(g_pTabDialog->m_tab.GetDlgIndex(pDlg));
		}
	}
	else {
		auto *pDlg = GetContainer()->AddPage(hContact, pwszInitialText, false);
		hwndContainer = pDlg->getOwner()->GetHwnd();
	}

	ShowWindow(hwndContainer, SW_RESTORE);
	return 0;
}

static INT_PTR SendMessageCommand_W(WPARAM wParam, LPARAM lParam)
{
	return SendMessageCmd(wParam, mir_wstrdup((wchar_t*)lParam));
}

static INT_PTR SendMessageCommand(WPARAM wParam, LPARAM lParam)
{
	return SendMessageCmd(wParam, mir_a2u((char*)lParam));
}

static INT_PTR ReadMessageCommand(WPARAM, LPARAM lParam)
{
	CLISTEVENT *cle = (CLISTEVENT*)lParam;
	if (cle)
		SendMessageCmd(cle->hContact, nullptr);

	return 0;
}

static int TypingMessage(WPARAM hContact, LPARAM lParam)
{
	if (!g_plugin.bShowTyping)
		return 0;

	hContact = db_mc_tryMeta(hContact);

	Skin_PlaySound((lParam) ? "TNStart" : "TNStop");

	auto *pDlg = Srmm_FindDialog(hContact);
	if (pDlg)
		pDlg->UserTyping(lParam);
	else if (lParam && g_plugin.bShowTypingTray) {
		wchar_t szTip[256];
		mir_snwprintf(szTip, TranslateT("%s is typing a message"), Clist_GetContactDisplayName(hContact));

		if (g_plugin.bShowTypingClist) {
			Clist_RemoveEvent(hContact, 1);

			CLISTEVENT cle = {};
			cle.hContact = hContact;
			cle.hDbEvent = 1;
			cle.flags = CLEF_ONLYAFEW | CLEF_UNICODE;
			cle.hIcon = Skin_LoadIcon(SKINICON_OTHER_TYPING);
			cle.pszService = MS_MSG_READMESSAGE;
			cle.szTooltip.w = szTip;
			g_clistApi.pfnAddEvent(&cle);

			IcoLib_ReleaseIcon(cle.hIcon);
		}
		else Clist_TrayNotifyW(nullptr, TranslateT("Typing notification"), szTip, NIIF_INFO, 1000 * 4);
	}
	return 0;
}

static int MessageSettingChanged(WPARAM hContact, LPARAM lParam)
{
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING*)lParam;
	if (cws->szModule == nullptr)
		return 0;

	if (!strcmp(cws->szModule, "CList"))
		Srmm_Broadcast(DM_UPDATETITLE, (WPARAM)cws, hContact);
	else if (hContact) {
		if (cws->szSetting && !strcmp(cws->szSetting, "Timezone"))
			Srmm_Broadcast(DM_NEWTIMEZONE, (WPARAM)cws, 0);
		else {
			char *szProto = Proto_GetBaseAccountName(hContact);
			if (szProto && !strcmp(cws->szModule, szProto))
				Srmm_Broadcast(DM_UPDATETITLE, (WPARAM)cws, hContact);
		}
	}
	return 0;
}

// If a contact gets deleted, close its message window if there is any
static int ContactDeleted(WPARAM hContact, LPARAM)
{
	if (auto *pDlg = Srmm_FindDialog(hContact))
		pDlg->CloseTab();

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
			bool autoPopup = false;

			DB::EventInfo dbei(hDbEvent, false);
			if (!dbei)
				continue;

			if (!dbei.markedRead() && DbEventIsShown(dbei)) {
				int windowAlreadyExists = Srmm_FindWindow(hContact) != nullptr;
				if (windowAlreadyExists)
					continue;

				char *szProto = Proto_GetBaseAccountName(hContact);
				if (szProto == nullptr)
					continue;

				if (g_plugin.popupFlags & SRMMStatusToPf2(Proto_GetStatus(szProto)))
					autoPopup = true;

				if (autoPopup && !windowAlreadyExists)
					GetContainer()->AddPage(hContact);
				else
					events.insert(new MSavedEvent(hContact, hDbEvent));
			}
		}
	}

	for (auto &e : events)
		Srmm_AddEvent(e->hContact, e->hEvent);
}

void RegisterSRMMFonts(void);

/////////////////////////////////////////////////////////////////////////////////////////
// toolbar buttons support

int RegisterToolbarIcons(WPARAM, LPARAM)
{
	BBButton bbd = {};
	bbd.pszModuleName = "SRMM";
	bbd.bbbFlags = BBBF_ISIMBUTTON | BBBF_CREATEBYID;

	bbd.dwButtonID = IDC_ADD;
	bbd.dwDefPos = 10;
	bbd.hIcon = Skin_GetIconHandle(SKINICON_OTHER_ADDCONTACT);
	bbd.pwszText = LPGENW("&Add");
	bbd.pwszTooltip = LPGENW("Add contact permanently to list");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_USERMENU;
	bbd.dwDefPos = 20;
	bbd.hIcon = Skin_GetIconHandle(SKINICON_OTHER_DOWNARROW);
	bbd.pwszText = LPGENW("&User menu");
	bbd.pwszTooltip = LPGENW("User menu");
	g_plugin.addButton(&bbd);

	bbd.dwButtonID = IDC_DETAILS;
	bbd.dwDefPos = 30;
	bbd.hIcon = Skin_GetIconHandle(SKINICON_OTHER_USERDETAILS);
	bbd.pwszText = LPGENW("User &details");
	bbd.pwszTooltip = LPGENW("View user's details");
	g_plugin.addButton(&bbd);

	bbd.bbbFlags |= BBBF_ISCHATBUTTON | BBBF_ISRSIDEBUTTON;
	bbd.dwButtonID = IDC_SRMM_HISTORY;
	bbd.dwDefPos = 40;
	bbd.hIcon = Skin_GetIconHandle(SKINICON_OTHER_HISTORY);
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

void CMsgDialog::SetButtonsPos()
{
	HDWP hdwp = BeginDeferWindowPos(Srmm_GetButtonCount());

	RECT rc;
	GetWindowRect(GetDlgItem(m_hwnd, IDC_SPLITTERY), &rc);
	POINT pt = { 0, rc.top };
	ScreenToClient(m_hwnd, &pt);
	int yPos = pt.y;

	GetClientRect(m_hwnd, &rc);
	int iLeftX = 2, iRightX = rc.right - 2, iGap = Srmm_GetButtonGap();

	CustomButtonData *cbd;
	for (int i = 0; cbd = Srmm_GetNthButton(i); i++) {
		HWND hwndButton = GetDlgItem(m_hwnd, cbd->m_dwButtonCID);
		if (hwndButton == nullptr || cbd->m_bHidden)
			continue;

		if (cbd->m_bRSided) {
			iRightX -= iGap + cbd->m_iButtonWidth;
			hdwp = DeferWindowPos(hdwp, hwndButton, nullptr, iRightX, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		}
		else {
			hdwp = DeferWindowPos(hdwp, hwndButton, nullptr, iLeftX, yPos, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
			iLeftX += iGap + cbd->m_iButtonWidth;
		}
	}

	EndDeferWindowPos(hdwp);
}

/////////////////////////////////////////////////////////////////////////////////////////

static int FontsChanged(WPARAM, LPARAM)
{
	Srmm_ApplyOptions();
	return 0;
}

static int SplitmsgModulesLoaded(WPARAM, LPARAM)
{
	RegisterSRMMFonts();
	LoadMsgLogIcons();
	OnCheckPlugins(0, 0);

	CMenuItem mi(&g_plugin);
	SET_UID(mi, 0x58d8dc1, 0x1c25, 0x49c0, 0xb8, 0x7c, 0xa3, 0x22, 0x2b, 0x3d, 0xf1, 0xd8);
	mi.position = -2000090000;
	mi.flags = CMIF_DEFAULT;
	mi.hIcolibItem = Skin_GetIconHandle(SKINICON_EVENT_MESSAGE);
	mi.name.a = LPGEN("&Message");
	mi.pszService = MS_MSG_SENDMESSAGE;
	hMsgMenuItem = Menu_AddContactMenuItem(&mi);

	HookEvent(ME_FONT_RELOAD, FontsChanged);
	
	HookTemporaryEvent(ME_MSG_TOOLBARLOADED, RegisterToolbarIcons);

	RestoreUnreadMessageAlerts();
	return 0;
}

static int Preshutdown(WPARAM, LPARAM)
{
	for (auto &it : g_arDialogs.rev_iter())
		it->CloseTab();
	return 0;
}

static int IconsChanged(WPARAM, LPARAM)
{
	FreeMsgLogIcons();
	LoadMsgLogIcons();

	// change all the icons
	for (auto &it : g_arDialogs) {
		it->RemakeLog();
		it->FixTabIcons();
	}
	return 0;
}

static int PrebuildContactMenu(WPARAM hContact, LPARAM)
{
	if (hContact) {
		bool bEnabled = false;
		char *szProto = Proto_GetBaseAccountName(hContact);
		if (szProto) {
			// leave this menu item hidden for chats
			if (!Contact::IsGroupChat(hContact, szProto))
				if (CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_1, 0) & PF1_IMSEND)
					bEnabled = true;
		}

		Menu_ShowItem(hMsgMenuItem, bEnabled);
	}
	return 0;
}

static wchar_t tszError[] = LPGENW("Miranda could not load the built-in message module, msftedit.dll is missing. Press 'Yes' to continue loading Miranda.");

int LoadSendRecvMessageModule(void)
{
	if ((hMsftEdit = LoadLibrary(L"Msftedit.dll")) == nullptr) {
		if (IDYES != MessageBox(nullptr, TranslateW(tszError), TranslateT("Information"), MB_YESNO | MB_ICONINFORMATION))
			return 1;
		return 0;
	}

	InitGlobals();

	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, PrebuildContactMenu);
	HookEvent(ME_DB_EVENT_ADDED, MessageEventAdded);
	HookEvent(ME_DB_EVENT_EDITED, MessageEventAdded);
	HookEvent(ME_DB_CONTACT_SETTINGCHANGED, MessageSettingChanged);
	HookEvent(ME_DB_CONTACT_DELETED, ContactDeleted);
	HookEvent(ME_OPT_INITIALISE, OptInitialise);
	HookEvent(ME_PROTO_CONTACTISTYPING, TypingMessage);
	HookEvent(ME_SKIN_ICONSCHANGED, IconsChanged);
	HookEvent(ME_SYSTEM_MODULESLOADED, SplitmsgModulesLoaded);
	HookEvent(ME_SYSTEM_PRESHUTDOWN, Preshutdown);

	CreateServiceFunction(MS_MSG_SENDMESSAGE, SendMessageCommand);
	CreateServiceFunction(MS_MSG_SENDMESSAGEW, SendMessageCommand_W);
	CreateServiceFunction(MS_MSG_READMESSAGE, ReadMessageCommand);

	g_plugin.addSound("RecvMsgActive",   LPGENW("Instant messages"), LPGENW("Incoming (focused window)"));
	g_plugin.addSound("RecvMsgInactive", LPGENW("Instant messages"), LPGENW("Incoming (unfocused window)"));
	g_plugin.addSound("AlertMsg",        LPGENW("Instant messages"), LPGENW("Incoming (new session)"));
	g_plugin.addSound("SendMsg",         LPGENW("Instant messages"), LPGENW("Outgoing"));
	g_plugin.addSound("SendError",       LPGENW("Instant messages"), LPGENW("Message send error"));
	g_plugin.addSound("TNStart",         LPGENW("Instant messages"), LPGENW("Contact started typing"));
	g_plugin.addSound("TNStop",          LPGENW("Instant messages"), LPGENW("Contact stopped typing"));

	InitStatusIcons();
	return 0;
}

void SplitmsgShutdown(void)
{
	FreeMsgLogIcons();
	FreeLibrary(hMsftEdit);
	msgQueue_destroy();
}
