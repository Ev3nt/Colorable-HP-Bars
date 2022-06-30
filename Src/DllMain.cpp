#include <windows.h>
#include <detours.h>
#include <vector>
#include "fp_call.h"
#include <unordered_map>

#define AttachDetour(pointer, detour) (DetourUpdateThread(GetCurrentThread()), DetourAttach(&(PVOID&)pointer, detour))
#define DetachDetour(pointer, detour) (DetourUpdateThread(GetCurrentThread()), DetourDetach(&(PVOID&)pointer, detour))

enum EFramePoint : UINT {
	FRAMEPOINT_TOPLEFT,
	FRAMEPOINT_TOP,
	FRAMEPOINT_TOPRIGHT,
	FRAMEPOINT_LEFT,
	FRAMEPOINT_CENTER,
	FRAMEPOINT_RIGHT,
	FRAMEPOINT_BOTTOMLEFT,
	FRAMEPOINT_BOTTOM,
	FRAMEPOINT_BOTTOMRIGHT
};

typedef struct {
	UINT vTable; // reserved
	UINT unk1;
	UINT Unit;
	UINT StatBar;
	UINT unk2;
	UINT unk3; // reserved
} CPreselectUI, * PCPreselectUI;

typedef struct {
	UINT Bar = NULL;
	DWORD BarColor = NULL;
	float BarValue = NULL;
	UINT BarFillDirection = NULL; // 0 - left, 1 - right
	UINT BarParentAnchor = NULL; // 0 - left, 1 - end, 2 - right
	int BarPriority = -1;
} CExtraBar;

typedef struct {
	DWORD HPBarColor = NULL;
	float HPBarWidth = NULL;
	float HPBarHeight = NULL;
	std::unordered_map<UINT, CExtraBar> ExtraBars;
} CUnitData;

UINT_PTR gameBase = (UINT_PTR)GetModuleHandle("game.dll");
HMODULE stormBase = GetModuleHandle("storm.dll");
HMODULE thismodule = NULL;

DWORD PlayerColor = 0xFFF3FF00;
DWORD AlliesColor = 0xFF13FF00;
DWORD EnemiesColor = 0xFFFF0000;

std::unordered_map<UINT, CUnitData> UnitsData;

auto SMemAlloc = (LPVOID(__stdcall*)(size_t amount, LPCSTR logfilename, int logline, int defaultValue))(GetProcAddress(stormBase, (LPCSTR)401));
auto SMemDestroy = (BOOL(__cdecl *)())(GetProcAddress(stormBase, (LPCSTR)402));
auto SMemFree = (BOOL(__stdcall*)(LPVOID location, LPCSTR logfilename, int logline, int defaultValue))(GetProcAddress(stormBase, (LPCSTR)403));
auto SMemGetSize = (size_t(__stdcall*)(LPVOID location, LPCSTR logfilename, int logline))(GetProcAddress(stormBase, (LPCSTR)404));
auto SMemReAlloc = (LPVOID(__stdcall*)(LPVOID location, size_t amount, LPCSTR logfilename, int logline, int defaultValue))(GetProcAddress(stormBase, (LPCSTR)405));

