/*******************************************************************************
* File Name        : smartwatch_app.h
*
* Description      : This file provides constants, parameter values, and
*                    API prototypes for the Smartwatch application.
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

#ifndef INCLUDE_SMARTWATCH_APP_H
#define INCLUDE_SMARTWATCH_APP_H

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "rtos.h"
#include "cy_pdl.h"
#include "cy_graphics.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#define TICK_VAL                            (1U)

#define APP_BUFFER_COUNT                    (2U)
/* 64 KB */
#define DEFAULT_GPU_CMD_BUFFER_SIZE         ((64U) * (1024U))
#define GPU_TESSELLATION_BUFFER_SIZE        ((MY_DISP_VER_RES) * 128U)


#define VGLITE_HEAP_SIZE                    (((DEFAULT_GPU_CMD_BUFFER_SIZE) * \
                                              (APP_BUFFER_COUNT)) + \
                                             ((GPU_TESSELLATION_BUFFER_SIZE) * \
                                              (APP_BUFFER_COUNT)))

#define I2C_IRQ_PRIORITY                    (2U)


#define MAX_BRIGHTNESS_PERCENT              (100U)
#define MIN_BRIGHTNESS_PERCENT              (10U)

/* Macro to calculate brightness count. 
 * 255 is the max PWM period for 7" display. 
 * The max value of brightness control register for 1.43" round display is 255. 
 */
#define SET_BRIGHTNESS(percentage)       ((uint8_t)(((percentage) * 255) / 100))


/*******************************************************************************
* Global Variables
*******************************************************************************/
extern GFXSS_Type* base;
extern volatile uint8_t brightness;
extern cy_stc_gfx_context_t gfx_context;
extern cy_stc_scb_i2c_context_t i2c_context;
#if defined(MTB_DISPLAY_CO5300)
extern TimerHandle_t lp_task_timer;
#endif /*defined(MTB_DISPLAY_CO5300)*/

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void setup_tickless_idle_timer(void);
cy_rslt_t smartwatch_app_init(void);
#if defined(MTB_DISPLAY_CO5300)
void stop_and_reset_timer(void);
#endif /*defined(MTB_DISPLAY_CO5300)*/


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* INCLUDE_SMARTWATCH_APP_H */

/* [] END OF FILE */
