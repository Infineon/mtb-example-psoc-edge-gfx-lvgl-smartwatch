/*******************************************************************************
* File Name        : lv_port_disp.c
*
* Description      : This file provides implementation of low level display
*                    device driver for LVGL.
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

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "lv_port_disp.h"
#include "lv_draw_sw.h"
#include <string.h>

#include "cybsp.h"
#include "smartwatch_app.h"
#include "app_state_manager.h"

#include "app_logger.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "cycfg_peripherals.h"

#if defined(MTB_DISPLAY_CO5300)
#include "mtb_display_co5300.h"
#elif defined(W4P3INCH_DISP)
#include "mtb_disp_dsi_waveshare_4p3.h"
#endif

#if (USE_PARTIAL_BUFFER_MODE != 0U) && defined(MTB_DISPLAY_CO5300)
/* VGLite headers for capability/scissor query at init time. */
#include "vg_lite.h"
#endif


/*******************************************************************************
* Macros
*******************************************************************************/
#define FRAME_TX_WAIT_MS                        (1U)
#if defined(W4P3INCH_DISP)
#define DISP_STABILIZATION_DELAY_MS             (125U)
#endif
/* Forward declaration to ensure correct type before first use */
void reset_frame_buffer(void);


/*******************************************************************************
* Global Variables
*******************************************************************************/
#if defined(MTB_DISPLAY_CO5300)
#if DISP_USE_XRGB8888
typedef uint32_t disp_px_t;
#else
typedef uint16_t disp_px_t;
#endif
#endif

#if (USE_PARTIAL_BUFFER_MODE != 0U) && defined(MTB_DISPLAY_CO5300)
/* Hybrid model: LVGL PARTIAL strip + persistent scanout shadow_fb.
 *
 *   - LVGL renders into a small "strip" buffer (PARTIAL_BUF_NLINES rows).
 *   - flush_cb blits the strip rectangle into shadow_fb at (x,y) and
 *     accumulates the dirty-row union bbox (refr_union_y1..refr_union_y2).
 *   - At LV_EVENT_REFR_READY we issue ONE coherent partial-window DBI
 *     transfer of shadow_fb covering only the union bbox rows.
 *
 *   The buffer element type follows the active demo mode:
 *     - Benchmark demo : 32-bit XRGB8888
 *     - Smartwatch UI  : 16-bit RGB565
 */
#ifndef PARTIAL_BUF_NLINES
#define PARTIAL_BUF_NLINES  48U
#endif

CY_SECTION(".cy_gpu_buf") __attribute__((aligned(64)))
    static disp_px_t shadow_fb[MY_DISP_HOR_RES * MY_DISP_VER_RES];

CY_SECTION(".cy_gpu_buf") __attribute__((aligned(64)))
    static disp_px_t lvgl_strip_buf[MY_DISP_HOR_RES * PARTIAL_BUF_NLINES];

/* Union-bbox accumulator. */
static int32_t  refr_union_y1    = INT32_MAX;
static int32_t  refr_union_y2    = -1;
static uint32_t refr_rect_count  = 0U;
#else
/* FULL mode: full-screen render buffers, pointer-swap flush. */
#if defined(MTB_DISPLAY_CO5300)
CY_SECTION(".cy_gpu_buf") __attribute__((aligned(64)))
    disp_px_t disp_buf1[MY_DISP_HOR_RES * MY_DISP_VER_RES];
#if !USE_SINGLE_BUFFER_MODE
/* Second frame buffer - only needed for double-buffer async mode. */
CY_SECTION(".cy_gpu_buf") __attribute__((aligned(64)))
    disp_px_t disp_buf2[MY_DISP_HOR_RES * MY_DISP_VER_RES];
#endif
#else
CY_SECTION(".cy_gpu_buf") lv_color_t disp_buf1[MY_DISP_HOR_RES *
                                               MY_DISP_VER_RES];