auto CreateCFrameLayout = (UINT(__thiscall*)(UINT data, UINT parent, UINT unknown1, UINT unknown2))(gameBase + 0x359cc0);
auto UpdateFrame = (UINT(__thiscall*)(UINT frame, UINT unknown))(gameBase + 0x605cc0);
auto GetUnitFramePosition = (void(__fastcall*)(UINT frame, float* x, float* y))(gameBase + 0x334180);
auto SetCLayoutFrameAbsolutePoint = (UINT(__fastcall*)(UINT frame, UINT, UINT point, float offsetX, float offsetY, UINT unknown))(gameBase + 0x6061b0);
auto SetCSimpleFramePriority = (BOOL(__thiscall*)(UINT frame, UINT priority))(gameBase + 0x2f59b0);
//auto ShowHPBar = (void(__thiscall*)(CPreselectUI * PreselectUI, UINT Unit, BOOL unk))(gameBase + 0x379a30);
auto StatBarDestructor = (BOOL(__thiscall*)(UINT frame, BOOL flag))(gameBase + 0x35a090);
auto SetStatBarValue = (void(__thiscall*)(UINT frame, float value))(gameBase + 0x60e430);
auto ShowStatBar = (BOOL(__thiscall*)(UINT frame))(gameBase + 0x609b50);
auto HideStatBar = (BOOL(__thiscall*)(UINT frame))(gameBase + 0x609ad0);
auto UpdateHPBar = (void(__thiscall*)(UINT StatBar, UINT unk))(gameBase + 0x364A50);
auto UnitDataUpdate = (void(__thiscall*)(UINT Unit))(gameBase + 0x2C74b0);
auto SetJassState = (void(__fastcall*)(BOOL jassState))(gameBase + 0x2ab0e0);
auto UnitDestructor = (BOOL(__thiscall*)(UINT Unit, UINT unk1))(gameBase + 0x28b670);
auto DisplayUnitPreselectUI = (BOOL(__thiscall*)(UINT unit, UINT unk1, UINT unk2))(gameBase + 0x2c7460);

auto SetSimpleTextureColor = (BOOL(__thiscall*)(UINT frame, DWORD* defaultColor))(gameBase + 0x60e740);
auto SetFrameWidth = (BOOL(__thiscall*)(UINT frame, float width))(gameBase + 0x605d90);
auto SetFrameHeight = (BOOL(__thiscall*)(UINT frame, float height))(gameBase + 0x605db0);
auto Player = (HANDLE(__cdecl*)(int num))(gameBase + 0x3bbb30);
auto GetLocalPlayer = (HANDLE(__cdecl*)())(gameBase + 0x3bbb60);
auto IsPlayerAlly = (BOOL(__cdecl*)(HANDLE player1, HANDLE player2))(gameBase + 0x3c9530);
auto IsPlayerEnemy = (BOOL(__cdecl*)(HANDLE player1, HANDLE player2))(gameBase + 0x3c9580);

bool ValidVersion();
void __fastcall SetJassStateCustom(BOOL jassState);

BOOL __fastcall ShowStatBarCustom(UINT frame);
BOOL __fastcall HideStatBarCustom(UINT frame);
//void __fastcall ShowHPBarCustom(CPreselectUI* PreselectUI, LPVOID, UINT Unit, BOOL unk);
void __fastcall UpdateHPBarCustom(UINT StatBar, LPVOID, UINT unk);
void __fastcall UnitDataUpdateCustom(UINT Unit);
BOOL __fastcall UnitDestructorCustom(UINT Unit, LPVOID, UINT unk1);
BOOL __fastcall StatBarDestructorCustom(UINT frame, LPVOID, BOOL flag);

float GetCLayoutFrameAbsolutePointX(UINT frame);
float GetCLayoutFrameAbsolutePointY(UINT frame);
void SetFramePriority(UINT frame, UINT priority);
UINT GetFramePriority(UINT frame);
float GetStatBarValue(UINT frame);
UINT GetSimpleTextureByStatBar(UINT frame);
UINT GetUnitBySimpleTexture(UINT frame);
UINT GetUnitByStatBar(UINT frame);
DWORD GetSimpleTextureColor(UINT frame);
CPreselectUI* GetUnitPreselectUI(UINT unit);
float GetFrameWidth(UINT frame);
float GetFrameHeight(UINT frame);

int GetUnitOwnerNumber(UINT unit);

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

extern "C" void __stdcall SetUnitHealthBarColor(UINT unit, DWORD color) {
	UnitsData[unit].HPBarColor = color;
}

extern "C" void __stdcall SetUnitHealthBarSize(UINT unit, float width, float height) {
	CUnitData& data = UnitsData[unit];
	data.HPBarWidth = width;
	data.HPBarHeight = height;
}

