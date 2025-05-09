/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-03 Miranda ICQ/IM project,
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

#define WM_DOCKCALLBACK   (WM_USER+121)
#define WM_CREATEDOCKED   (WM_USER+122)
#define EDGESENSITIVITY   3

#define DOCKED_NONE    0
#define DOCKED_LEFT    1
#define DOCKED_RIGHT   2
int g_iDocked;

extern RECT cluiPos;

static void Docking_GetMonitorRectFromPoint(POINT pt, RECT *rc)
{
	MONITORINFO monitorInfo;
	HMONITOR hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST); // always returns a valid value
	monitorInfo.cbSize = sizeof(MONITORINFO);

	if (GetMonitorInfo(hMonitor, &monitorInfo)) {
		memcpy(rc, &monitorInfo.rcMonitor, sizeof(RECT));
		return;
	}

	// "generic" win95/NT support, also serves as failsafe
	rc->left = 0;
	rc->top = 0;
	rc->bottom = GetSystemMetrics(SM_CYSCREEN);
	rc->right = GetSystemMetrics(SM_CXSCREEN);
}

static void Docking_GetMonitorRectFromWindow(HWND hWnd, RECT *rc)
{
	POINT ptWindow;
	GetWindowRect(hWnd, rc);
	ptWindow.x = rc->left;
	ptWindow.y = rc->top;
	Docking_GetMonitorRectFromPoint(ptWindow, rc);
}

static void Docking_AdjustPosition(HWND hwnd, RECT *rcDisplay, RECT *rc)
{
	APPBARDATA abd;

	memset(&abd, 0, sizeof(abd));
	abd.cbSize = sizeof(abd);
	abd.hWnd = hwnd;
	abd.uEdge = g_iDocked == DOCKED_LEFT ? ABE_LEFT : ABE_RIGHT;
	abd.rc = *rc;
	abd.rc.top = rcDisplay->top;
	abd.rc.bottom = rcDisplay->bottom;
	if (g_iDocked == DOCKED_LEFT) {
		abd.rc.right = rcDisplay->left + (abd.rc.right - abd.rc.left) - cfg::dat.bClipBorder;
		abd.rc.left = rcDisplay->left - cfg::dat.bClipBorder;
	}
	else {
		abd.rc.left = rcDisplay->right - (abd.rc.right - abd.rc.left) + cfg::dat.bClipBorder;
		abd.rc.right = rcDisplay->right + cfg::dat.bClipBorder;
	}
	SHAppBarMessage(ABM_SETPOS, &abd);
	*rc = abd.rc;
}

