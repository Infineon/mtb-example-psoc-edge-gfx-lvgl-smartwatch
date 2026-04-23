/*******************************************************************************
* File Name        : app_state_manager.c
*
* Description      : This file implements functions to handle application state
*                    change, application state manager task, input activity 
*                    timer callback and PLL configuration.
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
#include "cybsp.h"

#include "app_state_manager.h"
#include "smartwatch_app.h"
#include "lv_port_disp.h"
#include "ui.h"
#include "lv_draw_sw.h"
#include "lv_port_disp.h"
#include "lv_api_map_v8.h"

#if defined(MTB_DISPLAY_CO5300)
#include "mtb_display_co5300.h"
#elif defined(W4P3INCH_DISP)
#include "mtb_disp_dsi_waveshare_4p3.h"
#endif

#include "app_logger.h"


/*******************************************************************************
* Macros
*******************************************************************************/
/* ECO frequency = 17.2032 MHz */
#define DPLL_ECO_INPUT_FREQ_HZ           (17203200U)

/* DPLL LP frequency:
 * In HP mode  = 400 MHz 
 * In LP mode  = 140 MHz 
 * In ULP mode = 50 MHz 
 */
#define DPLL_LP0_OUTPUT_FREQ_HP_HZ       (400000000U)
#define DPLL_LP1_OUTPUT_FREQ_HP_HZ       (300000000U)
#define DPLL_LP0_OUTPUT_FREQ_LP_HZ       (140000000U)
#define DPLL_LP1_OUTPUT_FREQ_LP_HZ       (110000000U)
#define DPLL_LP_OUTPUT_FREQ_ULP_HZ       (50000000U)
#define DPLL_ENABLE_TIMEOUT_MS           (10000U)

#define SCREEN_TIMEOUT_MS                (30000U)

/* Clock divider configurations for Debug UART per core frequency */
#define DEBUG_UART_DIVIDER_NUM           (1U)
#define DEBUG_UART_HP_DIVIDER_VAL        (86U)

#if defined(W4P3INCH_DISP)
#define DEBOUNCE_TIME_MS                 (2U)
#define DEBUG_UART_LP_DIVIDER_VAL        (29U)
#endif /* W4P3INCH_DISP */

#define DEBUG_UART_ULP_DIVIDER_VAL       (10U)
#define UART_HP_DIV                      (86U)
#define UART_LP_DIV                      (30U)
#define UART_ULP_DIV                     (10U)
#define SET_VALUE                        (1U)
#define RESET_VALUE                      (0U)
#define USER_BTN1_ISR_MIN_GAP_MS         (500U)


/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Variable to track application state */
volatile application_states_t active_state = HIGH_PERFORMANCE_STATE;
/* Variable to track application state change */
volatile bool state_change_complete = false;
volatile bool gpu_enable = true;
#if defined(MTB_DISPLAY_CO5300)
TimerHandle_t input_inactivity_timer = NULL;
volatile bool display_active_timeout = false;
#endif /* MTB_DISPLAY_CO5300 */

/*******************************************************************************
* Function Name: dpll_lp0_set_freq
********************************************************************************
* Summary:
*  Configure and update LP-DPLL0 (Low Power DPLL0) frequency.
*
* Parameters:
*  freq: Output frequency
*
* Return:
*  void
*
*******************************************************************************/
void dpll_lp0_set_freq(uint32_t freq)
{
    cy_stc_pll_config_t dpll_lp;

    dpll_lp.inputFreq  = DPLL_ECO_INPUT_FREQ_HZ;
    dpll_lp.outputMode = CY_SYSCLK_FLLPLL_OUTPUT_AUTO;
    dpll_lp.outputFreq = freq;

    Cy_SysClk_PllDisable(SRSS_DPLL_LP_0_PATH_NUM);

    if (CY_SYSCLK_SUCCESS !=
        Cy_SysClk_PllConfigure(SRSS_DPLL_LP_0_PATH_NUM, &dpll_lp))
    {
        CY_ASSERT(0);
    }

    if (CY_SYSCLK_SUCCESS !=
        Cy_SysClk_PllEnable(SRSS_DPLL_LP_0_PATH_NUM, DPLL_ENABLE_TIMEOUT_MS))
    {
        CY_ASSERT(0);
    }
}

