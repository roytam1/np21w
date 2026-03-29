/**
 * @file	npdisp_rle.c
 * @brief	Implementation of the Neko Project II Display Adapter RLE
 */

#include	"compiler.h"

#if defined(SUPPORT_WAB_NPDISP)

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "npdisp_rle.h"

static int bmp_abs_i32(int32_t v, int32_t *out)
{
    if (v == INT32_MIN) return 0;
    *out = (v < 0) ? -v : v;
    return 1;
}

static int calc_bmp_stride_bytes(int32_t width, uint16_t bpp, size_t *out_stride)
{
    if (width <= 0) return 0;

    /* stride = ((width * bpp + 31) / 32) * 4 */
    uint64_t bits = (uint64_t)(uint32_t)width * (uint64_t)bpp;
    uint64_t stride = ((bits + 31u) / 32u) * 4u;
    if (stride > SIZE_MAX) return 0;

    *out_stride = (size_t)stride;
    return 1;
}

static void put_pixel_8bpp(uint8_t *dst, size_t stride, int32_t width, int32_t height_abs,
                           int x, int y_mem, uint8_t val)
{
    if (x < 0 || x >= width || y_mem < 0 || y_mem >= height_abs) return;
    dst[(size_t)y_mem * stride + (size_t)x] = val;
}

static void put_pixel_4bpp(uint8_t *dst, size_t stride, int32_t width, int32_t height_abs,
                           int x, int y_mem, uint8_t val)
{
    if (x < 0 || x >= width || y_mem < 0 || y_mem >= height_abs) return;

    uint8_t *p = dst + (size_t)y_mem * stride + (size_t)(x >> 1);
    val &= 0x0F;

    if ((x & 1) == 0) {
        *p = (uint8_t)((*p & 0x0F) | (val << 4));
    } else {
        *p = (uint8_t)((*p & 0xF0) | val);
    }
}

static int map_rle_y_to_memory_row(int y_rle, int32_t height_abs, int top_down)
{
    if (top_down) {
        return y_rle;
    } else {
        return (int)(height_abs - 1 - y_rle);
    }
}

int DecompressRLEBMP(const BITMAPINFOHEADER* bih, const UINT8* rle_data, int rle_size, UINT8* out_pixels)
{
    int32_t width, height_abs;
    int top_down;
    size_t stride, image_size;
    uint8_t *dst;
    size_t pos = 0;
    int x = 0;
    int y_rle = 0;

    if (!bih || !rle_data || !out_pixels) return 0;

    if (!bmp_abs_i32(bih->biHeight, &height_abs)) return 0;
    width = bih->biWidth;
    top_down = (bih->biHeight < 0);

    if (width <= 0 || height_abs <= 0) return 0;
    if (bih->biPlanes != 1) return 0;

    if (!((bih->biCompression == BI_RLE8 && bih->biBitCount == 8) ||
          (bih->biCompression == BI_RLE4 && bih->biBitCount == 4))) {
        return 0;
    }

    if (!calc_bmp_stride_bytes(width, bih->biBitCount, &stride)) return 0;
    if ((uint64_t)stride * (uint64_t)(uint32_t)height_abs > SIZE_MAX) return 0;
    image_size = stride * (size_t)height_abs;

    dst = out_pixels;
    memset(dst, 0, image_size);

    if (bih->biCompression == BI_RLE8) {
        while (pos + 2 <= rle_size) {
            uint8_t count = rle_data[pos++];
            uint8_t value = rle_data[pos++];

            if (count != 0) {
                /* Encoded mode */
                for (uint8_t i = 0; i < count; ++i) {
                    if (y_rle >= 0 && y_rle < height_abs) {
                        int y_mem = map_rle_y_to_memory_row(y_rle, height_abs, top_down);
                        put_pixel_8bpp(dst, stride, width, height_abs, x, y_mem, value);
                    }
                    x++;
                }
            } else {
                /* Escape */
                if (value == 0) {
                    /* EOL */
                    x = 0;
                    y_rle++;
                    if (y_rle > height_abs) break;
                } else if (value == 1) {
                    /* EOB */
                    return 1;
                } else if (value == 2) {
                    /* Delta */
                    if (pos + 2 > rle_size) break;
                    {
                        uint8_t dx = rle_data[pos++];
                        uint8_t dy = rle_data[pos++];
                        x += dx;
                        y_rle += dy;
                    }
                } else {
                    /* Absolute mode */
                    uint8_t n = value;
                    if (pos + n > rle_size) break;

                    for (uint8_t i = 0; i < n; ++i) {
                        uint8_t px = rle_data[pos++];
                        if (y_rle >= 0 && y_rle < height_abs) {
                            int y_mem = map_rle_y_to_memory_row(y_rle, height_abs, top_down);
                            put_pixel_8bpp(dst, stride, width, height_abs, x, y_mem, px);
                        }
                        x++;
                    }

                    if (n & 1) {
                        if (pos + 1 > rle_size) break;
                        pos++;
                    }
                }
            }
        }
    } else { /* BI_RLE4 */
        while (pos + 2 <= rle_size) {
            uint8_t count = rle_data[pos++];
            uint8_t value = rle_data[pos++];

            if (count != 0) {
                /* Encoded mode */
                uint8_t hi = (uint8_t)((value >> 4) & 0x0F);
                uint8_t lo = (uint8_t)(value & 0x0F);

                for (uint8_t i = 0; i < count; ++i) {
                    uint8_t px = ((i & 1) == 0) ? hi : lo;
                    if (y_rle >= 0 && y_rle < height_abs) {
                        int y_mem = map_rle_y_to_memory_row(y_rle, height_abs, top_down);
                        put_pixel_4bpp(dst, stride, width, height_abs, x, y_mem, px);
                    }
                    x++;
                }
            } else {
                /* Escape */
                if (value == 0) {
                    /* EOL */
                    x = 0;
                    y_rle++;
                    if (y_rle > height_abs) break;
                } else if (value == 1) {
                    /* EOB */
                    return 1;
                } else if (value == 2) {
                    /* Delta */
                    if (pos + 2 > rle_size) break;
                    {
                        uint8_t dx = rle_data[pos++];
                        uint8_t dy = rle_data[pos++];
                        x += dx;
                        y_rle += dy;
                    }
                } else {
                    /* Absolute mode */
                    uint8_t n = value;
                    size_t byte_count = (size_t)(n + 1) / 2;

                    if (pos + byte_count > rle_size) break;

                    for (uint8_t i = 0; i < n; ++i) {
                        uint8_t b = rle_data[pos + (size_t)(i >> 1)];
                        uint8_t px = ((i & 1) == 0)
                                   ? (uint8_t)((b >> 4) & 0x0F)
                                   : (uint8_t)(b & 0x0F);

                        if (y_rle >= 0 && y_rle < height_abs) {
                            int y_mem = map_rle_y_to_memory_row(y_rle, height_abs, top_down);
                            put_pixel_4bpp(dst, stride, width, height_abs, x, y_mem, px);
                        }
                        x++;
                    }

                    pos += byte_count;

                    if (byte_count & 1) {
                        if (pos + 1 > rle_size) break;
                        pos++;
                    }
                }
            }
        }
    }
    return 0;
}

#endif