/*******************************************************************************
* File Name        : time_date_task.c
*
* Description      : This file defines functions for initializing and updating
*                    the time and date display within the UI.
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
#include <time_date_task.h>
#include "cybsp.h"
#include "cycfg.h"
#include "ui.h"
#include "app_logger.h"
#include "app_state_manager.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define RTC_ACCESS_RETRY             (500U)
 /* 5 msec delay   */
#define RTC_RETRY_DELAY_MSEC         (5U)    
#define RESET_VALUE                  (0U)
#define TIME_DATE_TASK_DELAY_MS      (1000U)


/*******************************************************************************
* Global Variables
*******************************************************************************/
TaskHandle_t rtos_date_time_task_handle = NULL;


/*******************************************************************************
* Function Name: time_date_task_init
********************************************************************************
* Summary:
*  This function initializes Real Time Clock and creates the FreeRTOS task
*  to update time and date in UI.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void time_date_task_init(void)
{
    BaseType_t task_return    = RESET_VALUE;
    cy_en_rtc_status_t result = CY_RTC_SUCCESS;
    uint32_t rtc_access_retry = RTC_ACCESS_RETRY;

    /* Initialize RTC */

    /* RTC block doesn't allow to access, when synchronizing the user registers
     * and the internal actual RTC registers. It will return RTC_BUSY value, if
     * it is not available to update the configuration values. Needs to retry,
     * if it doesn't return CY_RTC_SUCCESS.
     */
    do
    {
        result = Cy_RTC_Init(&CYBSP_RTC_config);
        rtc_access_retry--;
        Cy_SysLib_Delay(RTC_RETRY_DELAY_MSEC);
    } while ((CY_RTC_SUCCESS != result) && rtc_access_retry);

    if (CY_RTC_SUCCESS != result)
    {
        process_error(result, "RTC initialization failed. STOP.");
    }

    task_return = xTaskCreate(time_date_task, TIME_DATE_TASK_NAME,
                              TIME_DATE_TASK_STACK_SIZE, NULL,
                              TIME_DATE_TASK_PRIORITY,
                              &rtos_date_time_task_handle);

    if (pdPASS != task_return) 
    {
        process_error(CY_RTOS_GENERAL_ERROR, "time_date_task creation failed. STOP.");
    }
}


/*******************************************************************************
* Function Name: time_date_task
********************************************************************************
* Summary:
*  This is a FreeRTOS task to fetch the current time, date and update the same
*  in UI @ every 1 sec.
*
* Parameters:
*  void *arg
*
* Return:
*  void
*
*******************************************************************************/
void time_date_task(void *arg)
{
    CY_UNUSED_PARAMETER(arg);

    static cy_stc_rtc_config_t date_time;

    /* Wait for UI init to complete */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    for(;;)
    {
        memset(&date_time, RESET_VALUE, sizeof(date_time));
        Cy_RTC_GetDateAndTime(&date_time);

        if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY))
        {
            ui_time_cb(date_time);
            xSemaphoreGive(lvgl_mutex);
        }
#if defined(MTB_DISPLAY_CO5300)
        if ( HIGH_PERFORMANCE_STATE != active_state )
        {
            vTaskSuspend(NULL);
        }
        else 
        {
            vTaskDelay(pdMS_TO_TICKS(TIME_DATE_TASK_DELAY_MS));
        }
#elif defined(W4P3INCH_DISP)
        vTaskDelay(pdMS_TO_TICKS(TIME_DATE_TASK_DELAY_MS));
#endif
    }
}

/* [] END OF FILE */
