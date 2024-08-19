#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"

#define LED_PIN GPIO_NUM_2
#define ESP_WIFI_SSID   "arknet_pi"
#define ESP_WIFI_PASS   "zanezane"
#define MAX_RETRY       5

static const char *TAG="example";
static int s_retry_num = 0;
static httpd_handle_t server = NULL;


static esp_err_t root_get_handler(httpd_req_t *req)
{
    char* buf;
    size_t buf_len;

    buf_len = httpd_req_get_hdr_value_len(req, "Host")+1;
    buf=malloc(buf_len);
    if(buf_len>1){
       httpd_req_get_url_query_str(req,buf,buf_len);
       ESP_LOGI(TAG,"Found URL query -> %s",buf);
       char param[32];
       if(httpd_query_key_value(buf,"q",param,sizeof(param))==ESP_OK){
        ESP_LOGI(TAG,"Responding with 'present'");
        httpd_resp_send(req,"present",HTTPD_RESP_USE_STRLEN);
       }else{
        httpd_resp_send(req,"Invalid request",HTTPD_RESP_USE_STRLEN);
       }       
    } else{
        httpd_resp_send(req,"Invalid request",HTTPD_RESP_USE_STRLEN);
    }
    free(buf);

    return ESP_OK;
}   

static const httpd_uri_t  uri_handler = {
    .uri        = "/",
    .method     = HTTP_GET,
    .handler    = root_get_handler,
    .user_ctx   =NULL
};

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if(httpd_start(&server,&config) == ESP_OK){
        httpd_register_uri_handler(server,&uri_handler);
    }
}

static void stop_webserver(void)
{
    if(server){
        httpd_stop(server);
        server = NULL;
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            ESP_LOGI(TAG,"connect to the AP fail");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
          char ip_str[IP4ADDR_STRLEN_MAX];
    esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, IP4ADDR_STRLEN_MAX);
    ESP_LOGI(TAG, "got ip: %s", ip_str);
    s_retry_num = 0;
    start_webserver();
    }
}

void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &event_handler,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &event_handler,
                                        NULL,
                                        &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	        //.threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    ESP_LOGI(TAG, "Connecting to %s...", ESP_WIFI_SSID);
}

void app_main() 
{
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
}