extern "C" UINT __stdcall SetUnitExtraBar(UINT unit, float fillAmount, UINT fillDirection, UINT parentAnchor, DWORD color) {
	CUnitData& UnitData = UnitsData[unit];
	if (UnitData.ExtraBars.size() >= 8) {
		return NULL;
	}
	
	UINT bar = (UINT)SMemAlloc(344, "ColorableArmorStatBar", 4, 0);
	CExtraBar& extraBar = UnitsData[unit].ExtraBars[bar];
	extraBar.Bar = bar;
	CreateCFrameLayout(extraBar.Bar, NULL, 0, 0);
	extraBar.BarValue = fillAmount;
	extraBar.BarFillDirection = fillDirection;
	extraBar.BarParentAnchor = parentAnchor;
	extraBar.BarColor = color;

	return extraBar.Bar;
}

extern "C" void __stdcall SetUnitExtraBarFillAmount(UINT unit, UINT bar, float fillAmount) {
	UnitsData[unit].ExtraBars[bar].BarValue = fillAmount;
}

extern "C" void __stdcall SetUnitExtraBarColor(UINT unit, UINT bar, DWORD color) {
	UnitsData[unit].ExtraBars[bar].BarColor = color;
}

extern "C" void __stdcall SetUnitExtraBarAnchors(UINT unit, UINT bar, UINT fillDirection, UINT parentAnchor) {
	UnitsData[unit].ExtraBars[bar].BarFillDirection = fillDirection;
	UnitsData[unit].ExtraBars[bar].BarParentAnchor = parentAnchor;
}

extern "C" void __stdcall SetUnitExtraBarPriority(UINT unit, UINT bar, UINT priority) {
	UnitsData[unit].ExtraBars[bar].BarPriority = priority;
}

extern "C" void __stdcall RemoveUnitExtraBar(UINT unit, UINT bar) {
	UnitsData[unit].ExtraBars[bar].Bar ? StatBarDestructorCustom(UnitsData[unit].ExtraBars[bar].Bar, NULL, TRUE) : NULL;
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

		//AttachDetour(ShowHPBar, ShowHPBarCustom);
		AttachDetour(ShowStatBar, ShowStatBarCustom);
		AttachDetour(HideStatBar, HideStatBarCustom);
		AttachDetour(UpdateHPBar, UpdateHPBarCustom);
		AttachDetour(UnitDataUpdate, UnitDataUpdateCustom);
		AttachDetour(UnitDestructor, UnitDestructorCustom);
		AttachDetour(StatBarDestructor, StatBarDestructorCustom);
		AttachDetour(SetJassState, SetJassStateCustom);

		DetourTransactionCommit();

		break;
	case DLL_PROCESS_DETACH:
		DetourTransactionBegin();

		//DetachDetour(ShowHPBar, ShowHPBarCustom);
		DetachDetour(ShowStatBar, ShowStatBarCustom);
		DetachDetour(HideStatBar, HideStatBarCustom);
		DetachDetour(UpdateHPBar, UpdateHPBarCustom);
		DetachDetour(UnitDataUpdate, UnitDataUpdateCustom);
		DetachDetour(UnitDestructor, UnitDestructorCustom);
		DetachDetour(StatBarDestructor, StatBarDestructorCustom);
		DetachDetour(SetJassState, SetJassStateCustom);

		DetourTransactionCommit();

		break;
	}
	return TRUE;
}

//---------------------------------------------------

BOOL __fastcall ShowStatBarCustom(UINT frame) {
	UINT unit = GetUnitByStatBar(frame);
	if (UnitsData.find(unit) != UnitsData.end()) {
		auto& extraBars = UnitsData[unit].ExtraBars;
		for (const auto& pairOfBarData : extraBars) {
			const CExtraBar& barData = pairOfBarData.second;
			barData.Bar ? ShowStatBar(barData.Bar) : NULL;
		}
	}

	return ShowStatBar(frame);
}

BOOL __fastcall HideStatBarCustom(UINT frame) {
	UINT unit = GetUnitByStatBar(frame);
	if (UnitsData.find(unit) != UnitsData.end()) {
		auto& extraBars = UnitsData[unit].ExtraBars;
		for (const auto& pairOfBarData : extraBars) {
			const CExtraBar& barData = pairOfBarData.second;
			barData.Bar ? HideStatBar(barData.Bar) : NULL;
		}
	}

	return HideStatBar(frame);
}

