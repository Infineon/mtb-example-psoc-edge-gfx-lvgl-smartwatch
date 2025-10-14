/*******************************************************************************
* File Name        : smartwatch_app.c
*
* Description      : This source file provides implementation for Smartwatch
*                    application based on Lvgl.
*
* Related Document : See README.md
*
********************************************************************************
* (c) 2024-2025, Infineon Technologies AG, or an affiliate of Infineon
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

#include "cybsp_types.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"

#include "vg_lite_platform.h"

#include "ui.h"
#include "app_state_manager.h"
#include "time_date_task.h"
#include "step_count_task.h"

#include "app_logger.h"
#include "lv_api_map_v8.h"

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
#define LP_TASK_TIMER_DEFAULT_TIME          (1000U)
#define LP_TASK_TIMER_RESET_TIME            (9000U)
#define LVGL_REFRESH_TIME_MS                (9000U)
#elif defined(W4P3INCH_DISP)
#define LVGL_REFRESH_TIME_MS                (1000U)
#define LOW_POWER_SCREEN_REFRESH_TIME_MS    (200U)
#endif 

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

/* Heap memory for VGLite to allocate memory for buffers, command, and
   tessellation buffers */
CY_SECTION(".cy_gpu_buf") uint8_t vglite_mem[VGLITE_HEAP_SIZE] = { 0xFF };

volatile uint8_t brightness      = MAX_BRIGHTNESS_PERCENT;

#if defined(MTB_DISPLAY_CO5300)
TaskHandle_t rtos_frame_tx_task_handle = NULL;
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
/*******************************************************************************
* Function Name: frame_transfer_task
********************************************************************************
* Summary:
*  This freeRTOS task handles transferring rendered frames to display panel from
*  display controller.
*
* Parameters:
*  *arg: Not used
*
* Return:
*  void
*
*******************************************************************************/
static void frame_transfer_task(void *arg)
{
    CY_UNUSED_PARAMETER(arg);

    frame_tx_done = true;

    for (;;)
    {
        if (pdPASS == xTaskNotifyWait(RESET_VAL, RESET_VAL, NULL, portMAX_DELAY))
        {
            Cy_GFXSS_Transfer_Frame((GFXSS_Type*) GFXSS, &gfx_context);

            frame_tx_done = true;
        }
    }
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
*  void
*
*******************************************************************************/
__STATIC_INLINE void refresh_screen(void)
{
    lv_display_t *disp        = NULL;
    lv_timer_t *anim_timer = NULL;

    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE)
    {
        lv_timer_handler();

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
    vg_lite_error_t result = VG_LITE_SUCCESS;

    CY_UNUSED_PARAMETER(arg);

    /* Allocate memory for VGLite from the vglite_heap_base */
    vg_lite_init_mem(&gpu_mem_params);

    /* Initialize the memory and data structures needed for VGLite draw/blit
       functions */
    result = vg_lite_init(DEFAULT_VG_LITE_TW_WIDTH, DEFAULT_VG_LITE_TW_HEIGHT);
    if (VG_LITE_SUCCESS != result)
    {
        vg_lite_close();
        process_error((cy_rslt_t)result, "VGLite initialization failed. STOP.");
    }

    /* Initialize LVGL library */
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();

    ui_init();

    /* Notify the date_time_task that UI init is done */
    xTaskNotifyGive(rtos_date_time_task_handle);

    /* Notify the step_count_task that UI init is done */
    xTaskNotifyGive(rtos_step_count_task_handle);

#if defined(MTB_DISPLAY_CO5300)
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
                active_state = HIGH_PERFORMANCE_STATE;
                xTaskNotify(rtos_app_state_manager_task_handle, RESET_VAL, eNoAction);
            }
            else
            {
                refresh_screen();

                /* Reset/restart the input_inactivity_timer */
                xTimerReset(input_inactivity_timer, RESET_VAL);
            }
            touch_activity = false;
        }
        else if ((ULTRA_LOW_POWER_STATE != active_state) && (!display_active_timeout))
        /* No input activity and display_active_timeout is false */
        {
            refresh_screen();
        }

        if (ULTRA_LOW_POWER_STATE != active_state)
        {
            vTaskDelay(pdMS_TO_TICKS(SCREEN_REFRESH_TIME_MS));
        }
#elif defined(W4P3INCH_DISP)
        switch (active_state)
        {
            case HIGH_PERFORMANCE_STATE:
                refresh_screen();
                vTaskDelay(pdMS_TO_TICKS(LV_DEF_REFR_PERIOD));
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
*  timer period is changed from 1 second to 10 seconds. The function also 
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
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    run_count++;
    CY_UNUSED_PARAMETER(timer);

    if (run_count == 1)
    {
        // Change period to 10 seconds after 1 runs
        xTimerChangePeriod(lp_task_timer, pdMS_TO_TICKS(LP_TASK_TIMER_RESET_TIME), 0);
    }

    xTaskNotifyFromISR(rtos_app_task_handle, 0, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/*******************************************************************************
* Function Name: stop_and_reset_timer
********************************************************************************
* Summary:
*  Stops the low power task timer (lp_task_timer) and resets its period back to 1 second.
*  The timer is not restarted after this call. Also resets the runCount variable.
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

    // Reset count
    run_count = 0;

    // Reset timer period back to 1 second
    status= xTimerChangePeriod(lp_task_timer, pdMS_TO_TICKS(LP_TASK_TIMER_DEFAULT_TIME), 0);
    if (!status)
    {
        printf("Failed to reset timer period!\n");
    }

    // Stop the timer
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
*  initializations. It creates application, state manager, frame transfer 
*  tasks and perform necessary initializations related to UI interaction tasks. 
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

    GFXSS_config.dc_cfg->gfx_layer_config->width  = MY_DISP_HOR_RES;
    GFXSS_config.dc_cfg->gfx_layer_config->height = MY_DISP_VER_RES;

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
    /* Create frame transfer FreeRTOS Task */
    task_status = xTaskCreate(frame_transfer_task, FRAME_TX_TASK_NAME,
                                                   FRAME_TX_TASK_STACK_SIZE,
                                                   NULL,
                                                   FRAME_TX_TASK_PRIORITY,
                                                   &rtos_frame_tx_task_handle);

    if (pdPASS != task_status)
    {
        process_error(CY_RTOS_GENERAL_ERROR, "frame_transfer_task creation failed. STOP.");
    }
#endif /* MTB_DISPLAY_CO5300 */

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
