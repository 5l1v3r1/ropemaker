#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "user_config.h"
#include "ropemaker.h"

extern ctrlparms run;
extern sysparms syscfg;

extern const unsigned int html_len;
extern const INFLASH uint8 html[];

void update_state(char *qstr)
{
    
}

void send_html(struct espconn *pConn)
{
	char buf[HTTP_BUFFER_SIZE];
            
    os_sprintf(buf, "Content Length: %d\r\nContent Type: text/html\r\nContent Encoding: gzip\r\n\r\n", html_len);
    espconn_sent(pConn, (uint8*)buf, os_strlen(buf));
    unsigned int n = html_len, nR;
    while(n > 0) {
        nR = n < HTTP_BUFFER_SIZE ? n : HTTP_BUFFER_SIZE;
        spi_flash_read((uint32)&html+html_len-n, (uint32*)buf, nR);
        espconn_sent(pConn, (uint8*)buf, nR);
        n -= nR;
    }
}

void send_state(struct espconn *pConn)
{
    
}

void api_recv(void *arg, char *pdata, unsigned short len)
{
    struct espconn *pConn = (struct espconn *) arg;
    if(!strncmp(pdata, "GET ", 4)) {
        if(!strncmp(pdata+4, "/state", 5)) { // get state
            send_state(pConn);
        } else  // is index.html
            send_html(pConn);
    } else if(!strncmp(pdata, "POST ", 5)) { // update state
        update_state(pdata+5);
    } else
        os_printf("api req: %s\n", pdata);
}

void api_disconnect(void *arg)
{
    os_printf("api close\n");
}

void ICACHE_FLASH_ATTR api_handler(void *arg)
{
    int i;
    struct espconn *pConn = (struct espconn *)arg;
    
    os_printf("api open\n");
    
    //pConn->reverse = my_http;
    espconn_regist_recvcb( pConn, api_recv );
    espconn_regist_disconcb( pConn, api_disconnect );
}

void api_server_init(int port)
{
    struct espconn *pAPISrv = (struct espconn *)os_zalloc(sizeof(struct espconn));
    ets_memset( pAPISrv, 0, sizeof( struct espconn ) );
    espconn_create( pAPISrv );
    pAPISrv->type = ESPCONN_TCP;
    pAPISrv->state = ESPCONN_NONE;
    pAPISrv->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    pAPISrv->proto.tcp->local_port = port;
    espconn_regist_connectcb(pAPISrv, api_handler);
    espconn_accept(pAPISrv);
    espconn_regist_time(pAPISrv, 15, 0);
}

void wifi_handler( System_Event_t *evt )
{
    switch ( evt->event )
    {
        case EVENT_STAMODE_CONNECTED:
            os_printf("connected: %s, channel %d\n", evt->event_info.connected.ssid, evt->event_info.connected.channel);
            break;
        case EVENT_STAMODE_DISCONNECTED:
            os_printf("disconnected: %s (%d)\n", evt->event_info.disconnected.ssid, evt->event_info.disconnected.reason);
            //deep_sleep_set_option( 0 );
            //system_deep_sleep( 30 * 1000 * 1000 );  // 30 seconds
            break;
        case EVENT_STAMODE_GOT_IP:
            os_printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR "\n", IP2STR(&evt->event_info.got_ip.ip),
                            IP2STR(&evt->event_info.got_ip.mask), IP2STR(&evt->event_info.got_ip.gw));
            api_server_init(80);
            break;     
        default:
            os_printf( "wifi event: %d\n", evt->event );
            break;
    }
}