/*******************************************************************************
* Function Name: dpll_lp1_set_freq
********************************************************************************
* Summary:
*  Configure and update LP-DPLL1 (Low Power DPLL1) frequency.
*
* Parameters:
*  freq: Output frequency
*
* Return:
*  void
*
*******************************************************************************/
void dpll_lp1_set_freq(uint32_t freq)
{
    cy_stc_pll_config_t dpll_lp1;

    dpll_lp1.inputFreq  = DPLL_ECO_INPUT_FREQ_HZ;
    dpll_lp1.outputMode = CY_SYSCLK_FLLPLL_OUTPUT_AUTO;
    dpll_lp1.outputFreq = freq;

    Cy_SysClk_PllDisable(SRSS_DPLL_LP_1_PATH_NUM);

    if (CY_SYSCLK_SUCCESS !=
        Cy_SysClk_PllConfigure(SRSS_DPLL_LP_1_PATH_NUM, &dpll_lp1))
    {
        CY_ASSERT(0);
    }

    if (CY_SYSCLK_SUCCESS !=
        Cy_SysClk_PllEnable(SRSS_DPLL_LP_1_PATH_NUM, DPLL_ENABLE_TIMEOUT_MS))
    {
        CY_ASSERT(0);
    }
}

/*******************************************************************************
* Function Name: app_state_update
********************************************************************************
* Summary:
*  Configures and manages clock settings, graphics subsystem, display brightness 
*  and screen/UI change in the corresponding application state for optimal 
*  system performance and power consumption.
*
* Parameters:
*  state: Application state 
*
* Return:
*  void
*
*******************************************************************************/
static void app_state_update(application_states_t state)
{
    cy_en_gfx_status_t result;
    vg_lite_error_t stat;
    lv_display_t *cur = NULL;
    
#if defined(MTB_DISPLAY_CO5300)
    cy_en_syspm_status_t status;
#endif /*defined(MTB_DISPLAY_CO5300)*/

    if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY))
    {
        Cy_GFXSS_Interrupt(base, &gfx_context);
#if defined(MTB_DISPLAY_CO5300)
        stop_and_reset_timer();
#endif /*defined(MTB_DISPLAY_CO5300)*/

#if defined(USE_PERFORMANCE_MONITOR) && ( configGENERATE_RUN_TIME_STATS == 1 )
        setup_run_time_stats_timer();
#endif /*defined(USE_PERFORMANCE_MONITOR) && ( configGENERATE_RUN_TIME_STATS == 1 )*/
        switch (state)
        {
            /* High performance graphics powered by GPU in HP mode. CM55 @400 MHz */
            case HIGH_PERFORMANCE_STATE:
                /* Resume performance monitor in HP state */
#if defined(USE_PERFORMANCE_MONITOR)
                performance_monitor_resume();
#endif /* USE_PERFORMANCE_MONITOR */
                /* Prevent CPU to go to DeepSleep
                 * to achieve high refresh rate and in that case 
                 * CPU has to handover the framebuffer to DC at every 16 ms                
                 */
                mtb_hal_syspm_lock_deepsleep();

                /* System Domain Idle Power Mode Configuration */
                Cy_SysPm_SetDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP_NONE);

                /* System SRAM (SoCMEM) Idle Power Mode Configuration */
                Cy_SysPm_SetSOCMEMDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP_NONE);

                /* Exit MIPI DSI ULPM */
                Cy_MIPIDSI_ExitULPM(&base->GFXSS_MIPIDSI);

#if defined(MTB_DISPLAY_CO5300)
                mtb_display_co5300_on(&base->GFXSS_MIPIDSI);
