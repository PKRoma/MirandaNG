/*
Popup Plus plugin for Miranda IM

Copyright	© 2002 Luca Santarelli,
© 2004-2007 Victor Pavlychko
© 2010 MPK
© 2010 Merlin_de

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

HWND hwndBox = nullptr;

LRESULT CALLBACK AvatarTrackBarWndProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK AlphaTrackBarWndProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);

// effekt name for drop down box
LIST<wchar_t> g_lstPopupVfx(5, wcsicmp);
void OptAdv_RegisterVfx(char *name)
{
	g_lstPopupVfx.insert(mir_a2u(name));
}

void OptAdv_UnregisterVfx()
{
	for (auto &it : g_lstPopupVfx)
		mir_free(it);
	g_lstPopupVfx.destroy();
}

// Main Dialog Proc
void LoadOption_AdvOpts()
{
	// History
	PopupOptions.bEnableHistory = g_plugin.getBool("EnableHistory", true);
	PopupOptions.HistorySize = g_plugin.getWord("HistorySize", SETTING_HISTORYSIZE_DEFAULT);
	PopupOptions.bUseHppHistoryLog = g_plugin.getBool("UseHppHistoryLog", true);

	// Avatars
	PopupOptions.bAvatarBorders = g_plugin.getBool("AvatarBorders", true);
	PopupOptions.bAvatarPNGBorders = g_plugin.getBool("AvatarPNGBorders", false);
	PopupOptions.avatarRadius = g_plugin.getByte("AvatarRadius", 2);
	PopupOptions.avatarSize = g_plugin.getWord("AvatarSize", SETTING_AVTSIZE_DEFAULT);
	PopupOptions.bEnableAvatarUpdates = g_plugin.getBool("EnableAvatarUpdates", false);

	// Monitor
	PopupOptions.Monitor = g_plugin.getByte("Monitor", SETTING_MONITOR_DEFAULT);

	// Transparency
	PopupOptions.bUseTransparency = g_plugin.getBool("UseTransparency", true);
	PopupOptions.Alpha = Clist::iAlpha;
	PopupOptions.bOpaqueOnHover = g_plugin.getBool("OpaqueOnHover", true);

	// Effects
	PopupOptions.bUseAnimations = g_plugin.getBool("UseAnimations", true);
	PopupOptions.bUseEffect = g_plugin.getBool("Fade", true);
	PopupOptions.Effect = (LPTSTR)DBGetContactSettingStringX(0, MODULENAME, "Effect", "", DBVT_WCHAR);
	PopupOptions.FadeIn = g_plugin.getDword("FadeInTime", SETTING_FADEINTIME_DEFAULT);
	PopupOptions.FadeOut = g_plugin.getDword("FadeOutTime", SETTING_FADEOUTTIME_DEFAULT);

	// other old stuff
	PopupOptions.MaxPopups = g_plugin.getWord("MaxPopups", 20);
}

INT_PTR CALLBACK DlgProcPopupAdvOpts(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	wchar_t tstr[64];
	static bool bDlgInit = false;	// some controls send WM_COMMAND before or during WM_INITDIALOG
	UINT idCtrl;

	switch (msg) {
	case WM_INITDIALOG:
		// Create preview box:
	{
		hwndBox = CreateWindowEx(
			WS_EX_TOOLWINDOW | WS_EX_TOPMOST,		//  dwStyleEx
			BOXPREVIEW_WNDCLASS,			//  Class name
			nullptr,								//  Title
			DS_SETFONT | DS_FIXEDSYS | WS_POPUP,	//  dwStyle
			CW_USEDEFAULT,						//  x
			CW_USEDEFAULT,						//  y
			CW_USEDEFAULT,						//  Width
			CW_USEDEFAULT,						//  Height
			HWND_DESKTOP,						//  Parent
			nullptr,								//  menu handle
			g_plugin.getInst(),								//  Instance
			(LPVOID)nullptr);
		ShowWindow(hwndBox, SW_HIDE);
	}
	// Group: History
		{
			CheckDlgButton(hwnd, IDC_ENABLE_HISTORY, PopupOptions.bEnableHistory ? BST_CHECKED : BST_UNCHECKED);
			SetDlgItemInt(hwnd, IDC_HISTORYSIZE, PopupOptions.HistorySize, FALSE);
			SendDlgItemMessage(hwnd, IDC_HISTORYSIZE_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(hwnd, IDC_HISTORYSIZE), 0);
			SendDlgItemMessage(hwnd, IDC_HISTORYSIZE_SPIN, UDM_SETRANGE, 0, MAKELONG(SETTING_HISTORYSIZE_MAX, 1));
			CheckDlgButton(hwnd, IDC_HPPLOG, PopupOptions.bUseHppHistoryLog ? BST_CHECKED : BST_UNCHECKED);

			HWND hCtrl = GetDlgItem(hwnd, IDC_SHOWHISTORY);
			SendMessage(hCtrl, BUTTONSETASFLATBTN, TRUE, 0);
			SendMessage(hCtrl, BUTTONADDTOOLTIP, (WPARAM)Translate("Popup history"), 0);
			SendMessage(hCtrl, BM_SETIMAGE, IMAGE_ICON, (LPARAM)g_plugin.getIcon(IDI_HISTORY));

			EnableWindow(GetDlgItem(hwnd, IDC_HISTORY_STATIC1), PopupOptions.bEnableHistory);
			EnableWindow(GetDlgItem(hwnd, IDC_HISTORYSIZE), PopupOptions.bEnableHistory);
			EnableWindow(GetDlgItem(hwnd, IDC_HISTORYSIZE_SPIN), PopupOptions.bEnableHistory);
			EnableWindow(GetDlgItem(hwnd, IDC_HISTORY_STATIC2), PopupOptions.bEnableHistory);
			EnableWindow(GetDlgItem(hwnd, IDC_SHOWHISTORY), PopupOptions.bEnableHistory);
			EnableWindow(GetDlgItem(hwnd, IDC_HPPLOG), PopupOptions.bEnableHistory && gbHppInstalled);
		}
		// Group: Avatars
		{
			// Borders
			CheckDlgButton(hwnd, IDC_AVT_BORDER, PopupOptions.bAvatarBorders ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hwnd, IDC_AVT_PNGBORDER, PopupOptions.bAvatarPNGBorders ? BST_CHECKED : BST_UNCHECKED);
			EnableWindow(GetDlgItem(hwnd, IDC_AVT_PNGBORDER), PopupOptions.bAvatarBorders);
			// Radius
			SetDlgItemInt(hwnd, IDC_AVT_RADIUS, PopupOptions.avatarRadius, FALSE);
			SendDlgItemMessage(hwnd, IDC_AVT_RADIUS_SPIN, UDM_SETRANGE, 0, (LPARAM)MAKELONG((PopupOptions.avatarSize / 2), 0));
			// Size
			mir_subclassWindow(GetDlgItem(hwnd, IDC_AVT_SIZE_SLIDE), AvatarTrackBarWndProc);

			SendDlgItemMessage(hwnd, IDC_AVT_SIZE_SLIDE, TBM_SETRANGE, FALSE,
				MAKELONG(SETTING_AVTSIZE_MIN, SETTING_AVTSIZE_MAX));
			SendDlgItemMessage(hwnd, IDC_AVT_SIZE_SLIDE, TBM_SETPOS, TRUE,
				max(PopupOptions.avatarSize, SETTING_AVTSIZE_MIN));
			SetDlgItemInt(hwnd, IDC_AVT_SIZE, PopupOptions.avatarSize, FALSE);
			// Request avatars
			CheckDlgButton(hwnd, IDC_AVT_REQUEST, PopupOptions.bEnableAvatarUpdates ? BST_CHECKED : BST_UNCHECKED);
		}
		// Group: Monitor
		{
			BOOL bMonitor = 0;

			bMonitor = GetSystemMetrics(SM_CMONITORS) > 1;

			CheckDlgButton(hwnd, IDC_MIRANDAWND, bMonitor ? ((PopupOptions.Monitor == MN_MIRANDA) ? BST_CHECKED : BST_UNCHECKED) : BST_CHECKED);
			CheckDlgButton(hwnd, IDC_ACTIVEWND, bMonitor ? ((PopupOptions.Monitor == MN_ACTIVE) ? BST_CHECKED : BST_UNCHECKED) : BST_UNCHECKED);
			EnableWindow(GetDlgItem(hwnd, IDC_GRP_MULTIMONITOR), bMonitor);
			EnableWindow(GetDlgItem(hwnd, IDC_MULTIMONITOR_DESC), bMonitor);
			EnableWindow(GetDlgItem(hwnd, IDC_MIRANDAWND), bMonitor);
			EnableWindow(GetDlgItem(hwnd, IDC_ACTIVEWND), bMonitor);
		}
		// Group: Transparency
		{
			// win2k+
			CheckDlgButton(hwnd, IDC_TRANS, PopupOptions.bUseTransparency ? BST_CHECKED : BST_UNCHECKED);
			SendDlgItemMessage(hwnd, IDC_TRANS_SLIDER, TBM_SETRANGE, FALSE, MAKELONG(1, 255));
			SendDlgItemMessage(hwnd, IDC_TRANS_SLIDER, TBM_SETPOS, TRUE, PopupOptions.Alpha);
			mir_subclassWindow(GetDlgItem(hwnd, IDC_TRANS_SLIDER), AlphaTrackBarWndProc);
			mir_snwprintf(tstr, L"%d%%", Byte2Percentile(PopupOptions.Alpha));
			SetDlgItemText(hwnd, IDC_TRANS_PERCENT, tstr);
			CheckDlgButton(hwnd, IDC_TRANS_OPAQUEONHOVER, PopupOptions.bOpaqueOnHover ? BST_CHECKED : BST_UNCHECKED);
			{
				BOOL how = TRUE;

				EnableWindow(GetDlgItem(hwnd, IDC_TRANS), how);
				EnableWindow(GetDlgItem(hwnd, IDC_TRANS_TXT1), how && PopupOptions.bUseTransparency);
				EnableWindow(GetDlgItem(hwnd, IDC_TRANS_SLIDER), how && PopupOptions.bUseTransparency);
				EnableWindow(GetDlgItem(hwnd, IDC_TRANS_PERCENT), how && PopupOptions.bUseTransparency);
				EnableWindow(GetDlgItem(hwnd, IDC_TRANS_OPAQUEONHOVER), how && PopupOptions.bUseTransparency);
			}
			ShowWindow(GetDlgItem(hwnd, IDC_TRANS), SW_SHOW);
		}
		// Group: Effects
		{
			// Use Animations
			CheckDlgButton(hwnd, IDC_USEANIMATIONS, PopupOptions.bUseAnimations ? BST_CHECKED : BST_UNCHECKED);
			// Fade
			SetDlgItemInt(hwnd, IDC_FADEIN, PopupOptions.FadeIn, FALSE);
			SetDlgItemInt(hwnd, IDC_FADEOUT, PopupOptions.FadeOut, FALSE);
			UDACCEL aAccels[] = { { 0, 50 }, { 1, 100 }, { 3, 500 } };
			SendDlgItemMessage(hwnd, IDC_FADEIN_SPIN, UDM_SETRANGE, 0, (LPARAM)MAKELONG(SETTING_FADEINTIME_MAX, SETTING_FADEINTIME_MIN));
			SendDlgItemMessage(hwnd, IDC_FADEIN_SPIN, UDM_SETACCEL, (WPARAM)_countof(aAccels), (LPARAM)&aAccels);
			SendDlgItemMessage(hwnd, IDC_FADEOUT_SPIN, UDM_SETRANGE, 0, (LPARAM)MAKELONG(SETTING_FADEOUTTIME_MAX, SETTING_FADEOUTTIME_MIN));
			SendDlgItemMessage(hwnd, IDC_FADEOUT_SPIN, UDM_SETACCEL, (WPARAM)_countof(aAccels), (LPARAM)&aAccels);

			BOOL how = PopupOptions.bUseAnimations || PopupOptions.bUseEffect;
			EnableWindow(GetDlgItem(hwnd, IDC_FADEIN_TXT1), how);
			EnableWindow(GetDlgItem(hwnd, IDC_FADEIN), how);
			EnableWindow(GetDlgItem(hwnd, IDC_FADEIN_SPIN), how);
			EnableWindow(GetDlgItem(hwnd, IDC_FADEIN_TXT2), how);
			EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT_TXT1), how);
			EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT), how);
			EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT_SPIN), how);
			EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT_TXT2), how);
			// effects drop down
			{
				how = TRUE;
				EnableWindow(GetDlgItem(hwnd, IDC_EFFECT), how);
				EnableWindow(GetDlgItem(hwnd, IDC_EFFECT_TXT), how);

				HWND hCtrl = GetDlgItem(hwnd, IDC_EFFECT);
				ComboBox_SetItemData(hCtrl, ComboBox_AddString(hCtrl, TranslateT("No effect")), -2);
				ComboBox_SetItemData(hCtrl, ComboBox_AddString(hCtrl, TranslateT("Fade in/out")), -1);
				uint32_t dwActiveItem = (uint32_t)PopupOptions.bUseEffect;
				for (int i = 0; i < g_lstPopupVfx.getCount(); ++i) {
					uint32_t dwItem = ComboBox_AddString(hCtrl, TranslateW(g_lstPopupVfx[i]));
					ComboBox_SetItemData(hCtrl, dwItem, i);
					if (PopupOptions.bUseEffect && !mir_wstrcmp(g_lstPopupVfx[i], PopupOptions.Effect))
						dwActiveItem = dwItem;
				}
				SendDlgItemMessage(hwnd, IDC_EFFECT, CB_SETCURSEL, dwActiveItem, 0);
			}
		}

		// later check stuff
		SetDlgItemInt(hwnd, IDC_MAXPOPUPS, PopupOptions.MaxPopups, FALSE);
		SendDlgItemMessage(hwnd, IDC_MAXPOPUPS_SPIN, UDM_SETBUDDY, (WPARAM)GetDlgItem(hwnd, IDC_MAXPOPUPS), 0);
		SendDlgItemMessage(hwnd, IDC_MAXPOPUPS_SPIN, UDM_SETRANGE, 0, MAKELONG(999, 1));
		TranslateDialogDefault(hwnd);	// do it on end of WM_INITDIALOG
		bDlgInit = true;
		return TRUE;

	case WM_HSCROLL:
		switch (idCtrl = GetDlgCtrlID((HWND)lParam)) {
		case IDC_AVT_SIZE_SLIDE:
			PopupOptions.avatarSize = SendDlgItemMessage(hwnd, IDC_AVT_SIZE_SLIDE, TBM_GETPOS, 0, 0);
			SetDlgItemInt(hwnd, IDC_AVT_SIZE, PopupOptions.avatarSize, FALSE);
			SendDlgItemMessage(hwnd, IDC_AVT_RADIUS_SPIN, UDM_SETRANGE, 0, (LPARAM)MAKELONG((PopupOptions.avatarSize / 2), 0));
			SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
			break;

		case IDC_TRANS_SLIDER:
			PopupOptions.Alpha = (uint8_t)SendDlgItemMessage(hwnd, IDC_TRANS_SLIDER, TBM_GETPOS, 0, 0);
			mir_snwprintf(tstr, TranslateT("%d%%"), Byte2Percentile(PopupOptions.Alpha));
			SetDlgItemText(hwnd, IDC_TRANS_PERCENT, tstr);
			SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
			break;
		}
		break;

	case WM_COMMAND:
		idCtrl = LOWORD(wParam);
		switch (HIWORD(wParam)) {
		case BN_CLICKED:		// Button controls
			switch (idCtrl) {
			case IDC_ENABLE_HISTORY:
				PopupOptions.bEnableHistory = !PopupOptions.bEnableHistory;
				EnableWindow(GetDlgItem(hwnd, IDC_HISTORY_STATIC1), PopupOptions.bEnableHistory);
				EnableWindow(GetDlgItem(hwnd, IDC_HISTORYSIZE), PopupOptions.bEnableHistory);
				EnableWindow(GetDlgItem(hwnd, IDC_HISTORYSIZE_SPIN), PopupOptions.bEnableHistory);
				EnableWindow(GetDlgItem(hwnd, IDC_HISTORY_STATIC2), PopupOptions.bEnableHistory);
				EnableWindow(GetDlgItem(hwnd, IDC_SHOWHISTORY), PopupOptions.bEnableHistory);
				EnableWindow(GetDlgItem(hwnd, IDC_HPPLOG), PopupOptions.bEnableHistory && gbHppInstalled);
				SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				break;

			case IDC_SHOWHISTORY:
				PopupHistoryShow();
				break;

			case IDC_HPPLOG:
				PopupOptions.bUseHppHistoryLog = !PopupOptions.bUseHppHistoryLog;
				SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				break;

			case IDC_AVT_BORDER:
				PopupOptions.bAvatarBorders = !PopupOptions.bAvatarBorders;
				EnableWindow(GetDlgItem(hwnd, IDC_AVT_PNGBORDER), PopupOptions.bAvatarBorders);
				SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				break;

			case IDC_AVT_PNGBORDER:
				PopupOptions.bAvatarPNGBorders = !PopupOptions.bAvatarPNGBorders;
				SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				break;

			case IDC_AVT_REQUEST:
				PopupOptions.bEnableAvatarUpdates = !PopupOptions.bEnableAvatarUpdates;
				SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				break;

			case IDC_MIRANDAWND:
				PopupOptions.Monitor = MN_MIRANDA;
				SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				break;

			case IDC_ACTIVEWND:
				PopupOptions.Monitor = MN_ACTIVE;
				SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				break;

			case IDC_TRANS:
				PopupOptions.bUseTransparency = !PopupOptions.bUseTransparency;
				{
					BOOL how = TRUE;
					EnableWindow(GetDlgItem(hwnd, IDC_TRANS_TXT1), how && PopupOptions.bUseTransparency);
					EnableWindow(GetDlgItem(hwnd, IDC_TRANS_SLIDER), how && PopupOptions.bUseTransparency);
					EnableWindow(GetDlgItem(hwnd, IDC_TRANS_PERCENT), how && PopupOptions.bUseTransparency);
					EnableWindow(GetDlgItem(hwnd, IDC_TRANS_OPAQUEONHOVER), how && PopupOptions.bUseTransparency);
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
				break;

			case IDC_TRANS_OPAQUEONHOVER:
				PopupOptions.bOpaqueOnHover = !PopupOptions.bOpaqueOnHover;
				SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				break;

			case IDC_USEANIMATIONS:
				PopupOptions.bUseAnimations = !PopupOptions.bUseAnimations;
				{
					BOOL enable = PopupOptions.bUseAnimations || PopupOptions.bUseEffect;
					EnableWindow(GetDlgItem(hwnd, IDC_FADEIN_TXT1), enable);
					EnableWindow(GetDlgItem(hwnd, IDC_FADEIN), enable);
					EnableWindow(GetDlgItem(hwnd, IDC_FADEIN_SPIN), enable);
					EnableWindow(GetDlgItem(hwnd, IDC_FADEIN_TXT2), enable);
					EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT_TXT1), enable);
					EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT), enable);
					EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT_SPIN), enable);
					EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT_TXT2), enable);
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
				break;

			case IDC_PREVIEW:
				PopupPreview();
				break;
			}
			break;

		case CBN_SELCHANGE:
			// lParam = Handle to the control
			switch (idCtrl) {
			case IDC_EFFECT:
			{
				int iEffect = ComboBox_GetItemData((HWND)lParam, ComboBox_GetCurSel((HWND)lParam));
				PopupOptions.bUseEffect = (iEffect != -2);
				mir_free(PopupOptions.Effect);
				PopupOptions.Effect = mir_wstrdup((iEffect >= 0) ? g_lstPopupVfx[iEffect] : L"");

				BOOL enable = PopupOptions.bUseAnimations || PopupOptions.bUseEffect;
				EnableWindow(GetDlgItem(hwnd, IDC_FADEIN_TXT1), enable);
				EnableWindow(GetDlgItem(hwnd, IDC_FADEIN), enable);
				EnableWindow(GetDlgItem(hwnd, IDC_FADEIN_SPIN), enable);
				EnableWindow(GetDlgItem(hwnd, IDC_FADEIN_TXT2), enable);
				EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT_TXT1), enable);
				EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT), enable);
				EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT_SPIN), enable);
				EnableWindow(GetDlgItem(hwnd, IDC_FADEOUT_TXT2), enable);
				SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
			}
			break;
			}
			break;

		case EN_CHANGE:			// Edit controls change
			if (!bDlgInit) break;
			// lParam = Handle to the control
			switch (idCtrl) {
			case IDC_MAXPOPUPS:
			{
				int maxPop = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (maxPop > 0) {
					PopupOptions.MaxPopups = maxPop;
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
			}
			break;
			case IDC_HISTORYSIZE:
			{
				int histSize = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (histSize > 0 && histSize <= SETTING_HISTORYSIZE_MAX) {
					PopupOptions.HistorySize = histSize;
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
			}
			break;
			case IDC_AVT_RADIUS:
			{
				int avtRadius = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (avtRadius <= SETTING_AVTSIZE_MAX / 2) {
					PopupOptions.avatarRadius = avtRadius;
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
			}
			break;
			case IDC_FADEIN:
			{
				int fadeIn = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (fadeIn >= SETTING_FADEINTIME_MIN && fadeIn <= SETTING_FADEINTIME_MAX) {
					PopupOptions.FadeIn = fadeIn;
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
			}
			break;
			case IDC_FADEOUT:
			{
				int fadeOut = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (fadeOut >= SETTING_FADEOUTTIME_MIN && fadeOut <= SETTING_FADEOUTTIME_MAX) {
					PopupOptions.FadeOut = fadeOut;
					SendMessage(GetParent(hwnd), PSM_CHANGED, 0, 0);
				}
			}
			break;
			}
			break;

		case EN_KILLFOCUS:		// Edit controls lost fokus
			// lParam = Handle to the control
			switch (idCtrl) {
			case IDC_MAXPOPUPS:
			{
				int maxPop = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (maxPop <= 0)
					PopupOptions.MaxPopups = 20;
				if (maxPop != PopupOptions.MaxPopups) {
					SetDlgItemInt(hwnd, idCtrl, PopupOptions.MaxPopups, FALSE);
					// ErrorMSG(1);
					SetFocus((HWND)lParam);
				}
			}
			break;
			case IDC_HISTORYSIZE:
			{
				int histSize = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (histSize <= 0)
					PopupOptions.HistorySize = SETTING_HISTORYSIZE_DEFAULT;
				else if (histSize > SETTING_HISTORYSIZE_MAX)
					PopupOptions.HistorySize = SETTING_HISTORYSIZE_MAX;
				if (histSize != PopupOptions.HistorySize) {
					SetDlgItemInt(hwnd, idCtrl, PopupOptions.HistorySize, FALSE);
					ErrorMSG(1, SETTING_HISTORYSIZE_MAX);
					SetFocus((HWND)lParam);
				}
			}
			break;
			case IDC_AVT_RADIUS:
			{
				int avtRadius = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (avtRadius > SETTING_AVTSIZE_MAX / 2)
					PopupOptions.avatarRadius = SETTING_AVTSIZE_MAX / 2;
				if (avtRadius != PopupOptions.avatarRadius) {
					SetDlgItemInt(hwnd, idCtrl, PopupOptions.avatarRadius, FALSE);
					ErrorMSG(0, SETTING_AVTSIZE_MAX / 2);
					SetFocus((HWND)lParam);
				}
			}
			break;
			case IDC_FADEIN:
			{
				int fade = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (fade < SETTING_FADEINTIME_MIN)
					PopupOptions.FadeIn = SETTING_FADEINTIME_MIN;
				else if (fade > SETTING_FADEINTIME_MAX)
					PopupOptions.FadeIn = SETTING_FADEINTIME_MAX;
				if (fade != (int)PopupOptions.FadeIn) {
					SetDlgItemInt(hwnd, idCtrl, PopupOptions.FadeIn, FALSE);
					ErrorMSG(SETTING_FADEINTIME_MIN, SETTING_FADEINTIME_MAX);
					SetFocus((HWND)lParam);
				}
			}
			break;
			case IDC_FADEOUT:
			{
				int fade = GetDlgItemInt(hwnd, idCtrl, nullptr, FALSE);
				if (fade < SETTING_FADEOUTTIME_MIN)
					PopupOptions.FadeOut = SETTING_FADEOUTTIME_MIN;
				else if (fade > SETTING_FADEOUTTIME_MAX)
					PopupOptions.FadeOut = SETTING_FADEOUTTIME_MAX;
				if (fade != (int)PopupOptions.FadeOut) {
					SetDlgItemInt(hwnd, idCtrl, PopupOptions.FadeOut, FALSE);
					ErrorMSG(SETTING_FADEOUTTIME_MIN, SETTING_FADEOUTTIME_MAX);
					SetFocus((HWND)lParam);
				}
			}
			}
		}
		break;

	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->idFrom) {
		case 0:
			switch (((LPNMHDR)lParam)->code) {
			case PSN_RESET:
				LoadOption_AdvOpts();
				return TRUE;

			case PSN_APPLY:
				// History
				g_plugin.setByte("EnableHistory", (uint8_t)PopupOptions.bEnableHistory);
				g_plugin.setWord("HistorySize", PopupOptions.HistorySize);
				PopupHistoryResize();
				g_plugin.setByte("UseHppHistoryLog", PopupOptions.bUseHppHistoryLog);
				// Avatars
				g_plugin.setByte("AvatarBorders", PopupOptions.bAvatarBorders);
				g_plugin.setByte("AvatarPNGBorders", PopupOptions.bAvatarPNGBorders);
				g_plugin.setByte("AvatarRadius", PopupOptions.avatarRadius);
				g_plugin.setWord("AvatarSize", PopupOptions.avatarSize);
				g_plugin.setByte("EnableAvatarUpdates", PopupOptions.bEnableAvatarUpdates);
				// Monitor
				g_plugin.setByte("Monitor", PopupOptions.Monitor);
				// Transparency
				g_plugin.setByte("UseTransparency", PopupOptions.bUseTransparency);
				g_plugin.setByte("Alpha", PopupOptions.Alpha);
				g_plugin.setByte("OpaqueOnHover", PopupOptions.bOpaqueOnHover);

				// Effects
				g_plugin.setByte("UseAnimations", PopupOptions.bUseAnimations);
				g_plugin.setByte("Fade", PopupOptions.bUseEffect);
				g_plugin.setWString("Effect", PopupOptions.Effect);
				g_plugin.setDword("FadeInTime", PopupOptions.FadeIn);
				g_plugin.setDword("FadeOutTime", PopupOptions.FadeOut);
				// other old stuff
				g_plugin.setWord("MaxPopups", (uint8_t)PopupOptions.MaxPopups);
			}
			return TRUE;
		}
		break;

	case WM_DESTROY:
		bDlgInit = false;
		break;
	}
	return FALSE;
}

LRESULT CALLBACK AvatarTrackBarWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!IsWindowEnabled(hwnd))
		return mir_callNextSubclass(hwnd, AvatarTrackBarWndProc, msg, wParam, lParam);

	static int oldVal = -1;
	switch (msg) {
	case WM_MOUSEWHEEL:
	case WM_KEYDOWN:
	case WM_KEYUP:
		if (!IsWindowVisible(hwndBox))
			break;

	case WM_MOUSEMOVE:
	{
		TRACKMOUSEEVENT tme = { 0 };
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.dwHoverTime = HOVER_DEFAULT;
		tme.hwndTrack = hwnd;
		_TrackMouseEvent(&tme);

		int newVal = (uint8_t)SendMessage(hwnd, TBM_GETPOS, 0, 0);
		if (oldVal != newVal) {
			if (oldVal < 0)
				SetWindowLongPtr(hwndBox, GWLP_USERDATA, 0);

			RECT rc; GetWindowRect(hwnd, &rc);
			SetWindowPos(hwndBox, nullptr,
				(rc.left + rc.right - newVal) / 2, rc.bottom + 2, newVal, newVal,
				SWP_NOACTIVATE | SWP_DEFERERASE | SWP_NOSENDCHANGING | SWP_SHOWWINDOW);

			HRGN rgn = CreateRoundRectRgn(0, 0, newVal, newVal, 2 * PopupOptions.avatarRadius, 2 * PopupOptions.avatarRadius);
			SetWindowRgn(hwndBox, rgn, TRUE);
			InvalidateRect(hwndBox, nullptr, FALSE);
			oldVal = newVal;
		}
	}
	break;

	case WM_MOUSELEAVE:
		SetWindowRgn(hwndBox, nullptr, TRUE);
		ShowWindow(hwndBox, SW_HIDE);
		oldVal = -1;
		break;
	}
	return mir_callNextSubclass(hwnd, AvatarTrackBarWndProc, msg, wParam, lParam);
}

LRESULT CALLBACK AlphaTrackBarWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!IsWindowEnabled(hwnd))
		return mir_callNextSubclass(hwnd, AlphaTrackBarWndProc, msg, wParam, lParam);

	static int oldVal = -1;
	switch (msg) {
	case WM_MOUSEWHEEL:
	case WM_KEYDOWN:
	case WM_KEYUP:
		if (!IsWindowVisible(hwndBox))
			break;

	case WM_MOUSEMOVE:
	{
		TRACKMOUSEEVENT tme = { 0 };
		tme.cbSize = sizeof(tme);
		tme.dwFlags = TME_LEAVE;
		tme.dwHoverTime = HOVER_DEFAULT;
		tme.hwndTrack = hwnd;
		_TrackMouseEvent(&tme);

		int newVal = (uint8_t)SendMessage(hwnd, TBM_GETPOS, 0, 0);
		if (oldVal != newVal)
		{

			if (oldVal < 0)
			{
				SetWindowLongPtr(hwndBox, GWLP_USERDATA, 1);
				RECT rc; GetWindowRect(hwnd, &rc);
				SetWindowPos(hwndBox, nullptr,
					(rc.left + rc.right - 170) / 2, rc.bottom + 2, 170, 50,
					SWP_NOACTIVATE | SWP_DEFERERASE | SWP_NOSENDCHANGING | SWP_SHOWWINDOW);
				SetWindowRgn(hwndBox, nullptr, TRUE);
			}
			SetWindowLongPtr(hwndBox, GWL_EXSTYLE, GetWindowLongPtr(hwndBox, GWL_EXSTYLE) | WS_EX_LAYERED);
			SetLayeredWindowAttributes(hwndBox, NULL, newVal, LWA_ALPHA);

			oldVal = newVal;
		}
	}
	break;

	case WM_MOUSELEAVE:
		SetWindowLongPtr(hwndBox, GWL_EXSTYLE, GetWindowLongPtr(hwndBox, GWL_EXSTYLE) & ~WS_EX_LAYERED);
		SetLayeredWindowAttributes(hwndBox, NULL, 255, LWA_ALPHA);

		ShowWindow(hwndBox, SW_HIDE);
		oldVal = -1;
		break;
	}
	return mir_callNextSubclass(hwnd, AlphaTrackBarWndProc, msg, wParam, lParam);
}

