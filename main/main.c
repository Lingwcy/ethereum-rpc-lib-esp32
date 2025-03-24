#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include "ethereum-lib/web3.h"
#include "ethereum-lib/eth_rpc.h"
#include "ethereum-lib/net_test.h"

static const char *TAG = "ETHEREUM_TEST";

/* FreeRTOS事件组，用于WiFi事件处理 */
static EventGroupHandle_t wifi_event_group;

/* FreeRTOS事件的位定义 */
#define CONNECTED_BIT BIT0
#define FAIL_BIT      BIT1

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupClearBits(wifi_event_group, FAIL_BIT);
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

void wifi_init_sta()
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "448",
            .password = "987654321",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished");

    /* 等待连接成功或失败 */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            CONNECTED_BIT | FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap");
    } else if (bits & FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect");
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void ethereum_test_task(void *pvParameter)
{
    const char* eth_url = "http://192.168.1.112:8545";
    const char* wallet_address = "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266";
    
    // 先测试网络连接
    ESP_LOGI(TAG, "Testing network connection to Ethereum node...");
    esp_err_t conn_err = test_url_connection(eth_url, 5000);
    if (conn_err != ESP_OK) {
        ESP_LOGE(TAG, "Network connectivity test failed: %s", esp_err_to_name(conn_err));
        ESP_LOGE(TAG, "Please check if:");
        ESP_LOGE(TAG, "1. Ethereum node is running on %s", eth_url);
        ESP_LOGE(TAG, "2. Firewall allows connections to this address/port");
        ESP_LOGE(TAG, "3. Node is configured to accept external connections (--rpc-external or --host 0.0.0.0)");
        vTaskDelete(NULL);
        return;
    }
    
    /* 初始化web3上下文 */
    web3_context_t context;
    ESP_LOGI(TAG, "Network connectivity test successful, initializing Web3...");
    esp_err_t err = web3_init(&context, eth_url);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize web3: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    
    /* 获取初始区块号 */
    uint64_t block_number;
    err = eth_get_block_number(&context, &block_number);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "区块高度: %llu", block_number);
    } else {
        ESP_LOGE(TAG, "获取区块高度失败: %s", esp_err_to_name(err));
    }
    
    // 循环检测ETH余额和区块高度，每10秒一次
    ESP_LOGI(TAG, "钱包周期性检查: %s", wallet_address);
    ESP_LOGI(TAG, "每 10 秒 一次...");
    
    int check_count = 0;
    while(1) {
        check_count++;
        ESP_LOGI(TAG, "------------------ 检查 #%d ------------------", check_count);
        
        /* 获取区块高度 */
        uint64_t current_block;
        err = eth_get_block_number(&context, &current_block);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "当前区块高度: %llu", current_block);
            
            // 如果区块高度有变化，显示增加了多少个区块
            if (check_count > 1 && current_block > block_number) {
                ESP_LOGI(TAG, "自从上次检查更新了 %llu 个区块", 
                        current_block - block_number);
            }
            block_number = current_block; // 更新区块高度
        } else {
            ESP_LOGW(TAG, "Block number check failed: %s", esp_err_to_name(err));
        }
        
        /* 获取ETH余额 */
        char balance[256] = {0};
        err = eth_get_balance(&context, wallet_address, balance, sizeof(balance));
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "账户余额(Wei): %s", balance);
        } else {
            ESP_LOGW(TAG, "余额检查失败: %s", esp_err_to_name(err));
        }
        
        // 延时10秒
        ESP_LOGI(TAG, "等待10秒进行下次检查...");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    
    /* 清理web3上下文 - 注意：这段代码在上面的无限循环中实际上不会执行到 */
    web3_cleanup(&context);
    vTaskDelete(NULL);
}

void app_main(void)
{
    /* 初始化NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    /* 初始化WiFi */
    wifi_init_sta();
    
    /* 创建Ethereum测试任务 */
    xTaskCreate(&ethereum_test_task, "ethereum_test_task", 8192, NULL, 5, NULL);
    
    printf("Ethereum RPC test 开始\n");
}