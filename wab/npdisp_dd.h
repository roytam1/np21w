/**
 * @file	npdisp_dd.h
 * @brief	Neko Project II Display Adapter DirectDraw HALインターフェース
 */

#pragma once

#if defined(SUPPORT_WAB_NPDISP)

#ifdef __cplusplus
extern "C" {
#endif

bool npdisp_dd_ensureOffscreenBacking(void);
void npdisp_dd_releaseOffscreenBacking(void);
bool npdisp_dd_rebuildModeDependentHalInfo(UINT32 lpPDeviceAddr);

bool npdisp_ddraw_isScanoutOffsetValid(UINT32 offset);
UINT8* npdisp_ddraw_getScanoutHostBase(void);
void npdisp_dd_vsync(void);

UINT16 npdisp_dd_controlCommand(UINT32 lpDestDevAddr, const NPDISP_DCICMD* cmd, UINT32 lpOutDataAddr);
UINT32 npdisp_dd_dispatchBridge(UINT32 callbackId, UINT32 lpDataAddr);

#ifdef __cplusplus
}
#endif

#endif
