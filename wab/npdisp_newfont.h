#ifndef NP2_WAB_NPDISP_NEWFONT_H
#define NP2_WAB_NPDISP_NEWFONT_H

#if defined(SUPPORT_WAB_NPDISP) && defined(SUPPORT_NPDISP_NEWFONTSEG)

bool npdisp_newfont_isPackedExtTextOut(UINT16 wOptions);
bool npdisp_newfont_tryExtTextOut(UINT32 lpDestDevAddr, SINT16 wDestXOrg, SINT16 wDestYOrg, UINT32 lpClipRectAddr, UINT32 lpStringAddr, SINT16 wCount, UINT32 lpFontInfoAddr, UINT32 lpDrawModeAddr, UINT32 lpTextXFormAddr, UINT32 lpCharWidthsAddr, UINT32 lpOpaqueRectAddr, UINT16 wOptions, UINT32* retValue);
void npdisp_newfont_reset(void);

#endif

#endif
