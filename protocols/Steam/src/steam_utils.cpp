#include "stdafx.h"

uint64_t getRandomInt()
{
	uint64_t ret;
	Utils_GetRandom(&ret, sizeof(ret));
	return ret & INT64_MAX;
}

/////////////////////////////////////////////////////////////////////////////////////////

bool IsNull(const ProtobufCBinaryData &buf)
{
	for (auto i = 0; i < buf.len; i++)
		if (buf.data[i] != 0)
			return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////

void EncodeBbcodes(SESSION_INFO *si, CMStringW &wszText)
{
	int idx = wszText.Find(':');
	if (idx != -1) {
		CMStringW wszNick(wszText.Left(idx));
		for (auto &it : si->getUserList()) {
			if (wszNick == it->pszNick) {
				wszText.Delete(0, idx+1);

				CMStringW wszReplace(FORMAT, L"[mention=%lld]@%s[/mention]", SteamIdToAccountId(_wtoi64(it->pszUID)), it->pszNick);
				wszText = wszReplace + wszText;
				break;
			}
		}
	}
}

void DecodeBbcodes(SESSION_INFO *si, CMStringA &szText)
{
	for (int idx = 0; idx != -1; idx = szText.Find('[', idx+1)) {
		bool isClosing = false;
		idx++;
		if (szText[idx] == '/') {
			isClosing = true;
			idx++;
		}

		int iEnd = szText.Find(']', idx + 1);
		if (iEnd == -1)
			return;

		bool bPlaceFirst = false;
		CMStringA szReplace;

		auto *p = szText.c_str() + idx;
		if (!strncmp(p, "emoticon", 8) || !strncmp(p, "sticker", 7))
			szReplace = ":";

		if (!isClosing) {
			if (!strncmp(p, "mention=", 8)) {
				CMStringW wszId(FORMAT, L"%lld", AccountIdToSteamId(_atoi64(p + 8)));
				if (auto *pUser = g_chatApi.UM_FindUser(si, wszId)) {
					int iEnd2 = szText.Find("[/mention]", iEnd);
					if (iEnd2 == -1)
						return;

					iEnd = iEnd2 + 10;
					szReplace.Format("%s:", T2Utf(pUser->pszNick).get());
					bPlaceFirst = true;
				}
			}
			else if (!strncmp(p, "lobbyinvite ", 12)) {
				szReplace = TranslateU("You were invited to play a game");
			}
			else if (!strncmp(p, "sticker ", 8)) {
				std::regex regex("type=\"(.+?)\"");
				std::smatch match;
				std::string content(p + 8);
				if (std::regex_search(content, match, regex)) {
					std::string szType = match[1];
					szReplace += szType.c_str();
					szReplace.Replace(" ", "_");
				}
				iEnd++;
			}
			else iEnd++;
		}
		else iEnd++, idx--;

		idx--;
		szText.Delete(idx, iEnd - idx);

		if (!szReplace.IsEmpty()) {
			if (bPlaceFirst)
				szText = szReplace + szText;
			else
				szText.Insert(idx, szReplace);
		}
		iEnd -= iEnd - idx;
		idx = iEnd;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

MBinBuffer createMachineID(const char *accName)
{
	uint8_t hashOut[MIR_SHA1_HASH_SIZE];
	char hashHex[MIR_SHA1_HASH_SIZE * 2 + 1];

	CMStringA _bb3 = CMStringA("SteamUser Hash BB3 ") + accName;
	CMStringA _ff2 = CMStringA("SteamUser Hash FF2 ") + accName;
	CMStringA _3b3 = CMStringA("SteamUser Hash 3B3 ") + accName;

	MBinBuffer ret;
	uint8_t c = 0;
	ret.append(&c, 1);
	ret.append("MessageObject", 14);

	c = 1;
	ret.append(&c, 1);
	ret.append("BB3", 4);
	mir_sha1_hash((uint8_t *)_bb3.c_str(), _bb3.GetLength(), hashOut);
	bin2hex(hashOut, sizeof(hashOut), hashHex);
	ret.append(hashHex, 41);

	ret.append(&c, 1);
	ret.append("FF2", 4);
	mir_sha1_hash((uint8_t *)_ff2.c_str(), _ff2.GetLength(), hashOut);
	bin2hex(hashOut, sizeof(hashOut), hashHex);
	ret.append(hashHex, 41);

	ret.append(&c, 1);
	ret.append("3B3", 4);
	mir_sha1_hash((uint8_t *)_3b3.c_str(), _3b3.GetLength(), hashOut);
	bin2hex(hashOut, sizeof(hashOut), hashHex);
	ret.append(hashHex, 41);

	ret.append("\x08\x08", 2);
	return ret;
}

/////////////////////////////////////////////////////////////////////////////////////////

int64_t CSteamProto::GetId(MCONTACT hContact, const char *pszSetting)
{
	return _atoi64(getMStringA(hContact, pszSetting));
}

void CSteamProto::SetId(MCONTACT hContact, const char *pszSetting, int64_t id)
{
	char szId[100];
	_i64toa(id, szId, 10);
	setString(hContact, pszSetting, szId);
}

/////////////////////////////////////////////////////////////////////////////////////////

int64_t CSteamProto::GetId(const char *pszSetting)
{
	return _atoi64(getMStringA(pszSetting));
}

void CSteamProto::SetId(const char *pszSetting, int64_t id)
{
	char szId[100];
	_i64toa(id, szId, 10);
	setString(pszSetting, szId);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Statuses

int SteamToMirandaStatus(uint32_t state)
{
	switch (state) {
	case PersonaState::Offline:
		return ID_STATUS_OFFLINE;
	case PersonaState::Online:
		return ID_STATUS_ONLINE;
	case PersonaState::Busy:
		return ID_STATUS_DND;
	case PersonaState::Away:
		return ID_STATUS_AWAY;
	case PersonaState::Snooze:
		return ID_STATUS_NA;
	case PersonaState::LookingToPlay:
	case PersonaState::LookingToTrade:
		return ID_STATUS_FREECHAT;
	case PersonaState::Invisible:
		return ID_STATUS_INVISIBLE;
	default:
		return ID_STATUS_ONLINE;
	}
}

uint32_t MirandaToSteamState(int status)
{
	switch (status) {
	case ID_STATUS_OFFLINE:
		return PersonaState::Offline;
	case ID_STATUS_ONLINE:
		return PersonaState::Online;
	case ID_STATUS_DND:
		return PersonaState::Busy;
	case ID_STATUS_AWAY:
		return PersonaState::Away;
	case ID_STATUS_NA:
		return PersonaState::Snooze;
	case ID_STATUS_FREECHAT:
		return PersonaState::LookingToPlay;
	case ID_STATUS_INVISIBLE:
		return PersonaState::Invisible;
	default:
		return PersonaState::Online;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// Popups

void ShowNotification(const wchar_t *caption, const wchar_t *message, int flags, MCONTACT hContact)
{
	if (Miranda_IsTerminated())
		return;

	if (Popup_Enabled()) {
		POPUPDATAW ppd;
		ppd.lchContact = hContact;
		wcsncpy(ppd.lpwzContactName, caption, MAX_CONTACTNAME);
		wcsncpy(ppd.lpwzText, message, MAX_SECONDLINE);
		ppd.lchIcon = g_plugin.getIcon(IDI_STEAM);

		if (!PUAddPopupW(&ppd))
			return;
	}

	MessageBox(nullptr, message, caption, MB_OK | flags);
}

void ShowNotification(const wchar_t *message, int flags, MCONTACT hContact)
{
	ShowNotification(_A2W(MODULENAME), message, flags, hContact);
}

/////////////////////////////////////////////////////////////////////////////////////////

INT_PTR CSteamProto::OnGetEventTextChatStates(WPARAM pEvent, LPARAM datatype)
{
	// Retrieves a chat state description from an event
	DBEVENTINFO *dbei = (DBEVENTINFO *)pEvent;
	if (dbei->cbBlob > 0 && dbei->pBlob[0] == STEAM_DB_EVENT_CHATSTATES_GONE)
		return (datatype == DBVT_WCHAR)
		? (INT_PTR)mir_wstrdup(TranslateT("closed chat session"))
		: (INT_PTR)mir_strdup(Translate("closed chat session"));

	return NULL;
}
