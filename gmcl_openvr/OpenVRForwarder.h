#pragma once

#include <openvr.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dx10.h>

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "d3dx11.lib")
#pragma comment (lib, "d3dx10.lib")
#pragma comment (lib, "openvr_api.lib")

class OpenVRForwarder {
private:
	vr::IVRSystem* m_openvr;

	ID3D11DeviceContext* m_context;
	ID3D11Device* m_device;
	ID3D11Texture2D* m_texture;
	HANDLE shared_handle;

	bool initDirectX();
	bool initOpenVR();
	bool initResources();

public:
	OpenVRForwarder();
	~OpenVRForwarder();
	void render();
	void setSharedHandle( HANDLE );
};