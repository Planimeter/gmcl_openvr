#include <Windows.h>
#include <GarrysMod/Lua/Interface.h>
#include <MinHook.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "OpenVR-DirectMode.h"

bool searching = false;
IDirect3DSurface9* surface;

IDirect3DDevice9* device;

typedef HRESULT(APIENTRY* Present) (IDirect3DDevice9*, RECT*, RECT*, HWND, RGNDATA* );
Present Present_orig = 0;

typedef HRESULT(APIENTRY* CreateTexture) (IDirect3DDevice9*, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture9**, HANDLE*);
CreateTexture CreateTexture_orig = 0;

OpenVRDirectMode* forwarder;

HRESULT APIENTRY Present_hook(IDirect3DDevice9* pDevice, RECT* pSourceRect, RECT* pDestRect, HWND hDestWindowOverride, RGNDATA* pDirtyRegion) {
	if (!forwarder) {
		IDirect3DDevice9Ex* dx9ex;
		
		if (SUCCEEDED(pDevice->QueryInterface(__uuidof(IDirect3DDevice9Ex), (void**)&dx9ex))) {
			forwarder = new OpenVRDirectMode();
			forwarder->Init(dx9ex);
		}
	}

	if (surface)
		forwarder->PrePresentEx(surface);

	HRESULT hr = Present_orig(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	if (surface)
		forwarder->PostPresentEx(surface);

	return hr;
}

HRESULT APIENTRY CreateTexture_hook(IDirect3DDevice9* pDevice, UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT format, D3DPOOL pool, IDirect3DTexture9** tex, HANDLE* shared_handle) {
	HRESULT res = CreateTexture_orig(pDevice, w, h, levels, usage, format, pool, tex, shared_handle);

	if ( !surface && searching ) {
		if (FAILED(res))
			return res;
	
		IDirect3DTexture9* reference = *tex;

		if ( FAILED( reference->GetSurfaceLevel(0, &surface) ) )
			return res;

		searching = false;
	}

	return res;
};

bool HookDirectX() {
	IDirect3D9* dx = NULL;

	HWND window = CreateWindowA( "BUTTON", "Hooking...", WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 100, 100, NULL, NULL, GetModuleHandle(NULL), NULL);

	if (!window) {
		MessageBoxA(NULL, "Failed to create window", "Error", MB_OK);
		return false;
	}
	
	dx = Direct3DCreate9(D3D_SDK_VERSION);

	if (!dx) {
		MessageBoxA(NULL, "Failed to create DX interface", "Error", MB_OK);

		DestroyWindow(window);
		return false;
	}

	D3DPRESENT_PARAMETERS params;
	ZeroMemory( &params, sizeof(params) );
	params.Windowed = TRUE;
	params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	params.hDeviceWindow = window;
	params.BackBufferFormat = D3DFMT_UNKNOWN;

	HRESULT res = dx->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &params, &device);

	if (FAILED(res)) {
		MessageBoxA(NULL, "Failed to create device", "Error", MB_OK);

		dx->Release();
		DestroyWindow(window);
		return false;
	}

	#if defined _M_X64
		DWORD64* dVtable = (DWORD64*)device;
		dVtable = (DWORD64*)dVtable[0];
	#elif defined _M_IX86
		DWORD* dVtable = (DWORD*)device;
		dVtable = (DWORD*)dVtable[0]; // == *d3ddev
	#endif

	CreateTexture_orig = (CreateTexture)dVtable[23];

	if (MH_Initialize() != MH_OK) {
		MessageBoxA(NULL, "Failed to initialize MinHook", "Error", MB_OK);
		return false;
	}

	if (MH_CreateHook((DWORD_PTR*)dVtable[17], &Present_hook, reinterpret_cast<void**>(&Present_orig)) != MH_OK) {
		MessageBoxA(NULL, "Failed to create Present hook", "Error", MB_OK);
		return false;
	}

	if (MH_EnableHook((DWORD_PTR*)dVtable[17]) != MH_OK) {
		MessageBoxA(NULL, "Failed to enable Present hook", "Error", MB_OK);
		return false;
	}

	if (MH_CreateHook((DWORD_PTR*)dVtable[23], &CreateTexture_hook, reinterpret_cast<void**>(&CreateTexture_orig)) != MH_OK) {
		MessageBoxA(NULL, "Failed to create CreateTexture hook", "Error", MB_OK);
		return false;
	}

	if (MH_EnableHook((DWORD_PTR*)dVtable[23]) != MH_OK) {
		MessageBoxA(NULL, "Failed to enable CreateTexture hook", "Error", MB_OK);
		return false;
	}

	dx->Release();
	DestroyWindow( window );
}

LUA_FUNCTION(CaptureTexture) {
	searching = true;
	return 0;
}

LUA_FUNCTION(GetViewParameters) {
	if (!forwarder)
		return 0;

	uint32_t w;
	uint32_t h;
	float aspect;
	float fov;

	forwarder->GetViewParameters(&w, &h, &aspect, &fov);

	LUA->PushNumber(w);
	LUA->PushNumber(h);
	LUA->PushNumber(aspect);
	LUA->PushNumber(fov);

	return 4;
}

