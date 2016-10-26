#include "stdafx.h"

int hLangpack;
HINSTANCE g_hInstance = NULL;

PLUGININFOEX pluginInfo =
{
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__AUTHOREMAIL,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {E08CE7C4-9EEB-4272-B544-0D32E18D90DE}
	{ 0xe08ce7c4, 0x9eeb, 0x4272, { 0xb5, 0x44, 0xd, 0x32, 0xe1, 0x8d, 0x90, 0xde } }
};

DWORD WINAPI DllMain(HINSTANCE hInstance, DWORD, LPVOID)
{
	g_hInstance = hInstance;

	return TRUE;
}

extern "C" __declspec(dllexport) PLUGININFOEX* MirandaPluginInfoEx(DWORD)
{
	return &pluginInfo;
}

extern "C" __declspec(dllexport) const MUUID MirandaInterfaces[] = { MIID_PROTOCOL, MIID_LAST };

extern "C" int __declspec(dllexport) Load(void)
{
	mir_getLP(&pluginInfo);

	PROTOCOLDESCRIPTOR pd = { sizeof(pd) };
	pd.szName = "EMLAN";
	pd.type = PROTOTYPE_PROTOCOL;
	pd.fnInit = (pfnInitProto)CEmLanProto::InitAccount;
	pd.fnUninit = (pfnUninitProto)CEmLanProto::UninitAccount;
	CallService(MS_PROTO_REGISTERMODULE, 0, (LPARAM)&pd);

	return 0;
}

extern "C" int __declspec(dllexport) Unload(void)
{
	return 0;
}
