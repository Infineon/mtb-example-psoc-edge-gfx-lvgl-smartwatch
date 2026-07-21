/*******************************************************************************
* File Name        : lv_port_disp.h
*
* Description      : This file provides constants and function prototypes
*                    for configuring low level display driver in LVGL.
*
* Related Document : See README.md
*
********************************************************************************
* (c) 2024-2026, Infineon Technologies AG, or an affiliate of Infineon
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

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "lvgl.h"
#include "FreeRTOS.h"
#include "semphr.h"


/*******************************************************************************
* Macros
*******************************************************************************/
/* Frame-buffer pixel format selector for the CO5300 round display.
 *
 * The CO5300 is shared by two UIs with different pixel-format needs (see the
 * LV_COLOR_DEPTH note in lv_conf.h):
 *   - Benchmark demo  (LV_USE_DEMO_BENCHMARK=1) -> 32-bit XRGB8888 buffer.
 *   - Smartwatch UI   (LV_USE_DEMO_BENCHMARK=0) -> 16-bit RGB565 buffer, which
 *     SquareLine Studio's exported assets and ui.c (LV_COLOR_DEPTH==16) require.
 *
 * DISP_USE_XRGB8888 keeps the frame-buffer element type, the LVGL color format,
 * and the GFXSS DC input format in sync with the LVGL color depth. Other panels
 * always use 16-bit RGB565. */
#if defined(MTB_DISPLAY_CO5300) && LV_USE_DEMO_BENCHMARK
#define DISP_USE_XRGB8888               (1U)
#else
#define DISP_USE_XRGB8888               (0U)
#endif

#if defined(MTB_DISPLAY_CO5300)
#if DISP_USE_XRGB8888
/* Benchmark demo (32-bit XRGB8888).
 * The LVGL buffer is rendered at MY_DISP_HOR_RES (480) so its per-row stride
 * (480*4 = 1920 B) is 128-byte aligned and matches the GFXSS DC line stride
 * roundup(472*4,128) = 1920. The DC reads/transmits only DISP_DC_HOR_RES (472)
 * visible columns per row - the panel's addressable width - skipping the 8-px
 * buffer padding each row. This matches the proven reference (gfx_layer.width =
 * display_width = 472); sending 480 columns to the 472-wide panel causes
 * per-row wrap / tearing. */
#define MY_DISP_HOR_RES                 (480U) /* pixels - LVGL canvas/buffer */
#define MY_DISP_VER_RES                 (466U) /* pixels */
#define DISP_DC_HOR_RES                 (472U) /* pixels - panel addressable */
#define DISP_DC_VER_RES                 (466U) /* pixels */
#else
/* Smartwatch UI (16-bit RGB565).
 * RGB565 (2 B/px) is the constraint here: the GFXSS DC line stride is always
 * roundup(width*2, 128), which can only be 896 or 1024 near this resolution -
 * never the 960 B that a 480-wide RGB565 buffer would have. 512 is the only
 * RGB565 width whose buffer stride (512*2 = 1024) is 128-aligned AND equals the
 * DC line stride roundup(512*2,128) = 1024, so the GPU render stride and the DC
 * read stride agree and there is no per-row drift. The panel shows the left 466
 * columns (CASET window); the remaining columns are off-screen padding. This is
 * the original, proven-good smartwatch geometry. Buffer = canvas = DC = 512. */
#define MY_DISP_HOR_RES                 (512U) /* pixels - LVGL canvas/buffer */
#define MY_DISP_VER_RES                 (466U) /* pixels */
#define DISP_DC_HOR_RES                 (512U) /* pixels - matches buffer stride */
#define DISP_DC_VER_RES                 (466U) /* pixels */
#endif /* DISP_USE_XRGB8888 */
#elif defined(W4P3INCH_DISP)
#define MY_DISP_HOR_RES                 (832U) /* pixels - 4.3 inch display */
#define MY_DISP_VER_RES                 (480U) /* pixels - 4.3 inch display */
#define DISP_DC_HOR_RES                 (832U) /* pixels - 4.3 inch display */
#define DISP_DC_VER_RES                 (480U) /* pixels - 4.3 inch display */
#endif /* MTB_DISPLAY_CO5300 */

/*******************************************************************************
* Partial-buffer rendering
*
* Set USE_PARTIAL_BUFFER_MODE to 1 to enable RT1170-style hybrid partial mode:
*   - LVGL renders into a small strip buffer (PARTIAL_BUF_NLINES rows).
*   - flush_cb blits strip -> persistent shadow_fb at (x,y).
*   - LV_EVENT_REFR_READY issues ONE partial-window DBI transfer covering
*     only the union of dirty rows via Cy_GFXSS_TransferPartialFrame_Async().
*
* The shadow/strip buffer element type follows the active CO5300 demo mode:
*   - Benchmark demo  -> 32-bit XRGB8888
*   - Smartwatch UI   -> 16-bit RGB565
*
* That keeps the LVGL draw buffer, shadow buffer, and DC input format aligned
* in both render modes. The memory footprint therefore scales with the active
* pixel format.
*
* Set to 0 to keep validated full-frame double-buffered path.
*******************************************************************************/
#define USE_PARTIAL_BUFFER_MODE         (0U)

/* Default tessellation window width and height, in pixels.
 * Partial mode uses full-screen TW to avoid VGLite tessellation clipping
 * artifacts with full-width strips. */
#if (USE_PARTIAL_BUFFER_MODE != 0U) && defined(MTB_DISPLAY_CO5300)
#define DEFAULT_VG_LITE_TW_WIDTH        (MY_DISP_HOR_RES) /* pixels */
#define DEFAULT_VG_LITE_TW_HEIGHT       (MY_DISP_VER_RES) /* pixels */
#else
#define DEFAULT_VG_LITE_TW_WIDTH        (256U) /* pixels */
#define DEFAULT_VG_LITE_TW_HEIGHT       (256U) /* pixels */
#endif

#define RESET_VALUE                     (0U)
#define SET_VALUE                       (1U)

/* RAM optimization mode for the CO5300 DBI command-mode display (FULL render
 * mode only; ignored when USE_PARTIAL_BUFFER_MODE is enabled).
 *
 * Each FULL-mode buffer is MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(disp_px_t),
 * so its size follows the active demo's pixel format: 2 B/px RGB565 for the
 * smartwatch UI (512x466x2 = 477 KB) and 4 B/px XRGB8888 for the benchmark demo
 * (480x466x4 = 895 KB). The figures below are the RGB565 smartwatch case.
 *
 *  USE_SINGLE_BUFFER_MODE 0 (default)
 *    Double-buffer FULL mode. Two 512x466x2 = 953 KB.
 *    No tearing. Async DBI flush. Best visual quality.
 *
 *  USE_SINGLE_BUFFER_MODE 1
 *    Single-buffer FULL mode. One 512x466x2 = 477 KB (~50% saving).
 *    Always blocking DBI flush so LVGL cannot start rendering the next frame
 *    until the DBI has finished reading the buffer. Minor tearing on fast
 *    animations.
 */
#define USE_SINGLE_BUFFER_MODE          (0U)


/*******************************************************************************
* Global Variables
*******************************************************************************/

extern lv_display_t * disp_gpu;

extern void *frame_buffer1;
extern void *frame_buffer2;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
/* Initialize low level display driver */
void lv_port_disp_init(void);
void reset_frame_buffer(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*LV_PORT_DISP_H*/

/* [] END OF FILE */