void PushMatrix( GarrysMod::Lua::ILuaBase* LUA, vr::HmdMatrix34_t matrix) {
	LUA->CreateTable();
	for (int i = 0; i < 3; i++) {
		LUA->PushNumber(i + 1);
		LUA->CreateTable();

		for (int j = 0; j < 4; j++) {
			LUA->PushNumber(j + 1);
			LUA->PushNumber(matrix.m[i][j]);
			LUA->SetTable(-3);
		}

		LUA->SetTable(-3);
	}
}

Vector GetPosition(vr::HmdMatrix34_t matrix)
{
	Vector vector;

	vector.x = -matrix.m[2][3] * 52.52100840336134;
	vector.y = -matrix.m[0][3] * 52.52100840336134;
	vector.z = matrix.m[1][3] * 52.52100840336134;

	return vector;
}

QAngle GetRotation(vr::HmdMatrix34_t matrix)
{	
	vr::HmdQuaternion_t q;

	q.x = sqrt(fmax(0, 1 + matrix.m[0][0] - matrix.m[1][1] - matrix.m[2][2])) / 2;
	q.y = sqrt(fmax(0, 1 - matrix.m[0][0] + matrix.m[1][1] - matrix.m[2][2])) / 2;
	q.z = sqrt(fmax(0, 1 - matrix.m[0][0] - matrix.m[1][1] + matrix.m[2][2])) / 2;
	q.x = copysign(q.x, (double)matrix.m[2][1] - (double)matrix.m[1][2]);
	q.y = copysign(q.y, (double)matrix.m[0][2] - (double)matrix.m[2][0]);
	q.z = copysign(q.z, (double)matrix.m[1][0] - (double)matrix.m[0][1]);

	QAngle a;

	a.x = q.x;
	a.y = q.y;
	a.z = q.z;
}

LUA_FUNCTION(Ready) {
	LUA->PushBool(forwarder != NULL);
	return 1;
}

LUA_FUNCTION(GetDeviceIndex) {
	if (!forwarder)
		return 0;

	int id = LUA->CheckNumber(1);

	switch (id) {
	case 1: LUA->PushNumber(vr::k_unTrackedDeviceIndex_Hmd); break; // headset
	case 2: LUA->PushNumber(forwarder->GetControllerIndex(vr::ETrackedControllerRole::TrackedControllerRole_LeftHand)); break; // left controller
	case 3: LUA->PushNumber(forwarder->GetControllerIndex(vr::ETrackedControllerRole::TrackedControllerRole_RightHand)); break; // right controller
	default: LUA->PushNumber(-1); break;
	}

	return 1;
}

LUA_FUNCTION(GetDevicePose) {
	if (!forwarder)
		return 0;
	
	vr::TrackedDeviceIndex_t index = LUA->CheckNumber();
	vr::TrackedDevicePose_t* pose = forwarder->GetDevicePose(index);

	Vector pos = GetPosition(pose->mDeviceToAbsoluteTracking);
	QAngle ang = GetRotation(pose->mDeviceToAbsoluteTracking);

	Vector vel;
	vel.x = pose->vVelocity.v[0];
	vel.y = pose->vVelocity.v[1];
	vel.z = pose->vVelocity.v[2];

	QAngle angvel;
	vel.x = pose->vAngularVelocity.v[0];
	vel.y = pose->vAngularVelocity.v[1];
	vel.z = pose->vAngularVelocity.v[2];

	LUA->CreateTable();
		PushMatrix(LUA, pose->mDeviceToAbsoluteTracking);
		LUA->SetField(-2, "pose");

		LUA->PushVector(pos);
		LUA->SetField(-2, "pos");

		LUA->PushAngle(ang);
		LUA->SetField(-2, "ang");

		LUA->PushVector(vel);
		LUA->SetField(-2, "vel");

		LUA->PushAngle(angvel);
		LUA->SetField(-2, "angvel");
	return 1;
}

LUA_FUNCTION(GetControllerState) {
	if (!forwarder)
		return 0;

	vr::TrackedDeviceIndex_t index = LUA->CheckNumber();
	vr::VRControllerState_t* state = forwarder->GetControllerState(index);

	// convert to lua
}

GMOD_MODULE_OPEN() {
	HookDirectX();

	LUA->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		LUA->CreateTable();
			LUA->PushCFunction(Ready);
			LUA->SetField(-2, "Ready");

			LUA->PushCFunction(GetViewParameters);
			LUA->SetField(-2, "GetViewParameters");

			LUA->PushCFunction(CaptureTexture);
			LUA->SetField(-2, "CaptureTexture");

			LUA->PushCFunction(GetDeviceIndex);
			LUA->SetField(-2, "GetDeviceIndex");

			LUA->PushCFunction(GetDevicePose);
			LUA->SetField(-2, "GetDevicePose");

			LUA->PushCFunction(GetControllerState);
			LUA->SetField(-2, "GetControllerState");
		LUA->SetField(-2, "vr");
	LUA->Pop();

	return 0;
}

GMOD_MODULE_CLOSE() {
	#if defined _M_X64
		DWORD64* dVtable = (DWORD64*)device;
		dVtable = (DWORD64*)dVtable[0];
	#elif defined _M_IX86
		DWORD* dVtable = (DWORD*)device;
		dVtable = (DWORD*)dVtable[0]; // == *d3ddev
	#endif

	MH_DisableHook((DWORD_PTR*)dVtable[17]);
	MH_DisableHook((DWORD_PTR*)dVtable[23]);
	MH_RemoveHook((DWORD_PTR*)dVtable[17]);
	MH_RemoveHook((DWORD_PTR*)dVtable[23]);

	return 0;
}