#endif /* MTB_DISPLAY_CO5300 */

                result = Cy_GFXSS_DeInit(base, &gfx_context);
                if (CY_GFX_SUCCESS != result)
                {
                    process_error((cy_rslt_t)result, "Gfxss deinitialization failed. STOP.");
                }

                /* Set HP as System Active Power Profile */
                Cy_SysPm_SystemEnterHp();

                /** Check if the system successfully entered HP mode. */
                if (Cy_SysPm_ReadStatus() & CY_SYSPM_STATUS_SYSTEM_HP)
                {
                    /** Set the RRAM to HP voltage mode*/
                    Cy_RRAM_SetVoltageMode(RRAMC0, CY_RRAM_VMODE_HP);
                    
                    Cy_SysClk_ClkHfSetDivider(CY_CFG_SYSCLK_CLKHF0, CY_SYSCLK_CLKHF_DIVIDE_BY_2);

                    dpll_lp0_set_freq(DPLL_LP0_OUTPUT_FREQ_HP_HZ);
                    dpll_lp1_set_freq(DPLL_LP1_OUTPUT_FREQ_HP_HZ);

                    /** Set the peripheral clock divider for the debug UART */
                    Cy_SysClk_PeriPclkSetDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM,
                            CY_SYSCLK_DIV_16_BIT, 1U, UART_HP_DIV);
                }
               
                SystemCoreClockUpdate();

                Cy_SysTick_Disable();

                Cy_SysTick_SetReload((configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - SET_VALUE);
                Cy_SysTick_Clear();
                Cy_SysTick_Enable();

                /* Update CLK divider for Debug UART as per core frequency */
                Cy_SysClk_PeriPclkDisableDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM, CY_SYSCLK_DIV_16_BIT, DEBUG_UART_DIVIDER_NUM);
                Cy_SysClk_PeriPclkSetDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM, CY_SYSCLK_DIV_16_BIT, DEBUG_UART_DIVIDER_NUM, DEBUG_UART_HP_DIVIDER_VAL);
                Cy_SysClk_PeriPclkEnableDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM, CY_SYSCLK_DIV_16_BIT, DEBUG_UART_DIVIDER_NUM);

                /* Re-initialize the graphics subsystem as per updated clock */
                GFXSS_config.clockHz = Cy_SysClk_ClkHfGetFrequency(CY_CFG_SYSCLK_CLKHF1);
                GFXSS_config.gpu_cfg->enable = true;

                /* Reset frame buffers before switching to different application
                 * state for smoother UI transition.
                 */
                reset_frame_buffer();

                result = Cy_GFXSS_Init(base, &GFXSS_config, &gfx_context);
                if (CY_GFX_SUCCESS != result)
                {
                    process_error((cy_rslt_t)result, "Gfxss re-initialization failed. STOP.");
                }
                /* Enable GPU interrupt */
                Cy_GFXSS_Enable_GPU_Interrupt(base);

                /* Enable GPU interrupt in NVIC */
                NVIC_EnableIRQ(GFXSS_GPU_IRQ);

#if defined(W4P3INCH_DISP)
                /* Enable DC interrupt in NVIC to synchronize frame transfers 
                 * with the completion interrupt of frame buffer transfers from
                 * DC.
                 */
                NVIC_EnableIRQ(GFXSS_DC_IRQ);
#endif

                /* Enable vglite to use GPU */
                stat = vg_lite_init(DEFAULT_VG_LITE_TW_WIDTH, DEFAULT_VG_LITE_TW_HEIGHT);
                if (VG_LITE_SUCCESS != stat)
                {
                    vg_lite_close();
                    process_error((cy_rslt_t)stat, "VGLite re-initialization failed. STOP.");
                }
                gpu_enable = true;
                
                extern lv_indev_t* indev_touchpad;                
                cur = lv_display_get_default();
                if (cur) 
                {
                    lv_indev_set_display(indev_touchpad, cur);
                    lv_timer_t * anim_timer_cpu_fb = lv_anim_get_timer();
                    lv_timer_set_period(anim_timer_cpu_fb, LV_DEF_REFR_PERIOD);
                    lv_timer_t * refr_timer_cpu_fb = lv_display_get_refr_timer(cur);
                    if (refr_timer_cpu_fb) 
                    {
                        lv_timer_set_period(refr_timer_cpu_fb, LV_DEF_REFR_PERIOD);
                    }
                }
                
                /* Display ui_DigitalScreen */
                brightness = MAX_BRIGHTNESS_PERCENT;
                _ui_screen_change(&ui_DigitalScreen, LV_SCR_LOAD_ANIM_FADE_ON, RESET_VALUE, RESET_VALUE, &ui_DigitalScreen_screen_init);
