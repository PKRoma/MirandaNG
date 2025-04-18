#include "stdafx.h"

CMPlugin g_plugin;

HANDLE hProfileFolderPath;

/////////////////////////////////////////////////////////////////////////////////////////

PLUGININFOEX pluginInfoEx =
{
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {00272A3E-F5FA-4090-8B67-3E62AC1EE0B4}
	{0x272a3e, 0xf5fa, 0x4090, {0x8b, 0x67, 0x3e, 0x62, 0xac, 0x1e, 0xe0, 0xb4}}
};

CMPlugin::CMPlugin() :
	ACCPROTOPLUGIN<CToxProto>("TOX", pluginInfoEx)
{
	SetUniqueId(TOX_SETTINGS_ID);
}

/////////////////////////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport) const MUUID MirandaInterfaces[] = { MIID_PROTOCOL, MIID_LAST };

/////////////////////////////////////////////////////////////////////////////////////////

int OnModulesLoaded(WPARAM, LPARAM)
{
	CToxProto::InitIcons();

	hProfileFolderPath = FoldersRegisterCustomPathW("Tox", LPGEN("Profiles folder"), MIRANDA_USERDATAW);

	if (ServiceExists(MS_ASSOCMGR_ADDNEWURLTYPE)) {
		CreateServiceFunction(MODULE "/ParseUri", CToxProto::ParseToxUri);
		AssocMgr_AddNewUrlTypeW("tox:", TranslateT("Tox link protocol"), g_plugin.getInst(), IDI_TOX, MODULE "/ParseUri", 0);
	}

	return 0;
}

int CMPlugin::Load()
{
	HookEvent(ME_SYSTEM_MODULESLOADED, OnModulesLoaded);
	return 0;
}
