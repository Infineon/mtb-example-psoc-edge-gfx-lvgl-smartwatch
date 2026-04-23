/*******************************************************************************
* File Name        : rtos.h
*
* Description      : This file contains all the declarations/definitions of
*                    freeRTOS task names, handles, priorities, semaphore,
*                    mutexes etc. for the application.
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

#ifndef RTOS_H
#define RTOS_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define APP_TASK_PRIORITY                   (configMAX_PRIORITIES * 3 / 7)
#define APP_STATE_MANAGER_TASK_PRIORITY     (configMAX_PRIORITIES * 3 / 7)
#define TIME_DATE_TASK_PRIORITY             (configMAX_PRIORITIES * 3 / 7)
#define STEP_COUNT_TASK_PRIORITY            (configMAX_PRIORITIES * 3 / 7)

#define GPU_INT_PRIORITY                    (3U)

#define APP_TASK_NAME                       ("Smartwatch App Task")
#define APP_STATE_MANAGER_TASK_NAME         ("App State Manager Task")
#define TIME_DATE_TASK_NAME                 ("Time Date Task")
#define STEP_COUNT_TASK_NAME                ("Step Count Task")

/* stack size in words */
#define APP_TASK_STACK_SIZE                 ((configMINIMAL_STACK_SIZE) * 3)
#define APP_STATE_MANAGER_TASK_STACK_SIZE   (configMINIMAL_STACK_SIZE * 2)
#define TIME_DATE_TASK_STACK_SIZE           (configMINIMAL_STACK_SIZE)
#define STEP_COUNT_TASK_STACK_SIZE          (configMINIMAL_STACK_SIZE)

#if defined(MTB_DISPLAY_CO5300)
#define INPUT_INACTIVITY_TIMER_NAME         ("Input Inactivity Timer")
#define FRAME_TX_TASK_PRIORITY              (configMAX_PRIORITIES * 3 / 7)
#define FRAME_TX_TASK_NAME                  ("Frame Tx Task")
#define FRAME_TX_TASK_STACK_SIZE            ((configMINIMAL_STACK_SIZE) / 2)
#endif /* MTB_DISPLAY_CO5300 */

#if defined(W4P3INCH_DISP)
#define DC_INT_PRIORITY                     (3U)
#endif /* W4P3INCH_DISP */


/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Task Handlers */
extern TaskHandle_t rtos_date_time_task_handle;
extern TaskHandle_t rtos_step_count_task_handle;
extern TaskHandle_t rtos_app_task_handle;
extern TaskHandle_t rtos_app_state_manager_task_handle;

#if defined(MTB_DISPLAY_CO5300)
extern TaskHandle_t rtos_frame_tx_task_handle;
#endif /* MTB_DISPLAY_CO5300 */

extern SemaphoreHandle_t lvgl_mutex;

#if defined(MTB_DISPLAY_CO5300)
extern TimerHandle_t input_inactivity_timer;
#endif /* MTB_DISPLAY_CO5300 */


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* RTOS_H */

/* [] END OF FILE */
