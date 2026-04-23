/*******************************************************************************
* File Name        : app_logger.c
*
* Description      : This file provides functions to derive and publish 
*                    application statistics. 
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
#include "app_logger.h"

#include "cy_pdl.h"
#include "cycfg.h"
#include <stdint.h>
#include <stdbool.h>
#include "cy_result.h"


/* ARM compiler also defines __GNUC__ */
#if defined (__GNUC__) && !defined(__ARMCC_VERSION)
#include <malloc.h>
#endif /* #if defined (__GNUC__) && !defined(__ARMCC_VERSION) */


/*******************************************************************************
* Macros
*******************************************************************************/
#define TO_KB(size_bytes)  ((float)(size_bytes)/1024)

#if defined(USE_PERFORMANCE_MONITOR)
#define PERF_MON_TIMER_MS       (1000U)
#define TIMER_RESET_VAL         (0x00)
#endif /* USE_PERFORMANCE_MONITOR */


/*******************************************************************************
* Global Variables
*******************************************************************************/
extern uint32_t SystemCoreClock;


/*******************************************************************************
* Function Name: process_error
********************************************************************************
* Summary:
*  This function processes unrecoverable errors such as any component
*  initialization errors etc. In that case the error message gets displayed
*  on the debug terminal when APP_LOG_PRINT_EN is defined. It asserts
*  and halts the processor in the debug state.
*
* Parameters:
*  status: Contains the status.
*  message: Contains the message to be printed on the serial terminal.
*
* Return :
*  void
*
*******************************************************************************/
void process_error(cy_rslt_t status, char *message)
{
    if (status)
    {
        if (CY_RSLT_SUCCESS != status)
        {
#ifdef APP_LOG_PRINT_EN
            if (NULL != message)
            {
                app_log_print("Error: %u, %s\r\n", (unsigned int) status, message);
                while (cy_retarget_io_is_tx_active()) {};
            }
#endif
            /* Disable all interrupts. */
            __disable_irq();

            CY_ASSERT(0);

            while(true);
        }
    }
}


#if defined (__GNUC__) && !defined(__ARMCC_VERSION)
/*******************************************************************************
* Function Name: app_print_mem_information
********************************************************************************
* Summary:
*  Prints memory information.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void app_print_mem_information(void)
{
    extern uint8_t __HeapBase;  /* Symbol exported by the linker. */
    extern uint8_t __HeapLimit; /* Symbol exported by the linker. */
    extern uint8_t __StackTop;
    extern uint8_t __StackLimit;

    struct mallinfo mall_info = mallinfo();

    uint8_t* stack_base = (uint8_t *)&__StackTop;
    uint8_t* stack_limit = (uint8_t *)&__StackLimit;
    uint32_t stack_size = (uint32_t)(stack_limit - stack_base);
    app_log_print("******************** Stack ********************\r\n");
    app_log_print("Stack    -[Addr: %p->%p] [Size: %u bytes/%.2f KB]\r\n\r\n",
                  stack_base, stack_limit, (unsigned int) stack_size, TO_KB(stack_size));


    uint8_t* heap_base = (uint8_t *)&__HeapBase;
    uint8_t* heap_limit = (uint8_t *)&__HeapLimit;
    uint32_t heap_size = (uint32_t)(heap_limit - heap_base);
    app_log_print("******************** Heap Usage ********************\r\n");
    app_log_print("Heap Range    -[Addr: %p->%p] [Size: %u bytes/%.2f KB]\r\n",
                  heap_base, heap_limit, (unsigned int) heap_size, TO_KB(heap_size));
    app_log_print("Heap utilized -[%u bytes/%.2f KB,   Percent: %.2f%%]\r\n",
            mall_info.arena, TO_KB(mall_info.arena), ((float) mall_info.arena * 100u)/heap_size);
    app_log_print("Heap InUse now-[%u bytes/%.2f KB,   Percent: %.2f%%]\r\n\r\n",
            mall_info.uordblks, TO_KB(mall_info.uordblks), ((float) mall_info.uordblks * 100u)/heap_size);
}
#endif /* #if defined (__GNUC__) && !defined(__ARMCC_VERSION) */


/*******************************************************************************
* Function Name: app_print_sys_information
********************************************************************************
* Summary:
*  Print system information such as CPU core number, core clock and memory
*  statistics.
*
* Parameters:
*  core_number - CPU core number 1/2 .
*
* Return:
*  void
*
*******************************************************************************/
void app_print_sys_information(unsigned int core_number)
{
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    app_log_print("\x1b[2J\x1b[;H");

    app_log_print("CORE %u APP STARTED, SysClk: %u, Pwrmode:0x%lx\r\n",
                  core_number, (unsigned int) SystemCoreClock, CY_CFG_PWR_SYS_IDLE_MODE);

#if defined (__GNUC__) && !defined(__ARMCC_VERSION)
    app_print_mem_information();
#endif /* #if defined (__GNUC__) && !defined(__ARMCC_VERSION) */
}