#if !USE_SINGLE_BUFFER_MODE
/* Second frame buffer - only needed for double-buffer async mode. */
CY_SECTION(".cy_gpu_buf") lv_color_t disp_buf2[MY_DISP_HOR_RES *
                                               MY_DISP_VER_RES];
#endif
#endif
#endif

#if defined(MTB_DISPLAY_CO5300)
extern SemaphoreHandle_t frame_tx_sem;
#endif

#if defined(W4P3INCH_DISP)
volatile bool fb_pending = false;
#endif

/* GPU and CPU based display driver structures */
lv_display_t * disp_gpu = NULL;

/* Frame buffers used by GFXSS to render UI */
#if (USE_PARTIAL_BUFFER_MODE != 0U) && defined(MTB_DISPLAY_CO5300)
void *frame_buffer1 = shadow_fb;
void *frame_buffer2 = shadow_fb;
#else
void *frame_buffer1 = &disp_buf1;
#if USE_SINGLE_BUFFER_MODE
void *frame_buffer2 = NULL;  /* Single-buffer mode: no second buffer */
#else
void *frame_buffer2 = &disp_buf2;
#endif
#endif

cy_stc_gfx_context_t gfx_context;

/* LVGL frame-buffer color format per display/demo. The benchmark demo uses
 * 32-bit XRGB8888 (clean anti-aliased text); the SquareLine smartwatch UI and
 * the other panels use 16-bit RGB565. Must match LV_COLOR_DEPTH in lv_conf.h
 * and the GFXSS DC input_format_type set in smartwatch_app.c. */
#if DISP_USE_XRGB8888
#define DISP_LV_COLOR_FORMAT  LV_COLOR_FORMAT_XRGB8888
#else
#define DISP_LV_COLOR_FORMAT  LV_COLOR_FORMAT_RGB565
#endif

#if defined(MTB_DISPLAY_CO5300)
/* 1.43" display driver pin configuration */
mtb_display_co5300_config_t co5300_pins =
{
    .rst_port    = CYBSP_DISP_RST_PORT,
    .rst_pin     = CYBSP_DISP_RST_PIN,
    .vci_en_port = CYBSP_DISP_VCI_EN_PORT,
    .vci_en_pin  = CYBSP_DISP_VCI_EN_PIN,
    .gfx_config  = &GFXSS_config
};
#endif /* MTB_DISPLAY_CO5300 */

/*******************************************************************************
* Function Definition
*******************************************************************************/

/* Return system tick in milliseconds for LVGL */
static uint32_t get_tick_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

#if (USE_PARTIAL_BUFFER_MODE != 0U) && defined(MTB_DISPLAY_CO5300)
/*******************************************************************************
* Function Name: disp_invalidate_area_cb
********************************************************************************
* Summary:
*  LV_EVENT_INVALIDATE_AREA callback. Expands every invalidation rectangle by
*  1 pixel on each side before LVGL records it in the dirty-area list.
*
*  Why this is needed in partial mode:
*    The VGLite GPU may produce blended edge pixels (sub-pixel coverage) that
*    fall outside the widget's declared coordinate area. In full-frame mode
*    every pixel is redrawn unconditionally so this is invisible. In partial
*    mode, stale edge pixels persist in shadow_fb and appear as small ghosts
*    on the trailing edge of moving objects. Padding the invalidation by 1 px
*    guarantees those edge pixels are always included in the next redraw.
*******************************************************************************/
static void disp_invalidate_area_cb(lv_event_t *e)
{
    lv_area_t *a = lv_event_get_invalidated_area(e);
    if(a == NULL) return;

    a->x1 -= 1;
    a->y1 -= 1;
    a->x2 += 1;
    a->y2 += 1;

    /* Clamp to display bounds. */
    if(a->x1 < 0)                   a->x1 = 0;
    if(a->y1 < 0)                   a->y1 = 0;
    if(a->x2 >= MY_DISP_HOR_RES)    a->x2 = MY_DISP_HOR_RES - 1;
    if(a->y2 >= MY_DISP_VER_RES)    a->y2 = MY_DISP_VER_RES - 1;
}

