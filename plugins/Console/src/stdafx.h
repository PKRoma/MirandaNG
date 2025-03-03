/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-08 Miranda ICQ/IM project,
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

#pragma once

#include <windows.h>
#include <commctrl.h>
#include <malloc.h>

#include <newpluginapi.h>
#include <m_netlib.h>
#include <m_database.h>
#include <m_options.h>
#include <m_langpack.h>
#include <m_clist.h>
#include <m_button.h>
#include <m_fontservice.h>
#include <m_hotkeys.h>

#include <m_toptoolbar.h>

#include "resource.h"
#include "version.h"

#define MODULENAME "Console"

struct CMPlugin : public PLUGIN<CMPlugin>
{
	CMPlugin();

	HANDLE hConsoleThread;

	int Load() override;
	int Unload() override;
};

void InitConsole();
void ShutdownConsole();
HANDLE LoadIcon(int iIconID);

#define MS_NETLIB_LOGWIN "Netlib/Log/Win"