#if defined(MTB_DISPLAY_CO5300)
                lv_arc_set_value(ui_DigiScreenStepsArc, brightness);
                lv_obj_send_event(ui_DigiScreenStepsArc, LV_EVENT_VALUE_CHANGED, NULL);
#elif defined(W4P3INCH_DISP)
                lv_arc_set_value(ui_DigiScreenBrightnessArc, brightness);
                lv_obj_send_event(ui_DigiScreenBrightnessArc, LV_EVENT_VALUE_CHANGED, NULL);
#endif /* MTB_DISPLAY_CO5300 */

                vTaskResume(rtos_date_time_task_handle);
                vTaskResume(rtos_step_count_task_handle);

#if defined(W4P3INCH_DISP)
                vTaskResume(rtos_app_task_handle);
#endif /* W4P3INCH_DISP */

#if defined(MTB_DISPLAY_CO5300)
                /* Reset/restart the input_inactivity_timer */
                xTimerStart(input_inactivity_timer, RESET_VALUE);
#endif /* MTB_DISPLAY_CO5300 */
                break;

            /* Low power always-on graphics powered by CPU in LP/ULP mode. CM55 @140/50 MHz */
            case LOW_POWER_STATE:
                /* Suspend performance monitor in LP depending on display */
#if defined(USE_PERFORMANCE_MONITOR)
#if defined(MTB_DISPLAY_CO5300) 
                /* ROUND display */
                performance_monitor_suspend();

#endif
#endif /* USE_PERFORMANCE_MONITOR */
                stat = vg_lite_close();
                if (VG_LITE_SUCCESS != stat)
                {
                    process_error((cy_rslt_t)stat, "VGLite close failed. STOP.");
                }
                gpu_enable = false;
                /* Disable GPU interrupt */
                Cy_GFXSS_Disable_GPU_Interrupt(base);

                /* Disable GPU interrupt in NVIC */
                NVIC_DisableIRQ(GFXSS_GPU_IRQ);
#if defined(W4P3INCH_DISP)
                /* Disable DC interrupt in NVIC */
                NVIC_DisableIRQ(GFXSS_DC_IRQ);
#endif
                result = Cy_GFXSS_DeInit(base, &gfx_context);
                if (CY_GFX_SUCCESS != result)
                {
                    process_error((cy_rslt_t)result, "Gfxss deinitialization failed. STOP.");
                }

#if defined(MTB_DISPLAY_CO5300)
                /* Set ULP as System Active Power Profile */
                dpll_lp0_set_freq(DPLL_LP_OUTPUT_FREQ_ULP_HZ);
                dpll_lp1_set_freq(DPLL_LP_OUTPUT_FREQ_ULP_HZ);
                status= Cy_SysPm_SystemEnterUlp();
                if (CY_SYSPM_SUCCESS != status)
                {
                    process_error((cy_rslt_t)result, "System enter ULP failed. STOP.");
                }

                /** Check if the system successfully entered ULP mode. */
                if (Cy_SysPm_ReadStatus() & CY_SYSPM_STATUS_SYSTEM_ULP)
                {
                    /** Set the RRAM to ULP voltage mode for lower power 
                     * consumption. */
                    Cy_RRAM_SetVoltageMode(RRAMC0, CY_RRAM_VMODE_ULP);

                    /** Set the high-frequency clock (CLKHF) to no divide */
                    Cy_SysClk_ClkHfSetDivider(CY_CFG_SYSCLK_CLKHF0, CY_SYSCLK_CLKHF_NO_DIVIDE);

                    /** Set the peripheral clock divider for the debug UART */
                    Cy_SysClk_PeriPclkSetDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM,
                            CY_SYSCLK_DIV_16_BIT, 1U, UART_ULP_DIV);
                }
                

