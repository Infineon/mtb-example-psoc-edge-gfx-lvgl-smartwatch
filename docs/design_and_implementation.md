[Click here](../README.md) to view the README.

## Design and implementation

All PSOC&trade; Edge E84 MCU applications have a dual-CPU three-project structure to develop code for the CM33 and CM55 cores. The CM33 core has two separate projects for the secure processing environment (SPE) and non-secure processing environment (NSPE). A project folder consists of various subfolders – each denoting a specific aspect of the project.

The folder structure of this application is as follows:

   **Figure 1. Folder structure**

   ![](../images/folder-tree.png)

All three projects are programmed to the external QSPI flash and executed in XIP mode. In this code example, at device reset, the secure boot process starts from the ROM boot with the secure enclave (SE) as the root of trust (RoT). From the secure enclave, the boot flow is passed on to the system CPU subsystem where the secure CM33 application starts. After all necessary secure configurations, the flow is passed on to the non-secure CM33 application. Resource initialization for this example is performed by this CM33 non-secure project. It configures the system clocks, pins, clock to peripheral connections, and other platform resources. It then enables the CM55 core using the `Cy_SysEnableCM55()` function and subsequently put itself into Deep Sleep mode. Once CM55 is enabled it configures system clocks, pins, clock to peripheral connections, and other platform resources. To conserve power, the CM55 CPU employs Multi-counter Watchdog Timer (MCWDT) 1 as a Low Power Timer (LPTIMER). This integration allows the FreeRTOS to enter a tickless idle state, enabling the device to transition into Deep Sleep when the CPU is idle, minimizing power consumption. Here, the CM55 application implements the logic for this code example.

This code example is supported on two display setups: two variants of 1.43-inch circular display (driven by CO5300 controller) and the Waveshare 4.3-inch Raspberry-Pi DSI LCD. <br>
The 1.43-inch circular display is interfaced with MIPI DSI command mode protocol. Whereas, the Waveshare 4.3-inch display is interfaced with MIPI DSI Video mode protocol.

The Waveshare 4.3-inch display's touch panel interrupt line is not connected to the PSOC&trade; Edge Evaluation Kit because of which, the application changes the operating states based on the User Button 1 (USER BTN1) interrupt. 

Because of the difference in the interface protocol and the display hardware capability, the source code also has certain differences which are noted further in this document. The project can be built for either of the displays using the following defines in the *common.mk* file:<br>

**For 4.3-inch display:**
```
CONFIG_DISPLAY=RECTANGLE_4_3_INCH
```

**For 1.43-inch display:**
```
CONFIG_DISPLAY=DASTEK_ROUND_1_43_INCH
```
or
```
CONFIG_DISPLAY=MICROTECH_ROUND_1_43_INCH
```

> **Note:** Captions such as **For 1.43-inch display** and **For 4.3-inch display** will be used to note the differences for the respective displays.

### Application structure
The smartwatch application code present under the CM55 project folder is FreeRTOS-based with the following tasks:

**Table 1. FreeRTOS tasks in the application**

FreeRTOS tasks | Description
--------|------------------------
Smartwatch App Task | This FreeRTOS task performs VGLite configuration, LVGL and UI initializations and regular updates to the screen/UI
App State Manager Task | The system power and clock settings are managed by this task based on the state of the application <br><br> ***For 1.43-inch display:*** This tasks changes the state of the application based on the 30 seconds of inactivity period <br> ***For 4.3-inch display:*** This tasks changes the state of the application based on button press of USER BUTTON 1 (USER BTN1) <br>
Frame Tx Task | This task handles transferring rendered frames to display panel from display controller for 1.43-inch display
Step Count Task | This task handles step counting by incrementing the count and updating the UI through the ui_step_cb function
Time Date Task | This is a task for reading the Real-Time Clock every 1 sec and updating the time to respective RTOS queue

### Firmware flow

**For 4.3-inch display**

The "Smartwatch App Task" calls the refresh_screen() function every LV_DEF_REFR_PERIOD (set to 16 milliseconds) in synchronization with the 60 Hz display refresh rate in a thread-safe manner. The refresh_screen() function, in turn calls the lv_timer_handler() API to drive LVGL time-related tasks ensuring invalidated areas of the screen are checked and redrawn every LV_DEF_REFR_PERIOD.

**For 1.43-inch display**

Similar to video mode display, the application calls the refresh_screen() function every SCREEN_REFRESH_TIME_MS (30 ms) which in turn uses lv_timer_handler() to invalidate and redraw the invalidated areas of screen at every LV_DEF_REFR_PERIOD. This use case also incorporates the disp_flush() function, which sets the frame buffer address, allowing the DC to render the frame from it. Upon completion, disp_flush() sends a notification to the "Frame Tx Task," which then utilizes the Cy_GFXSS_Transfer_Frame() API to initiate DBI transfers from the DC to the MIPI DSI host to render the frame buffer onto the display.

> **Note:** This application uses the double buffering configuration of the LVGL library for drawing the graphics data. In this configuration, the LVGL draws the next frame in one buffer while the other buffer is being rendered to the display.