//void __fastcall ShowHPBarCustom(CPreselectUI* PreselectUI, LPVOID, UINT Unit, BOOL unk) {
//	ShowHPBar(PreselectUI, Unit, unk);
//}

void __fastcall UpdateHPBarCustom(UINT StatBar, LPVOID, UINT unk) {
	UpdateHPBar(StatBar, unk);

	if (!((UINT*)StatBar)[27]) {
		UINT SimpleTexture = GetSimpleTextureByStatBar(StatBar);
		if (SimpleTexture) {
			UINT unit = GetUnitBySimpleTexture(SimpleTexture);
			if (unit) {
				SetFrameHeight(StatBar, 0.004f); // Default

				int playerOwnerNum = GetUnitOwnerNumber(unit);
				if (playerOwnerNum >= 0 && playerOwnerNum <= 16) {
					HANDLE localPlayer = GetLocalPlayer();
					HANDLE ownerPlayer = Player(playerOwnerNum);

					DWORD color = NULL;
					if (localPlayer == ownerPlayer) color = PlayerColor;
					else if (IsPlayerAlly(localPlayer, ownerPlayer)) color = AlliesColor;
					else if (IsPlayerEnemy(localPlayer, ownerPlayer)) color = EnemiesColor;

					SetSimpleTextureColor(SimpleTexture, &color);
				}
				
				if (UnitsData.find(unit) != UnitsData.end()) {
					CUnitData& data = UnitsData[unit];
					data.HPBarColor ? SetSimpleTextureColor(SimpleTexture, &data.HPBarColor) : NULL;
					data.HPBarWidth ? SetFrameWidth(StatBar, data.HPBarWidth) : NULL;
					data.HPBarHeight ? SetFrameHeight(StatBar, data.HPBarHeight) : NULL;
				}
			}
		}
	}
}

void __fastcall UnitDataUpdateCustom(UINT Unit) {
	CPreselectUI* preselectUI = GetUnitPreselectUI(Unit);
	if (preselectUI) {
		if (UnitsData.find(Unit) != UnitsData.end()) {
			UINT priority = 1;
			for (auto& pairOfBarData : UnitsData[Unit].ExtraBars) {
				CExtraBar& barData = pairOfBarData.second;
				if (barData.Bar) {
					UINT statBar = preselectUI->StatBar;
					float displayValue = barData.BarValue > 1.f ? 1.f : barData.BarValue;
					float width = GetFrameWidth(statBar);

					SetFrameWidth(barData.Bar, width);
					SetFrameHeight(barData.Bar, GetFrameHeight(statBar));
					SetStatBarValue(barData.Bar, displayValue);

					SetSimpleTextureColor(GetSimpleTextureByStatBar(barData.Bar), &barData.BarColor);
					((DWORD*)((UINT*)barData.Bar)[81])[35] = NULL; // background

					SetCSimpleFramePriority(barData.Bar, barData.BarPriority >= 0 && barData.BarPriority <= 9 ? barData.BarPriority : ++priority);

					float statBarX = GetCLayoutFrameAbsolutePointX(statBar);
					float x = statBarX;
					float y = GetCLayoutFrameAbsolutePointY(statBar);
					//GetUnitFramePosition(Unit, &x, &y);

					switch (barData.BarParentAnchor) {
					case 0: // Left
						if (barData.BarFillDirection == 0) { // LeftDirection
							x -= width * GetStatBarValue(barData.Bar);
						}
						else {  // RightDirection

						}

						break;
					case 1: // Mid
						if (barData.BarFillDirection == 0) { // LeftDirection
							x += width * (GetStatBarValue(statBar) - GetStatBarValue(barData.Bar));
							x = x - statBarX < 0 ? statBarX : x;
						}
						else {  // RightDirection
							x += width * GetStatBarValue(statBar);
							x = x + width * GetStatBarValue(barData.Bar) > statBarX + width ? statBarX + width - width * GetStatBarValue(barData.Bar) : x;

						}

						break;
					case 2: // Right
						if (barData.BarFillDirection == 0) { // LeftDirection
							x += width - width * GetStatBarValue(barData.Bar);
						}
						else {  // RightDirection
							x += width;
						}

						break;
					}

					//x += width * (GetStatBarValue(statBar) - GetStatBarValue(data.ArmorBar));

					//x = x - statBarX < 0 ? statBarX : x;

					SetCLayoutFrameAbsolutePoint(barData.Bar, NULL, FRAMEPOINT_TOP, x, y, 1);
					//this_call<BOOL>((*(LPVOID**)barData.Bar)[26], barData.Bar);
				}
			}
		}
	}

	UnitDataUpdate(Unit);
}

