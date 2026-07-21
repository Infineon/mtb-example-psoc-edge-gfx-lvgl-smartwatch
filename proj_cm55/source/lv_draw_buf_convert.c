/*******************************************************************************
* File Name        : lv_draw_buf_convert.c
*
* Description      : Local override of LVGL's lv_draw_buf_convert.c.
*                    Skips helium convert include so premultiply uses the C
*                    fallback (done once at decode time, not per-frame).
*                    Helium blend is still used for SW rendering.
*
* Related Document : See README.md
*
********************************************************************************
* (c) 2025-2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*********************
 *      INCLUDES
 *********************/
#include "lv_draw_buf_convert.h"
#include "../../misc/lv_profiler.h"

/* Local override: skip helium convert include.
 * Helium blend is used for SW rendering; premultiply conversion
 * uses the C fallback (done once at decode time, not per-frame). */
#if LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_NEON
    #include "neon/lv_draw_buf_convert_neon.h"
#elif LV_USE_DRAW_SW_ASM == LV_DRAW_SW_ASM_CUSTOM
    #include LV_DRAW_SW_ASM_CUSTOM_INCLUDE
#endif

/*********************
 *      DEFINES
 *********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**********************
 *      MACROS
 **********************/

#ifndef LV_DRAW_CONVERT_PREMULTIPLY_INDEXED
    #define LV_DRAW_CONVERT_PREMULTIPLY_INDEXED(...)                         LV_RESULT_INVALID
#endif

#ifndef LV_DRAW_CONVERT_PREMULTIPLY_ARGB8888
    #define LV_DRAW_CONVERT_PREMULTIPLY_ARGB8888(...)                         LV_RESULT_INVALID
#endif

#ifndef LV_DRAW_CONVERT_PREMULTIPLY_RGB565A8
    #define LV_DRAW_CONVERT_PREMULTIPLY_RGB565A8(...)                         LV_RESULT_INVALID
#endif

#ifndef LV_DRAW_CONVERT_PREMULTIPLY_ARGB8565
    #define LV_DRAW_CONVERT_PREMULTIPLY_ARGB8565(...)                         LV_RESULT_INVALID
#endif

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_result_t lv_draw_buf_convert_premultiply(lv_draw_buf_t * draw_buf)
{
    LV_PROFILER_DRAW_BEGIN;
    LV_ASSERT_NULL(draw_buf);

    /*Premultiply color with alpha, do case by case by judging color format*/
    lv_color_format_t cf = (lv_color_format_t)draw_buf->header.cf;
    if(LV_COLOR_FORMAT_IS_INDEXED(cf)) {
        if(LV_RESULT_INVALID == LV_DRAW_CONVERT_PREMULTIPLY_INDEXED(draw_buf)) {
            int size = LV_COLOR_INDEXED_PALETTE_SIZE(cf);
            lv_color32_t * palette = (lv_color32_t *)draw_buf->data;
            for(int i = 0; i < size; i++) {
                lv_color_premultiply(&palette[i]);
            }
        }
    }
    else if(cf == LV_COLOR_FORMAT_ARGB8888) {
        if(LV_RESULT_INVALID == LV_DRAW_CONVERT_PREMULTIPLY_ARGB8888(draw_buf)) {
            uint32_t h = draw_buf->header.h;
            uint32_t w = draw_buf->header.w;
            uint32_t stride = draw_buf->header.stride;
            uint8_t * line = (uint8_t *)draw_buf->data;
            for(uint32_t y = 0; y < h; y++) {
                lv_color32_t * pixel = (lv_color32_t *)line;
                for(uint32_t x = 0; x < w; x++) {
                    lv_color_premultiply(pixel);
                    pixel++;
                }
                line += stride;
            }
        }
    }
    else if(cf == LV_COLOR_FORMAT_RGB565A8) {
        if(LV_RESULT_INVALID == LV_DRAW_CONVERT_PREMULTIPLY_RGB565A8(draw_buf)) {
            uint32_t h = draw_buf->header.h;
            uint32_t w = draw_buf->header.w;
            uint32_t stride = draw_buf->header.stride;
            uint32_t alpha_stride = stride / 2;
            uint8_t * line = (uint8_t *)draw_buf->data;
            lv_opa_t * alpha = (lv_opa_t *)(line + stride * h);
            for(uint32_t y = 0; y < h; y++) {
                lv_color16_t * pixel = (lv_color16_t *)line;
                for(uint32_t x = 0; x < w; x++) {
                    lv_color16_premultiply(pixel, alpha[x]);
                    pixel++;
                }
                line += stride;
                alpha += alpha_stride;
            }
        }
    }
    else if(cf == LV_COLOR_FORMAT_ARGB8565) {
        if(LV_RESULT_INVALID == LV_DRAW_CONVERT_PREMULTIPLY_ARGB8565(draw_buf)) {
            uint32_t h = draw_buf->header.h;
            uint32_t w = draw_buf->header.w;
            uint32_t stride = draw_buf->header.stride;
            uint8_t * line = (uint8_t *)draw_buf->data;
            for(uint32_t y = 0; y < h; y++) {
                uint8_t * pixel = line;
                for(uint32_t x = 0; x < w; x++) {
                    uint8_t alpha = pixel[2];
                    lv_color16_premultiply((lv_color16_t *)pixel, alpha);
                    pixel += 3;
                }
                line += stride;
            }
        }
    }
    else {
        LV_LOG_WARN("color format: %d not supported for premultiply", cf);
        LV_PROFILER_DRAW_END;
        return LV_RESULT_INVALID;
    }

    LV_PROFILER_DRAW_END;
    return LV_RESULT_OK;
}


/* [] END OF FILE */
