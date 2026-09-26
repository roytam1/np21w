/**
 * @file	npdisp_ogl.h
 * @brief	NPDISP graphics API compatibility bridge interface
 *
 * This is an independent compatibility implementation.
 * It has not undergone any conformance process,
 * and no claim of conformance is made.
 */

#pragma once

#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_D3D) && defined(SUPPORT_NPDISP_GL)

#define NPDISP_OGL_STATE_MAX_SIZE 0x40000000UL

#ifdef __cplusplus
extern "C" {
#endif

void npdisp_ogl_reset(void);
UINT32 npdisp_ogl_stateSize(void);
bool npdisp_ogl_saveState(UINT8* dst, UINT32 size);
bool npdisp_ogl_loadState(const UINT8* src, UINT32 size);
UINT32 npdisp_ogl_dispatch(UINT32 command, UINT32 lpDataAddr);

#ifdef __cplusplus
}
#endif

#endif
