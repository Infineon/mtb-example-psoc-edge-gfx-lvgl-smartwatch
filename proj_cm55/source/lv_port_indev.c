/*******************************************************************************
* File Name        : lv_port_indev.c
*
* Description      : This file provides implementation of low level input device
*                    driver for LVGL.
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
#include "lv_port_indev.h"
#include "lv_indev_private.h"
#include "smartwatch_app.h"
#include "cy_utils.h"
#include "lv_port_disp.h"
#include "app_logger.h"
#if defined(MTB_CTP_FT6146)
#include "mtb_ctp_ft6146.h"
#elif defined(MTB_CTP_FT3268)
#include "mtb_ctp_ft3268.h"
#elif defined(MTB_CTP_FT5406)
#include "mtb_ctp_ft5406.h"
#endif 
#include "cybsp.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#if defined(MTB_CTP_FT5406)
#define TOUCH_X_OFFSET_PX                    (32U)
#define TOUCH_LAST_PIXEL_OFFSET              (1U)
#endif

/*******************************************************************************
* Global Variables
*******************************************************************************/
lv_indev_t* indev_touchpad;

#if defined(MTB_DISPLAY_CO5300)
volatile bool touch_activity = false;
#endif /* MTB_DISPLAY_CO5300 */

#if defined(MTB_CTP_FT6146)
mtb_ctp_ft6146_config_t ctp_ft6146_cfg =
{
    .scb_inst    = CYBSP_I2C_CONTROLLER_HW,
    .i2c_context = &i2c_context,
    .rst_port    = CYBSP_DISP_TP_RST_PORT,
    .int_port    = CYBSP_DISP_TP_INT_PORT,
    .rst_pin     = CYBSP_DISP_TP_RST_PIN,
    .int_pin     = CYBSP_DISP_TP_INT_PIN,
    .int_num     = CYBSP_DISP_TP_INT_IRQ,
    .touch_event = false
};
#endif /* MTB_CTP_FT6146 */

#if defined(MTB_CTP_FT3268)
mtb_ctp_ft3268_config_t ctp_ft3268_cfg =
{
    .scb_inst    = CYBSP_I2C_CONTROLLER_HW,
    .i2c_context = &i2c_context,
    .rst_port    = CYBSP_DISP_TP_RST_PORT,
    .int_port    = CYBSP_DISP_TP_INT_PORT,
    .rst_pin     = CYBSP_DISP_TP_RST_PIN,
    .int_pin     = CYBSP_DISP_TP_INT_PIN,
    .int_num     = CYBSP_DISP_TP_INT_IRQ,
    .touch_event = false
};
#endif /* MTB_CTP_FT3268 */

#if defined(MTB_CTP_FT5406)
mtb_ctp_ft5406_config_t ctp_ft5406_cfg = {
    .i2c_base    = CYBSP_I2C_CONTROLLER_HW,
    .i2c_context = &i2c_context,

};
#endif  // MTB_CTP_FT5406

#if defined(MTB_CTP_FT6146)
/*******************************************************************************
* Function Name: mtb_ctp_ft6146_interrupt_handler
********************************************************************************
* Summary:
*  Touch panel GPIO interrupt handler to detect touch events on the sensor.
*  When a finger touches on the sensor surface, the INT pin will be pulled high
*  by FT6146 and we set the touch_event flag true. This driver function is 
*  overriden to cater application requirements.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void LV_ATTRIBUTE_FAST_MEM mtb_ctp_ft6146_interrupt_handler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (Cy_GPIO_GetInterruptStatus(ctp_ft6146_cfg.int_port, ctp_ft6146_cfg.int_pin))
    {
        ctp_ft6146_cfg.touch_event = true;
        touch_activity = true;

        xTaskNotifyFromISR(rtos_app_task_handle, 0, eNoAction, &xHigherPriorityTaskWoken);
        
        Cy_GPIO_ClearInterrupt(ctp_ft6146_cfg.int_port, ctp_ft6146_cfg.int_pin);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
#endif /* MTB_CTP_FT6146 */


#if defined(MTB_CTP_FT3268)
/*******************************************************************************
* Function Name: mtb_ctp_ft3268_interrupt_handler
********************************************************************************
* Summary:
*  Touch panel GPIO interrupt handler to detect touch events on the sensor.
*  When a finger touches on the sensor surface, the INT pin will be pulled high
*  by FT3268 and we set the touch_event flag true. This driver function is
*  overriden to cater application requirements.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void LV_ATTRIBUTE_FAST_MEM mtb_ctp_ft3268_interrupt_handler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (Cy_GPIO_GetInterruptStatus(ctp_ft3268_cfg.int_port, ctp_ft3268_cfg.int_pin))
    {
        ctp_ft3268_cfg.touch_event = true;
        touch_activity = true;

        xTaskNotifyFromISR(rtos_app_task_handle, 0, eNoAction, &xHigherPriorityTaskWoken);
        Cy_GPIO_ClearInterrupt(ctp_ft3268_cfg.int_port, ctp_ft3268_cfg.int_pin);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
#endif /* MTB_CTP_FT3268 */