/*******************************************************************************
* Function Name: disp_refr_ready_cb
********************************************************************************
* Summary:
*  End-of-refresh callback registered on LV_EVENT_REFR_READY.  After LVGL has
*  flushed all dirty strips into shadow_fb, this issues ONE partial-window DBI
*  transfer covering only the union of dirty rows via
*  Cy_GFXSS_TransferPartialFrame_Async().
*
*  Shadow-fb ownership protocol:
*    disp_flush (first call)  -- xSemaphoreTake: CPU owns shadow_fb
*    disp_flush (N calls)     -- CPU blits strips into shadow_fb safely
*    disp_refr_ready_cb       -- kicks DBI, ISR completion gives semaphore
*******************************************************************************/
static void disp_refr_ready_cb(lv_event_t *e)
{
    (void)e;

    /* Nothing rendered this refresh -> nothing to push.
     * Semaphore was never taken (disp_flush was not called). */
    if(refr_rect_count == 0U)
    {
        return;
    }

    /* Skip DBI transfer when display hardware is not ready (LP/ULP transition).
     * Release the semaphore that disp_flush claimed. */
    if (!dbi_transfer_enabled)
    {
        xSemaphoreGive(frame_tx_sem);
        refr_union_y1   = INT32_MAX;
        refr_union_y2   = -1;
        refr_rect_count = 0U;
        return;
    }

    /* Clamp union bbox to the panel and produce a half-open [y0, y1) range
     * for the GFXSS partial-window API. CASET is left at full-width by
     * Cy_GFXSS_TransferPartialFrame_Async (CASET reprogram = 0..hres-1),
     * so we only need PASET = (y0..y1-1).                                */
    int32_t y0 = (refr_union_y1 < 0) ? 0 : refr_union_y1;
    int32_t y1 = refr_union_y2 + 1;
    if(y0 < 0)                  y0 = 0;
    if(y1 > MY_DISP_VER_RES)    y1 = MY_DISP_VER_RES;
    if(y1 <= y0)
    {
        /* Defensive: degenerate bbox -> skip. Release semaphore. */
        xSemaphoreGive(frame_tx_sem);
        refr_union_y1   = INT32_MAX;
        refr_union_y2   = -1;
        refr_rect_count = 0U;
        return;
    }

    /* Semaphore is already held (taken in disp_flush on first strip).
     * Drain the GPU pipeline before issuing the DBI transfer. */
    (void)vg_lite_finish();

    /* Ensure all shadow_fb writes are globally visible before GFXSS reads.
     * shadow_fb is non-cacheable, so a barrier (not a cache clean) suffices. */
    __DMB();
    __DSB();

    /* Protect the async DBI chain from DeepSleep entry. */
    mtb_hal_syspm_lock_deepsleep();
    dbi_transfer_in_flight = true;

    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t *)shadow_fb, &gfx_context);

    NVIC_ClearPendingIRQ(GFXSS_DC_IRQ);
    NVIC_EnableIRQ(GFXSS_DC_IRQ);
    (void)Cy_GFXSS_TransferPartialFrame_Async((GFXSS_Type *)GFXSS,
                                              (uint32_t)y0,
                                              (uint32_t)y1,
                                              &gfx_context);

#if defined(USE_PERFORMANCE_MONITOR)
    frame_count++;
#endif

    refr_union_y1   = INT32_MAX;
    refr_union_y2   = -1;
    refr_rect_count = 0U;
}
#endif /* USE_PARTIAL_BUFFER_MODE && MTB_DISPLAY_CO5300 */

/*******************************************************************************
* Function Name: disp_init
********************************************************************************
* Summary:
*  Display initialization function configuring 1.43"/4.3" display driver.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void disp_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

#if defined(MTB_DISPLAY_CO5300)
    /* Initialize 1.43" display driver */
    result = (cy_rslt_t)mtb_display_co5300_init(&base->GFXSS_MIPIDSI, &co5300_pins);
