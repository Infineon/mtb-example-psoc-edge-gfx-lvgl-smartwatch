/*******************************************************************************
* File Name        : smartwatch_app.c
*
* Description      : This source file provides implementation for Smartwatch
*                    application based on Lvgl.
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
#include "smartwatch_app.h"

#include "cybsp.h"
#include "cybsp_types.h"
#include "cycfg_peripherals.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

#include "vg_lite_platform.h"

#if !LV_USE_DEMO_BENCHMARK
#include "ui.h"
#endif
#include "app_state_manager.h"
#include "time_date_task.h"
#include "step_count_task.h"

#include "app_logger.h"
#include "lv_api_map_v8.h"

#if LV_USE_DEMO_BENCHMARK
#include "demos/lv_demos.h"
#endif

#if defined(MTB_DISPLAY_CO5300)
#include "mtb_display_co5300.h"
#elif defined(W4P3INCH_DISP)
#include "mtb_disp_dsi_waveshare_4p3.h"
#endif


/*******************************************************************************
* Macros
*******************************************************************************/
#if defined(W4P3INCH_DISP)
#define USER_BTN1_IRQ_PRIORITY              (3U)
#endif /* W4P3INCH_DISP */

#define DEFAULT_SYSPM_CALLBACK_ORDER        (0U)
#define RESET_VAL                           (0U)
#define SET_VAL                             (1U)

#if defined(MTB_DISPLAY_CO5300)
#define SCREEN_REFRESH_TIME_MS              (30U)
#if LV_USE_DEMO_BENCHMARK
#define HIGH_PERF_REFRESH_MIN_TIME_MS       (1U)
#endif /* LV_USE_DEMO_BENCHMARK */
#define LP_TASK_TIMER_DEFAULT_TIME          (1000U)
#define LP_TASK_TIMER_RESET_TIME            (9000U)
#define LVGL_REFRESH_TIME_MS                (9000U)
#elif defined(W4P3INCH_DISP)
#define LVGL_REFRESH_TIME_MS                (1000U)
#define HIGH_PERF_REFRESH_MIN_TIME_MS       (10U)
#define LOW_POWER_SCREEN_REFRESH_TIME_MS    (200U)
/*
 * Toolchain-specific tearing workaround for W4P3 rectangle panel.
 *
 * To take full advantage of the double-buffer, the disp_flush commits the
 * new buffer (vsync-latched) and releases LVGL right away, so LVGL renders
 * the next frame while the just-committed buffer is still being scanned out.
 * The DCNano only latches the new address at the NEXT 60 Hz vsync. When the
 * LVGL loop runs faster than one vsync per frame, LVGL re-touches an on-screen
 * buffer mid-scan causing tearing.
 *
 * Some toolchain may render fast enough to sustain ~60 FPS and hit this race.
 * Capping the frame cadence to around ~36 FPS timing margin removes the tearing. */
#if LV_USE_DEMO_BENCHMARK
#define W4P3_HP_FRAME_PERIOD_MIN_MS         (15U)
#else
#define W4P3_HP_FRAME_PERIOD_MIN_MS         (13U)
#endif
#endif /* W4P3INCH_DISP */

/* The shared CO5300 driver hard-wires its DSI pixel_clock for a 512-px line
 * (DISPLAY_RES_WIDTH=512). We re-scale it to the real DC line width
 * (DISP_DC_HOR_RES) so the MIPI-DSI per-line timing matches the DC transfer
 * width and the panel - see gfx_init(). 14315 * 472/512 = 13197. */
#define MIPI_PIXEL_CLOCK_512                (14315U)

/* Enabling or disabling a MCWDT requires a wait time of upto 2 CLK_LF cycles
 * to come into effect. This wait time value will depend on the actual CLK_LF
 * frequency set by the BSP.
 */
#define LPTIMER_1_WAIT_TIME_USEC            (62U)

/* Define the LPTimer interrupt priority number. '1' implies highest priority.
 */
#define APP_LPTIMER_INTERRUPT_PRIORITY      (1U)


/*******************************************************************************
* Global Variables
*******************************************************************************/
GFXSS_Type* base = (GFXSS_Type*) GFXSS;
extern cy_stc_gfx_context_t gfx_context;
cy_stc_scb_i2c_context_t i2c_context;
extern volatile bool fb_pending;

/* Heap memory for VGLite to allocate memory for buffers, command, and
   tessellation buffers */
CY_SECTION(".cy_gpu_buf") uint8_t vglite_mem[VGLITE_HEAP_SIZE] = { 0xFF };

volatile uint8_t brightness      = MAX_BRIGHTNESS_PERCENT;

