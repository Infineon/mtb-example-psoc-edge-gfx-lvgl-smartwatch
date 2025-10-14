/*
 * ui_events.c
 *
 *  Created on: 05-Jun-2024
 *      Author: majumdar
 */


#include "ui.h"
#include "stdlib.h"
#include "cy_rtc.h"

void ui_time_cb(cy_stc_rtc_config_t rtc_date_time)
{
    /* Variable used for storing days in the week */
    const char day_str[CY_RTC_DAYS_PER_WEEK][4] = {
            "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

    /* Variable used for storing months */
    const char month_str[CY_RTC_MONTHS_PER_YEAR][4] = {
            "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

    if (lv_obj_is_visible(ui_LPScreen))
    {
        lv_label_set_text_fmt(ui_LPScreenHourLabel, "%02u", (unsigned int) rtc_date_time.hour);
        lv_label_set_text_fmt(ui_LPScreenMinLabel, "%02u", (unsigned int) rtc_date_time.min);
        lv_label_set_text_fmt(ui_LPScreenDateLabel, "%02u", (unsigned int) rtc_date_time.date);

        lv_label_set_text_fmt(ui_LPScreenDayLabel, "%s", day_str[rtc_date_time.dayOfWeek - 1]);
        lv_label_set_text_fmt(ui_LPScreenMonthLabel, "%s", month_str[rtc_date_time.month - 1]);
    }
    else if (lv_obj_is_visible(ui_DigitalScreen))
    {
        lv_label_set_text_fmt(ui_DigiScreenHourLabel, "%02u", (unsigned int) rtc_date_time.hour);
        lv_label_set_text_fmt(ui_DigiScreenMinLabel, "%02u", (unsigned int) rtc_date_time.min);
        lv_label_set_text_fmt(ui_DigiScreenDateLabel, "%02u", (unsigned int) rtc_date_time.date);

        lv_label_set_text_fmt(ui_DigiScreenDayLabel, "%s", day_str[rtc_date_time.dayOfWeek - 1]);
        lv_label_set_text_fmt(ui_DigiScreenMonthLabel, "%s", month_str[rtc_date_time.month - 1]);

        lv_label_set_text_fmt(ui_DigiScreenYearLabel, "%04u", (unsigned int) rtc_date_time.year + 2000);

    }
    else if (lv_obj_is_visible(ui_AnalogScreen))
    {
        lv_label_set_text_fmt(ui_AnalogScreenDateLabel, "%02u", (unsigned int) rtc_date_time.date);
        lv_label_set_text_fmt(ui_AnalogScreenYearLabel, "%04u", (unsigned int) rtc_date_time.year + 2000);

        lv_label_set_text_fmt(ui_AnalogScreenDayLabel, "%s", day_str[rtc_date_time.dayOfWeek - 1]);
        lv_label_set_text_fmt(ui_AnalogScreenMonthLabel, "%s", month_str[rtc_date_time.month - 1]);
    }
    else if (lv_obj_is_visible(ui_WeatherScreen1))
    {
        lv_label_set_text_fmt(ui_weather1DateLabel, "%02u . %02u . %04u", (unsigned int) rtc_date_time.date, (unsigned int) rtc_date_time.month,
                (unsigned int) rtc_date_time.year + 2000);
    }
    else if (lv_obj_is_visible(ui_WeatherScreen2))
    {
        lv_label_set_text_fmt(ui_weather2DateLabel, "%02u . %02u . %04u", (unsigned int) rtc_date_time.date, (unsigned int) rtc_date_time.month,
                (unsigned int) rtc_date_time.year + 2000);
    }
}

void ui_step_cb(uint32_t count)
{
    if (lv_obj_is_visible(ui_LPScreen))
    {
        lv_label_set_text_fmt(ui_LPScreenStepsLabel, "%u", (unsigned int) count);
    }
    else if (lv_obj_is_visible(ui_DigitalScreen))
    {
        lv_label_set_text_fmt(ui_DigiScreenStepsCountLabel, "%u", (unsigned int) count);
    }
    else if (lv_obj_is_visible(ui_AnalogScreen))
    {
        lv_label_set_text_fmt(ui_AnalogScreenStepsCountLabel, "%u", (unsigned int) count);
    }
}


void ui_health_screen_cb(void)
{
    int sys_bp;
    int dia_bp;
    int heart_rate;

    if (lv_obj_is_visible(ui_HealthScreen))
    {
        /* Systolic bp value between 120 - 140 mmhg */
        sys_bp = 120 + (rand() % (140 - 120 + 1));

        /* Diastolic  bp value between 65 - 80 mmhg */
        dia_bp = 65 + (rand() % (80 - 65 + 1));

        lv_label_set_text_fmt(ui_BloodPressureSYSValueLabel, "%ld", (long int)sys_bp);

        lv_label_set_text_fmt(ui_BloodPressureDIAValueLabel, "%ld", (long int)dia_bp);

        /* Heart rate value between 60 - 100 bpm */
        heart_rate = 60 + (rand() % (100 - 60 + 1));

        lv_label_set_text_fmt(ui_HeartBeatValueLabel, "%ld", (long int)heart_rate);
    }
}