#elif defined(W4P3INCH_DISP)
    /* Display controller stabilization delay */
    vTaskDelay(pdMS_TO_TICKS(DISP_STABILIZATION_DELAY_MS));
    /* Initialize 4.3" display driver */
    result = mtb_disp_waveshare_4p3_init(CYBSP_I2C_CONTROLLER_HW, &i2c_context);
    if (CY_SCB_I2C_SUCCESS != result)
    {
        process_error((cy_rslt_t)result, "Waveshare 4.3-Inch display init failed. STOP.");
        CY_ASSERT(0);
    }
#endif
    if (CY_RSLT_SUCCESS != result)
    {
        process_error(result, "Display initialization failed. STOP.");
    }
}


/*******************************************************************************
* Function Name: disp_flush
********************************************************************************
* Summary:
*  Flush the content of the internal buffer the specific area on the display.
*  You can use DMA or any hardware acceleration to do this operation in the
*  background but 'lv_disp_flush_ready()' has to be called when finished.
*
* Parameters:
*  *disp: Pointer to the LVGL display object.
*  *area: Pointer to the area of the screen to flush (used in partial-render mode).
*  *px_map: Pointer to the pixel buffer to copy to the frame buffer.
*
* Return:
*  void
*
*******************************************************************************/

void LV_ATTRIBUTE_FAST_MEM disp_flush(lv_display_t *disp, const lv_area_t *area,
        uint8_t *px_map)
{
#if (USE_PARTIAL_BUFFER_MODE != 0U) && defined(MTB_DISPLAY_CO5300)
    /* Hybrid model: LVGL PARTIAL strip + persistent scanout shadow_fb.
     * flush_cb here just blits the strip's dirty rect into shadow_fb at
     * (x,y). The single partial-window DBI transfer happens in REFR_READY. */

    /* Skip when display hardware is not ready (LP/ULP transition) */
    if (!dbi_transfer_enabled)
    {
        lv_display_flush_ready(disp);
        return;
    }

    /* On the first flush of a refresh cycle, wait for the previous
     * async DBI transfer to complete before writing to shadow_fb.
     * Without this, disp_flush overwrites shadow_fb rows while the
     * DBI engine is still reading them, causing tearing and ghosting. */
    if(refr_rect_count == 0U)
    {
        xSemaphoreTake(frame_tx_sem, portMAX_DELAY);
    }

    const uint32_t  bpp     = (uint32_t)sizeof(disp_px_t);
    const uint32_t  strip_h = (uint32_t)(area->y2 - area->y1 + 1);
    const uint32_t  strip_w = (uint32_t)(area->x2 - area->x1 + 1);

    /* Blit strip -> shadow_fb at (x1,y1).
     * In LVGL PARTIAL mode the strip's row stride is determined by LVGL
     * (width_to_stride() rounds up to LV_DRAW_BUF_STRIDE_ALIGN). Query LVGL
     * for the actual current draw_buf stride so any alignment combo is
     * handled correctly. */
    lv_draw_buf_t * cur_buf = lv_display_get_buf_active(disp);
    const uint32_t src_row_bytes = strip_w * bpp;
    const uint32_t src_stride_bytes =
        (cur_buf != NULL && cur_buf->header.stride != 0U)
            ? (uint32_t)cur_buf->header.stride
            : src_row_bytes;
    const uint8_t * src_b = (const uint8_t *)px_map;
    for(uint32_t row = 0U; row < strip_h; row++)
    {
        disp_px_t *dst = &shadow_fb[((uint32_t)area->y1 + row) * MY_DISP_HOR_RES
                                    + (uint32_t)area->x1];
        (void)memcpy(dst, src_b + row * src_stride_bytes, src_row_bytes);
    }

    /* Accumulate union dirty bbox for REFR_READY. */
    if((int32_t)area->y1 < refr_union_y1) refr_union_y1 = (int32_t)area->y1;
    if((int32_t)area->y2 > refr_union_y2) refr_union_y2 = (int32_t)area->y2;
    refr_rect_count++;

    lv_display_flush_ready(disp);
#else
    CY_UNUSED_PARAMETER(area);

#if defined(MTB_DISPLAY_CO5300)
    /* Skip DBI transfer when display hardware is not ready (LP/ULP transition) */
    if (!dbi_transfer_enabled)
    {
        lv_display_flush_ready(disp);
        return;
    }

    /* Block until the previous DBI transfer is complete. */
    BaseType_t take_result = xSemaphoreTake(frame_tx_sem, portMAX_DELAY);
    CY_ASSERT(take_result == pdTRUE);
    CY_UNUSED_PARAMETER(take_result);

    /* Make sure CPU writes to the (non-cacheable) frame buffer globally visible
     * before the GFXSS DMA reads it. The .cy_gpu_buf region (gfx_mem) is mapped
     * Normal Non-Cacheable by the CM55 MPU, so a write-buffer ordering barrier
     * is sufficient and correct for both demos - no cache clean needed. */
    __DSB();

    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*) px_map, &gfx_context);