#if defined(MTB_DISPLAY_CO5300)
SemaphoreHandle_t frame_tx_sem = NULL;
#endif /* MTB_DISPLAY_CO5300 */

TaskHandle_t rtos_app_state_manager_task_handle = NULL;
TaskHandle_t rtos_app_task_handle = NULL;
SemaphoreHandle_t lvgl_mutex = NULL;

/* SCB0 - I2C interrupt configuration */
const cy_stc_sysint_t i2c_scb_irq_cfg =
{
    .intrSrc      = CYBSP_I2C_CONTROLLER_IRQ,
    .intrPriority = I2C_IRQ_PRIORITY,
};

/* GPU interrupt configuration */
const cy_stc_sysint_t gpu_int_config =
{
    .intrSrc      = GFXSS_GPU_IRQ,
    .intrPriority = GPU_INT_PRIORITY
};

#if defined(W4P3INCH_DISP)
/* Display controller interrupt configuration */
const cy_stc_sysint_t dc_int_config =
{
    .intrSrc      = GFXSS_DC_IRQ,
    .intrPriority = DC_INT_PRIORITY
};

/* USER BTN1 interrupt configuration */
const cy_stc_sysint_t user_btn1_irq_cfg =
{
    .intrSrc      = CYBSP_USER_BTN1_IRQ,
    .intrPriority = USER_BTN1_IRQ_PRIORITY
};
#endif /* W4P3INCH_DISP */

cy_stc_syspm_callback_params_t gfx_deep_sleep_app_params =
{
    .base = (GFXSS_Type*) GFXSS,
    .context = &gfx_context
};

cy_stc_syspm_callback_t gfx_deep_sleep_callback_cfg =
{
    .callback       = Cy_GFXSS_DeepSleepCallback,
    .type           = CY_SYSPM_DEEPSLEEP,
    .skipMode       = SYSPM_SKIP_MODE,
    .callbackParams = &gfx_deep_sleep_app_params,
    .prevItm        = NULL,
    .nextItm        = NULL,
    .order          = DEFAULT_SYSPM_CALLBACK_ORDER
};

/* GPU memory configuration */
vg_module_parameters_t gpu_mem_params =
{
    .register_mem_base      = (uint32_t) GFXSS_GFXSS_GPU_GCNANO,
    .gpu_mem_base[0]        = RESET_VAL,
    .contiguous_mem_base[0] = (volatile void *) vglite_mem,
    .contiguous_mem_size[0] = VGLITE_HEAP_SIZE
};

/* LPTimer HAL object */
static mtb_hal_lptimer_t lptimer_obj;

#if defined(MTB_DISPLAY_CO5300)
TimerHandle_t lp_task_timer = NULL;
static int run_count = 0;
#endif /*defined(MTB_DISPLAY_CO5300)*/

#if defined(W4P3INCH_DISP)
/*******************************************************************************
* Function Name: dc_irq_handler
********************************************************************************
* Summary:
*  Display Controller interrupt handler which gets invoked when the DC finishes
*  utilizing the current frame buffer.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void dc_irq_handler(void)
{
    fb_pending = false;
    Cy_GFXSS_Clear_DC_Interrupt(base, &gfx_context);
}
#endif /* W4P3INCH_DISP */


#if defined(MTB_DISPLAY_CO5300)
/* DC interrupt configuration for DBI command mode - slice completion */
const cy_stc_sysint_t dc_dbi_int_config =
{
    .intrSrc      = GFXSS_DC_IRQ,
    .intrPriority = DC_DBI_INT_PRIORITY
};

