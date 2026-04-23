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
#include <stdbool.h>
#include <string.h>

#include "smartwatch_app.h"

#include "app_logger.h"

#include "FreeRTOS.h"
#include "task.h"
#include "cycfg_peripherals.h"

#if defined(MTB_DISPLAY_CO5300)
#include "mtb_display_co5300.h"
#elif defined(W4P3INCH_DISP)
#include "mtb_disp_dsi_waveshare_4p3.h"
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
/* Display frame buffers */
CY_SECTION(".cy_gpu_buf") lv_color_t disp_buf1[MY_DISP_HOR_RES *
                                               MY_DISP_VER_RES];
CY_SECTION(".cy_gpu_buf") lv_color_t disp_buf2[MY_DISP_HOR_RES *
                                               MY_DISP_VER_RES];

#if defined(MTB_DISPLAY_CO5300)
volatile bool frame_tx_done = false;
#elif defined(W4P3INCH_DISP)
volatile bool fb_pending = false;
#endif

/* GPU and CPU based display driver structures */
lv_display_t * disp_gpu = NULL;

/* Frame buffers used by GFXSS to render UI */
void *frame_buffer1 = &disp_buf1;
void *frame_buffer2 = &disp_buf2;

cy_stc_gfx_context_t gfx_context;

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

/* Return system tick in milliseconds for LVGL */
static uint32_t get_tick_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/*******************************************************************************
* Function Name: disp_init
********************************************************************************
* Summary:
*  Display initialization function configuring 1.43"/7" display driver.
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
*  *disp_drv: Pointer to the display driver structure to be registered by HAL.
*  *area: Pointer to the area of the screen (not used).
*  *color_p: Pointer to the frame buffer address.
*
* Return:
*  void
*
*******************************************************************************/

void LV_ATTRIBUTE_FAST_MEM disp_flush(lv_display_t *disp, const lv_area_t *area,
        uint8_t *px_map)
{
    CY_UNUSED_PARAMETER(area);

    /* Wait until frame is transmitted to display */
#if defined(MTB_DISPLAY_CO5300)
    while (!frame_tx_done)
#elif defined(W4P3INCH_DISP)
    while (fb_pending)
#endif
    {
        vTaskDelay(pdMS_TO_TICKS(FRAME_TX_WAIT_MS));
    }


    Cy_GFXSS_Set_FrameBuffer(base, (uint32_t*) px_map, &gfx_context);


    /* Kick the transfer task (non-blocking flush) */
#if defined(MTB_DISPLAY_CO5300)
    extern TaskHandle_t rtos_frame_tx_task_handle;
    if (rtos_frame_tx_task_handle != NULL) 
    {
        xTaskNotify(rtos_frame_tx_task_handle, 0, eNoAction);
    }
#endif

    /* Count frames for performance monitor (FPS) */
#if defined(USE_PERFORMANCE_MONITOR)
    frame_count++;
#endif

    /* Complete the flush for LVGL immediately (non-blocking) */
    lv_display_flush_ready(disp);
#if defined(MTB_DISPLAY_CO5300)
    frame_tx_done = false;
#elif defined(W4P3INCH_DISP)
    fb_pending = true;
#endif
}


/*******************************************************************************
* Function Name: lv_port_disp_init
********************************************************************************
* Summary:
*  Initialization function for display devices supported by LVGL.
*  LVGL requires a buffer where it internally draws the widgets.
*  Later this buffer will be passed to your display driver's `flush_cb` to copy
*  its content to your display.
*  The buffer has to be greater than 1 display row
*
*  There are 3 buffering configurations:
*  1. Create ONE buffer:
*     LVGL will draw the display's content here and writes it to your display
*
*  2. Create TWO buffer:
*     LVGL will draw the display's content to a buffer and writes it your
*     display.
*     You should use DMA to write the buffer's content to the display.
*     It will enable LVGL to draw the next part of the screen to the other
*     buffer while the data is being sent form the first buffer.
*     It makes rendering and flushing parallel.
*
*  3. Double buffering
*     Set 2 screens sized buffers and set disp_drv.full_refresh = 1.
*     This way LVGL will always provide the whole rendered screen in `flush_cb`
*     and you only need to change the frame buffer's address.
*
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

    /* Double buffers, full-screen, full render mode */
    lv_display_set_buffers(disp_gpu,
                           disp_buf1,
                           disp_buf2,
                           sizeof(disp_buf1),
                           LV_DISPLAY_RENDER_MODE_FULL);

    /* 16bpp RGB565 to fit gfx_mem and match panel */
    lv_display_set_color_format(disp_gpu, LV_COLOR_FORMAT_RGB565);
    lv_display_set_render_mode(disp_gpu, LV_DISPLAY_RENDER_MODE_FULL);

    /* Make it the default display */
    lv_display_set_default(disp_gpu);


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
    memset(disp_buf1, RESET_VALUE, sizeof(disp_buf1));
    memset(disp_buf2, RESET_VALUE, sizeof(disp_buf2));
}


/* [] END OF FILE */