BOOL __fastcall UnitDestructorCustom(UINT Unit, LPVOID, UINT unk1) {
	if (UnitsData.find(Unit) != UnitsData.end()) {
		for (auto& pairOfBarData : UnitsData[Unit].ExtraBars) {
			CExtraBar& barData = pairOfBarData.second;
			if (barData.Bar) {
				StatBarDestructorCustom(barData.Bar, NULL, TRUE);
				barData.Bar = NULL;
			}
		}

		UnitsData.erase(Unit);
	}
	//StatBarDestructor

	return UnitDestructor(Unit, unk1);
}

BOOL __fastcall StatBarDestructorCustom(UINT frame, LPVOID, BOOL flag) {
	for (auto& unitData : UnitsData) {
		auto& extraBars = unitData.second.ExtraBars;
		extraBars[frame].Bar = NULL;
		extraBars.erase(frame);
	}

	return StatBarDestructor(frame, flag);
}

void __fastcall SetJassStateCustom(BOOL jassState) {
	if (jassState == TRUE && thismodule) {
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)FreeLibrary, thismodule, NULL, NULL);
	}

	return SetJassState(jassState);
}

//---------------------------------------------------

float GetCLayoutFrameAbsolutePointX(UINT frame) {
	return frame ? ((PVOID*)frame)[3] ? ((float**)frame)[3][1] : NULL : NULL;
}

float GetCLayoutFrameAbsolutePointY(UINT frame) {
	return frame ? ((PVOID*)frame)[3] ? ((float**)frame)[3][2] : NULL : NULL;
}

void SetFramePriority(UINT frame, UINT priority) {
	if (frame) {
		this_call<void>((*(LPVOID**)frame)[25], frame);
		((UINT*)frame)[33] = priority; // 42
		this_call<void>((*(LPVOID**)frame)[26], frame);
	}
}

UINT GetFramePriority(UINT frame) {
	return frame ? ((UINT*)frame)[33] : NULL; // 42
}

float GetStatBarValue(UINT frame) {
	return frame ? ((float*)frame)[76] : NULL;
}

UINT GetSimpleTextureByStatBar(UINT frame) {
	return frame ? ((UINT*)frame)[77] : NULL;
}

DWORD GetSimpleTextureColor(UINT frame) {
	return frame ? ((UINT*)frame)[26] : NULL;
}

CPreselectUI* GetUnitPreselectUI(UINT unit) {
	return unit ? ((CPreselectUI**)unit)[20] : NULL;
}

int GetUnitOwnerNumber(UINT unit) {
	return unit ? ((UINT*)unit)[22] : -1;
}

UINT GetUnitBySimpleTexture(UINT frame) {
	return frame ? ((LPVOID*)frame)[29] && *((LPVOID**)frame)[29] == (LPVOID)(gameBase + 0x93E604) ? ((UINT*)(((LPVOID*)frame)[29]))[85] : NULL : NULL;
}

UINT GetUnitByStatBar(UINT frame) {
	return frame ? ((UINT*)frame)[85] : NULL;
}

float GetFrameWidth(UINT frame) {
	return frame ? ((float*)frame)[22] : NULL;
}

float GetFrameHeight(UINT frame) {
	return frame ? ((float*)frame)[23] : NULL;
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