int Docking_ProcessWindowMessage(WPARAM wParam, LPARAM lParam)
{
	APPBARDATA abd;
	static int draggingTitle;
	MSG *msg = (MSG *)wParam;

	if (msg->message == WM_DESTROY)
		g_plugin.setByte("Docked", (uint8_t)g_iDocked);
	if (!g_iDocked && msg->message != WM_CREATE && msg->message != WM_MOVING && msg->message != WM_CREATEDOCKED && msg->message != WM_MOVE)
		return 0;

	switch (msg->message) {
	case WM_CREATE:
		//if (GetSystemMetrics(SM_CMONITORS)>1) return 0;
		if (g_plugin.getByte("Docked", 0))
			PostMessage(msg->hwnd, WM_CREATEDOCKED, 0, 0);
		draggingTitle = 0;
		return 0;

	case WM_CREATEDOCKED:
		//we need to post a message just after creation to let main message function do some work
		g_iDocked = (int)(char)g_plugin.getByte("Docked", 0);
		if (IsWindowVisible(msg->hwnd) && !IsIconic(msg->hwnd)) {
			RECT rc, rcMonitor;
			memset(&abd, 0, sizeof(abd));
			abd.cbSize = sizeof(abd);
			abd.hWnd = msg->hwnd;
			abd.lParam = 0;
			abd.uCallbackMessage = WM_DOCKCALLBACK;
			SHAppBarMessage(ABM_NEW, &abd);
			GetWindowRect(msg->hwnd, &rc);
			Docking_GetMonitorRectFromWindow(msg->hwnd, &rcMonitor);
			Docking_AdjustPosition(msg->hwnd, &rcMonitor, &rc);
			MoveWindow(msg->hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
		}
		break;

	case WM_ACTIVATE:
		memset(&abd, 0, sizeof(abd));
		abd.cbSize = sizeof(abd);
		abd.hWnd = msg->hwnd;
		SHAppBarMessage(ABM_ACTIVATE, &abd);
		return 0;

	case WM_WINDOWPOSCHANGED:
		memset(&abd, 0, sizeof(abd));
		abd.cbSize = sizeof(abd);
		abd.hWnd = msg->hwnd;
		SHAppBarMessage(ABM_WINDOWPOSCHANGED, &abd);
		return 0;

	case WM_MOVING:
		{
			RECT rcMonitor;
			POINT ptCursor;

			// stop early
			if (GetAsyncKeyState(VK_CONTROL) & 0x8000)
				return 0;

			// GetMessagePos() is no good, position is always unsigned
			GetCursorPos(&ptCursor);
			Docking_GetMonitorRectFromPoint(ptCursor, &rcMonitor);

			if ((ptCursor.x < rcMonitor.left + EDGESENSITIVITY) || (ptCursor.x >= rcMonitor.right - EDGESENSITIVITY)) {
				if (!(GetWindowLongPtr(msg->hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
					SendMessage(msg->hwnd, CLUIINTM_REDRAW, 0, 0);
					MessageBox(nullptr, TranslateT("The contact list cannot be docked when using the default title bar and border. Use a toolwindow or borderless style instead."),
						TranslateT("Contact list docking"), MB_OK);
					return 0;
				}
				memset(&abd, 0, sizeof(abd));
				abd.cbSize = sizeof(abd);
				abd.hWnd = msg->hwnd;
				abd.lParam = 0;
				abd.uCallbackMessage = WM_DOCKCALLBACK;
				SHAppBarMessage(ABM_NEW, &abd);
				if (ptCursor.x < rcMonitor.left + EDGESENSITIVITY)
					g_iDocked = DOCKED_LEFT;
				else
					g_iDocked = DOCKED_RIGHT;
				SendMessage(msg->hwnd, WM_LBUTTONUP, 0, MAKELPARAM(ptCursor.x, ptCursor.y));
				GetWindowRect(msg->hwnd, (LPRECT)msg->lParam);
				Docking_AdjustPosition(msg->hwnd, (LPRECT)&rcMonitor, (LPRECT)msg->lParam);
				PostMessage(msg->hwnd, CLUIINTM_REDRAW, 0, 0);
				return TRUE;
			}
		}
		return 0;

	case WM_MOVE:
		if (g_iDocked) {
			RECT rc, rcMonitor;
			Docking_GetMonitorRectFromWindow(msg->hwnd, &rcMonitor);
			GetWindowRect(msg->hwnd, &rc);
			Docking_AdjustPosition(msg->hwnd, &rcMonitor, &rc);
			MoveWindow(msg->hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
			return 1;
		}
		return 0;

	case WM_SIZING:
		{
			RECT rcMonitor;
			Docking_GetMonitorRectFromWindow(msg->hwnd, &rcMonitor);
			Docking_AdjustPosition(msg->hwnd, &rcMonitor, (LPRECT)msg->lParam);
			*((LRESULT *)lParam) = TRUE;
		}
		return TRUE;

	case WM_SHOWWINDOW:
		if (msg->lParam)
			return 0;
		if ((msg->wParam && g_iDocked < 0) || (!msg->wParam && g_iDocked > 0))
			g_iDocked = -g_iDocked;
		memset(&abd, 0, sizeof(abd));
		abd.cbSize = sizeof(abd);
		abd.hWnd = msg->hwnd;
		if (msg->wParam) {
			RECT rc, rcMonitor;
			Docking_GetMonitorRectFromWindow(msg->hwnd, &rcMonitor);
			abd.lParam = 0;
			abd.uCallbackMessage = WM_DOCKCALLBACK;
			SHAppBarMessage(ABM_NEW, &abd);
			GetWindowRect(msg->hwnd, &rc);
			Docking_AdjustPosition(msg->hwnd, &rcMonitor, &rc);
			MoveWindow(msg->hwnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, FALSE);
		}
		else SHAppBarMessage(ABM_REMOVE, &abd);
		return 0;

	case WM_NCHITTEST:
		{
			LONG result = DefWindowProc(msg->hwnd, WM_NCHITTEST, msg->wParam, msg->lParam);
			if (result == HTSIZE || result == HTTOP || result == HTTOPLEFT || result == HTTOPRIGHT || result == HTBOTTOM || result == HTBOTTOMRIGHT || result == HTBOTTOMLEFT) {
				*((LRESULT *)lParam) = HTCLIENT; return TRUE;
			}
			if (g_iDocked == DOCKED_LEFT && result == HTLEFT) {
				*((LRESULT *)lParam) = HTCLIENT; return TRUE;
			}
			if (g_iDocked == DOCKED_RIGHT && result == HTRIGHT) {
				*((LRESULT *)lParam) = HTCLIENT; return TRUE;
			}
		}
		return 0;

	case WM_SYSCOMMAND:
		if ((msg->wParam & 0xFFF0) != SC_MOVE)
			return 0;
		SetActiveWindow(msg->hwnd);
		SetCapture(msg->hwnd);
		draggingTitle = 1;
		*((LRESULT *)lParam) = 0;
		return TRUE;

	case WM_MOUSEMOVE:
		if (!draggingTitle)
			return 0; {
			RECT rc;
			POINT pt;
			GetClientRect(msg->hwnd, &rc);
			if (((g_iDocked == DOCKED_LEFT || g_iDocked == -DOCKED_LEFT) && (short)LOWORD(msg->lParam) > rc.right) || ((g_iDocked == DOCKED_RIGHT || g_iDocked == -DOCKED_RIGHT) && (short)LOWORD(msg->lParam) < 0)) {
				ReleaseCapture();
				draggingTitle = 0;
				memset(&abd, 0, sizeof(abd));
				abd.cbSize = sizeof(abd);
				abd.hWnd = msg->hwnd;
				SHAppBarMessage(ABM_REMOVE, &abd);
				g_iDocked = 0;
				GetCursorPos(&pt);
				PostMessage(msg->hwnd, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(pt.x, pt.y));
				SetWindowPos(msg->hwnd, nullptr, pt.x - rc.right / 2, pt.y - GetSystemMetrics(SM_CYFRAME) - GetSystemMetrics(SM_CYSMCAPTION) / 2, cluiPos.right, cluiPos.bottom, SWP_NOZORDER);
			}
		}
		return 1;

	case WM_LBUTTONUP:
		if (draggingTitle) {
			ReleaseCapture();
			draggingTitle = 0;
		}
		return 0;

	case WM_DOCKCALLBACK:
		switch (msg->wParam) {
		case ABN_WINDOWARRANGE:
			ShowWindow(msg->hwnd, msg->lParam ? SW_HIDE : SW_SHOW);
			break;
		}
		return TRUE;

	case WM_DESTROY:
		if (g_iDocked > 0) {
			memset(&abd, 0, sizeof(abd));
			abd.cbSize = sizeof(abd);
			abd.hWnd = msg->hwnd;
			SHAppBarMessage(ABM_REMOVE, &abd);
		}
		return 0;
	}
	return 0;
}
