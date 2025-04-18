/*

Object UI extensions
Copyright (c) 2008  Victor Pavlychko, George Hazan
Copyright (C) 2012-25 Miranda NG team

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

#include "../stdafx.h"

/////////////////////////////////////////////////////////////////////////////////////////
// CCtrlColor class

CCtrlColor::CCtrlColor(CDlgBase *dlg, int ctrlId) :
	CCtrlData(dlg, ctrlId)
{}

BOOL CCtrlColor::OnCommand(HWND, uint16_t, uint16_t)
{
	NotifyChange();
	return TRUE;
}

bool CCtrlColor::OnApply()
{
	if (m_hwnd && m_dbLink)
		SaveInt(GetColor());
	return true;
}

void CCtrlColor::OnReset()
{
	if (m_hwnd && m_dbLink)
		SetColor(LoadInt());
}

uint32_t CCtrlColor::GetColor()
{
   return ::SendMessage(m_hwnd, CPM_GETCOLOUR, 0, 0);
}

void CCtrlColor::SetColor(uint32_t dwValue)
{
   ::SendMessage(m_hwnd, CPM_SETCOLOUR, 0, dwValue);
}