#elif defined(W4P3INCH_DISP)
                /* Set LP as System Active Power Profile.
                 * Note: On 4.3 inch display with 50 MHz core clock in ULP mode, 
                 * the pixel clock limitation (<= 25 MHz) prevents graphics 
                 * rendering.
                 */
                dpll_lp0_set_freq(DPLL_LP0_OUTPUT_FREQ_LP_HZ);
                dpll_lp1_set_freq(DPLL_LP1_OUTPUT_FREQ_LP_HZ);
                Cy_SysPm_SystemEnterLp();

                /** Check if the system successfully entered LP mode. */
                if (Cy_SysPm_ReadStatus() & CY_SYSPM_STATUS_SYSTEM_LP)
                {
                    /** Set the RRAM to LP voltage mode for lower power 
                        * consumption. */
                    Cy_RRAM_SetVoltageMode(RRAMC0, CY_RRAM_VMODE_LP);
                    
                    Cy_SysClk_ClkHfSetDivider(CY_CFG_SYSCLK_CLKHF0, CY_SYSCLK_CLKHF_DIVIDE_BY_2);

                    /** Set the peripheral clock divider for the debug UART */
                    Cy_SysClk_PeriPclkSetDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM,
                            CY_SYSCLK_DIV_16_BIT, 1U, UART_LP_DIV);
                }
#endif

                SystemCoreClockUpdate();

                Cy_SysTick_Disable();

                Cy_SysTick_SetReload((configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - SET_VALUE);
                Cy_SysTick_Clear();
                Cy_SysTick_Enable();

#if defined(W4P3INCH_DISP)
                /* Update CLK divider for Debug UART as per core frequency */
                Cy_SysClk_PeriPclkDisableDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM, CY_SYSCLK_DIV_16_BIT, DEBUG_UART_DIVIDER_NUM);
                Cy_SysClk_PeriPclkSetDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM, CY_SYSCLK_DIV_16_BIT, DEBUG_UART_DIVIDER_NUM, DEBUG_UART_LP_DIVIDER_VAL);
                Cy_SysClk_PeriPclkEnableDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM, CY_SYSCLK_DIV_16_BIT, DEBUG_UART_DIVIDER_NUM);
#elif defined(MTB_DISPLAY_CO5300)
                /* Update CLK divider for Debug UART as per core frequency */
                Cy_SysClk_PeriPclkDisableDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM, CY_SYSCLK_DIV_16_BIT, DEBUG_UART_DIVIDER_NUM);
                Cy_SysClk_PeriPclkSetDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM, CY_SYSCLK_DIV_16_BIT, DEBUG_UART_DIVIDER_NUM, DEBUG_UART_ULP_DIVIDER_VAL);
                Cy_SysClk_PeriPclkEnableDivider((en_clk_dst_t)CYBSP_DEBUG_UART_CLK_DIV_GRP_NUM, CY_SYSCLK_DIV_16_BIT, DEBUG_UART_DIVIDER_NUM);
#endif
                /* Re-initialize the graphics subsystem as per updated clock */
                GFXSS_config.clockHz = Cy_SysClk_ClkHfGetFrequency(CY_CFG_SYSCLK_CLKHF1);
                GFXSS_config.gpu_cfg->enable = false;

                /* Reset frame buffers before switching to different application
                 * state for smoother UI transition.
                 */
                reset_frame_buffer();

                result = Cy_GFXSS_Init(base, &GFXSS_config, &gfx_context);
                if (CY_GFX_SUCCESS != result)
                {
                    process_error((cy_rslt_t)result, "Gfxss re-initialization failed. STOP.");
                }

#if defined(W4P3INCH_DISP)
                /* Enable DC interrupt in NVIC to synchronize frame transfers 
                 * with the completion interrupt of frame buffer transfers from
                 * DC.
                 */
                NVIC_EnableIRQ(GFXSS_DC_IRQ);
