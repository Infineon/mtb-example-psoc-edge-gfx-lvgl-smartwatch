/*******************************************************************************
* File Name    : step_count_task.c
*
* Description  : This file defines functions for initializing and updating
*                the steps display within the UI.
*
* Note         : See README.md
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
#include "step_count_task.h"
#include <stdlib.h>
#include "ui.h"
#include "app_logger.h"
#include "app_state_manager.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define RESET_VALUE                  (0U)
#define BASE_INTERVAL_MS             (1000U)
#define OFFSET_INTERVAL_MS           (500U)
#define STEP_COUNT_INCREMENT         (1U)
#if defined(MTB_DISPLAY_CO5300)
#define STEP_COUNT_INCREMENT_IN_LP   (10U)
#endif /*MTB_DISPLAY_CO5300*/

/*******************************************************************************
* Global Variables
*******************************************************************************/
TaskHandle_t rtos_step_count_task_handle = NULL;


/*******************************************************************************
* Function Name: step_count_task_init
********************************************************************************
* Summary:
*  This function creates the FreeRTOS task for counting steps.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void step_count_task_init(void)
{
    BaseType_t task_return = RESET_VALUE;
    
    task_return = xTaskCreate(step_count_task, STEP_COUNT_TASK_NAME,
                              STEP_COUNT_TASK_STACK_SIZE, NULL,
                              STEP_COUNT_TASK_PRIORITY, 
                              &rtos_step_count_task_handle);

    if (pdPASS != task_return) 
    {
        process_error(CY_RTOS_GENERAL_ERROR, "step_count_task creation failed. STOP.");
    }
}


/*******************************************************************************
* Function Name: step_count_task
********************************************************************************
* Summary:
*  This is a FreeRTOS task to simulate steps counting and update the same in
*  UI between 500 ms to 1500 ms interval.
*
* Parameters:
*  void *arg
*
* Return:
*  void
*
*******************************************************************************/
void step_count_task(void *arg)
{
    CY_UNUSED_PARAMETER(arg);

    static uint32_t step_count = RESET_VALUE;
    uint32_t step_interval     = RESET_VALUE;
    uint32_t count_inc = RESET_VALUE;

    /* Wait for UI init to complete */
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    for(;;)
    {
        /* Interval between 500 ms and 1500 ms */
        /* coverity[DC.WEAK_CRYPTO] */
        step_interval = rand() % BASE_INTERVAL_MS + OFFSET_INTERVAL_MS;
#if defined(MTB_DISPLAY_CO5300)
        if ( HIGH_PERFORMANCE_STATE != active_state )
        {
            count_inc = STEP_COUNT_INCREMENT_IN_LP;
        }
        else 
        {
            count_inc = STEP_COUNT_INCREMENT;
        }
#elif defined(W4P3INCH_DISP)
        count_inc = STEP_COUNT_INCREMENT;
#endif
        step_count = (UINT32_MAX <= step_count) ? RESET_VALUE : step_count + count_inc;

        if (pdTRUE == xSemaphoreTake(lvgl_mutex, portMAX_DELAY))
        {
            ui_step_cb(step_count);
            ui_health_screen_cb();
            xSemaphoreGive(lvgl_mutex);
        }
#if defined(MTB_DISPLAY_CO5300)
        if ( HIGH_PERFORMANCE_STATE != active_state )
        {
            vTaskSuspend(NULL);
        }
        else 
        {
            vTaskDelay(pdMS_TO_TICKS(step_interval));
        }
#elif defined(W4P3INCH_DISP)
        vTaskDelay(pdMS_TO_TICKS(step_interval));
#endif
    }
}

/* [] END OF FILE */
