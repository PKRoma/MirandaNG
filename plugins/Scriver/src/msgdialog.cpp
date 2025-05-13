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

/////////////////////////////////////////////////////////////////////////////////////////

static INT_PTR CALLBACK ConfirmSendAllDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM)
{
	switch (msg) {
	case WM_INITDIALOG:
		RECT rcParent, rcChild;
		TranslateDialogDefault(hwndDlg);
		GetWindowRect(GetParent(hwndDlg), &rcParent);
		GetWindowRect(hwndDlg, &rcChild);
		rcChild.bottom -= rcChild.top;
		rcChild.right -= rcChild.left;
		rcParent.bottom -= rcParent.top;
		rcParent.right -= rcParent.left;
		rcChild.left = rcParent.left + (rcParent.right - rcChild.right) / 2;
		rcChild.top = rcParent.top + (rcParent.bottom - rcChild.bottom) / 2;
		MoveWindow(hwndDlg, rcChild.left, rcChild.top, rcChild.right, rcChild.bottom, FALSE);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDYES:
		case IDNO:
		case IDCANCEL:
			int result = LOWORD(wParam);
			if (IsDlgButtonChecked(hwndDlg, IDC_REMEMBER))
				result |= 0x10000;
			EndDialog(hwndDlg, result);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////////////////

CMsgDialog::CMsgDialog(MCONTACT hContact, bool bIncoming) :
	CSuper(g_plugin, IDD_MSG, hContact),
	m_bIncoming(bIncoming),
	m_splitterX(this, IDC_SPLITTERX),
	m_splitterY(this, IDC_SPLITTERY),

	m_btnAdd(this, IDC_ADD),
	m_btnQuote(this, IDC_QUOTE),
	m_btnDetails(this, IDC_DETAILS),
	m_btnUserMenu(this, IDC_USERMENU)
{
	m_btnQuote.OnClick = Callback(this, &CMsgDialog::onClick_Quote);

	if (isChat()) {
		m_btnNickList.OnClick = Callback(this, &CMsgDialog::onClick_ShowList);

		m_splitterX.OnChange = Callback(this, &CMsgDialog::onChange_SplitterX);
	}
	else {
		m_btnAdd.OnClick = Callback(this, &CMsgDialog::onClick_Add);
		m_btnDetails.OnClick = Callback(this, &CMsgDialog::onClick_Details);
		m_btnUserMenu.OnClick = Callback(this, &CMsgDialog::onClick_UserMenu);
	}

	m_autoClose = CLOSE_ON_CANCEL;
	m_szProto = Proto_GetBaseAccountName(m_hContact);

	SetParent(GetParentWindow(m_hContact, isChat()));
	m_pParent = (ParentWindowData *)GetWindowLongPtr(m_hwndParent, GWLP_USERDATA);

	m_btnOk.OnClick = Callback(this, &CMsgDialog::onClick_Ok);

	timerType.OnEvent = Callback(this, &CMsgDialog::onType);

	m_message.OnChange = Callback(this, &CMsgDialog::onChange_Message);
	m_splitterY.OnChange = Callback(this, &CMsgDialog::onChange_SplitterY);
}

bool CMsgDialog::OnInitDialog()
{
	CSuper::OnInitDialog();

	NotifyEvent(MSG_WINDOW_EVT_OPENING);

	if (m_hContact && m_szProto != nullptr)
		m_wStatus = db_get_w(m_hContact, m_szProto, "Status", ID_STATUS_OFFLINE);
	else
		m_wStatus = ID_STATUS_OFFLINE;

	m_nTypeMode = PROTOTYPE_SELFTYPING_OFF;
	timerType.Start(1000);

	m_bUseRtl = g_plugin.getByte(m_hContact, "UseRTL", 0) != 0;

	PARAFORMAT2 pf2;
	memset(&pf2, 0, sizeof(pf2));
	pf2.cbSize = sizeof(pf2);
	pf2.dwMask = PFM_RTLPARA;
	if (!m_bUseRtl) {
		pf2.wEffects = 0;
		SetWindowLongPtr(m_message.GetHwnd(), GWL_EXSTYLE, GetWindowLongPtr(m_message.GetHwnd(), GWL_EXSTYLE) & ~(WS_EX_RIGHT | WS_EX_RTLREADING | WS_EX_LEFTSCROLLBAR));
	}
	else {
		pf2.wEffects = PFE_RTLPARA;
		SetWindowLongPtr(m_message.GetHwnd(), GWL_EXSTYLE, GetWindowLongPtr(m_message.GetHwnd(), GWL_EXSTYLE) | WS_EX_RIGHT | WS_EX_RTLREADING | WS_EX_LEFTSCROLLBAR);
	}
	m_message.SendMsg(EM_SETPARAFORMAT, 0, (LPARAM)&pf2);
	
	RECT rc;
	GetWindowRect(m_message.GetHwnd(), &rc);
	m_minLogBoxHeight = m_minEditBoxHeight = rc.bottom - rc.top;
	if (m_minEditBoxHeight < g_dat.minInputAreaHeight)
		m_minEditBoxHeight = g_dat.minInputAreaHeight;
	if (m_pParent->iSplitterY == -1)
		m_pParent->iSplitterY = m_minEditBoxHeight;
	if (m_pParent->iSplitterX == -1) {
		GetWindowRect(m_nickList.GetHwnd(), &rc);
		m_pParent->iSplitterX = rc.right - rc.left;
	}

	SetMinSize(BOTTOM_RIGHT_AVATAR_HEIGHT, m_minLogBoxHeight + TOOLBAR_HEIGHT + m_minEditBoxHeight + max(INFO_BAR_HEIGHT, BOTTOM_RIGHT_AVATAR_HEIGHT - TOOLBAR_HEIGHT) + 5);

	if (m_wszInitialText) {
		m_message.SetText(m_wszInitialText);
		mir_free(m_wszInitialText);
	}
	else if (g_dat.flags.bSaveDrafts) {
		int len = 0;
		ptrW ptszSavedMsg(db_get_wsa(m_hContact, "SRMM", "SavedMsg"));
		if (ptszSavedMsg)
			len = m_message.SetRichText(ptszSavedMsg);
		PostMessage(m_message.GetHwnd(), EM_SETSEL, len, len);
	}

	SendMessage(m_hwnd, DM_CHANGEICONS, 0, 0);

	m_message.SendMsg(EM_SETLANGOPTIONS, 0, (LPARAM)m_message.SendMsg(EM_GETLANGOPTIONS, 0, 0) & ~IMF_AUTOKEYBOARD);
	m_message.SendMsg(EM_SETEVENTMASK, 0, ENM_MOUSEEVENTS | ENM_KEYEVENTS | ENM_CHANGE | ENM_REQUESTRESIZE);
	if (m_hContact && m_szProto) {
		int nMax = CallProtoService(m_szProto, PS_GETCAPS, PFLAG_MAXLENOFMESSAGE, m_hContact);
		if (nMax)
			m_message.SendMsg(EM_EXLIMITTEXT, 0, nMax);
	}

	m_pParent->AddChild(this);

	CreateInfobar();
	OnOptionsApplied();

	if (isChat()) {
		UpdateStatusBar();
		UpdateTitle();
		UpdateNickList();
		UpdateChatLog();
	}
	else {
		m_nickList.Hide();
		m_splitterX.Hide();

		bool notifyUnread = GetFirstEvent();

		DB::ECPTR pCursor(DB::EventsRev(m_hContact));
		while (MEVENT hdbEvent = pCursor.FetchNext()) {
			DBEVENTINFO dbei = {};
			db_event_get(hdbEvent, &dbei);
			if (dbei.eventType == EVENTTYPE_MESSAGE && !dbei.bSent) {
				m_lastMessage = dbei.getUnixtime();
				break;
			}
		}

		if (notifyUnread) {
			if (GetForegroundWindow() != m_hwndParent || m_pParent->m_hwndActive != m_hwnd) {
				m_iShowUnread = 1;
				UpdateIcon();
				SetTimer(m_hwnd, TIMERID_UNREAD, TIMEOUT_UNREAD, nullptr);
			}
			StartFlashing();
		}

		m_iMessagesInProgress = SendQueue::ReattachItems(this, m_hContact);
		if (m_iMessagesInProgress > 0)
			ShowMessageSending();
	}
	PopupWindow(m_bIncoming);

	NotifyEvent(MSG_WINDOW_EVT_OPEN);
	return true;
}

void CMsgDialog::OnDestroy()
{
	NotifyEvent(MSG_WINDOW_EVT_CLOSING);

	if (m_nTypeMode == PROTOTYPE_SELFTYPING_ON)
		NotifyTyping(PROTOTYPE_SELFTYPING_OFF);

	if (m_hXStatusTip != nullptr) {
		DestroyWindow(m_hXStatusTip);
		m_hXStatusTip = nullptr;
	}

	IcoLib_ReleaseIcon(m_hStatusIcon); m_hStatusIcon = nullptr;
	IcoLib_ReleaseIcon(m_hStatusIconBig); m_hStatusIconBig = nullptr;
	if (m_hStatusIconOverlay != nullptr) {
		DestroyIcon(m_hStatusIconOverlay);
		m_hStatusIconOverlay = nullptr;
	}	

	SendQueue::ReleaseItems(this);
	if (g_dat.flags.bSaveDrafts) {
		ptrA szText(m_message.GetRichTextRtf(true));
		if (szText)
			db_set_utf(m_hContact, "SRMM", "SavedMsg", szText);
		else
			db_unset(m_hContact, "SRMM", "SavedMsg");
	}

	tcmdlist_free(cmdList);

	HFONT hFont = (HFONT)m_message.SendMsg(WM_GETFONT, 0, 0);
	if (hFont != nullptr && hFont != (HFONT)m_btnOk.SendMsg(WM_GETFONT, 0, 0))
		DeleteObject(hFont);

	g_plugin.setByte(m_hContact, "UseRTL", m_bUseRtl);
	if (m_hContact && g_dat.flags.bDelTemp) {
		m_hContact = INVALID_CONTACT_ID; // to prevent recursion

		if (!Contact::OnList(m_hContact))
			db_delete_contact(m_hContact);
	}

	if (m_si)
		m_si->pDlg = nullptr;

	m_pParent->RemoveChild(m_hwnd);

	NotifyEvent(MSG_WINDOW_EVT_CLOSE);

	mir_free(m_wszSearchQuery);
	mir_free(m_wszSearchResult);

	CSuper::OnDestroy();
}

/////////////////////////////////////////////////////////////////////////////////////////

void CMsgDialog::onClick_Ok(CCtrlButton *pButton)
{
	if (!pButton->Enabled() || m_hContact == 0)
		return;

	ptrA streamOut(m_message.GetRichTextRtf(!m_bSendFormat));
	if (streamOut == nullptr)
		return;

	CMStringW wszText(ptrW(mir_utf8decodeW(streamOut)));
	if (wszText.IsEmpty())
		return;

	if (m_bSendFormat)
		DoRtfToTags(wszText);
	wszText.TrimRight();

	if (m_nTypeMode == PROTOTYPE_SELFTYPING_ON)
		NotifyTyping(PROTOTYPE_SELFTYPING_OFF);

	if (isChat()) {
		if (m_si->pMI->bAckMsg) {
			EnableWindow(m_message.GetHwnd(), FALSE);
			m_message.SendMsg(EM_SETREADONLY, TRUE, 0);
		}
		else m_message.SetText(L"");

		Chat_DoEventHook(m_si, GC_USER_MESSAGE, nullptr, wszText, 0);
	}
	else {
		PARAFORMAT2 pf2;
		memset(&pf2, 0, sizeof(pf2));
		pf2.cbSize = sizeof(pf2);
		pf2.dwMask = PFM_RTLPARA;
		m_message.SendMsg(EM_GETPARAFORMAT, 0, (LPARAM)&pf2);

		SendQueue::Item msi = {};
		if (pf2.wEffects & PFE_RTLPARA)
			msi.flags |= DBEF_RTL;

		msi.sendBuffer = mir_utf8encodeW(wszText);
		msi.sendBufferSize = (int)mir_strlen(msi.sendBuffer);

		if (Utils_IsRtl(wszText))
			msi.flags |= DBEF_RTL;

		// Store messaging history
		TCmdList *cmdListNew = tcmdlist_last(cmdList);
		while (cmdListNew != nullptr && cmdListNew->temporary) {
			cmdList = tcmdlist_remove(cmdList, cmdListNew);
			cmdListNew = tcmdlist_last(cmdList);
		}
		if (msi.sendBuffer != nullptr)
			cmdList = tcmdlist_append(cmdList, mir_strdup(rtrim(msi.sendBuffer)), 20, FALSE);

		cmdListCurrent = nullptr;

		if (pButton == nullptr)
			m_pParent->MessageSend(msi);
		else
			MessageSend(msi);
	}

	m_message.SetText(L"");
	m_btnOk.Disable();
	if (g_plugin.bAutoMin)
		ShowWindow(m_hwndParent, SW_MINIMIZE);

	m_btnCloseQuote.Click();
}

void CMsgDialog::onClick_UserMenu(CCtrlButton *pButton)
{
	if (GetKeyState(VK_SHIFT) & 0x8000) { // copy user name
		ptrW id(Contact::GetInfo(CNF_UNIQUEID, m_hContact, m_szProto));
		Utils_ClipboardCopy(MClipUnicode(id));
	}
	else {
		RECT rc;
		HMENU hMenu = Menu_BuildContactMenu(m_hContact);
		GetWindowRect(pButton->GetHwnd(), &rc);
		TrackPopupMenu(hMenu, 0, rc.left, rc.bottom, 0, m_hwnd, nullptr);
		DestroyMenu(hMenu);
	}
}

void CMsgDialog::onClick_Quote(CCtrlButton*)
{
	MEVENT hDbEventLast = db_event_last(m_hContact);
	if (!hDbEventLast)
		return;

	wchar_t *buffer = m_pLog->GetSelectedText();
	if (buffer != nullptr) {
		SetMessageText(Srmm_Quote(buffer));
		mir_free(buffer);
	}
	else {
		DB::EventInfo dbei(hDbEventLast);
		if (!dbei)
			return;

		if (dbei.isSrmm()) {
			ptrW wszText(dbei.getText());
			SetMessageText(Srmm_Quote(wszText));
		}
	}
	SetFocus(m_message.GetHwnd());
}

void CMsgDialog::onClick_Add(CCtrlButton*)
{
	Contact::Add(m_hContact, m_hwnd);

	if (Contact::OnList(m_hContact))
		ShowWindow(GetDlgItem(m_hwnd, IDC_ADD), SW_HIDE);
}

void CMsgDialog::onClick_Details(CCtrlButton*)
{
	CallService(MS_USERINFO_SHOWDIALOG, m_hContact, 0);
}

void CMsgDialog::onChange_Message(CCtrlEdit*)
{
	cmdListCurrent = nullptr;

	int len = m_message.GetRichTextLength();
	UpdateReadChars();
	EnableWindow(GetDlgItem(m_hwnd, IDOK), len != 0);

	if (!(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_SHIFT) & 0x8000)) {
		m_nLastTyping = GetTickCount();
		if (len != 0) {
			if (m_nTypeMode == PROTOTYPE_SELFTYPING_OFF)
				NotifyTyping(PROTOTYPE_SELFTYPING_ON);
		}
		else if (m_nTypeMode == PROTOTYPE_SELFTYPING_ON)
			NotifyTyping(PROTOTYPE_SELFTYPING_OFF);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void CMsgDialog::onClick_ShowList(CCtrlButton *pButton)
{
	if (!pButton->Enabled() || m_si->iType == GCW_SERVER)
		return;

	m_bNicklistEnabled = !m_bNicklistEnabled;
	UpdateFilterButton();
	m_pLog->ScrollToBottom();
	Resize();
}

/////////////////////////////////////////////////////////////////////////////////////////

void CMsgDialog::onChange_SplitterX(CSplitter *pSplitter)
{
	RECT rc;
	GetClientRect(m_hwnd, &rc);

	m_pParent->iSplitterX = rc.right - pSplitter->GetPos() + 1;
	if (m_pParent->iSplitterX < 35)
		m_pParent->iSplitterX = 35;
	if (m_pParent->iSplitterX > rc.right - rc.left - 35)
		m_pParent->iSplitterX = rc.right - rc.left - 35;
}

void CMsgDialog::onChange_SplitterY(CSplitter *pSplitter)
{
	RECT rc;
	GetClientRect(m_hwnd, &rc);
	m_pParent->iSplitterY = rc.bottom - pSplitter->GetPos();
}

/////////////////////////////////////////////////////////////////////////////////////////

void CMsgDialog::onType(CTimer *)
{
	if (m_nTypeMode == PROTOTYPE_SELFTYPING_ON && GetTickCount() - m_nLastTyping > TIMEOUT_TYPEOFF)
		NotifyTyping(PROTOTYPE_SELFTYPING_OFF);

	if (m_bShowTyping) {
		if (m_nTypeSecs)
			m_nTypeSecs--;
		else {
			m_bShowTyping = false;
			UpdateStatusBar();
			UpdateIcon();
		}
	}
	else {
		if (m_nTypeSecs) {
			m_bShowTyping = true;
			UpdateStatusBar();
			UpdateIcon();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

void CMsgDialog::MessageDialogResize(int w, int h)
{
	ParentWindowData *pdat = m_pParent;
	HWND hwndLog = GetDlgItem(m_hwnd, IDC_SRMM_LOG);
	bool bToolbar = pdat->flags2.bShowToolBar;
	int toolbarHeight = (bToolbar) ? TOOLBAR_HEIGHT : 0;
	int logY, logH;
	int avatarWidth = 0, avatarHeight = 0;
	int toolbarWidth = w;
	int messageEditWidth = w - 2;
	int hSplitterPos = (m_bReadOnly) ? 0 : pdat->iSplitterY;

	int infobarInnerHeight = INFO_BAR_INNER_HEIGHT;
	int infobarHeight = INFO_BAR_HEIGHT;

	if (!pdat->flags2.bShowInfoBar || m_si) {
		infobarHeight = 0;
		infobarInnerHeight = 0;
	}

	int hSplitterMinTop = toolbarHeight + m_minLogBoxHeight, hSplitterMinBottom = m_minEditBoxHeight;
	if (hSplitterMinBottom < g_dat.minInputAreaHeight)
		hSplitterMinBottom = g_dat.minInputAreaHeight;

	if (hSplitterPos > (h - toolbarHeight - infobarHeight + SPLITTER_HEIGHT + 1) / 2)
		hSplitterPos = (h - toolbarHeight - infobarHeight + SPLITTER_HEIGHT + 1) / 2;

	if (h - hSplitterPos - infobarHeight < hSplitterMinTop)
		hSplitterPos = h - hSplitterMinTop - infobarHeight;

	hSplitterMinBottom -= toolbarHeight - 2;
	if (hSplitterPos < hSplitterMinBottom)
		hSplitterPos = hSplitterMinBottom;

	if (!pdat->flags2.bShowInfoBar) {
		if (m_hbmpAvatarPic && g_dat.flags.bShowAvatar) {
			avatarWidth = BOTTOM_RIGHT_AVATAR_HEIGHT;
			avatarHeight = toolbarHeight + hSplitterPos - 2;
			if (avatarHeight < BOTTOM_RIGHT_AVATAR_HEIGHT) {
				avatarHeight = BOTTOM_RIGHT_AVATAR_HEIGHT;
				hSplitterPos = avatarHeight - toolbarHeight + 2;
			}
			else avatarHeight = BOTTOM_RIGHT_AVATAR_HEIGHT;

			avatarWidth = avatarHeight;
			if (avatarWidth > BOTTOM_RIGHT_AVATAR_HEIGHT && avatarWidth > w / 4)
				avatarWidth = w / 4;

			if ((toolbarWidth - avatarWidth - 2) < 0)
				avatarWidth = toolbarWidth - 2;

			toolbarWidth -= avatarWidth + 2;
			messageEditWidth -= avatarWidth + 1;
		}
	}

	if (m_bReadOnly)
		hSplitterPos = 0;
	else
		pdat->iSplitterY = hSplitterPos;

	HDWP hdwp;
	if (isChat()) {
		bool bNick = m_si->iType != GCW_SERVER && m_bNicklistEnabled;

		m_splitterX.Show(bNick);
		if (m_si->iType != GCW_SERVER)
			m_nickList.Show(m_bNicklistEnabled);
		else
			m_nickList.Hide();

		if (m_si->iType == GCW_SERVER) {
			m_btnNickList.Disable();
			m_btnFilter.Disable();
			m_btnChannelMgr.Disable();
		}
		else {
			m_btnNickList.Enable();
			m_btnFilter.Enable();
			if (m_si->iType == GCW_CHATROOM)
				m_btnChannelMgr.Enable(m_si->pMI->bChanMgr);
		}
		
		logY = 0;
		logH = h - hSplitterPos - toolbarHeight - infobarInnerHeight - SPLITTER_HEIGHT;

		hdwp = BeginDeferWindowPos(8);
		hdwp = DeferWindowPos(hdwp, hwndLog, nullptr, 1, 0, bNick ? w - pdat->iSplitterX - 1 : w - 2, logH, SWP_NOZORDER);
		hdwp = DeferWindowPos(hdwp, m_nickList.GetHwnd(), nullptr, w - pdat->iSplitterX + 2, 0, pdat->iSplitterX - 3, logH, SWP_NOZORDER);
		hdwp = DeferWindowPos(hdwp, m_splitterX.GetHwnd(), nullptr, w - pdat->iSplitterX, 1, 2, logH, SWP_NOZORDER);

		RedrawWindow(m_nickList.GetHwnd(), nullptr, nullptr, RDW_INVALIDATE);
	}
	else {
		logY = infobarInnerHeight;
		logH = h - hSplitterPos - toolbarHeight - infobarInnerHeight - SPLITTER_HEIGHT;

		hdwp = BeginDeferWindowPos(7);
		hdwp = DeferWindowPos(hdwp, m_hwndInfo, nullptr, 1, 0, w - 2, infobarInnerHeight - 2, SWP_NOZORDER);
		hdwp = DeferWindowPos(hdwp, hwndLog, nullptr, 1, logY, w - 2, logH, SWP_NOZORDER);

		RefreshInfobar();
	}

	int yTop = h - hSplitterPos - 1;
	if (m_hQuoteEvent) {
		hdwp = DeferWindowPos(hdwp, m_message.GetHwnd(), nullptr, 1, yTop + 22, messageEditWidth, hSplitterPos + 22, SWP_NOZORDER);
		hdwp = DeferWindowPos(hdwp, m_Quote.GetHwnd(), nullptr, 1, yTop, messageEditWidth - 22, 22, SWP_NOZORDER);
		hdwp = DeferWindowPos(hdwp, m_btnCloseQuote.GetHwnd(), nullptr, messageEditWidth - 21, yTop, 22, 22, SWP_NOZORDER);
	}
	else hdwp = DeferWindowPos(hdwp, m_message.GetHwnd(), nullptr, 1, yTop, messageEditWidth, hSplitterPos, SWP_NOZORDER);

	hdwp = DeferWindowPos(hdwp, GetDlgItem(m_hwnd, IDC_AVATAR), nullptr, w - avatarWidth - 1, h - (avatarHeight + avatarWidth) / 2 - 1, avatarWidth, avatarWidth, SWP_NOZORDER);
	hdwp = DeferWindowPos(hdwp, m_splitterY.GetHwnd(), nullptr, 0, h - hSplitterPos - SPLITTER_HEIGHT - 1, toolbarWidth, SPLITTER_HEIGHT, SWP_NOZORDER);
	EndDeferWindowPos(hdwp);

	m_pLog->Resize();

	RedrawWindow(GetDlgItem(m_hwnd, IDC_AVATAR), nullptr, nullptr, RDW_INVALIDATE);

	SetButtonsPos(m_hwnd, m_hContact, bToolbar);

	RedrawWindow(m_message.GetHwnd(), nullptr, nullptr, RDW_INVALIDATE);
}

/////////////////////////////////////////////////////////////////////////////////////////

LRESULT CMsgDialog::WndProc_Message(UINT msg, WPARAM wParam, LPARAM lParam)
{
	int result = InputAreaShortcuts(m_message.GetHwnd(), msg, wParam, lParam);
	if (result != -1)
		return result;

	switch (msg) {
	case EM_ACTIVATE:
		SetActiveWindow(m_hwnd);
		break;

	case WM_SYSCHAR:
		if ((wParam == 's' || wParam == 'S') && (GetKeyState(VK_MENU) & 0x8000)) {
			PostMessage(m_hwnd, WM_COMMAND, IDOK, 0);
			return 0;
		}
		break;

	case WM_CONTEXTMENU:
		InputAreaContextMenu(m_message.GetHwnd(), wParam, lParam, m_hContact);
		return TRUE;

	case WM_KEYDOWN:
		bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
		bool isAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;

		if (isChat()) {
			if (wParam == VK_TAB && isShift && !isCtrl) { // SHIFT-TAB (go to nick list)
				SetFocus(m_nickList.GetHwnd());
				return TRUE;
			}

			if (wParam == VK_TAB && !isCtrl && !isShift) { // tab-autocomplete
				m_message.SetDraw(false);
				TabAutoComplete();
				m_message.SetDraw(true);
				RedrawWindow(m_nickList.GetHwnd(), nullptr, nullptr, RDW_INVALIDATE);
				return 0;
			}

			if (wParam != VK_RIGHT && wParam != VK_LEFT) {
				replaceStrW(m_wszSearchQuery, nullptr);
				replaceStrW(m_wszSearchResult, nullptr);
			}
		}

		if (ProcessHotkeys(wParam, isShift, isCtrl, isAlt))
			return TRUE;
		break;
	}
	return CSuper::WndProc_Message(msg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////

LRESULT CMsgDialog::WndProc_Nicklist(UINT msg, WPARAM wParam, LPARAM lParam)
{
	int result = InputAreaShortcuts(m_nickList.GetHwnd(), msg, wParam, lParam);
	if (result != -1)
		return result;

	switch (msg) {
	case WM_GETDLGCODE:
		{
			BOOL isAlt = GetKeyState(VK_MENU) & 0x8000;
			BOOL isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) && !isAlt;

			LPMSG lpmsg;
			if ((lpmsg = (LPMSG)lParam) != nullptr) {
				if (lpmsg->message == WM_KEYDOWN
					&& (lpmsg->wParam == VK_RETURN || lpmsg->wParam == VK_ESCAPE || (lpmsg->wParam == VK_TAB && (isAlt || isCtrl))))
					return DLGC_WANTALLKEYS;
			}
		}
		break;
	}

	return CSuper::WndProc_Nicklist(msg, wParam, lParam);
}

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CMsgDialog::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	LPNMHDR pNmhdr;

	switch (msg) {
	case DM_GETCONTEXTMENU:
		{
			HMENU hMenu = Menu_BuildContactMenu(m_hContact);
			SetWindowLongPtr(m_hwnd, DWLP_MSGRESULT, (LONG_PTR)hMenu);
		}
		return TRUE;

	case WM_CONTEXTMENU:
		if (m_hwndParent == (HWND)wParam) {
			POINT pt;
			HMENU hMenu = Menu_BuildContactMenu(m_hContact);
			GetCursorPos(&pt);
			TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, m_hwnd, nullptr);
			DestroyMenu(hMenu);
		}
		break;

	case WM_LBUTTONDBLCLK:
		if (LOWORD(lParam) < 30)
			m_pLog->ScrollToBottom();
		else
			SendMessage(m_hwndParent, WM_SYSCOMMAND, SC_MINIMIZE, 0);
		break;

	case DM_AVATARCHANGED:
		ShowAvatar();
		break;

	case DM_CHANGEICONS:
		UpdateStatusBar();
		SetStatusIcon();
		__fallthrough;

	case DM_CLISTSETTINGSCHANGED:
		if (wParam == m_hContact && m_hContact && m_szProto) {
			DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING *)lParam;
			wchar_t buf[128];
			mir_snwprintf(buf, TranslateT("User menu - %s"), ptrW(Contact::GetInfo(CNF_UNIQUEID, m_hContact, m_szProto)).get());
			SendDlgItemMessage(m_hwnd, IDC_USERMENU, BUTTONADDTOOLTIP, (WPARAM)buf, BATF_UNICODE);

			if (cws && !mir_strcmp(cws->szModule, m_szProto) && !mir_strcmp(cws->szSetting, "Status"))
				m_wStatus = cws->value.wVal;

			SetStatusIcon();
			UpdateIcon();
			UpdateTitle();
			UpdateTabControl();
			ShowAvatar();
		}
		break;

	case DM_ACTIVATE:
		if (isChat()) {
			m_si->markRead();

			FixTabIcons();
			if (!m_si->pDlg) {
				g_chatApi.ShowRoom(m_si);
				SendMessage(m_hwnd, WM_ACTIVATE, WA_ACTIVE, 0);
			}
			break;
		}
		__fallthrough;

	case WM_ACTIVATE:
		if (LOWORD(wParam) != WA_ACTIVE)
			break;

		SetFocus(m_message.GetHwnd());
		if (isChat()) {
			m_si->markRead(true);
		}
		else {
			if (m_hDbUnreadEventFirst != 0) {
				MEVENT hDbEvent = m_hDbUnreadEventFirst;
				m_hDbUnreadEventFirst = 0;
				while (hDbEvent != 0) {
					DB::EventInfo dbei(hDbEvent, false);
					if (!dbei.bSent && dbei.isSrmm())
						Clist_RemoveEvent(-1, hDbEvent);
					hDbEvent = db_event_next(m_hContact, hDbEvent);
				}
			}
			if (m_iShowUnread) {
				m_iShowUnread = 0;
				KillTimer(m_hwnd, TIMERID_UNREAD);
				UpdateIcon();
			}
		}
		break;

	case WM_CTLCOLORLISTBOX:
		SetBkColor((HDC)wParam, g_Settings.crUserListBGColor);
		return (INT_PTR)g_chatApi.hListBkgBrush;

	case WM_LBUTTONDOWN:
		SendMessage(m_hwndParent, WM_LBUTTONDOWN, wParam, lParam);
		return TRUE;

	case DM_SETFOCUS:
		if (lParam == WM_MOUSEACTIVATE) {
			POINT pt;
			GetCursorPos(&pt);

			RECT rc;
			GetWindowRect(m_pLog->GetHwnd(), &rc);
			if (pt.x >= rc.left && pt.x <= rc.right && pt.y >= rc.top && pt.y <= rc.bottom)
				return TRUE;
		}
		if (g_dat.hFocusWnd == m_hwnd)
			SetFocus(m_message.GetHwnd());
		return TRUE;

	case WM_SETFOCUS:
		m_pParent->ActivateChild(this);
		g_dat.hFocusWnd = m_hwnd;
		PostMessage(m_hwnd, DM_SETFOCUS, 0, 0);
		return TRUE;

	case DM_SETPARENT:
		SetParent((HWND)lParam);
		m_pParent = (ParentWindowData *)GetWindowLongPtr(m_hwndParent, GWLP_USERDATA);
		return TRUE;

	case WM_SIZE:
		if (wParam == SIZE_MAXIMIZED)
			m_pLog->ScrollToBottom();

		if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
			RECT rc;
			GetClientRect(m_hwnd, &rc);
			if (rc.right && rc.bottom)
				MessageDialogResize(rc.right, rc.bottom);
		}
		return TRUE;

	case DM_REMAKELOG:
		if (wParam == 0 || wParam == m_hContact)
			RemakeLog();

		InvalidateRect(m_pLog->GetHwnd(), nullptr, FALSE);
		break;

	case WM_TIMER:
		if (wParam == TIMERID_MSGSEND)
			SendQueue::ReportTimeouts(this);
		else if (wParam == TIMERID_UNREAD) {
			TabControlData tcd;
			tcd.iFlags = TCDF_ICON;
			if (!m_bShowTyping) {
				m_iShowUnread++;
				tcd.hIcon = (m_iShowUnread & 1) ? m_hStatusIconOverlay : m_hStatusIcon;
				m_pParent->UpdateTabControl(tcd, m_hwnd);
			}
		}
		break;

	case WM_MEASUREITEM:
		{
			MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lParam;
			if (mis->CtlID == IDC_SRMM_NICKLIST) {
				int ih = Chat_GetTextPixelSize(L"AQGgl'", g_Settings.UserListFont, false);
				int ih2 = Chat_GetTextPixelSize(L"AQGg'", g_Settings.UserListHeadingsFont, false);
				int font = ih > ih2 ? ih : ih2;
				int height = db_get_b(0, CHAT_MODULE, "NicklistRowDist", 12);
				// make sure we have space for icon!
				if (Chat::bShowContactStatus)
					font = font > 16 ? font : 16;
				mis->itemHeight = height > font ? height : font;
			}
		}
		return Menu_MeasureItem(lParam);

	case WM_DRAWITEM:
		{
			LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
			if (dis->CtlID == IDC_AVATAR) {
				int itemWidth = dis->rcItem.right - dis->rcItem.left + 1, itemHeight = dis->rcItem.bottom - dis->rcItem.top + 1;
				HDC hdcMem = CreateCompatibleDC(dis->hDC);
				HBITMAP hbmMem = CreateCompatibleBitmap(dis->hDC, itemWidth, itemHeight);
				hbmMem = (HBITMAP)SelectObject(hdcMem, hbmMem);

				RECT rect;
				rect.top = 0;
				rect.left = 0;
				rect.right = itemWidth - 1;
				rect.bottom = itemHeight - 1;
				FillRect(hdcMem, &rect, GetSysColorBrush(COLOR_BTNFACE));

				if (m_hbmpAvatarPic && g_dat.flags.bShowAvatar) {
					BITMAP bminfo;
					GetObject(m_hbmpAvatarPic, sizeof(bminfo), &bminfo);
					if (bminfo.bmWidth != 0 && bminfo.bmHeight != 0) {
						int avatarHeight = itemHeight;
						int avatarWidth = bminfo.bmWidth * avatarHeight / bminfo.bmHeight;
						if (avatarWidth > itemWidth) {
							avatarWidth = itemWidth;
							avatarHeight = bminfo.bmHeight * avatarWidth / bminfo.bmWidth;
						}

						AVATARDRAWREQUEST adr = { sizeof(adr) };
						adr.hContact = m_hContact;
						adr.hTargetDC = hdcMem;
						adr.rcDraw.left = (itemWidth - avatarWidth) / 2;
						adr.rcDraw.top = (itemHeight - avatarHeight) / 2;
						adr.rcDraw.right = avatarWidth - 1;
						adr.rcDraw.bottom = avatarHeight - 1;
						adr.dwFlags = AVDRQ_DRAWBORDER | AVDRQ_HIDEBORDERONTRANSPARENCY;
						CallService(MS_AV_DRAWAVATAR, 0, (LPARAM)&adr);
					}
				}
				BitBlt(dis->hDC, 0, 0, itemWidth, itemHeight, hdcMem, 0, 0, SRCCOPY);
				hbmMem = (HBITMAP)SelectObject(hdcMem, hbmMem);
				DeleteObject(hbmMem);
				DeleteDC(hdcMem);
				return TRUE;
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SENDALL:
			int result;
			if (m_iSendAllConfirm == 0) {
				result = DialogBoxParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_CONFIRM_SENDALL), m_hwnd, ConfirmSendAllDlgProc, (LPARAM)m_hwnd);
				if (result & 0x10000)
					m_iSendAllConfirm = result;
			}
			else result = m_iSendAllConfirm;

			if (LOWORD(result) == IDYES)
				onClick_Ok(nullptr);
			break;
		}
		break;

	case WM_NOTIFY:
		pNmhdr = (LPNMHDR)lParam;
		switch (pNmhdr->idFrom) {
		case IDC_SRMM_LOG:
			if (pNmhdr->code == EN_MSGFILTER) {
				int result = InputAreaShortcuts(m_message.GetHwnd(), ((MSGFILTER *)lParam)->msg, ((MSGFILTER *)lParam)->wParam, ((MSGFILTER *)lParam)->lParam);
				if (result != -1) {
					SetWindowLongPtr(m_hwnd, DWLP_MSGRESULT, TRUE);
					return TRUE;
				}
			}
			break;
		}
		break;

	case WM_CHAR:
		SetFocus(m_message.GetHwnd());
		m_message.SendMsg(msg, wParam, lParam);
		break;
	}

	return CSuper::DlgProc(msg, wParam, lParam);
}