#endif

                /* Display ui_LPScreen */
                brightness = MIN_BRIGHTNESS_PERCENT;
#if defined(MTB_DISPLAY_CO5300)
                mtb_display_co5300_set_brightness(&base->GFXSS_MIPIDSI, SET_BRIGHTNESS(brightness));
#elif defined(W4P3INCH_DISP)
                mtb_disp_waveshare_4p3_set_brightness(CYBSP_I2C_CONTROLLER_HW, &i2c_context,brightness );
#endif
                _ui_screen_change(&ui_LPScreen, LV_SCR_LOAD_ANIM_FADE_ON, RESET_VALUE, RESET_VALUE, &ui_LPScreen_screen_init);

#if defined(MTB_DISPLAY_CO5300)
                /* Reset/restart the input_inactivity_timer */
                xTimerStart(input_inactivity_timer, RESET_VALUE);
                /* System Domain Idle Power Mode Configuration */
                Cy_SysPm_SetDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP);
                /* System SRAM (SoCMEM) Idle Power Mode Configuration */
                Cy_SysPm_SetSOCMEMDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP);
                /* Allow CPU to go DeepSleep */
                mtb_hal_syspm_unlock_deepsleep();
                xTimerStart(lp_task_timer, RESET_VALUE);
#endif /*defined(MTB_DISPLAY_CO5300)*/
                break;

            /* System DeepSleep */
            case ULTRA_LOW_POWER_STATE:
                /* Always suspend performance monitor in ULP */
#if defined(USE_PERFORMANCE_MONITOR)
                performance_monitor_suspend();
#endif /* USE_PERFORMANCE_MONITOR */
                vTaskSuspend(rtos_date_time_task_handle);
                vTaskSuspend(rtos_step_count_task_handle);

                /* Reset frame buffers before switching to different application
                 * state for smoother UI transition.
                 */
                reset_frame_buffer();

#if defined(MTB_DISPLAY_CO5300)
                mtb_display_co5300_off(&base->GFXSS_MIPIDSI);
#elif defined(W4P3INCH_DISP)

                /* System Domain Idle Power Mode Configuration */
                Cy_SysPm_SetDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP);
                /* System SRAM (SoCMEM) Idle Power Mode Configuration */
                Cy_SysPm_SetSOCMEMDeepSleepMode(CY_SYSPM_MODE_DEEPSLEEP);

                /* Allow CPU to go DeepSleep */
                mtb_hal_syspm_unlock_deepsleep();

                /* Disable DC interrupt in NVIC */
                NVIC_DisableIRQ(GFXSS_DC_IRQ);
                vTaskSuspend(rtos_app_task_handle);
#endif

                result = (cy_en_gfx_status_t)Cy_MIPIDSI_EnterULPM(&base->GFXSS_MIPIDSI);
                if (CY_GFX_SUCCESS != result)
                {
                    process_error((cy_rslt_t)result, "Entering ULPS mode failed. STOP.");
                }
                break;
        }

#if defined(MTB_DISPLAY_CO5300)
        display_active_timeout = false;
#endif /* MTB_DISPLAY_CO5300 */

        state_change_complete = true;

        xSemaphoreGive(lvgl_mutex);
    }
}


#if defined(MTB_DISPLAY_CO5300)
/*******************************************************************************
* Function Name: input_inactivity_timer_callback
********************************************************************************
* Summary:
*  Timer callback to handle touch inactivity on smartwatch round display.
*
* Parameters:
*  timer: not used
*
* Return:
*  void
*
*******************************************************************************/
void input_inactivity_timer_callback(TimerHandle_t timer)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    CY_UNUSED_PARAMETER(timer);

    active_state = (HIGH_PERFORMANCE_STATE == active_state) ? LOW_POWER_STATE : ULTRA_LOW_POWER_STATE;

    display_active_timeout = true;

    xTaskNotifyFromISR(rtos_app_state_manager_task_handle, RESET_VALUE, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}


