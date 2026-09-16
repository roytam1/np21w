/**
 * @file	npdisp_ddbridge.h
 * @brief	NPDISP DirectDraw 32bit HAL bridge interface
 */

#pragma once

#if defined(SUPPORT_WAB_NPDISP)

#ifdef __cplusplus
extern "C" {
#endif

bool npdisp_ddbridge_prepareDriverInit(UINT32 ddHalInfoAddr, UINT32 ddCallbacksAddr, UINT32 ddSurfaceCallbacksAddr, UINT32 ddPaletteCallbacksAddr, UINT32 ddBridgeInfoAddr, UINT32 d3dGlobalDriverDataAddr, UINT32 d3dHalCallbacksAddr, UINT32 *linearContext);
bool npdisp_ddbridge_validateDriverInit(UINT32 ddHalInfoAddr, UINT32 ddCallbacksAddr, UINT32 ddSurfaceCallbacksAddr, UINT32 ddPaletteCallbacksAddr, UINT32 ddBridgeInfoAddr, UINT32 d3dGlobalDriverDataAddr, UINT32 d3dHalCallbacksAddr, const NPDISP_DDHALINFO *bootstrapInfo, UINT32 *negotiatedFeatures);

#ifdef __cplusplus
}
#endif

#endif