/*******************************************************************************
* Function Name: dc_dbi_irq_handler
********************************************************************************
* Summary:
*  DC interrupt handler for DBI command mode frame transfers.
*  Interrupt fires when the DC finishes consuming the framebuffer
*  for one DBI slice. The PDL handler chains slices and invokes the registered
*  completion callback when the frame is done.
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void dc_dbi_irq_handler(void)
{
    Cy_GFXSS_DC_DBI_InterruptHandler(base, &gfx_context);
}

/*******************************************************************************
* Function Name: on_frame_transfer_complete
********************************************************************************
* Summary:
*  Completion callback invoked by the PDL from ISR context when all DBI slices
*  have been transferred. Gives the binary semaphore so the LVGL flush task
*  can proceed with the next frame.
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void on_frame_transfer_complete(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Transfer done - disable DC_IRQ until the next async transfer starts.
     * This prevents spurious DC interrupts during DeepSleep idle windows
     * (the GFXSS AFTER_TRANSITION callback restores INTR_MASK on every wake,
     * which would fire a DC interrupt if DC_IRQ remained enabled in NVIC).
     * disp_flush() re-enables DC_IRQ right before the next transfer. */
    NVIC_DisableIRQ(GFXSS_DC_IRQ);

    /* Allow DeepSleep now that the ISR chain is complete. */
    dbi_transfer_in_flight = false;
    mtb_hal_syspm_unlock_deepsleep();

    xSemaphoreGiveFromISR(frame_tx_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
#endif /* MTB_DISPLAY_CO5300 */


/*******************************************************************************
* Function Name: gpu_irq_handler
********************************************************************************
* Summary:
*  GPU interrupt handler which gets invoked when the GPU finishes composing
*  a frame. It clears the GPU interrupt and invokes VGLite IRQ handler.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void gpu_irq_handler(void)
{
    Cy_GFXSS_Clear_GPU_Interrupt(base, &gfx_context);
    vg_lite_IRQHandler();
}


/*******************************************************************************
* Function Name: i2c_interrupt_callback
********************************************************************************
* Summary:
*  I2C interrupt callback enabling Controller High-Level functions to work.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void i2c_interrupt_callback(void)
{
    Cy_SCB_I2C_Interrupt(CYBSP_I2C_CONTROLLER_HW, &i2c_context);
}


#if LV_USE_DEMO_BENCHMARK
/*******************************************************************************
* Function Name: benchmark_end_cb
********************************************************************************
* Summary:
*  Called by lv_demo_benchmark() when all test scenes have completed.
*  Re-enables I2C + touch, then displays the summary results table.
*
*  The I2C SCB block is fully re-initialized (not just Disable/Enable) because
*  the IRQ was disabled for the entire benchmark run. During that time the
*  touch controller may have clocked data onto the bus that the ISR never
*  serviced, leaving the context struct and hardware state machine corrupted.
*  A full Cy_SCB_I2C_Init() resets both the registers and the software context.
*
* Parameters:
*  summary: Pointer to the benchmark summary provided by LVGL
*
* Return:
*  void
*
*******************************************************************************/
static void benchmark_end_cb(const lv_demo_benchmark_summary_t *summary)
{
    /* 1. Full I2C peripheral re-initialization */
    Cy_SCB_I2C_Disable(CYBSP_I2C_CONTROLLER_HW, &i2c_context);
    Cy_SCB_I2C_Init(CYBSP_I2C_CONTROLLER_HW,
                    &CYBSP_I2C_CONTROLLER_config,
                    &i2c_context);
    Cy_SCB_I2C_Enable(CYBSP_I2C_CONTROLLER_HW);

    /* 2. Clear stale IRQ flags and re-enable the ISR */
    NVIC_ClearPendingIRQ(i2c_scb_irq_cfg.intrSrc);
    NVIC_EnableIRQ(i2c_scb_irq_cfg.intrSrc);

    /* 3. Re-initialize touch controller and register LVGL indev */
    lv_port_indev_init();

    /* 4. Show the benchmark results table */
    lv_demo_benchmark_summary_display(summary);
}
#endif /* LV_USE_DEMO_BENCHMARK */


/*******************************************************************************
* Function Name: lptimer_interrupt_handler
********************************************************************************
* Summary:
*  Interrupt handler function for LPTimer instance.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void lptimer_interrupt_handler(void)
{
    mtb_hal_lptimer_process_interrupt(&lptimer_obj);
}


/*******************************************************************************
* Function Name: refresh_screen
********************************************************************************
* Summary:
*  This function handles calling "lv_task_handler" in thread safe manner.
*  "lv_task_handler" redraws the screen if required, handle input devices,
*  animation etc.
*
* Parameters:
*  void
*
* Return:
*  uint32_t time to wait
*
*******************************************************************************/
__STATIC_INLINE uint32_t refresh_screen(void)
{
    lv_display_t *disp     = NULL;
    lv_timer_t *anim_timer = NULL;
    uint32_t ret           = RESET_VAL;

    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE)
    {
        ret = lv_timer_handler();

        if ((LOW_POWER_STATE == active_state) && state_change_complete)
        {
            disp = lv_display_get_default();

            lv_timer_t *refr_timer = lv_display_get_refr_timer(disp);
            if (refr_timer)
            {
                lv_timer_set_period(refr_timer, LVGL_REFRESH_TIME_MS);
            }

            anim_timer = lv_anim_get_timer();
            lv_timer_set_period(anim_timer, LVGL_REFRESH_TIME_MS);

            state_change_complete = false;

        }
        else if (state_change_complete)
        {
            state_change_complete = false;
        }
        xSemaphoreGive(lvgl_mutex);
    }

    return ret;
}