#if ( configGENERATE_RUN_TIME_STATS == 1 )
/*******************************************************************************
* Function Name: setup_run_time_stats_timer
********************************************************************************
* Summary:
*  This function configuresTCPWM 0 GRP 0 Counter 0 as the timer source for  
*  FreeRTOS runtime statistics.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void setup_run_time_stats_timer(void)
{
    /* Initialze TCPWM block with required timer configuration */
    if (CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(CYBSP_GENERAL_PURPOSE_TIMER_HW, 
        CYBSP_GENERAL_PURPOSE_TIMER_NUM, 
        &CYBSP_GENERAL_PURPOSE_TIMER_config))
    {
        // handle_app_error();
        printf("Error in Timer initialization\r\n");
    }

    /* Enable the initialized counter */
    Cy_TCPWM_Counter_Enable(CYBSP_GENERAL_PURPOSE_TIMER_HW, 
                            CYBSP_GENERAL_PURPOSE_TIMER_NUM);

    /* Start the counter */
    Cy_TCPWM_TriggerStart_Single(CYBSP_GENERAL_PURPOSE_TIMER_HW, 
                                 CYBSP_GENERAL_PURPOSE_TIMER_NUM);
}


/*******************************************************************************
* Function Name: get_run_time_counter_value
********************************************************************************
* Summary:
*  Function to fetch run time counter value. This will be used by FreeRTOS for 
*  run time statistics calculation.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: TCPWM 0 GRP 0 Counter 0 value
*
*******************************************************************************/
uint32_t get_run_time_counter_value(void)
{
   return (Cy_TCPWM_Counter_GetCounter(CYBSP_GENERAL_PURPOSE_TIMER_HW, 
                                       CYBSP_GENERAL_PURPOSE_TIMER_NUM));
}


/*******************************************************************************
* Function Name: calculate_idle_percentage
********************************************************************************
* Summary:
*  Function to calculate CPU idle percentage. This function is used by LVGL to  
*  showcase CPU usage.
*
* Parameters:
*  void
*
* Return:
*  uint32_t: CPU idle percentage
*
*******************************************************************************/
uint32_t calculate_idle_percentage(void)
{

    static uint32_t previousIdleTime = 0;
    static TickType_t previousTick = 0;
    uint32_t time_diff = 0;
    uint32_t idle_percent = 0;

    uint32_t currentIdleTime = ulTaskGetIdleRunTimeCounter();
    TickType_t currentTick = portGET_RUN_TIME_COUNTER_VALUE();

    time_diff = currentTick - previousTick;

    if((currentIdleTime >= previousIdleTime) && (currentTick > previousTick))
    {
        idle_percent = ((currentIdleTime - previousIdleTime) * 100)/time_diff;
    }

    previousIdleTime = ulTaskGetIdleRunTimeCounter();
    previousTick = portGET_RUN_TIME_COUNTER_VALUE();

    return idle_percent;
}
#endif

#if defined(USE_PERFORMANCE_MONITOR)
TimerHandle_t perf_monitor_timer = NULL;
volatile uint32_t frame_count    = 0U;
uint32_t start_time_ms           = 0;

/*******************************************************************************
* Function Name: performance_monitor_callback
********************************************************************************
* Summary:
*  Derive and publish the performance data on debug UART.
*
* Parameters:
*  timer: Not used
*
* Return:
*  void
*
*******************************************************************************/
void performance_monitor_callback(TimerHandle_t timer)
{
    uint32_t current_time_ms = 0;
    uint32_t elapsed_time_ms = 0;
    uint32_t fps          = 0;

    CY_UNUSED_PARAMETER(timer);

    current_time_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS);
    elapsed_time_ms = current_time_ms - start_time_ms;

    /* Calculate FPS */
    fps = (1000 * frame_count) / elapsed_time_ms;

    app_log_print("\rFPS: %3u | CPU usage: %3u%%", (unsigned int) fps,
    (uint8_t)(100 - calculate_idle_percentage()));
    fflush(stdout);

    /* Reset variables for the next calculation */
    start_time_ms = current_time_ms;
    frame_count = 0;
}


/*******************************************************************************
* Function Name: performance_monitor_init
********************************************************************************
* Summary:
*  Initialize 5 sec timer to publish performance data (Fps).
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void performance_monitor_init(void)
{
    /* Create timer to derive and publish graphics frames per sec data */
    perf_monitor_timer = xTimerCreate("Perf Monitor Timer",
                                      pdMS_TO_TICKS(PERF_MON_TIMER_MS),
                                      pdTRUE, NULL,
                                      performance_monitor_callback);

    CY_ASSERT(NULL != perf_monitor_timer);

    /* Start the fps monitor timer */
    xTimerStart(perf_monitor_timer, 0);

    /* Get the start time */
    start_time_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/*******************************************************************************
* Function Name: performance_monitor_suspend
********************************************************************************
* Summary:
*  Suspend the performance monitoring timer to stop FPS data collection.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void performance_monitor_suspend(void)
{
    if (NULL != perf_monitor_timer)
    {
        xTimerStop(perf_monitor_timer, TIMER_RESET_VAL);
    }
}

/*******************************************************************************
* Function Name: performance_monitor_resume
********************************************************************************
* Summary:
*  Resume the performance monitoring timer and reset counters for fresh FPS
*  data collection window.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void performance_monitor_resume(void)
{
    if (NULL != perf_monitor_timer)
    {
        /* Reset counters for fresh FPS window */
        frame_count = TIMER_RESET_VAL;
        start_time_ms = (xTaskGetTickCount() * portTICK_PERIOD_MS);
        xTimerStart(perf_monitor_timer, TIMER_RESET_VAL);
    }
}

#endif /* USE_PERFORMANCE_MONITOR */


/* [] END OF FILE */
