#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "mem.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "user_config.h"

void wifi_handler( System_Event_t *evt );

static volatile os_timer_t tick_timer;

void tick_handler(void *arg)
{
    static int count = 0;
    if( count++ % 1000 == 0 )
        os_printf("tick\n");
}

void user_init( void )
{
    system_timer_reinit();
    
    uart_div_modify( 0, UART_CLK_FREQ / ( 115200 ) );
    os_printf( "\nRopeMaker started.\n");

    wifi_station_set_hostname( "ropemaker" );
    wifi_set_opmode_current( STATION_MODE );

    gpio_init();
    
    os_timer_setfn(&tick_timer, (os_timer_func_t *)tick_handler, NULL);
    os_timer_arm_us(&tick_timer, 1000, 1);
    
    static struct station_config config;
    config.bssid_set = 0;
    os_memcpy( &config.ssid, "OpenWrt", 32 );
    os_memcpy( &config.password, "Tc9dimZKioonBAlQ6SkoqiOP1ckc6VOU", 64 );
    wifi_station_set_config( &config );
    
    wifi_set_event_handler_cb( wifi_handler );
}