The application operates in three states as described in **Table 2**.

**Table 2. Operating states of the application**

Application state | Description
--------|------------------------
High-performance state | - System Active power profile is set to high performance <br> - The CM55 operates at 400 MHz frequency <br> - The display shows high-performance UI with max possible refresh rate <br> - The graphics are rendered using GPU <br>
Low-power state | - GPU is turned off. The graphics are rendered with minimal CPU usage <br> **For 1.43-inch display:** <br>- The display shows "Always-ON" UI with 1/9 Hz refresh rate <br> - The System Active power profile is set to ultra-low power mode <br> - The CM55 operates at 50 MHz frequency and system goes to DeepSleep periodically when CPU is idle and no frame is getting rendered <br> **For 4.3-inch display:** <br>- The display shows "Always-ON" UI with 1 Hz refresh rate <br> - The System Active power profile is set to low-power mode <br> - The CM55 operates at 140 MHz frequency <br>
Ultra-low power state | - The display is turned off and the MCU enters into system Deep Sleep for maximum power conservation <br> **For 1.43-inch display:** - The application waits for touch activity for wake-up <br> **For 4.3-inch display:** - The application waits for button press of USER BUTTON 1 (USER BTN1)  <br> 

   **Figure 2. State change diagram**

   ![](../images/state-diagram-circular.png)

## Key performance indicator
Key performance indicator (KPI) data, including current consumption, CPU usage, and FPS, is being collected during various stages of application operation.

#### CPU current profile

The MCU power consumption is estimated by measuring the currents of the VBAT = 3.3 V, VDDD = 1.8 V,VDDUSB = 3.3 V and VDD/VDDIO = 1.8 V domain. The VBAT, VDDD, VDDUSB and VDD/VDDIO domain currents can be measured using the J25, J26, J18 and J24 jumpers, respectively on the PSOC&trade; Edge Evaluation Kit.

   **Figure 3. Current measurement pin**

   ![](../images/power-measurement.png)

**Table 3. CPU currents, battery-powered configuration, VBAT = 3.3 V (J25), VDDD=1.8 V (J26), VDDUSB = 3.3 V (J18), VDD/VDDIO = 3.3 V (J24) with 4.3-inch display setup**

Application state | CPU frequency | IBAT | IDDD | IDDUSB | IDD_IDDIO | Total power consumption <sup># | SRAM retention in DeepSleep
:-------- | :----------  | :---------- | :-------- | :-------- | :-------- | :--------- | :---------
High performance | 400 MHz | MIN: 17.2 mA <br> MAX: 27.3 mA <br> AVG: 20.5 mA | MIN: 10.8 mA <br> MAX: 16.8 mA <br> AVG: 12.8 mA | MIN: 118.0 µA <br> MAX: 118.2 µA <br> AVG: 118.1 µA | MIN: 60.5 µA <br> MAX: 60.9 µA <br> AVG: 60.7 µA | MIN: 76.2 mW <br> MAX: 120.3 mW <br> AVG: 90.6 mW | NA
Low power | 140 MHz | MIN: 4.0 mA <br> MAX: 7.2 mA <br> AVG: 4.9 mA | MIN: 9.8 mA <br> MAX: 12.9 mA <br> AVG: 11.1 mA | MIN: 117.9 µA <br> MAX: 118.1 µA <br> AVG: 118.0 µA | MIN: 60.4 µA <br> MAX: 60.7 µA <br> AVG: 60.5 µA | MIN: 30.8 mW <br> MAX: 46.9 mW <br> AVG: 36.1 mW | NA
Ultra-low power | CPU in Sleep mode | 67.1 µA | 198.4 µA | 52.6 µA | 5 µA | 578.5 µW | Both SRAM and System SRAM (SoCMEM) are fully retained

<br>

**Table 4. CPU currents, battery-powered configuration, VBAT = 3.3 V (J25), VDDD=1.8 V (J26), VDDUSB = 3.3 V (J18), VDD/VDDIO = 3.3 V (J24) with 1.43-inch display setup**

Application state | CPU frequency | IBAT | IDDD | IDDUSB | IDD_IDDIO | Total power consumption <sup># | SRAM retention in DeepSleep
:-------- | :----------  | :---------- | :-------- | :-------- | :-------- | :--------- | :---------
High performance | 400 MHz | MIN: 15.7 mA <br> MAX: 29.7 mA <br> AVG: 20.8 mA | MIN: 7.0 mA <br> MAX: 14.7 mA <br> AVG: 11.8 mA | MIN: 116.2 µA <br> MAX: 117.2 µA <br> AVG: 116.5 µA | MIN: 75.6 µA <br> MAX: 76.6 µA <br> AVG: 76.0 µA | MIN: 64.4 mW <br> MAX: 124.4 mW <br> AVG: 89.8 mW | NA
Low power | 50 MHz | MIN: 91.3 µA <br> MAX: 2.7 mA <br> AVG: 119.2 µA | MIN: NA <br> MAX: 11.1 mA <br> AVG: 307.8 µA | MIN: 53.3 µA <br> MAX: 54.1 µA <br> AVG: 52.3 µA | MIN: 5.6 µA <br> MAX: 7.7 µA <br> AVG: 7.1 µA |  MIN: 0.3 mW <br> MAX: 28.8 mW <br> AVG: 0.9 mW | NA
Ultra-low power | CPU in Sleep mode | 96.8 µA | 255 µA <sup>** | 53.8 µA | 7.7 µA | 778.4 µW | Both SRAM and System SRAM (SoCMEM) are fully retained