/*******************************************************************************
* Function Name: smartwatch_app_task
********************************************************************************
* Summary:
*  This FreeRTOS task performs VGLite configuration, Lvgl and UI
*  initializations and regular updates to the screen/UI.
*
* Parameters:
*  *arg: Not used
*
* Return:
*  void
*
*******************************************************************************/
static void smartwatch_app_task(void *arg)
{
    vg_lite_error_t result  = VG_LITE_SUCCESS;
#if defined(W4P3INCH_DISP) || defined(MTB_DISPLAY_CO5300)
    uint32_t time_till_next = RESET_VAL;
#endif /* defined(W4P3INCH_DISP) || (defined(MTB_DISPLAY_CO5300) */

    CY_UNUSED_PARAMETER(arg);

    /* Allocate memory for VGLite from the vglite_heap_base */
    vg_lite_init_mem(&gpu_mem_params);

    /* Initialize the memory and data structures needed for VGLite draw/blit
     * functions */
    result = vg_lite_init(DEFAULT_VG_LITE_TW_WIDTH, DEFAULT_VG_LITE_TW_HEIGHT);
    if (VG_LITE_SUCCESS != result)
    {
        vg_lite_close();
        process_error((cy_rslt_t)result, "VGLite initialization failed. STOP.");
    }

    /* Initialize LVGL library */
    lv_init();
    lv_port_disp_init();

#if LV_USE_DEMO_BENCHMARK
    /* Disable I2C interrupt during benchmark - only needed for display panel
     * init. Re-enabled inside benchmark_end_cb() after all scenes finish. */
    NVIC_DisableIRQ(i2c_scb_irq_cfg.intrSrc);

    /* Register end callback - LVGL calls it when all scenes complete.
     * Inside the callback we re-init I2C, enable touch, and display the
     * summary table. */
    lv_demo_benchmark_set_end_cb(benchmark_end_cb);

    lv_demo_benchmark();
#else
    lv_port_indev_init();
    ui_init();

    /* Notify the date_time_task that UI init is done */
    xTaskNotifyGive(rtos_date_time_task_handle);

    /* Notify the step_count_task that UI init is done */
    xTaskNotifyGive(rtos_step_count_task_handle);
#endif

#if defined(MTB_DISPLAY_CO5300) && !LV_USE_DEMO_BENCHMARK
    /* Start input_inactivity_timer to track inactivity on display */
    if (pdPASS != input_inactivity_timer_start())
    {
        process_error(CY_RTOS_GENERAL_ERROR, "input_inactivity_timer start failed. STOP.");
    }
#elif defined(W4P3INCH_DISP)
    /* Ensure button interrupt is unmasked and enabled (handled in main too) */
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN);
    NVIC_ClearPendingIRQ(CYBSP_USER_BTN1_IRQ);
    Cy_GPIO_SetInterruptMask(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN, CYBSP_BTN_OFF);
    NVIC_EnableIRQ(user_btn1_irq_cfg.intrSrc);
#endif

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");
    printf("****************** "
           "PSOC Edge MCU: Smartwatch Demo using LVGL "
           "****************** \r\n\n");

#if defined(USE_PERFORMANCE_MONITOR)
    /* Initialize FPS monitor module */
    performance_monitor_init();
#endif /* USE_PERFORMANCE_MONITOR */

    for (;;)
    {
#if defined(MTB_DISPLAY_CO5300)
        if ( ULTRA_LOW_POWER_STATE == active_state )
        {
            xTaskNotifyWait(RESET_VAL, RESET_VAL, NULL, portMAX_DELAY);
        }
        else if ( ( LOW_POWER_STATE == active_state ) )
        {
            xTaskNotifyWait(RESET_VAL, RESET_VAL, NULL, portMAX_DELAY);
            vTaskResume(rtos_date_time_task_handle);
            vTaskResume(rtos_step_count_task_handle);
        }

        /* Input activity detected and display_active_timeout is false */
        if ((touch_activity) && (!display_active_timeout))
        {
            if (HIGH_PERFORMANCE_STATE != active_state)
            {
                dbi_transfer_enabled = false;
                active_state = HIGH_PERFORMANCE_STATE;
                xTaskNotify(rtos_app_state_manager_task_handle, RESET_VAL, eNoAction);
            }
            else
            {
                time_till_next = refresh_screen();

                /* Reset/restart the input_inactivity_timer (NULL in benchmark mode) */
                if (input_inactivity_timer != NULL)
                {
                    xTimerReset(input_inactivity_timer, RESET_VAL);
                }
            }
            touch_activity = false;
        }
        else if ((ULTRA_LOW_POWER_STATE != active_state) && (!display_active_timeout))
        /* No input activity and display_active_timeout is false */
        {
            time_till_next = refresh_screen();
        }

        if (ULTRA_LOW_POWER_STATE != active_state)
        {
#if LV_USE_DEMO_BENCHMARK
            if (HIGH_PERFORMANCE_STATE == active_state)
            {
                /* Dynamic timing: sleep until next LVGL timer fires, with a minimum delay
                 * to avoid tight polling. Mirrors the W4P3INCH approach below. */
                if (time_till_next)
                {
                    vTaskDelay(pdMS_TO_TICKS(time_till_next));
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(HIGH_PERF_REFRESH_MIN_TIME_MS));
                }
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(SCREEN_REFRESH_TIME_MS));
            }