#if USE_SINGLE_BUFFER_MODE
    /* Single-buffer FULL mode: always use a blocking transfer. With only one
     * buffer, LVGL must not start rendering the next frame until the DBI has
     * finished reading the buffer, so we block here and signal completion. */
    Cy_GFXSS_Transfer_Frame((GFXSS_Type*) GFXSS, &gfx_context);
    xSemaphoreGive(frame_tx_sem);
#else
    /* Async DBI transfer (double-buffer).
     * Lock DeepSleep so the GFXSS CHECK_READY callback cannot clear
     * INTR_MASK while the ISR chain is running. The completion callback
     * (on_frame_transfer_complete) disables DC_IRQ and unlocks DeepSleep. */
    mtb_hal_syspm_lock_deepsleep();
    dbi_transfer_in_flight = true;
    NVIC_ClearPendingIRQ(GFXSS_DC_IRQ);
    NVIC_EnableIRQ(GFXSS_DC_IRQ);
    Cy_GFXSS_Transfer_Frame_Async((GFXSS_Type*) GFXSS, &gfx_context);
#endif

    /* Count frames for performance monitor (FPS) */
#if defined(USE_PERFORMANCE_MONITOR)
    frame_count++;
#endif

    /* Complete the flush for LVGL immediately (non-blocking) */
    lv_display_flush_ready(disp);

#elif defined(W4P3INCH_DISP)
    while (fb_pending)
    {
        vTaskDelay(pdMS_TO_TICKS(FRAME_TX_WAIT_MS));
    }

    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*) px_map, &gfx_context);

    /* Count frames for performance monitor (FPS) */
#if defined(USE_PERFORMANCE_MONITOR)
    frame_count++;
#endif

    /* Complete the flush for LVGL immediately (non-blocking) */
    lv_display_flush_ready(disp);
    fb_pending = true;
#endif
#endif /* USE_PARTIAL_BUFFER_MODE */
}


/*******************************************************************************
* Function Name: lv_port_disp_init
********************************************************************************
* Summary:
*  Initialization function for display devices supported by LVGL.
*   LVGL requires a buffer where it internally draws the widgets.
*   Later this buffer will be passed to your display driver's `flush_cb` to copy
*   its content to your display.
*   The buffer has to be greater than 1 display row
*
*   There are 3 buffering configurations:
*   1. Create ONE buffer:
*      LVGL will draw the display's content here and writes it to your display
*
*   2. Create TWO buffer:
*      LVGL will draw the display's content to a buffer and writes it your
*      display.
*      You should use DMA to write the buffer's content to the display.
*      It will enable LVGL to draw the next part of the screen to the other
*      buffer while the data is being sent from the first buffer.
*      It makes rendering and flushing parallel.
*
*   3. Double buffering
*      Set 2 screens sized buffers and set disp_drv.full_refresh = 1.
*      This way LVGL will always provide the whole rendered screen in `flush_cb`
*      and you only need to change the frame buffer's address.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void lv_port_disp_init(void)
{

    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    reset_frame_buffer();

    /* Create LVGL v9 display and configure it */
    disp_gpu = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    if(disp_gpu == NULL) return;

    lv_display_set_flush_cb(disp_gpu, disp_flush);

    /* Provide LVGL a millisecond tick */
    lv_tick_set_cb(get_tick_ms);