<br>

<sup># - Total power consumption value excludes power from VDDUSB and VDDIO, as there is no direct contribution to this application's functionality. <br>
<sup>** - Large spikes of current are observed in MCU current @ VDDD = 1.8 V (J26) due to kit rework to interface 1.43-inch display.

> **Note:** 
For accurate current measurements and avoid leakage current, the PSOC&trade; Edge E84 Evaluation Kit may require hardware rework described in “Rework for PSOC&trade; Edge E84 MCU low power current measurement” section of the [KIT_PSE84_EVAL PSOC&trade; Edge E84 Evaluation Kit guide](https://www.infineon.com/KIT_PSE84_EVAL_UG); without this rework, readings can be up to ~100 µA higher in HP mode. Because the rework affects the functionality of WIFI/BT, Analog microphone interface, on-board KitProg3 JTAG interface and potentiometer interface, its not taken into account for this code example’s power measurements.

> **Note:**
Here ECO is used as PLL source to drive the CPU core clocks, System RAM and graphics subsystem in all application states.
The ECO has a much longer lock time compared to other clock sources. This can significantly increase the wake-up time from DEEPSLEEP if the ECO is (directly or indirectly) providing a clock root.
If any clock root is sourced from the ECO (either directly or as a reference to a DPLL), it is recommended to switch to a different configuration before entering DEEPSLEEP. After wake-up, the original clock configuration can be restored once the ECO becomes stable.
Specifically, it is advised to change the clock source from ECO (→ DPLL) to either IHO or IHO → DPLL before entering DEEPSLEEP. This allows the system to continue running while the ECO stabilizes. Once stable, the firmware can restore the original settings.

#### Performance profile
The CPU usage percentage and Frame per second (FPS) are used as performance parameters for this code example. Both metrics are recorded using the performance monitor code snippet present in *COMPONENTS/APP_LOGGER/app_logger.c*. 

To enable the FPS and CPU usage percentage data logging, the following macro must be defined in the *proj_cm55/Makefile*:

```
DEFINES+=USE_PERFORMANCE_MONITOR
```
And set the following macro in the *proj_cm55/include/FreeRTOSConfig.h*:

```
#define configGENERATE_RUN_TIME_STATS           1
```

The peak CPU usage percentage is reported in the below table.

**Table 5. Peak CPU usage percentage for 4.3-inch display**

 High-performance state | Low-power state
 :----------    | :--------
 **Start Screen**: 51 % <br> **Analog watch**: 50 % <br> **Heart rate**: 52 %  <br> **Music screen**: 51 % <br> **Weather Screen**: 56 % <br> | 10 %

 <br>

**Table 6. Peak CPU usage percentage for 1.43-inch display**

 High-performance state | Low-power state
 :----------    | :--------
 **Start Screen**: 73 % <br> **Analog watch**: 68 % <br> **Heart rate**: 73 %  <br> **Music screen**: 76 % <br> **Weather Screen**: 74 % <br> | NA**

 <br>

**For 1.43 display the performance monitors is suspended to achieve optimum power. In this state device enters deepsleep after each screen refresh.

The frames per seconds can be tuned using the following values for the respective displays:

**For 1.43-inch display:**

`SCREEN_REFRESH_TIME_MS`

> **Note:** The value of `SCREEN_REFRESH_TIME_MS` is set to 30 ms in this application.

**For 4.3-inch display:**

`HIGH_PERF_REFRESH_MIN_TIME_MS` and `LOW_POWER_SCREEN_REFRESH_TIME_MS` 

> **Note:** The value of `HIGH_PERF_REFRESH_MIN_TIME_MS` is set to 10 ms in high-performance state while `LOW_POWER_SCREEN_REFRESH_TIME_MS` is 200 ms in low-power state.

Decreasing this value increases the FPS of the display. However, this also results in an increase in CPU usage percentage.

The FPS data obtained using the above mentioned delay values is shown in **Table 7** and **Table 8**.

**Table 7. Frames per second for 4.3-inch display**

 High-performance state | Low-power state
 :----------    | :--------
 **Start Screen**: 36 <br> **Analog watch**: 30 <br> **Heart rate**: 36 <br> **Music screen**: 34 <br> **Weather Screen**: 32 <br> | 1 FPS

**Table 8. Frames per second for 1.43-inch display**

 High-performance state | Low-power state
 :----------    | :--------
 **Start Screen**: 24 <br> **Analog watch**: 24 <br> **Heart rate**: 24 <br> **Music screen**: 22 <br> **Weather Screen**: 23 <br> | <= 1 FPS

<br>