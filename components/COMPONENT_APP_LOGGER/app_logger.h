/*******************************************************************************
* File Name        : app_logger.h
*
* Description      : This file provides declaration of the functions used to 
*                    derive and publish application statistics.
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
#ifndef __APP_LOGGER_H__
#define __APP_LOGGER_H__

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


/*******************************************************************************
* Header Files
*******************************************************************************/
#include "retarget_io_init.h"
#include "cyabs_rtos.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#ifdef APP_LOG_PRINT_EN
#define app_log_print(format,...)           printf(format "",##__VA_ARGS__);
#else 
#define app_log_print(format,...)
#endif /* APP_LOG_PRINT_EN */


/*******************************************************************************
* Global Variables
*******************************************************************************/
#if defined(USE_PERFORMANCE_MONITOR)
extern volatile uint32_t frame_count;
#endif /* USE_PERFORMANCE_MONITOR */


/*******************************************************************************
* Functions Prototypes
*******************************************************************************/
#if defined (__GNUC__) && !defined(__ARMCC_VERSION)
void app_print_mem_information(void);
#endif /* #if defined (__GNUC__) && !defined(__ARMCC_VERSION) */

void app_print_sys_information(unsigned int core_number);

#if defined(USE_PERFORMANCE_MONITOR)
void performance_monitor_init(void);
void performance_monitor_suspend(void);
void performance_monitor_resume(void);
#endif /* USE_PERFORMANCE_MONITOR */

void process_error(cy_rslt_t status, char *message);


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* __APP_LOGGER_H__ */

/* [] END OF FILE */
