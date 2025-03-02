#ifndef _COMMON_H_
#define _COMMON_H_

#include <malloc.h>
#include <time.h>
#include <windows.h>

#include <algorithm>
#include <map>
#include <memory>

#include <newpluginapi.h>

#include <m_protoint.h>
#include <m_protosvc.h>

#include <m_avatars.h>
#include <m_clist.h>
#include <m_chat_int.h>
#include <m_contacts.h>
#include <m_database.h>
#include <m_extraicons.h>
#include <m_history.h>
#include <m_icolib.h>
#include <m_ignore.h>
#include <m_json.h>
#include <m_langpack.h>
#include <m_message.h>
#include <m_messagestate.h>
#include <m_netlib.h>
#include <m_NewStory.h>
#include <m_options.h>
#include <m_popup.h>
#include <m_skin.h>
#include <m_smileyadd.h>
#include <m_timezones.h>
#include <m_userinfo.h>

#include "../../libs/freeimage/src/FreeImage.h"

#include "td/telegram/Client.h"
#include "td/telegram/td_api.h"
#include "td/telegram/td_api.hpp"
namespace TD = td::td_api;

#define MODULE "Telegram"

#include "version.h"
#include "resource.h"
#include "proto.h"
#include "utils.h"

struct CMPlugin : public ACCPROTOPLUGIN<CTelegramProto>
{
	CMPlugin();

	bool hasMessageState = false, hasNewStory = false;
	HANDLE m_hIcon = 0;

	int Load() override;
};

#endif //_COMMON_H_