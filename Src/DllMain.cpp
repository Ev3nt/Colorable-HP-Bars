#include <windows.h>
#include <detours.h>
#include "fp_call.h"
#include <iostream>

#define AttachDetour(pointer, detour) (DetourUpdateThread(GetCurrentThread()), DetourAttach(&(PVOID&)pointer, detour))
#define DetachDetour(pointer, detour) (DetourUpdateThread(GetCurrentThread()), DetourDetach(&(PVOID&)pointer, detour))

HMODULE gameBase = GetModuleHandle("game.dll");
HMODULE thismodule = NULL;

DWORD PlayerColor = 0xFFF3FF00;
DWORD AlliesColor = 0xFF13FF00;
DWORD EnemiesColor = 0xFFFF0000;

auto SetJassState = (void(__fastcall*)(BOOL jassState))((std::ptrdiff_t)gameBase + 0x2ab0e0);

auto SetHPBarColor = (BOOL(__fastcall*)(LPVOID _this, LPVOID, DWORD* defaultColor))((UINT_PTR)gameBase + 0x60e740);
auto Player = (HANDLE(__cdecl*)(int num))((UINT_PTR)gameBase + 0x3bbb30);
auto GetLocalPlayer = (HANDLE(__cdecl*)())((UINT_PTR)gameBase + 0x3bbb60);
auto IsPlayerAlly = (BOOL(__cdecl*)(HANDLE player1, HANDLE player2))((UINT_PTR)gameBase + 0x3c9530);
auto IsPlayerEnemy = (BOOL(__cdecl*)(HANDLE player1, HANDLE player2))((UINT_PTR)gameBase + 0x3c9580);

bool ValidVersion();
void __fastcall SetJassStateCustom(BOOL jassState);

BOOL __fastcall SetHPBarColorCustom(LPVOID _this, LPVOID, DWORD* defaultColor);

HANDLE GetUnitBySimpleTexture(LPVOID frame);

int GetUnitOwnerNumber(HANDLE unit);

extern "C" void __stdcall SetForceColor(DWORD force, DWORD color) {
	switch (force)
	{
	case 0:
		PlayerColor = color;

		break;
	case 1:
		AlliesColor = color;

		break;
	case 2:
		EnemiesColor = color;

		break;
	}
}

//---------------------------------------------------

BOOL APIENTRY DllMain(HMODULE module, UINT reason, LPVOID reserved) {
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		DisableThreadLibraryCalls(module);

		if (!gameBase || !ValidVersion()) {
			return FALSE;
		}

		thismodule = module;

		DetourTransactionBegin();

		AttachDetour(SetHPBarColor, SetHPBarColorCustom);
		AttachDetour(SetJassState, SetJassStateCustom);

		DetourTransactionCommit();

		break;
	case DLL_PROCESS_DETACH:
		DetourTransactionBegin();

		DetachDetour(SetHPBarColor, SetHPBarColorCustom);
		DetachDetour(SetJassState, SetJassStateCustom);

		DetourTransactionCommit();

		break;
	}
	return TRUE;
}

//---------------------------------------------------

BOOL __fastcall SetHPBarColorCustom(LPVOID _this, LPVOID, DWORD* defaultColor) {
	HANDLE unit = GetUnitBySimpleTexture(_this);
	if (unit) {
		int playerOwnerNum = GetUnitOwnerNumber(unit);
		if (playerOwnerNum >= 0 && playerOwnerNum <= 16) {
			HANDLE localPlayer = GetLocalPlayer();
			HANDLE ownerPlayer = Player(playerOwnerNum);

			if (localPlayer == ownerPlayer) *defaultColor = PlayerColor;
			else if (IsPlayerAlly(localPlayer, ownerPlayer)) *defaultColor = AlliesColor;
			else if (IsPlayerEnemy(localPlayer, ownerPlayer)) *defaultColor = EnemiesColor;
		}
	}

	return SetHPBarColor(_this, NULL, defaultColor);
}

int GetUnitOwnerNumber(HANDLE unit) {
	return unit ? ((UINT*)unit)[0x16] : -1;
}

HANDLE GetUnitBySimpleTexture(LPVOID frame) {
	return frame ? ((LPVOID*)frame)[0x1D] && *((LPVOID**)frame)[0x1D] == (LPVOID)((UINT_PTR)gameBase + 0x93E604) ? ((HANDLE*)(((LPVOID*)frame)[0x1D]))[0x55] : NULL : NULL;
}

bool ValidVersion() {
	DWORD handle;
	DWORD size = GetFileVersionInfoSize("game.dll", &handle);

	LPSTR buffer = new char[size];
	GetFileVersionInfo("game.dll", handle, size, buffer);

	VS_FIXEDFILEINFO* verInfo;
	size = sizeof(VS_FIXEDFILEINFO);
	VerQueryValue(buffer, "\\", (LPVOID*)&verInfo, (UINT*)&size);
	delete[] buffer;

	return (((verInfo->dwFileVersionMS >> 16) & 0xffff) == 1 && (verInfo->dwFileVersionMS & 0xffff) == 26 && ((verInfo->dwFileVersionLS >> 16) & 0xffff) == 0 && ((verInfo->dwFileVersionLS >> 0) & 0xffff) == 6401);
}

void __fastcall SetJassStateCustom(BOOL jassState) {
	if (jassState == TRUE && thismodule) {
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)FreeLibrary, thismodule, NULL, NULL);
	}

	return SetJassState(jassState);
}