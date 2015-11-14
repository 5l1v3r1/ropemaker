#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "mem.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "user_config.h"
#include "ropemaker.h"

static volatile os_timer_t tick_timer;
runparms run;
sysparms syscfg = { "OpenWrt", "Tc9dimZKioonBAlQ6SkoqiOP1ckc6VOU", "ropemaker", LOAD_SPEED, INIT_TWIST*16 };

unsigned int get_foot_speed(int max_speed)
{
    int foot = system_adc_read();
    return (foot < 50) ? 0 : (foot-50)*(max_speed-20)/974 + 20;   // simple linear, range 20 -> max_speed steps/sec
}

void tick_handler(void *arg)
{
    if(--run.scan_count == 0) {
        run.scan_count = TICKS_SCAN;
        if(run.speed < 0)
            run.foot_speed = get_foot_speed(-run.speed);
        if(GPIO_INPUT_GET(RUN_BTN) == 0) {
            os_printf("RUN %d %d\n", syscfg.twist16 / 16, LOAD_SPEED * syscfg.twist16 / 16);
            run.speed = LOAD_SPEED * syscfg.twist16 / 16;
            GPIO_OUTPUT_SET(SPIN_DIR, syscfg.twist16 > 0 ? 1 : 0);
            GPIO_OUTPUT_SET(STEP_ENABLE, 1);
        } else {
            if(run.speed != 0)
                os_printf("STOP %d\n", run.feed_total);
            GPIO_OUTPUT_SET(STEP_ENABLE, 0);
            GPIO_OUTPUT_SET(SPIN_STEP, 0);
            GPIO_OUTPUT_SET(FEED_STEP, 0);
            run.speed = run.spin_count = run.feed_count = 0;
        }
    }
    if(run.speed == 0)
        return;
    if(--run.spin_count <= 0) {
        run.spin_count = TICKS_SEC / (run.speed > 0 ? run.speed : run.foot_speed);
        GPIO_OUTPUT_SET(SPIN_STEP, 1);
    } else
        GPIO_OUTPUT_SET(SPIN_STEP, 0);
    
    if(--run.feed_count <= 0) {
        run.feed_count = TICKS_SEC / ((run.speed > 0 ? run.speed : run.foot_speed) * 16 / abs(syscfg.twist16));
        GPIO_OUTPUT_SET(FEED_STEP, 1);
        run.feed_total++;
        if(run.feed_stop > 0 && run.feed_total >= run.feed_stop)
            run.speed = 0;
    } else
        GPIO_OUTPUT_SET(FEED_STEP, 0);
}

void user_init( void )
{
    system_timer_reinit();
    
    uart_div_modify( 0, UART_CLK_FREQ / ( BAUD_RATE ) );
    os_printf( "\nRopeMaker started.\n");
    
    // should load syscfg from flash here

    wifi_station_set_hostname( syscfg.hostname );
    wifi_set_opmode_current( STATION_MODE );

    gpio_init();
    GPIO_DIS_OUTPUT(RUN_BTN);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
    GPIO_OUTPUT_SET(STEP_ENABLE, 0);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
    GPIO_OUTPUT_SET(SPIN_DIR, 0);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
    GPIO_OUTPUT_SET(SPIN_STEP, 0);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
    GPIO_OUTPUT_SET(FEED_STEP, 0);
    
    run.scan_count = TICKS_SCAN;
    
    os_timer_disarm(&tick_timer);
    os_timer_setfn(&tick_timer, (os_timer_func_t *)tick_handler, NULL);
    os_timer_arm_us(&tick_timer, TICK_RATE, 1);
    
    static struct station_config config;
    config.bssid_set = 0;
    os_memcpy( &config.ssid, syscfg.ssid, 32 );
    os_memcpy( &config.password, syscfg.pwd, 64 );
    wifi_station_set_config( &config );
    
    wifi_set_event_handler_cb( wifi_handler );
}
