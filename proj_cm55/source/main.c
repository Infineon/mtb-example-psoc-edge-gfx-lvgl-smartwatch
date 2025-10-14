/*******************************************************************************
* File Name        : main.c
*
* Description      : This source file contains the main routine for 
*                    application running on CM55 CPU.
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
#include "cybsp.h"

#include "smartwatch_app.h"
#include "app_logger.h"
#include "app_state_manager.h"
#include "cy_time.h"
/*******************************************************************************
* Macros
*******************************************************************************/
#define USER_BTN1_IRQ_PRIORITY            (7U)


/*******************************************************************************
* Global Variables
*******************************************************************************/

/* RTC HAL object */
static mtb_hal_rtc_t rtc_obj;

#if defined(W4P3INCH_DISP)
/* Interrupt config structure */
cy_stc_sysint_t btn_intr_cfg =
{
    CYBSP_USER_BTN1_IRQ,      /* Interrupt source */
    USER_BTN1_IRQ_PRIORITY   /* Interrupt priority */
};

/*******************************************************************************
* Function Name: configure_gpio_btn
********************************************************************************
* Summary:
*  Clears pending button interrupts and configures the NVIC for USER BTN1/BTN2
*  on the PSOC Edge E84 kit. Registers the interrupt handler for USER BTN1 and
*  enables the IRQ line for button events.
*
* Parameters:
*  void
*
* Return:
*  void
*
********************************************************************************/
void configure_gpio_btn(void)
{
    /* CYBSP_USER_BTN1 (SW2) and CYBSP_USER_BTN2 (SW4) share the same port in the
    * PSoC Edge E84 evaluation kit and hence they share the same NVIC IRQ line.
    * Since both are configured in the BSP via the Device Configurator, the
    * interrupt flags for both the buttons are set right after they get initialized
    * through the call to cybsp_init(). The flags must be cleared before initializing
    * the interrupt, otherwise the interrupt line will be constantly asserted.
    */
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PIN);
    NVIC_ClearPendingIRQ(CYBSP_USER_BTN1_IRQ);

    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PIN);
    NVIC_ClearPendingIRQ(CYBSP_USER_BTN2_IRQ);


    /* Initialize the interrupt and register interrupt callback */
    cy_en_sysint_status_t btn_interrupt_init_status = Cy_SysInt_Init(&btn_intr_cfg,
            &user_button_1_interrupt_handler);
    if(CY_SYSINT_SUCCESS != btn_interrupt_init_status)
    {
        printf("Failed to initialize GPIO Interrupt\n");
        handle_error();
    }

    /* Enable the interrupt in the NVIC */
    NVIC_EnableIRQ(btn_intr_cfg.intrSrc);

}
#endif /*W4P3INCH_DISP*/


/*******************************************************************************
* Function Name: setup_clib_support
********************************************************************************
* Summary:
* 1. This function configures and initializes the Real-Time Clock (RTC)).
* 2. It then initializes the RTC HAL object to enable CLIB support library 
* to work with the provided Real-Time Clock (RTC) module.
*
* Parameters:
* void
*
* Return:
* void
*
*******************************************************************************/
static void setup_clib_support(void)
{
    /* RTC Initialization */
    Cy_RTC_Init(&CYBSP_RTC_config);
    Cy_RTC_SetDateAndTime(&CYBSP_RTC_config);

    /* Initialize the ModusToolbox CLIB support library */
    mtb_clib_support_init(&rtc_obj);
}


/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  This is the main function for CM55 non-secure application.
*    1. It initializes the device and board peripherals.
*    2. Initializes retarget-io to use KitProg debug UART port.
*    3. Initializes CM55 core based Smartwatch application and
*       starts the RTOS task scheduler.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (CY_RSLT_SUCCESS != result)
    {
        handle_error();
    }

    /* Setup CLIB support library. */
    setup_clib_support();

    /* Setup the LPTimer instance for CM55 */
    setup_tickless_idle_timer();

    /* Initialize retarget-io to use the debug UART port */
    init_retarget_io();
#if defined(W4P3INCH_DISP)
    configure_gpio_btn();
#endif /*W4P3INCH_DISP*/

    /* Initialize CM55 core based Smartwatch application */
    result = smartwatch_app_init();

    /* Start the FreeRTOS scheduler on smartwatch_app_init successful */
    if (CY_RSLT_SUCCESS == result)
    {
        /* Enable global interrupts */
        __enable_irq();
        /* Start the RTOS Scheduler */
        vTaskStartScheduler();
    }
    else
    {
        process_error(result, "Smartwatch app initialization failed. STOP.");
    }
}


/* [] END OF FILE */