#else
            CY_UNUSED_PARAMETER(time_till_next);
            vTaskDelay(pdMS_TO_TICKS(SCREEN_REFRESH_TIME_MS));
#endif /* LV_USE_DEMO_BENCHMARK */
        }
#elif defined(W4P3INCH_DISP)
        switch (active_state)
        {
            case HIGH_PERFORMANCE_STATE:
                /*
                * Refresh the screen and get the time (in ms) until the next update.
                * In some cases, refresh_screen() may return 0, indicating an immediate
                * refresh. However, taking a 0 ms delay causes continuous screen updates,
                * leading to display flickering.
                *
                * To avoid this, when time_till_next is 0, we introduce a minimum delay
                * (HIGH_PERF_REFRESH_MIN_TIME_MS) before the next refresh cycle. This ensures
                * smoother display rendering and prevents flickering.
                */
                time_till_next = refresh_screen();
#if (W4P3_HP_FRAME_PERIOD_MIN_MS > 0U)
                /* Toolchain dependent FPS cap: enforce a minimum frame period so the
                 * immediate-release flush never re-renders an on-screen buffer mid-scan. */
                if (time_till_next < W4P3_HP_FRAME_PERIOD_MIN_MS)
                {
                    time_till_next = W4P3_HP_FRAME_PERIOD_MIN_MS;
                }
#endif
                if (time_till_next)
                {
                    vTaskDelay(pdMS_TO_TICKS(time_till_next));
                }
                else
                {
                    vTaskDelay(pdMS_TO_TICKS(HIGH_PERF_REFRESH_MIN_TIME_MS));
                }
                break;

            case LOW_POWER_STATE:
                refresh_screen();
                vTaskDelay(pdMS_TO_TICKS(LOW_POWER_SCREEN_REFRESH_TIME_MS));
                break;

            default:
                break;
        }

#endif
    }
}


/*******************************************************************************
* Function Name: setup_tickless_idle_timer
********************************************************************************
* Summary:
*  1. This function first configures and initializes an interrupt for LPTimer.
*  2. Then it initializes the LPTimer HAL object to be used in the RTOS
*     tickless idle mode implementation to allow the device enter deep sleep
*     when idle task runs. LPTIMER_1 instance is configured for CM55 CPU.
*  3. It then passes the LPTimer object to abstraction RTOS library that
*     implements tickless idle mode
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void setup_tickless_idle_timer(void)
{
    /* Interrupt configuration structure for LPTimer */
    cy_stc_sysint_t lptimer_intr_cfg =
    {
        .intrSrc      = CYBSP_CM55_LPTIMER_1_IRQ,
        .intrPriority = APP_LPTIMER_INTERRUPT_PRIORITY
    };

    /* Initialize the LPTimer interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status =
                                             Cy_SysInt_Init(&lptimer_intr_cfg,
                                                    lptimer_interrupt_handler);

    /* LPTimer interrupt initialization failed. Stop program execution. */
    if (CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(lptimer_intr_cfg.intrSrc);

    /* Initialize the MCWDT block */
    cy_en_mcwdt_status_t mcwdt_init_status =
                                       Cy_MCWDT_Init(CYBSP_CM55_LPTIMER_1_HW,
                                                &CYBSP_CM55_LPTIMER_1_config);

    /* MCWDT initialization failed. Stop program execution. */
    if (CY_MCWDT_SUCCESS != mcwdt_init_status)
    {
        handle_error();
    }

    /* Enable MCWDT instance */
    Cy_MCWDT_Enable(CYBSP_CM55_LPTIMER_1_HW,
                    CY_MCWDT_CTR_Msk,
                    LPTIMER_1_WAIT_TIME_USEC);

    /* Setup LPTimer using the HAL object and desired configuration as defined
     * in the device configurator. */
    cy_rslt_t result = mtb_hal_lptimer_setup(&lptimer_obj,
                                             &CYBSP_CM55_LPTIMER_1_hal_config);

    /* LPTimer setup failed. Stop program execution. */
    if (CY_RSLT_SUCCESS != result)
    {
        handle_error();
    }

    /* Pass the LPTimer object to abstraction RTOS library that implements
     * tickless idle mode
     */
    cyabs_rtos_set_lptimer(&lptimer_obj);
}


