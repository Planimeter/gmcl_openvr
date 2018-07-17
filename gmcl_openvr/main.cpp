#include <Windows.h>
#include <GarrysMod/Lua/Interface.h>
#include <MinHook.h>
#include <d3d9.h>
#include <d3dx9.h>
#include "OpenVR-DirectMode.h"

#define VR_TO_HAMMER 52.52100840336134
#define EPSILON 0.0001

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

	return Present_orig(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);;
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

	vector.x = matrix.m[0][3] * VR_TO_HAMMER;
	vector.y = -matrix.m[2][3] * VR_TO_HAMMER;
	vector.z = matrix.m[1][3] * VR_TO_HAMMER;

	return vector;
}

QAngle GetRotation(vr::HmdMatrix34_t matrix)
{	
	QAngle a;

	Vector vr_forward;
	vr_forward.x = -matrix.m[0][2];
	vr_forward.y = -matrix.m[1][2];
	vr_forward.z = -matrix.m[2][2];

	Vector vr_right;
	vr_right.x = matrix.m[0][0];
	vr_right.y = matrix.m[1][0];
	vr_right.z = matrix.m[2][0];

	Vector vr_up;
	vr_up.x = matrix.m[0][1];
	vr_up.y = matrix.m[1][1];
	vr_up.z = matrix.m[3][1];

	Vector forward;
	forward.x = vr_forward.x;
	forward.y = -vr_forward.z;
	forward.z = vr_forward.y;

	Vector right;
	right.x = vr_right.x;
	right.y = -vr_right.z;
	right.z = vr_right.y;

	Vector up;
	up.x = vr_up.x;
	up.y = -vr_up.z;
	up.z = vr_up.y;

	float sr, sp, sy, cr, cp, cy;

	sp = -forward.z;

	float cp_x_cy = forward.x;
	float cp_x_sy = forward.y;
	float cp_x_sr = -right.z;
	float cp_x_cr = up.z;

	float yaw = atan2(cp_x_sy, cp_x_cy);
	float roll = atan2(cp_x_sr, cp_x_cr);

	cy = cos(yaw);
	sy = sin(yaw);
	cr = cos(roll);
	sr = sin(roll);

	if (abs(cy) > EPSILON)
		cp = cp_x_cy / cy;
	else if (abs(sy) > EPSILON)
		cp = cp_x_sy / sy;
	else if (abs(sr) > EPSILON)
		cp = cp_x_sr / sr;
	else if (abs(cr) > EPSILON)
		cp = cp_x_cr / cr;
	else
		cp = cos(asin(sp));

	float pitch = atan2(sp, cp);

	float pi = 3.14159265359;

	a.x = -(180 - (360 - (pitch / (pi * 2 / 360))));
	a.y = -(180 - (yaw / (pi * 2 / 360)));
	a.z = -(180 - (roll / (pi * 2 / 360)));

	return a;
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
	
	vr::TrackedDeviceIndex_t index = LUA->CheckNumber( 1 );
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
		//PushMatrix(LUA, pose->mDeviceToAbsoluteTracking);
		//LUA->SetField(-2, "pose");

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

	vr::TrackedDeviceIndex_t index = LUA->CheckNumber(1);
	vr::VRControllerState_t state;
	
	if (!forwarder->GetControllerState(index, &state))
		return 0;

	LUA->CreateTable();
		LUA->PushNumber(state.unPacketNum);
		LUA->SetField(-2, "packet");

		LUA->CreateTable();
			for (int i = 0; i < 64; i++) {
				LUA->PushNumber(i + 1);
				LUA->PushBool( state.ulButtonPressed & (1ULL << i) );
				LUA->SetTable(-3);
			}
		LUA->SetField(-2, "pressed");

		LUA->CreateTable();
			for (int i = 0; i < 64; i++) {
				LUA->PushNumber(i + 1);
				LUA->PushBool(state.ulButtonTouched & (1ULL << i) );
				LUA->SetTable(-3);
			}
		LUA->SetField(-2, "touched");

		LUA->CreateTable();
			for (uint32_t i = 0; i < vr::k_unControllerStateAxisCount; i++) {
				LUA->PushNumber(i + 1);
				LUA->CreateTable();
					LUA->PushNumber(state.rAxis[i].x);
					LUA->SetField(-2, "x");

					LUA->PushNumber(state.rAxis[i].y);
					LUA->SetField(-2, "y");
				LUA->SetTable(-3);
			}
		LUA->SetField(-2, "axes");
	return 1;
}

LUA_FUNCTION(Submit) {
	if (surface) {
		forwarder->PrePresentEx(surface);
		forwarder->PostPresentEx(surface);
	}

	return 0;
}

LUA_FUNCTION(Update) {
	if (!forwarder)
		return 0;

	vr::VREvent_t e;
	while (forwarder->PollNextEvent(&e, sizeof(e))) {};

	return 0;
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

			LUA->PushCFunction(Update);
			LUA->SetField(-2, "Update");

			LUA->PushCFunction(Submit);
			LUA->SetField(-2, "Submit");
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