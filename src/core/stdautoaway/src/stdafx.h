/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (C) 2012-25 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-12 Miranda IM project,
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

#include <winsock2.h>
#include <shlobj.h>
#include <commctrl.h>
#include <vssym32.h>

#include <stdio.h>
#include <time.h>
#include <stddef.h>
#include <process.h>
#include <io.h>
#include <limits.h>
#include <string.h>
#include <locale.h>
#include <direct.h>
#include <malloc.h>

#include <m_system.h>
#include <newpluginapi.h>
#include <m_utils.h>
#include <m_netlib.h>
#include <m_clist.h>
#include <m_crypto.h>
#include <m_langpack.h>
#include <m_button.h>
#include <m_protosvc.h>
#include <m_protocols.h>
#include <m_options.h>
#include <m_skin.h>
#include <m_contacts.h>
#include <m_message.h>
#include <m_userinfo.h>
#include <m_findadd.h>
#include <m_awaymsg.h>
#include <m_idle.h>
#include <m_icolib.h>
#include <m_timezones.h>
#include <m_gui.h>

#include "version.h"

#include "../../mir_app/src/resource.h"

#define MODULENAME "AutoAway"
#define IDLENAME   "Idle"

struct CMPlugin : public PLUGIN<CMPlugin>
{
	CMPlugin();

	int Load() override;
	int Unload() override;

	CMOption<bool> bIdleCheck, bIdleMethod, bIdleOnSaver, bIdleOnFullScr, bIdleOnLock;
	CMOption<bool> bIdlePrivate, bIdleSoundsOff, bIdleOnTerminal, bIdleStatusLock;
	CMOption<bool> bAAEnable;
	CMOption<uint16_t> iAAStatus;
	CMOption<uint32_t> iIdleTime1st;
};

void IdleObject_Destroy();
void IdleObject_Create();

void LoadIdleModule();
void UnloadIdleModule();