#if (USE_PARTIAL_BUFFER_MODE != 0U) && defined(MTB_DISPLAY_CO5300)
    /* Hybrid model: PARTIAL strip + persistent scanout shadow_fb. */
    {
        /* LVGL renders into the strip; flush_cb copies strip -> shadow_fb. */
        lv_display_set_buffers(disp_gpu,
                               lvgl_strip_buf,
                               NULL,
                               sizeof(lvgl_strip_buf),
                               LV_DISPLAY_RENDER_MODE_PARTIAL);

        lv_display_set_color_format(disp_gpu, DISP_LV_COLOR_FORMAT);
        lv_display_set_render_mode(disp_gpu, LV_DISPLAY_RENDER_MODE_PARTIAL);
    }
#elif USE_SINGLE_BUFFER_MODE
    /* Single-buffer FULL mode: ~50% saving vs double-buffer (one buffer of
     * MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(disp_px_t)). */
    lv_display_set_buffers(disp_gpu,
                           disp_buf1,
                           NULL,
                           sizeof(disp_buf1),
                           LV_DISPLAY_RENDER_MODE_FULL);

    /* CO5300: 32bpp XRGB8888 (128-aligned stride); others: 16bpp RGB565 */
    lv_display_set_color_format(disp_gpu, DISP_LV_COLOR_FORMAT);
    lv_display_set_render_mode(disp_gpu, LV_DISPLAY_RENDER_MODE_FULL);
#else
    /* Double buffers, full-screen, full render mode */
    lv_display_set_buffers(disp_gpu,
                           disp_buf1,
                           disp_buf2,
                           sizeof(disp_buf1),
                           LV_DISPLAY_RENDER_MODE_FULL);

    /* CO5300: 32bpp XRGB8888 (128-aligned stride); others: 16bpp RGB565 */
    lv_display_set_color_format(disp_gpu, DISP_LV_COLOR_FORMAT);
    lv_display_set_render_mode(disp_gpu, LV_DISPLAY_RENDER_MODE_FULL);
#endif

    /* Make it the default display */
    lv_display_set_default(disp_gpu);

#if (USE_PARTIAL_BUFFER_MODE != 0U) && defined(MTB_DISPLAY_CO5300)
    /* End-of-refresh hook: issues the partial-window DBI transfer. */
    lv_display_add_event_cb(disp_gpu, disp_refr_ready_cb, LV_EVENT_REFR_READY, NULL);
    /* Expand invalidation areas by 1 px to cover GPU edge blending artifacts. */
    lv_display_add_event_cb(disp_gpu, disp_invalidate_area_cb, LV_EVENT_INVALIDATE_AREA, NULL);
#endif

    Cy_GFXSS_Clear_DC_Interrupt(base, &gfx_context);
}


/*******************************************************************************
* Function Name: reset_frame_buffer
********************************************************************************
* Summary:
*  Resets both the frame buffers before their usage.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void LV_ATTRIBUTE_FAST_MEM reset_frame_buffer(void)
{
#if (USE_PARTIAL_BUFFER_MODE != 0U) && defined(MTB_DISPLAY_CO5300)
    memset(shadow_fb, RESET_VALUE, sizeof(shadow_fb));
#else
    memset(disp_buf1, RESET_VALUE, sizeof(disp_buf1));
#if !USE_SINGLE_BUFFER_MODE
    memset(disp_buf2, RESET_VALUE, sizeof(disp_buf2));
#endif
#endif

#if defined(MTB_DISPLAY_CO5300)
    /* Frame buffers are non-cacheable; just order the writes before any
     * subsequent GFXSS access. 
     */
    __DSB();
#endif
}


/* [] END OF FILE */
