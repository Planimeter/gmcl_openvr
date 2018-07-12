#include <iostream>
#include "OpenVRForwarder.h"

OpenVRForwarder::OpenVRForwarder()
{
	if (!initDirectX())
		MessageBoxA(NULL, "Failed to create DirectX 11 device", "Error", MB_OK);

	if (!initOpenVR())
		MessageBoxA(NULL, "Failed to initialize OpenVR", "Error", MB_OK);

	if (!initResources())
		MessageBoxA(NULL, "Failed to initialize resources", "Error", MB_OK);
}

OpenVRForwarder::~OpenVRForwarder()
{
}

bool OpenVRForwarder::initDirectX()
{
	if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, NULL, D3D11_SDK_VERSION, &m_device, NULL, &m_context)))
		return false;

	return true;
}

bool OpenVRForwarder::initOpenVR()
{
	m_openvr = vr::VR_Init(NULL, vr::EVRApplicationType::VRApplication_Scene);

	if (!m_openvr)
		return false;

	return true;
}

bool OpenVRForwarder::initResources()
{
	return true;
}

void OpenVRForwarder::render() {
	vr::VRCompositor()->WaitGetPoses(NULL, 0, NULL, 0);

	if (!m_texture) {
		ID3D11Resource* res;
		if (FAILED(m_device->OpenSharedResource(shared_handle, __uuidof(ID3D11Resource), (void**)&res))) {
			MessageBoxA(NULL, "OpenSharedResource failed", "err", MB_OK);
			return;
		}

		if (FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&m_texture))) {
			MessageBoxA(NULL, "QueryInterface", "err", MB_OK);
			return;
		}
	}

	vr::Texture_t eye = { m_texture, vr::TextureType_DirectX, vr::ColorSpace_Auto };
	vr::VRCompositor()->Submit(vr::EVREye::Eye_Left, &eye);
	vr::VRCompositor()->Submit(vr::EVREye::Eye_Right, &eye);
}

void OpenVRForwarder::setSharedHandle( HANDLE handle ) {
	shared_handle = handle;
}