#if defined(MTB_DISPLAY_CO5300)
/*******************************************************************************
* Function Name: lp_task_timer_callback
********************************************************************************
* Summary:
*  Callback function for the low power task timer (lp_task_timer). Increments
*  the run_count each time the timer expires. After the first expiry, the
*  timer period is changed from 1 second to 9 seconds. The function also
*  notifies the rtos_app_task_handle task to perform further processing.
*
* Parameters:
*  timer : Timer handle (not used, marked as unused).
*
* Return:
*  None
*
*******************************************************************************/
void lp_task_timer_callback(TimerHandle_t timer)
{
    run_count++;
    CY_UNUSED_PARAMETER(timer);

    if (run_count == 1)
    {
        // Change period to 9 seconds after the first run
        xTimerChangePeriod(lp_task_timer, pdMS_TO_TICKS(LP_TASK_TIMER_RESET_TIME), 0);
    }

    xTaskNotify(rtos_app_task_handle, 0, eNoAction);
}


/*******************************************************************************
* Function Name: stop_and_reset_timer
********************************************************************************
* Summary:
*  Stops the low power task timer (lp_task_timer) and resets its period back to 1 second.
*  The timer is not restarted after this call. Also resets the run_count variable.
*
* Parameters:
*  None
*
* Return:
*  void
*
*******************************************************************************/
void stop_and_reset_timer(void)
{
    BaseType_t status;

    /* Reset count */
    run_count = 0;

    /* Reset timer period back to 1 second */
    status= xTimerChangePeriod(lp_task_timer, pdMS_TO_TICKS(LP_TASK_TIMER_DEFAULT_TIME), 0);
    if (!status)
    {
        printf("Failed to reset timer period!\n");
    }

    /* Stop the timer */
    status= xTimerStop(lp_task_timer, 0);
    if (!status)
    {
        printf("Failed to stop timer!\n");
    }
}


/*******************************************************************************
* Function Name: lp_task_timer_init
********************************************************************************
* Summary:
*  Creates and initializes the low power timer (lp_task_timer) with a 1 second
*  period.The timer runs in auto-reload mode and executes
*  lp_task_timer_callback() on expiry.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void lp_task_timer_init(void)
{
    /* Create timer to derive and publish graphics frames per sec data */
    lp_task_timer = xTimerCreate("LP timer",
                                      pdMS_TO_TICKS(LP_TASK_TIMER_DEFAULT_TIME),
                                      pdTRUE, NULL,
                                      lp_task_timer_callback);

    CY_ASSERT(NULL != lp_task_timer);

}
#endif /*defined(MTB_DISPLAY_CO5300)*/

