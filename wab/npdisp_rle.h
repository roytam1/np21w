/**
 * @file	npdisp_rle.h
 * @brief	Interface of the Neko Project II Display Adapter RLE
 */

#if defined(SUPPORT_WAB_NPDISP)

#ifdef __cplusplus
extern "C" {
#endif
	int DecompressRLEBMP(const BITMAPINFOHEADER* bih, const UINT8* rle_data, int rle_size, UINT8* out_pixels);
#ifdef __cplusplus
}
#endif



#endif