#ifndef _STEAM_UTILS_H_
#define _STEAM_UTILS_H_

int SteamToMirandaStatus(uint32_t state);
uint32_t MirandaToSteamState(int status);

void ShowNotification(const wchar_t *message, int flags = 0, MCONTACT hContact = NULL);
void ShowNotification(const wchar_t *caption, const wchar_t *message, int flags = 0, MCONTACT hContact = NULL);

MBinBuffer RsaEncrypt(const char *pszModulus, const char *exponent, const char *data);
MBinBuffer createMachineID(const char *accName);

void EncodeBbcodes(SESSION_INFO *si, CMStringW &szText);

#define now() time(0)

bool IsNull(const ProtobufCBinaryData &buf);

uint64_t getRandomInt();
CMStringA protobuf_c_text_to_string(const ProtobufCMessage &msg);

inline uint64_t AccountIdToSteamId(uint64_t accountId)
{
	return accountId | 0x110000100000000ll;
}

inline uint64_t SteamIdToAccountId(uint64_t steamId)
{
	return steamId & 0xFFFFFFFFll;
}

#endif //_STEAM_UTILS_H_