/*******************************************************************************
* Function Name: touchpad_init
********************************************************************************
* Summary:
*  Initialization function for touchpad supported by LVGL.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
LV_ATTRIBUTE_FAST_MEM void touchpad_init(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;

#if defined(MTB_CTP_FT6146)
    /* Initialize FT6146-00 touch driver */
    result = mtb_ctp_ft6146_init(&ctp_ft6146_cfg);
#elif defined(MTB_CTP_FT5406)
    result = (cy_rslt_t)mtb_ctp_ft5406_init(&ctp_ft5406_cfg);
#elif defined(MTB_CTP_FT3268)
    /* Initialize FT3268 touch driver */
    result = mtb_ctp_ft3268_init(&ctp_ft3268_cfg);
#endif
    if (CY_RSLT_SUCCESS != result)
    {
        process_error(result, "Touch driver initialization failed. STOP.");
    }
}


/*******************************************************************************
* Function Name: touchpad_read
********************************************************************************
* Summary:
*  Touchpad read function called periodically by the LVGL library. It returns
*  the latest touch state and coordinates from the cache populated by the
*  background touch polling task, so it performs no I2C access and never blocks
*  the rendering thread.
*
* Parameters:
*  *indev_drv: Pointer to the input driver structure to be registered by LVGL.
*  *data: Pointer to the data buffer holding touch coordinates.
*
* Return:
*  void
*
*******************************************************************************/
LV_ATTRIBUTE_FAST_MEM void touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
#if defined(MTB_CTP_FT3268) || defined(MTB_CTP_FT5406)
    mtb_ctp_touch_event_t touch_event = MTB_CTP_TOUCH_RESERVED;
#endif
    static int touch_x = 0;
    static int touch_y = 0;
    cy_en_scb_i2c_status_t result = CY_SCB_I2C_SUCCESS;

    CY_UNUSED_PARAMETER(indev_drv);

    data->state = LV_INDEV_STATE_REL;

#if defined(MTB_CTP_FT6146)
    if (ctp_ft6146_cfg.touch_event)
    {
        /* Pointer to x and y reversed to align touch screen data to UI. */
        result = mtb_ctp_ft6146_get_single_touch(&touch_y, &touch_x);
#elif defined(MTB_CTP_FT3268)
    if (ctp_ft3268_cfg.touch_event)
    {
        result = mtb_ctp_ft3268_get_single_touch(&touch_event, &touch_x, &touch_y);
#elif defined(MTB_CTP_FT5406)
        result = mtb_ctp_ft5406_get_single_touch(&touch_event, &touch_x, &touch_y);
#endif
#if defined(MTB_CTP_FT3268) || defined(MTB_CTP_FT5406)
        if ((CY_SCB_I2C_SUCCESS == result) && ((MTB_CTP_TOUCH_DOWN == touch_event) || (MTB_CTP_TOUCH_CONTACT == touch_event)))
#elif defined(MTB_CTP_FT6146)
        if (CY_SCB_I2C_SUCCESS == result)
#endif
        {
            data->state = LV_INDEV_STATE_PR;
        }
#if defined(MTB_CTP_FT6146)
        ctp_ft6146_cfg.touch_event = false;
    }
#elif defined(MTB_CTP_FT3268)
        ctp_ft3268_cfg.touch_event = false;
    }
#endif

    /* Set the last pressed coordinates */
#if defined(MTB_CTP_FT6146)
    data->point.x = MY_DISP_HOR_RES - touch_x;  /* X Coordinates subtracted to align touch screen data to UI. */
    data->point.y = touch_y;
#elif defined(MTB_CTP_FT3268)
    data->point.x = touch_x;  
    data->point.y = touch_y;
#elif defined(MTB_CTP_FT5406)
    data->point.x = (MY_DISP_HOR_RES - TOUCH_X_OFFSET_PX - TOUCH_LAST_PIXEL_OFFSET) - touch_x;
    data->point.y = (MY_DISP_VER_RES - TOUCH_LAST_PIXEL_OFFSET) - touch_y;
#endif
}


/*******************************************************************************
* Function Name: lv_port_indev_init
********************************************************************************
* Summary:
*  Initialization function for input devices supported by LVGL.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void lv_port_indev_init(void)
{
    /* Initialize the touch controller hardware */
    touchpad_init();

    /* Register a touchpad input device */
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read);
    /* Bind input to our display */
    lv_indev_set_display(indev, disp_gpu);
}

/* [] END OF FILE */
