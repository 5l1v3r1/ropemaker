#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "user_config.h"
#include "ffs.h"
#include "ropemaker.h"

extern runparms run;
extern sysparms syscfg;

extern const unsigned int ffs_len;
extern const ffsinfo ffs_dir[];
extern const INFLASH unsigned char ffs_data[];

void update_state(struct espconn *pConn, char *pS)
{
    char buf[HTTP_BUFFER_SIZE];
    
    char *pZ = pS;
    while(os_strncmp(pZ, "HTTP", 4)) pZ++; *pZ = 0;
    
    // actions: reset, run, stop, set 
    if(!strcmp(pS, "reset"))
        run.feed_total = 0;
    else if(!strcmp(pS, "run"))
        run.speed = syscfg.speed;
    else if(!strcmp(pS, "stop"))
        run.speed = 0;
    else if(!strcmp(pS, "set")) {
        // set: autostop, speed, twist, foot, profile - json values to set
        char pJ = pZ+1;
        while(os_strncmp(pJs, "\r\n\r\n", 4)) pJ++; pJ += 4;
        
    }
    
    os_printf("POST %s \nJSON %s\n", qstr, pJs);
    os_sprintf(buf, "HTTP/1.1 200 OK\r\n\r\nOK\n");
    espconn_send(pConn, (uint8*)buf, os_strlen(buf));
    espconn_disconnect(pConn);
}

void send_state(struct espconn *pConn)
{
    char buf[HTTP_BUFFER_SIZE];
    
    os_sprintf(buf, "{\"count\":%d,\"speed\":%d,\"stepsm\":%d,\"twist\":%d,\"stop\":%d}", 
        run.feed_total, run.speed, STEPS_METER, syscfg.twist16, run.feed_stop);
    espconn_send(pConn, (uint8*)buf, os_strlen(buf));
    espconn_disconnect(pConn);
}

void send_file_chunk(void *arg)
{
    char buf[HTTP_BUFFER_SIZE];
    
    struct espconn *pConn = (struct espconn *)arg;
    unsigned int idx = ((ffsending *)pConn->reverse)->idx;
    unsigned int nRem = ((ffsending *)pConn->reverse)->rem; 
            
    if(nRem > 0) {
        unsigned int nDo = nRem < HTTP_BUFFER_SIZE ? nRem : HTTP_BUFFER_SIZE;
        spi_flash_read((uint32)ffs_data&0xFFFFF + ffs_dir[idx+1].off - nRem, (uint32*)buf, (nDo+3)&~3);
        espconn_send(pConn, (uint8*)buf, nDo);
        ((ffsending *)pConn->reverse)->rem -= nDo;
    } else {
        os_free(pConn->reverse);
        espconn_disconnect(pConn);
    }
}

void send_file(struct espconn *pConn, char *path)
{
	char buf[HTTP_BUFFER_SIZE];
    int n;
    
    if(path[0] == '/' && path[1] == ' ')
        path = "/index.html";
    
    for(n = 0; n < ffs_len; n++)
        if(!os_strncmp(path+1, ffs_dir[n].path, os_strlen(ffs_dir[n].path))) 
            break;
            
    if(n < ffs_len) {
        os_printf("req %s\n", ffs_dir[n].path);
        os_sprintf(buf, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n", ffs_dir[n+1].off - ffs_dir[n].off, ffs_dir[n].mime);
        if(ffs_dir[n].enc[0]) 
            os_sprintf(buf+os_strlen(buf)-2, "Content-Encoding: %s\r\n\r\n", ffs_dir[n].enc);
        pConn->reverse = (ffsending *)os_malloc(sizeof(ffsending));
        ((ffsending *)pConn->reverse)->idx = n;
        ((ffsending *)pConn->reverse)->rem = (ffs_dir[n+1].off - ffs_dir[n].off);
        espconn_regist_sentcb( pConn, send_file_chunk );
        espconn_send(pConn, (uint8*)buf, os_strlen(buf));
    } else {
        os_sprintf(buf, "HTTP/1.1 404 NOT FOUND");
        espconn_send(pConn, (uint8*)buf, os_strlen(buf));
    }
}

void api_recv(void *arg, char *pdata, unsigned short len)
{
    struct espconn *pConn = (struct espconn *) arg;

    if(!os_strncmp(pdata, "GET ", 4)) {
        if(!os_strncmp(pdata+4, "/state", 5)) { // get state
            send_state(pConn);
        } else  // is a file
            send_file(pConn, pdata+4);
    } else if(!os_strncmp(pdata, "POST ", 5)) { // update state
        update_state(pConn, pdata+5);
    } else
        os_printf("req: %s\n", pdata);
}

void api_disconnect(void *arg)
{
    //os_printf("req close\n");
}

void ICACHE_FLASH_ATTR api_handler(void *arg)
{
    int i;
    struct espconn *pConn = (struct espconn *)arg;
    
    //os_printf("req open\n");
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