/*******************************************************************************
* Function Name: input_inactivity_timer_start
********************************************************************************
* Summary:
*  Initializes and starts the timer to handle touch inactivity on smartwatch
*  round display.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
BaseType_t input_inactivity_timer_start(void)
{
    BaseType_t result = pdFAIL;

    /* Create timer to track no input activity on display */
    input_inactivity_timer = xTimerCreate(INPUT_INACTIVITY_TIMER_NAME,
                                          pdMS_TO_TICKS(SCREEN_TIMEOUT_MS),
                                          pdFALSE, NULL,
                                          input_inactivity_timer_callback);

    /* Handle input_inactivity_timer creation error */
    if (NULL != input_inactivity_timer)
    {
        /* Start the input inactivity timer */
        result = xTimerStart(input_inactivity_timer, RESET_VALUE);
    }

    return result;
}


#elif defined(W4P3INCH_DISP)
/*******************************************************************************
* Function Name: user_button_1_interrupt_handler
********************************************************************************
* Summary:
*  User button 1 emulates touch to wake up functionality for the 7-inch RPi 
*  display. The USER BTN1 button cycles through three application states: 
*  high-performance screen, low-power always-on screen, and system deep sleep, 
*  with each press transitioning to the next mode.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void user_button_1_interrupt_handler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static TickType_t last_isr_time = 0;

    if (Cy_GPIO_GetInterruptStatus(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN))
    {
        Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN);
        NVIC_ClearPendingIRQ(CYBSP_USER_BTN1_IRQ);

        TickType_t now = xTaskGetTickCountFromISR();

        if ((now - last_isr_time) > pdMS_TO_TICKS(USER_BTN1_ISR_MIN_GAP_MS))
        {
            last_isr_time = now;
            if (!(Cy_GPIO_Read(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN)))
            {
                switch (active_state)
                {
                case HIGH_PERFORMANCE_STATE:
                    active_state = LOW_POWER_STATE;
                    break;

                case LOW_POWER_STATE:
                    active_state = ULTRA_LOW_POWER_STATE;
                    break;

                case ULTRA_LOW_POWER_STATE:
                    active_state = HIGH_PERFORMANCE_STATE;
                    break;
                }
                xTaskNotifyFromISR(rtos_app_state_manager_task_handle, RESET_VALUE,
                        eNoAction, &xHigherPriorityTaskWoken);
            }

        }

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    
    /* USER_BTN2 pin is present on the same GPIO port as USER_BTN1 pin */
    /* This cause the ISR to execute even when USER_BTN2 is pressed accidentally */
    /* To handle such scenarios we clear the interrupt */
    
    /* Check if USER_BTN2 was pressed */
    if(Cy_GPIO_GetInterruptStatus(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN))
    {
        /* Clear the USER_BTN2 interrupt */
        Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN);
        NVIC_ClearPendingIRQ(CYBSP_USER_BTN2_IRQ);
    }
}
#endif 


/*******************************************************************************
* Function Name: app_state_manager_task
********************************************************************************
* Summary:
*  This FreeRTOS task manages different application states.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void app_state_manager_task(void *arg)
{
    CY_UNUSED_PARAMETER(arg);

    for (;;)
    {
        if (pdPASS == xTaskNotifyWait(RESET_VALUE, RESET_VALUE, NULL, portMAX_DELAY))
        {
#if defined(MTB_DISPLAY_CO5300)
            /* Suspend frame transfer task applicable to 1.43 inch
             * command mode display 
             */
            vTaskSuspend(rtos_frame_tx_task_handle);
#endif /* MTB_DISPLAY_CO5300 */

            switch (active_state)
            {
                case HIGH_PERFORMANCE_STATE:
                case LOW_POWER_STATE:
                    app_state_update(active_state);
#if defined(MTB_DISPLAY_CO5300)
                    vTaskResume(rtos_frame_tx_task_handle);
#endif /* MTB_DISPLAY_CO5300 */
                    break;

                case ULTRA_LOW_POWER_STATE:
                    app_state_update(active_state);
                    break;
            }
        }
    }
}

/* [] END OF FILE */