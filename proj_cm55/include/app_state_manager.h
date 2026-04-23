/*******************************************************************************
* File Name        : app_state_manager.h
*
* Description      : This file provides constants, parameter values, and
*                    function prototypes for the application state machine.
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

#ifndef INCLUDE_APP_STATE_MANAGER_H
#define INCLUDE_APP_STATE_MANAGER_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "rtos.h"


/*******************************************************************************
* Data Types
*******************************************************************************/
typedef enum
{
    HIGH_PERFORMANCE_STATE = 0x00, /* High performance graphics powered by GPU in HP mode. CM55 @400 MHz */
    LOW_POWER_STATE,               /* Low power always-on graphics powered by CPU in LP/ULP mode. CM55 @140/50 MHz */
    ULTRA_LOW_POWER_STATE          /* System DeepSleep */
} application_states_t;


/*******************************************************************************
* Global Variables
*******************************************************************************/
extern volatile application_states_t active_state;
extern volatile bool state_change_complete;

#if defined(MTB_DISPLAY_CO5300)
extern volatile bool display_active_timeout;
#endif /* MTB_DISPLAY_CO5300 */


/*******************************************************************************
* Function Prototypes
*******************************************************************************/
#if defined(MTB_DISPLAY_CO5300)
BaseType_t input_inactivity_timer_start(void);
#elif defined(W4P3INCH_DISP)
void user_button_1_interrupt_handler(void);
#endif 

void app_state_manager_task(void *arg);


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* INCLUDE_APP_STATE_MANAGER_H */

/* [] END OF FILE */
