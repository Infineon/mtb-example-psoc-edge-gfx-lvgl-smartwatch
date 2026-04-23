/*******************************************************************************
* File Name        : lv_port_disp.h
*
* Description      : This file provides constants and function prototypes
*                    for configuring low level display driver in LVGL.
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

#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*******************************************************************************
* Header Files
*******************************************************************************/
#include "lvgl.h"


/*******************************************************************************
* Macros
*******************************************************************************/
#if defined(MTB_DISPLAY_CO5300)
#define MY_DISP_HOR_RES                 (512U) /* pixels */
#define MY_DISP_VER_RES                 (466U) /* pixels */
#elif defined(W4P3INCH_DISP)
#define MY_DISP_HOR_RES                 (832U) /* pixels - 4.3 inch display */
#define MY_DISP_VER_RES                 (480U) /* pixels - 4.3 inch display */
#endif /* MTB_DISPLAY_CO5300 */

/* Default tessellation window width and height, in pixels */
#define DEFAULT_VG_LITE_TW_WIDTH        (256U) /* pixels */
#define DEFAULT_VG_LITE_TW_HEIGHT       (256U) /* pixels */

#define RESET_VALUE                     (0U)
#define SET_VALUE                       (1U)


/*******************************************************************************
* Global Variables
*******************************************************************************/
extern lv_display_t * disp_gpu;

#if defined(MTB_DISPLAY_CO5300)
extern volatile bool frame_tx_done;
#elif defined(W4P3INCH_DISP)
extern volatile bool fb_pending;
#endif
extern void *frame_buffer1;
extern void *frame_buffer2;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
/* Initialize low level display driver */
void lv_port_disp_init(void);
void reset_frame_buffer(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*LV_PORT_DISP_H*/

/* [] END OF FILE */