/*******************************************************************************
* Function Name: smartwatch_app_init
********************************************************************************
* Summary:
*  This function performs I2C, graphics subsystem, GPU and DC interrupt
*  initializations. It creates the application and state manager tasks and
*  performs necessary initializations related to UI interaction tasks.
*
* Parameters:
*  void
*
* Return:
*  cy_rslt_t: CY_RSLT_SUCCESS on successful initializations pertaining to the
*             application else the corresponding error encountered.
*
*******************************************************************************/
cy_rslt_t smartwatch_app_init(void)
{
    cy_rslt_t  result                 = CY_RSLT_SUCCESS;
    BaseType_t task_status            = RESET_VAL;
    cy_en_gfx_status_t status         = CY_GFX_SUCCESS;
    cy_en_scb_i2c_status_t i2c_status = CY_SCB_I2C_SUCCESS;

    /* Prevent CPU to go to DeepSleep */
    mtb_hal_syspm_lock_deepsleep();

    /* Configure graphics subsystem deepsleep callback */
    Cy_SysPm_RegisterCallback(&gfx_deep_sleep_callback_cfg);

    /* Configure SCB as I2C controller. Its used to interface with touch controller
     * and display initialization.
     */
    i2c_status = Cy_SCB_I2C_Init(CYBSP_I2C_CONTROLLER_HW, &CYBSP_I2C_CONTROLLER_config, &i2c_context);
    if (CY_SCB_I2C_SUCCESS != i2c_status)
    {
        process_error((cy_rslt_t)i2c_status, "I2C initialization failed. STOP.");
    }

    /* Interrupt initialization for SCB block */
    cy_en_sysint_status_t interrupt_init_status =
                                             Cy_SysInt_Init(&i2c_scb_irq_cfg,
                                                &i2c_interrupt_callback);
    if (CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_error();
    }
    NVIC_EnableIRQ(i2c_scb_irq_cfg.intrSrc);

    /* Enable I2C */
    Cy_SCB_I2C_Enable(CYBSP_I2C_CONTROLLER_HW);

    /* To make lvgl thread safe */
    lvgl_mutex = xSemaphoreCreateMutex();

    /* Overwrite device-configurator generated
     * MIPI DSI configuration with driver defined configurations
     */
#if defined(MTB_DISPLAY_CO5300)
    GFXSS_config.mipi_dsi_cfg = &mtb_display_co5300_gfx_mipi_dsi_config;

    /* The shared CO5300 driver's DSI config is hard-wired to a 512-px line
     * (DISPLAY_RES_WIDTH=512, pixel_clock=14315). That programs the MIPI-DSI
     * controller's VID_PKT_SIZE and per-line byte-clock (LBCC) timing, which
     * must match the DC transfer width (DISP_DC_HOR_RES):
     *   - Benchmark demo (32-bit) : DC width 472. The default 512-px DSI line
     *     would mis-frame every row (512 vs 472) -> progressive per-row shift
     *     (the "complete tearing + horizontal lines"). Re-scale to hdisplay=472,
     *     pixel_clock=13197 = 14315 * 472/512 (the proven reference values).
     *   - Smartwatch UI (16-bit) : DC width 512. The formula yields hdisplay=512
     *     and pixel_clock=14315 - identical to the driver default - so this is a
     *     no-op and keeps the original, proven-good 512-px geometry.
     * One expression covers both because pixel_clock scales linearly with the
     * line width. */
    GFXSS_config.mipi_dsi_cfg->display_params->hdisplay    = DISP_DC_HOR_RES;
    GFXSS_config.mipi_dsi_cfg->display_params->pixel_clock =
            (uint32_t)(((uint64_t)MIPI_PIXEL_CLOCK_512 * DISP_DC_HOR_RES) / 512U);

    /* DC/panel transfer width = DISP_DC_HOR_RES (472 benchmark / 512 smartwatch
     * - see lv_port_disp.h). The value is chosen per color depth so the DC line
     * stride roundup(DISP_DC_HOR_RES * bytes_per_px, 128) equals the LVGL buffer
     * row stride (MY_DISP_HOR_RES * bytes_per_px); a mismatch causes per-row
     * wrap / tearing or a blank panel. */
    GFXSS_config.dc_cfg->display_width  = DISP_DC_HOR_RES;
    GFXSS_config.dc_cfg->display_height = DISP_DC_VER_RES;

    /* Tell the DC graphics layer which source pixel format to read; it must
     * match the LVGL frame-buffer depth (LV_COLOR_DEPTH / DISP_USE_XRGB8888):
     *   - Benchmark demo : 32-bit XRGB8888 source (vivARGB8888).
     *   - Smartwatch UI  : 16-bit RGB565 source (vivRGB565), matching the
     *     SquareLine assets. The panel itself stays 24-bit RGB888
     *     (display_format unchanged); the DC converts the source on the fly. */
#if DISP_USE_XRGB8888
    GFXSS_config.dc_cfg->gfx_layer_config->input_format_type = vivARGB8888;
#else
    GFXSS_config.dc_cfg->gfx_layer_config->input_format_type = vivRGB565;
#endif

#elif defined(W4P3INCH_DISP)
    GFXSS_config.dc_cfg->ovl0_layer_config->layer_enable = false;

    GFXSS_config.dc_cfg->gfx_layer_config->pos_x        = 0;
    GFXSS_config.dc_cfg->gfx_layer_config->pos_y        = 0;
    GFXSS_config.dc_cfg->gfx_layer_config->layer_enable = true;
    GFXSS_config.dc_cfg->display_type                   = GFX_DISP_TYPE_DSI_DPI;
    GFXSS_config.dc_cfg->display_format                 = vivD24;
    GFXSS_config.dc_cfg->display_width                  = MTB_DISP_WAVESHARE_4P3_HOR_RES;
    GFXSS_config.dc_cfg->display_height                 = MTB_DISP_WAVESHARE_4P3_VER_RES;
    GFXSS_config.mipi_dsi_cfg                           = &mtb_disp_waveshare_4p3_dsi_config;
#endif

    GFXSS_config.dc_cfg->gfx_layer_config->buffer_address    = frame_buffer1;
    GFXSS_config.dc_cfg->gfx_layer_config->uv_buffer_address = frame_buffer1;

    /* Graphics layer reads DISP_DC_HOR_RES visible columns per row with a
     * 128-aligned line stride that matches the LVGL buffer stride (472/480 for
     * the 32-bit benchmark, 512/512 for the 16-bit smartwatch UI). */
    GFXSS_config.dc_cfg->gfx_layer_config->width  = DISP_DC_HOR_RES;
    GFXSS_config.dc_cfg->gfx_layer_config->height = DISP_DC_VER_RES;

    /* Updated GFXSS clk/CLK_HF1 */
    GFXSS_config.clockHz = Cy_SysClk_ClkHfGetFrequency(CY_CFG_SYSCLK_CLKHF1);

    /* Initializes the graphics subsystem according to the configuration */
    status = Cy_GFXSS_Init(base, &GFXSS_config, &gfx_context);
    if (CY_GFX_SUCCESS != status)
    {
        process_error((cy_rslt_t)status, "Gfxss initialization failed. STOP.");
    }

    /* Create Smartwatch app FreeRTOS Task */
    task_status = xTaskCreate(smartwatch_app_task, APP_TASK_NAME,
                                                   APP_TASK_STACK_SIZE,
                                                   NULL,
                                                   APP_TASK_PRIORITY,
                                                   &rtos_app_task_handle);
    if (pdPASS != task_status)
    {
        process_error(CY_RTOS_GENERAL_ERROR, "smartwatch_app_task creation failed. STOP.");
    }

#if defined(MTB_DISPLAY_CO5300)
    /* Create binary semaphore for frame transfer completion signaling */
    frame_tx_sem = xSemaphoreCreateBinary();
    CY_ASSERT(frame_tx_sem != NULL);
    xSemaphoreGive(frame_tx_sem);  /* Initially available for first flush */
#endif /* MTB_DISPLAY_CO5300 */

#if !LV_USE_DEMO_BENCHMARK
    /* Create application state manager FreeRTOS Task */
    task_status = xTaskCreate(app_state_manager_task, APP_STATE_MANAGER_TASK_NAME, APP_STATE_MANAGER_TASK_STACK_SIZE,
            NULL, APP_STATE_MANAGER_TASK_PRIORITY, &rtos_app_state_manager_task_handle);

    if (pdPASS != task_status)
    {
        process_error(CY_RTOS_GENERAL_ERROR, "app_state_manager_task creation failed. STOP.");
    }

    /* Perform initialization pertaining to time and date update FreeRTOS task */
    time_date_task_init();

    /* Perform initialization pertaining to step count task */
    step_count_task_init();
#endif /* !LV_USE_DEMO_BENCHMARK */

    /* Intialize GPU interrupt */
    result = (cy_rslt_t)Cy_SysInt_Init(&gpu_int_config, gpu_irq_handler);
    if (CY_RSLT_SUCCESS != result)
    {
        process_error(result, "GPU interrupt registration failed. STOP.");
    }
    /* Enable GPU interrupt */
    Cy_GFXSS_Enable_GPU_Interrupt(base);

    /* Enable GPU interrupt in NVIC */
    NVIC_EnableIRQ(gpu_int_config.intrSrc);

    /* Clear pending GPU interrupt in NVIC */
    NVIC_ClearPendingIRQ(gpu_int_config.intrSrc);

#if defined(MTB_DISPLAY_CO5300)
    lp_task_timer_init();

    /* Register completion callback - PDL calls this from ISR when frame done */
    Cy_GFXSS_RegisterTransferCompleteCallback(&gfx_context, on_frame_transfer_complete);

    /* Initialize DC interrupt for DBI command mode frame transfers */
    result = (cy_rslt_t)Cy_SysInt_Init(&dc_dbi_int_config, dc_dbi_irq_handler);
    if (CY_RSLT_SUCCESS != result)
    {
        process_error(result, "DC DBI interrupt registration failed. STOP.");
    }

    /* Enable DC DBI interrupt in NVIC */
    NVIC_EnableIRQ(dc_dbi_int_config.intrSrc);

    /* Clear pending DC DBI interrupt in NVIC */
    NVIC_ClearPendingIRQ(dc_dbi_int_config.intrSrc);
#endif /*MTB_DISPLAY_CO5300*/

#if defined(W4P3INCH_DISP)
    /* Intialize Display Controller (DC) interrupt */
    result = (cy_rslt_t)Cy_SysInt_Init(&dc_int_config, dc_irq_handler);
    if (CY_RSLT_SUCCESS != result)
    {
        process_error(result, "DC interrupt registration failed. STOP.");
    }

    /* Enable DC interrupt in NVIC */
    NVIC_EnableIRQ(dc_int_config.intrSrc);

    /* Clear pending DC interrupt in NVIC */
    NVIC_ClearPendingIRQ(dc_int_config.intrSrc);
#endif /* W4P3INCH_DISP */

    return result;
}

/* [] END OF FILE */
