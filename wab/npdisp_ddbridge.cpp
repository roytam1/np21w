/**
 * @file	npdisp_ddbridge.cpp
 * @brief	NPDISP DirectDraw 32bit HAL bridge
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP)

#include	<map>
#include	<vector>

#include	"pccore.h"

#include	"npdispdef.h"
#include	"npdisp.h"
#include	"npdisp_mem.h"
#include	"npdisp_ddbridge.h"

static bool npdisp_ddbridge_writeRequestMasks(UINT32 ddCallbacksAddr, UINT32 ddSurfaceCallbacksAddr, UINT32 ddPaletteCallbacksAddr)
{
	return npdisp_writeMemory32(NPDISP_DDBRIDGE_DD_REQUEST_MASK, ddCallbacksAddr + offsetof(NPDISP_DDHAL_DDCALLBACKS, dwFlags)) &&
		npdisp_writeMemory32(NPDISP_DDBRIDGE_SURFACE_REQUEST_MASK, ddSurfaceCallbacksAddr + offsetof(NPDISP_DDHAL_DDSURFACECALLBACKS, dwFlags)) &&
		npdisp_writeMemory32(NPDISP_DDBRIDGE_PALETTE_REQUEST_MASK, ddPaletteCallbacksAddr + offsetof(NPDISP_DDHAL_DDPALETTECALLBACKS, dwFlags));
}

static bool npdisp_ddbridge_clearBootstrapHalInfo(UINT32 ddHalInfoAddr)
{
	return npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDCallbacksAddr)) &&
		npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDSurfaceCallbacksAddr)) &&
		npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDPaletteCallbacksAddr)) &&
		npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, GetDriverInfoAddr)) &&
		npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, hInstance)) &&
		npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpD3DGlobalDriverData)) &&
		npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpD3DHALCallbacks)) &&
		npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDExeBufCallbacksAddr));
}

static bool npdisp_ddbridge_prepareV2(UINT32 ddHalInfoAddr, UINT32 ddCallbacksAddr, UINT32 ddSurfaceCallbacksAddr, UINT32 ddPaletteCallbacksAddr, UINT32 ddBridgeInfoAddr, UINT32 d3dGlobalDriverDataAddr, UINT32 d3dHalCallbacksAddr, UINT32 *linearContext)
{
	NPDISP_DDBRIDGEINFO32 bridge = { 0 };
	UINT32 linearHalInfo = 0;
	UINT32 linearCallbacks = 0;
	UINT32 linearSurfaceCallbacks = 0;
	UINT32 linearPaletteCallbacks = 0;
	UINT32 linearBridgeInfo = 0;
	UINT32 linearD3DGlobalDriverData = 0;
	UINT32 linearD3DHalCallbacks = 0;

	if (!ddBridgeInfoAddr ||
		!npdisp_memory_getLinearAddress(ddHalInfoAddr, &linearHalInfo) ||
		!npdisp_memory_getLinearAddress(ddCallbacksAddr, &linearCallbacks) ||
		!npdisp_memory_getLinearAddress(ddSurfaceCallbacksAddr, &linearSurfaceCallbacks) ||
		!npdisp_memory_getLinearAddress(ddPaletteCallbacksAddr, &linearPaletteCallbacks) ||
		!npdisp_memory_getLinearAddress(ddBridgeInfoAddr, &linearBridgeInfo)) {
		return false;
	}

	UINT32 hostFeatures = NPDISP_DDBRIDGE_V2_HOST_FEATURES;
	UINT32 d3dProfile = NPDISP_DDBRIDGE_V2_D3D_PROFILE;
	if (npdisp.version < 16) {
		hostFeatures &= ~(NPDISP_DDBRIDGE_FEATURE_D3D_HAL | NPDISP_DDBRIDGE_FEATURE_D3D_SHARED_DATA | NPDISP_DDBRIDGE_FEATURE_D3D_HOST_METADATA);
		d3dProfile = 0;
	}
	if ((hostFeatures & NPDISP_DDBRIDGE_FEATURE_D3D_HAL) &&
		(!d3dGlobalDriverDataAddr || !d3dHalCallbacksAddr ||
		 !npdisp_memory_getLinearAddress(d3dGlobalDriverDataAddr, &linearD3DGlobalDriverData) ||
		 !npdisp_memory_getLinearAddress(d3dHalCallbacksAddr, &linearD3DHalCallbacks))) {
		hostFeatures &= ~(NPDISP_DDBRIDGE_FEATURE_D3D_HAL | NPDISP_DDBRIDGE_FEATURE_D3D_SHARED_DATA | NPDISP_DDBRIDGE_FEATURE_D3D_HOST_METADATA);
		d3dProfile = 0;
		linearD3DGlobalDriverData = 0;
		linearD3DHalCallbacks = 0;
	}

	if (!npdisp_ddbridge_writeRequestMasks(ddCallbacksAddr, ddSurfaceCallbacksAddr, ddPaletteCallbacksAddr) ||
		!npdisp_ddbridge_clearBootstrapHalInfo(ddHalInfoAddr)) {
		return false;
	}

	bridge.dwSize = sizeof(bridge);
	bridge.dwMagic = NPDISP_DDBRIDGE_V2_MAGIC;
	bridge.dwAbiVersion = NPDISP_DDBRIDGE_ABI_V2;
	bridge.dwStatus = NPDISP_DDBRIDGE_STATUS_HOST_READY;
	bridge.dwHostFeaturesOffered = hostFeatures;
	bridge.dwD3DProfileId = d3dProfile;
	bridge.lpDDHalInfo = linearHalInfo;
	bridge.lpDDCallbacks = linearCallbacks;
	bridge.lpDDSurfaceCallbacks = linearSurfaceCallbacks;
	bridge.lpDDPaletteCallbacks = linearPaletteCallbacks;
	bridge.dwDDRequestMask = NPDISP_DDBRIDGE_DD_REQUEST_MASK;
	bridge.dwSurfaceRequestMask = NPDISP_DDBRIDGE_SURFACE_REQUEST_MASK;
	bridge.dwPaletteRequestMask = NPDISP_DDBRIDGE_PALETTE_REQUEST_MASK;
	bridge.lpD3DGlobalDriverData = linearD3DGlobalDriverData;
	bridge.lpD3DHALCallbacks = linearD3DHalCallbacks;

	if (!npdisp_writeMemory(&bridge, ddBridgeInfoAddr, sizeof(bridge))) {
		return false;
	}
	*linearContext = linearBridgeInfo;
	return true;
}

static bool npdisp_ddbridge_prepareLegacyV1(UINT32 ddHalInfoAddr, UINT32 ddCallbacksAddr, UINT32 ddSurfaceCallbacksAddr, UINT32 ddPaletteCallbacksAddr, UINT32 *linearContext)
{
	UINT32 linearCallbacks = 0;
	UINT32 linearSurfaceCallbacks = 0;
	UINT32 linearPaletteCallbacks = 0;

	if (!npdisp_memory_getLinearAddress(ddHalInfoAddr, linearContext) ||
		!npdisp_memory_getLinearAddress(ddCallbacksAddr, &linearCallbacks) ||
		!npdisp_memory_getLinearAddress(ddSurfaceCallbacksAddr, &linearSurfaceCallbacks) ||
		!npdisp_memory_getLinearAddress(ddPaletteCallbacksAddr, &linearPaletteCallbacks)) {
		return false;
	}

	if (!npdisp_ddbridge_writeRequestMasks(ddCallbacksAddr, ddSurfaceCallbacksAddr, ddPaletteCallbacksAddr)) {
		return false;
	}

	return npdisp_writeMemory32(linearCallbacks, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDCallbacksAddr)) &&
		npdisp_writeMemory32(linearSurfaceCallbacks, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDSurfaceCallbacksAddr)) &&
		npdisp_writeMemory32(linearPaletteCallbacks, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDPaletteCallbacksAddr)) &&
		npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, GetDriverInfoAddr)) &&
		npdisp_writeMemory32(0, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, hInstance)) &&
		npdisp_writeMemory32(NPDISP_DDBRIDGE_V1_REQUEST_MAGIC, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpD3DGlobalDriverData)) &&
		npdisp_writeMemory32(NPDISP_DDBRIDGE_ABI_V1, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpD3DHALCallbacks)) &&
		npdisp_writeMemory32(NPDISP_DDBRIDGE_V1_REQUEST_FEATURES, ddHalInfoAddr + offsetof(NPDISP_DDHALINFO, lpDDExeBufCallbacksAddr));
}

bool npdisp_ddbridge_prepareDriverInit(UINT32 ddHalInfoAddr, UINT32 ddCallbacksAddr, UINT32 ddSurfaceCallbacksAddr, UINT32 ddPaletteCallbacksAddr, UINT32 ddBridgeInfoAddr, UINT32 d3dGlobalDriverDataAddr, UINT32 d3dHalCallbacksAddr, UINT32 *linearContext)
{
	if (!linearContext || !ddHalInfoAddr || !ddCallbacksAddr || !ddSurfaceCallbacksAddr || !ddPaletteCallbacksAddr) {
		return false;
	}
	*linearContext = 0;

	if (npdisp.version >= 15) {
		return npdisp_ddbridge_prepareV2(ddHalInfoAddr, ddCallbacksAddr, ddSurfaceCallbacksAddr, ddPaletteCallbacksAddr, ddBridgeInfoAddr, d3dGlobalDriverDataAddr, d3dHalCallbacksAddr, linearContext);
	}
	return npdisp_ddbridge_prepareLegacyV1(ddHalInfoAddr, ddCallbacksAddr, ddSurfaceCallbacksAddr, ddPaletteCallbacksAddr, linearContext);
}

static bool npdisp_ddbridge_validateV2(UINT32 ddHalInfoAddr, UINT32 ddCallbacksAddr, UINT32 ddSurfaceCallbacksAddr, UINT32 ddPaletteCallbacksAddr, UINT32 ddBridgeInfoAddr, UINT32 d3dGlobalDriverDataAddr, UINT32 d3dHalCallbacksAddr, UINT32 *negotiatedFeatures)
{
	NPDISP_DDBRIDGEINFO32 bridge = { 0 };
	UINT32 linearHalInfo = 0;
	UINT32 linearCallbacks = 0;
	UINT32 linearSurfaceCallbacks = 0;
	UINT32 linearPaletteCallbacks = 0;
	UINT32 linearD3DGlobalDriverData = 0;
	UINT32 linearD3DHalCallbacks = 0;

	if (!ddBridgeInfoAddr || !npdisp_readMemory(&bridge, ddBridgeInfoAddr, sizeof(bridge))) {
		return false;
	}
	if (bridge.dwSize < sizeof(NPDISP_DDBRIDGEINFO32) ||
		bridge.dwMagic != NPDISP_DDBRIDGE_V2_MAGIC ||
		(bridge.dwAbiVersion & NPDISP_DDBRIDGE_ABI_MAJOR_MASK) != (NPDISP_DDBRIDGE_ABI_V2 & NPDISP_DDBRIDGE_ABI_MAJOR_MASK) ||
		bridge.dwStatus != NPDISP_DDBRIDGE_STATUS_DRIVER_READY) {
		return false;
	}

	if (!npdisp_memory_getLinearAddress(ddHalInfoAddr, &linearHalInfo) ||
		!npdisp_memory_getLinearAddress(ddCallbacksAddr, &linearCallbacks) ||
		!npdisp_memory_getLinearAddress(ddSurfaceCallbacksAddr, &linearSurfaceCallbacks) ||
		!npdisp_memory_getLinearAddress(ddPaletteCallbacksAddr, &linearPaletteCallbacks)) {
		return false;
	}
	if (bridge.lpDDHalInfo != linearHalInfo ||
		bridge.lpDDCallbacks != linearCallbacks ||
		bridge.lpDDSurfaceCallbacks != linearSurfaceCallbacks ||
		bridge.lpDDPaletteCallbacks != linearPaletteCallbacks ||
		bridge.dwDDRequestMask != NPDISP_DDBRIDGE_DD_REQUEST_MASK ||
		bridge.dwSurfaceRequestMask != NPDISP_DDBRIDGE_SURFACE_REQUEST_MASK ||
		bridge.dwPaletteRequestMask != NPDISP_DDBRIDGE_PALETTE_REQUEST_MASK) {
		return false;
	}

	UINT32 expectedHostFeatures = NPDISP_DDBRIDGE_V2_HOST_FEATURES;
	UINT32 expectedProfile = NPDISP_DDBRIDGE_V2_D3D_PROFILE;
	if (npdisp.version < 16) {
		expectedHostFeatures &= ~(NPDISP_DDBRIDGE_FEATURE_D3D_HAL | NPDISP_DDBRIDGE_FEATURE_D3D_SHARED_DATA | NPDISP_DDBRIDGE_FEATURE_D3D_HOST_METADATA);
		expectedProfile = 0;
	}
	if ((expectedHostFeatures & NPDISP_DDBRIDGE_FEATURE_D3D_HAL) &&
		(!d3dGlobalDriverDataAddr || !d3dHalCallbacksAddr ||
		 !npdisp_memory_getLinearAddress(d3dGlobalDriverDataAddr, &linearD3DGlobalDriverData) ||
		 !npdisp_memory_getLinearAddress(d3dHalCallbacksAddr, &linearD3DHalCallbacks))) {
		expectedHostFeatures &= ~(NPDISP_DDBRIDGE_FEATURE_D3D_HAL | NPDISP_DDBRIDGE_FEATURE_D3D_SHARED_DATA | NPDISP_DDBRIDGE_FEATURE_D3D_HOST_METADATA);
		expectedProfile = 0;
		linearD3DGlobalDriverData = 0;
		linearD3DHalCallbacks = 0;
	}

	if (bridge.dwHostFeaturesOffered != expectedHostFeatures ||
		(bridge.dwNegotiatedFeatures & ~bridge.dwHostFeaturesOffered) != 0 ||
		(bridge.dwNegotiatedFeatures & ~bridge.dwDriverFeaturesSupported) != 0 ||
		(bridge.dwNegotiatedFeatures & ~NPDISP_DDBRIDGE_V2_HOST_FEATURES) != 0 ||
		bridge.dwD3DProfileId != expectedProfile) {
		return false;
	}

	if (expectedHostFeatures & NPDISP_DDBRIDGE_FEATURE_D3D_SHARED_DATA) {
		if (bridge.lpD3DGlobalDriverData != linearD3DGlobalDriverData ||
			bridge.lpD3DHALCallbacks != linearD3DHalCallbacks) {
			return false;
		}
	}
	else if (bridge.lpD3DGlobalDriverData || bridge.lpD3DHALCallbacks ||
		(bridge.dwNegotiatedFeatures & NPDISP_DDBRIDGE_FEATURE_D3D_HAL)) {
		return false;
	}

	if (bridge.dwNegotiatedFeatures & NPDISP_DDBRIDGE_FEATURE_D3D_HOST_METADATA) {
		if (!bridge.lpD3DThunkTable ||
			bridge.dwD3DThunkTableSize < sizeof(NPDISP_D3D_THUNK_TABLE32) ||
			bridge.dwD3DThunkTableVersion != NPDISP_D3D_THUNK_TABLE_VERSION) {
			return false;
		}
	}
	else if (bridge.lpD3DThunkTable || bridge.dwD3DThunkTableSize || bridge.dwD3DThunkTableVersion) {
		return false;
	}

	*negotiatedFeatures = bridge.dwNegotiatedFeatures;
	return true;
}

static bool npdisp_ddbridge_validateLegacyV1(const NPDISP_DDHALINFO *bootstrapInfo, UINT32 *negotiatedFeatures)
{
	if (!bootstrapInfo ||
		bootstrapInfo->lpD3DGlobalDriverData != NPDISP_DDBRIDGE_V1_ACK_MAGIC ||
		bootstrapInfo->lpD3DHALCallbacks != NPDISP_DDBRIDGE_ABI_V1 ||
		(bootstrapInfo->lpDDExeBufCallbacksAddr & ~NPDISP_DDBRIDGE_V1_FEATURE_SUPPORTED) != 0) {
		return false;
	}
	*negotiatedFeatures = bootstrapInfo->lpDDExeBufCallbacksAddr & NPDISP_DDBRIDGE_V1_FEATURE_SUPPORTED;
	return true;
}

bool npdisp_ddbridge_validateDriverInit(UINT32 ddHalInfoAddr, UINT32 ddCallbacksAddr, UINT32 ddSurfaceCallbacksAddr, UINT32 ddPaletteCallbacksAddr, UINT32 ddBridgeInfoAddr, UINT32 d3dGlobalDriverDataAddr, UINT32 d3dHalCallbacksAddr, const NPDISP_DDHALINFO *bootstrapInfo, UINT32 *negotiatedFeatures)
{
	if (!negotiatedFeatures) {
		return false;
	}
	*negotiatedFeatures = 0;

	if (npdisp.version >= 15) {
		return npdisp_ddbridge_validateV2(ddHalInfoAddr, ddCallbacksAddr, ddSurfaceCallbacksAddr, ddPaletteCallbacksAddr, ddBridgeInfoAddr, d3dGlobalDriverDataAddr, d3dHalCallbacksAddr, negotiatedFeatures);
	}
	return npdisp_ddbridge_validateLegacyV1(bootstrapInfo, negotiatedFeatures);
}

#endif
