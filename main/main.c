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
#include "ethereum-lib/eth_abi.h"
#include "farmkeeper-rpc/device/device.h"

#include "cJSON.h"
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

// Test network account details
typedef struct {
    const char* address;
    const char* private_key;
} eth_account_t;

// Test account data
static const eth_account_t test_accounts[] = {
    {
        .address = "0xa0Ee7A142d267C1f36714E4a8F75612F20a79720", // anvil default account
        .private_key = "0x2a871d0798f97d79848a013d4936a73bf4cc922c825d33c1cf7073dff6d409c6"
    }
};

// 测试交易签名功能
void test_transaction_signing(web3_context_t* context) {
    ESP_LOGI(TAG, "测试交易签名...");
    
    // 准备交易参数
    const char* from_address = test_accounts[0].address;
    const char* to_address = test_accounts[1].address;
    const char* value = "0xDE0B6B3A7640000"; // 转账金额 1 ETH (1 ETH = 10^18 Wei)
    const char* gas = "0x5208"; // 21000 gas - 标准转账所需的gas量
    
    // 获取账户nonce
    char nonce[32] = {0};
    esp_err_t err = eth_getTransactionCount(context, from_address, nonce, sizeof(nonce));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "获取nonce失败: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "当前账户nonce: %s", nonce);
    
    // 获取当前gas价格
    char gas_price[64] = {0};
    err = get_eth_gasPrice(context, gas_price, sizeof(gas_price));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "获取gas价格失败: %s", esp_err_to_name(err));
        return;
    }
    
    // 正确地提取十六进制gas价格部分
    char gas_price_hex[32] = {0};
    if (strncmp(gas_price, "0x", 2) == 0) {
        // 找到十六进制字符串的结束位置 (空格或括号处)
        char* end = strchr(gas_price, ' ');
        if (!end) {
            end = strchr(gas_price, '(');
        }
        
        if (end) {
            size_t hex_len = end - gas_price;
            strncpy(gas_price_hex, gas_price, hex_len);
            gas_price_hex[hex_len] = '\0';
        } else {
            // 如果没找到分隔符，复制整个字符串
            strcpy(gas_price_hex, gas_price);
        }
    } else {
        // 如果不是以0x开头，使用默认值
        strcpy(gas_price_hex, "0x1");
    }
    ESP_LOGI(TAG, "当前gas价格 (仅十六进制部分): %s", gas_price_hex);
    
    // 对于简单的ETH转账，data字段为空
    const char* data = "0x"; // 或NULL
    
    // 签名交易
    char signed_tx[1024] = {0};
    err = eth_signTransaction(
        context,
        from_address,
        to_address,
        gas,
        gas_price_hex,  // 使用提取的纯十六进制gas价格
        value,
        data,
        nonce,
        signed_tx,
        sizeof(signed_tx)
    );
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "交易签名失败: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "交易签名成功: %s", signed_tx);
    
    // 可选: 发送已签名的交易
    char tx_hash[128] = {0};
    if (true) { // 设置为true以实际发送交易
        err = eth_sendRawTransaction(context, signed_tx, tx_hash, sizeof(tx_hash));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "发送交易失败: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(TAG, "交易已发送，交易哈希: %s", tx_hash);
    }
}

// 调用ERC20代币合约transfer函数示例
void test_erc20_transfer(web3_context_t* context) {
    ESP_LOGI(TAG, "测试ERC20代币转账签名...");
    
    // 准备交易参数
    const char* from_address = test_accounts[0].address;
    const char* token_contract = "0x5FbDB2315678afecb367f032d93F642f64180aa3"; // 假设的ERC20合约地址
    const char* to_address = test_accounts[1].address;
    const char* gas = "0x15F90"; // 90000 gas (合约调用需要更多gas)
    
    // 构造ERC20 transfer方法的data字段
    // 格式: 0x + 方法签名(4字节) + 参数(填充到32字节)
    // transfer(address,uint256) => 0xa9059cbb
    // 参数1: 接收地址 (右对齐到32字节)
    // 参数2: 代币数量 (右对齐到32字节)
    char data[256];
    snprintf(data, sizeof(data), 
             "0xa9059cbb000000000000000000000000%s0000000000000000000000000000000000000000000000000000000000000064", 
             to_address + 2); // 去掉地址的0x前缀，转账金额为100 (0x64)
    
    // 获取账户nonce
    char nonce[32] = {0};
    esp_err_t err = eth_getTransactionCount(context, from_address, nonce, sizeof(nonce));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "获取nonce失败: %s", esp_err_to_name(err));
        return;
    }
    
    // 签名交易
    char signed_tx[1024] = {0};
    err = eth_signTransaction(
        context,
        from_address,
        token_contract,
        gas,
        "0x1", // 简化的gas价格
        "0x0", // 不发送ETH
        data,
        nonce,
        signed_tx,
        sizeof(signed_tx)
    );
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ERC20交易签名失败: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "ERC20交易签名成功: %s", signed_tx);
}
// 测试ABI编码
void test_abi_encoding(web3_context_t* context) {
    ESP_LOGI(TAG, "测试ABI编码...");

    // 测试ERC20 transfer函数调用
    char to_address[42] = "0x70997970C51812dc3A010C7d01b50e0d17dc79C8";  // 目标地址
    uint8_t to_addr_bytes[20];
    
    // 移除0x前缀并转换为二进制
    for (int i = 0; i < 20; i++) {
        sscanf(&to_address[2+i*2], "%2hhx", &to_addr_bytes[i]);
    }
    
    // 修复one_eth数组，确保正好32个元素
    const uint8_t one_eth[32] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0d, 0xe0, 0xb6, 0xb3
    };

    // 定义参数
    abi_param_t params[2] = {
        ABI_ADDRESS(to_addr_bytes),                   // 接收地址
        ABI_UINT(256, one_eth)                        // 金额 (1.0 ETH)
    };

    // 编码函数调用 - 使用更新后的函数签名
    uint8_t encoded[512] = {0};
    size_t encoded_len = 0;
    esp_err_t err = abi_encode_function_call(
        context,  // 传递web3上下文
        "transfer(address,uint256)", 
        params, 
        2, 
        encoded, 
        sizeof(encoded), 
        &encoded_len
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ABI编码失败: %s", esp_err_to_name(err));
        return;
    }

    // 转换为十六进制字符串
    char hex_data[1024] = {0};
    err = abi_binary_to_hex(encoded, encoded_len, hex_data, sizeof(hex_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "二进制转十六进制失败: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "编码后的函数调用数据: %s", hex_data);
    ESP_LOGI(TAG, "数据长度: %d 字节", encoded_len);


}

// 测试调用合约函数获取作者信息
void test_get_author_info(web3_context_t* context) {
    ESP_LOGI(TAG, "测试调用合约函数获取作者信息...");
    
    // 合约地址
    const char* contract_address = "0x8aCd85898458400f7Db866d53FCFF6f0D49741FF";
    
    // 创建函数调用数据 - getAuthorInformation()
    // 无需参数，仅需函数选择器
    uint8_t encoded[4] = {0}; // 只需要4字节的函数选择器
    size_t encoded_len = 0;
    esp_err_t err = abi_encode_function_call(
        context,
        "getAuthorInformation()",
        NULL, // 无参数
        0,
        encoded,
        sizeof(encoded),
        &encoded_len
    );
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ABI编码函数调用失败: %s", esp_err_to_name(err));
        return;
    }
    
    // 转换为十六进制字符串用于eth_call
    char hex_data[16] = {0}; // 足够容纳0x + 8个十六进制字符
    err = abi_binary_to_hex(encoded, encoded_len, hex_data, sizeof(hex_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "十六进制转二进制失败: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "编码后的函数调用数据: %s", hex_data);
    
    // 增加缓冲区大小以处理更大的响应
    char result[4096] = {0}; // 增加缓冲区大小以处理返回的三个字符串
    
    // 使用params缓冲区构造请求参数
    char params[512];
    snprintf(params, sizeof(params), 
             "[{\"to\":\"%s\",\"data\":\"%s\"},\"latest\"]", 
             contract_address, hex_data);
    
    // 直接使用web3_send_request，避免在eth_call中再次格式化参数
    err = web3_send_request(context, "eth_call", params, result, sizeof(result));
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "调用合约函数失败: %s", esp_err_to_name(err));
        return;
    }
    
    // 解析响应
    ESP_LOGI(TAG, "合约调用响应: %s", result);
    
    cJSON *json = cJSON_Parse(result);
    if (!json) {
        ESP_LOGE(TAG, "解析JSON响应失败");
        return;
    }
    
    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj || !cJSON_IsString(result_obj)) {
        ESP_LOGE(TAG, "合约调用结果解析失败");
        cJSON_Delete(json);
        return;
    }
    
    const char* encoded_result = result_obj->valuestring;
    ESP_LOGI(TAG, "合约返回的编码数据: %s", encoded_result);
    
    // 确保编码结果有效且不为空
    if (!encoded_result || strlen(encoded_result) < 2) {
        ESP_LOGE(TAG, "返回的编码数据无效");
        cJSON_Delete(json);
        return;
    }
    
    // 将十六进制编码结果转换为二进制数据
    uint8_t binary_data[4096] = {0}; // 增加缓冲区大小
    size_t binary_len = 0;
    
    err = abi_hex_to_binary(encoded_result, binary_data, sizeof(binary_data), &binary_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "十六进制转二进制失败: %s", esp_err_to_name(err));
        cJSON_Delete(json);
        return;
    }
    
    if (binary_len == 0) {
        ESP_LOGE(TAG, "二进制数据长度为0");
        cJSON_Delete(json);
        return;
    }
    
    // 解码返回的三个字符串
    abi_decoded_value_t decoded_values[3] = {0}; // 三个字符串
    size_t decoded_count = 0;
    
    // 在解码前清空结构体
    memset(decoded_values, 0, sizeof(decoded_values));
    
    err = abi_decode_returns(binary_data, binary_len, decoded_values, 3, &decoded_count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "解码返回值失败: %s", esp_err_to_name(err));
        cJSON_Delete(json);
        return;
    }
    
    ESP_LOGI(TAG, "成功解码 %d 个返回值", decoded_count);
    
    // 显示解码后的字符串
    for (size_t i = 0; i < decoded_count; i++) {
        if (decoded_values[i].value.string) {
            ESP_LOGI(TAG, "返回值 %d: %s", i+1, decoded_values[i].value.string);
        } else {
            ESP_LOGI(TAG, "返回值 %d: <null>", i+1);
        }
    }
    
    // 释放分配的内存
    for (size_t i = 0; i < decoded_count; i++) {
        abi_free_decoded_value(&decoded_values[i]);
    }
    
    cJSON_Delete(json);
}

// 测试调用addFarm合约方法
void test_add_farm(web3_context_t* context) {
    ESP_LOGI(TAG, "测试添加农田合约方法...");
    
    // 合约地址
    const char* contract_address = "0xeC4cFde48EAdca2bC63E94BB437BbeAcE1371bF3";
    
    // 准备Location结构体参数
    uint8_t latitude_bytes[32] = {0}; // 初始化为零
    uint8_t longitude_bytes[32] = {0}; // 初始化为零
    
    // 设置纬度为3456789（34.56789度，5位小数）
    latitude_bytes[31] = 0x15;
    latitude_bytes[30] = 0xFA;
    latitude_bytes[29] = 0x05;
    
    // 设置经度为1234567（12.34567度，5位小数）
    longitude_bytes[31] = 0x12;
    longitude_bytes[30] = 0xD6;
    longitude_bytes[29] = 0x87;
    
    // 定义Location结构体的参数数组
    abi_param_t location_params[2] = {
        ABI_UINT(256, latitude_bytes),     // 纬度
        ABI_UINT(256, longitude_bytes)     // 经度
    };
    
    // 编码函数调用
    uint8_t encoded[512] = {0};
    size_t encoded_len = 0;
    esp_err_t err = abi_encode_function_call(
        context,
        "addFarm((uint256,uint256))",  // 带结构体类型的函数签名
        location_params,
        2,
        encoded,
        sizeof(encoded),
        &encoded_len
    );
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "编码addFarm函数调用失败: %s", esp_err_to_name(err));
        return;
    }
    
    // 将二进制数据转换为十六进制以用于交易
    char hex_data[1024] = {0};
    err = abi_binary_to_hex(encoded, encoded_len, hex_data, sizeof(hex_data));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "二进制转十六进制失败: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "编码后的函数调用数据: %s", hex_data);
    
    // 获取发送者地址和nonce
    const char* from_address = test_accounts[0].address;
    
    char nonce[32] = {0};
    err = eth_getTransactionCount(context, from_address, nonce, sizeof(nonce));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "获取nonce失败: %s", esp_err_to_name(err));
        return;
    }
    
    // 获取gas价格
    char gas_price[64] = {0};
    err = get_eth_gasPrice(context, gas_price, sizeof(gas_price));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "获取gas价格失败: %s", esp_err_to_name(err));
        return;
    }
    
    // 提取十六进制gas价格
    char gas_price_hex[32] = {0};
    if (strncmp(gas_price, "0x", 2) == 0) {
        char* end = strchr(gas_price, ' ');
        if (!end) {
            end = strchr(gas_price, '(');
        }
        
        if (end) {
            size_t hex_len = end - gas_price;
            strncpy(gas_price_hex, gas_price, hex_len);
            gas_price_hex[hex_len] = '\0';
        } else {
            strcpy(gas_price_hex, gas_price);
        }
    } else {
        strcpy(gas_price_hex, "0x1");
    }
    
    // 为合约交互设置更高的gas限制
    const char* gas = "0x100000"; // 为合约调用设置更高的gas限制
    
    // 签署交易
    char signed_tx[1024] = {0};
    err = eth_signTransaction(
        context,
        from_address,
        contract_address,
        gas,
        gas_price_hex,
        "0x0", // 不发送ETH
        hex_data,
        nonce,
        signed_tx,
        sizeof(signed_tx)
    );
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "签署交易失败: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "交易签署成功");
    
    // 发送交易
    char tx_hash[128] = {0};
    err = eth_sendRawTransaction(context, signed_tx, tx_hash, sizeof(tx_hash));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "发送交易失败: %s", esp_err_to_name(err));
        return;
    }
    
    ESP_LOGI(TAG, "交易发送成功。哈希: %s", tx_hash);
    
    // 等待一段时间，然后检查交易收据
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    char receipt[2048] = {0};
    err = eth_get_transaction_receipt(context, tx_hash, receipt, sizeof(receipt));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "交易收据: %s", receipt);
    } else {
        ESP_LOGW(TAG, "获取交易收据失败（可能仍在等待确认）: %s", esp_err_to_name(err));
    }
}

// 测试设备挑战功能
void test_device_challenge(web3_context_t* context) {
    ESP_LOGI(TAG, "设备可信连接测试开始 (ESP32 side)...");
    
    // Configure the device
    farmkeeper_device_config_t device_config = {
        .web3_ctx = context,
        .contract_address = "0xeC4cFde48EAdca2bC63E94BB437BbeAcE1371bF3", // FarmKeeper contract address
        .device_private_key = test_accounts[0].private_key,  // Use test account #0 as the device  // Removed 0x prefix
        .device_address = test_accounts[0].address,
        .device_id = 0,  // Device ID in the blockchain
        .poll_interval_ms = 30000  // Check every 30 seconds
    };
    
    // Initialize the device challenge module
    esp_err_t err = farmkeeper_device_init(&device_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize device challenge module: %s", esp_err_to_name(err));
        return;
    }
    
    // Device role: Check for challenges and respond to them
    ESP_LOGI(TAG, "Device is checking for pending challenges...");
    
    // Check if there's a pending challenge
    bool has_challenge = false;
    err = farmkeeper_device_has_challenge(&has_challenge);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to check for challenge: %s", esp_err_to_name(err));
        return;
    }
    
    if (has_challenge) {
        ESP_LOGI(TAG, "Device found a pending challenge - responding to it...");
        
        // Get the challenge
        char challenge[512] = {0};
        err = farmkeeper_device_get_challenge(challenge, sizeof(challenge));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get challenge: %s", esp_err_to_name(err));
            return;
        }
        
        ESP_LOGI(TAG, "Retrieved challenge: %s", challenge);
        
        // Sign and verify the challenge
        err = farmkeeper_device_verify_challenge(challenge);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to verify challenge: %s", esp_err_to_name(err));
            return;
        }
        
        ESP_LOGI(TAG, "Challenge verification successful!");
    } else {
        ESP_LOGI(TAG, "No pending challenge for the device");
        ESP_LOGI(TAG, "In a real device, we would set up a periodic task to poll for challenges");
        ESP_LOGI(TAG, "Use the web interface to create a challenge for this device (ID: 1)");
    }
    
    // In a real application, this would be a recurring task
    ESP_LOGI(TAG, "In production, use the following task structure to continuously monitor for challenges:");
    ESP_LOGI(TAG, "void device_challenge_task(void *pvParameter) {");
    ESP_LOGI(TAG, "    while(1) {");
    ESP_LOGI(TAG, "        farmkeeper_device_check_and_respond_challenge();");
    ESP_LOGI(TAG, "        vTaskDelay(pdMS_TO_TICKS(device_config.poll_interval_ms));");
    ESP_LOGI(TAG, "    }");
    ESP_LOGI(TAG, "}");
}

// 设备挑战监听任务 - 简化版本，验证成功后自动退出
void device_challenge_monitor_task(void *pvParameter) {
    farmkeeper_device_config_t *input_config = (farmkeeper_device_config_t*)pvParameter;
    
    // 获取web3上下文
    web3_context_t context;
    
    // 使用静态字符串而非局部变量指针
    const char* eth_url = "http://192.168.1.100:8545"; // RPC URL
    ESP_LOGI(TAG, "设备挑战监听任务启动，初始化Web3...");
    esp_err_t err = web3_init(&context, eth_url);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "初始化web3失败: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    
    // 创建新的设备配置结构，确保深度复制所有指针数据
    farmkeeper_device_config_t device_config = {
        .web3_ctx = &context,
        .contract_address = input_config->contract_address,
        .device_private_key = input_config->device_private_key,
        .device_address = input_config->device_address,  
        .device_id = input_config->device_id,
        .poll_interval_ms = input_config->poll_interval_ms
    };
    
    err = farmkeeper_device_init(&device_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "初始化设备挑战模块失败: %s", esp_err_to_name(err));
        web3_cleanup(&context);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "设备挑战监听任务已启动 - 设备ID: %d", device_config.device_id);
    ESP_LOGI(TAG, "开始持续监听链上挑战...");
    
    // 设置检查间隔
    const int CHECK_INTERVAL_MS = 3000; // 每3秒检查一次
    int attempts = 0;
    const int MAX_ATTEMPTS = 100; // 最多尝试100次，防止无限循环
    
    // 主循环 - 持续检查并响应链上挑战，成功后自动退出
    while (attempts < MAX_ATTEMPTS) {
        attempts++;
        bool has_challenge = false;
        
        // 检查是否有挑战
        err = farmkeeper_device_has_challenge(&has_challenge);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "检查挑战状态失败: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(5000)); // 错误后等待5秒
            continue;
        }
        
        if (has_challenge) {
            ESP_LOGI(TAG, "检测到链上挑战，正在处理...");
            
            // 获取挑战内容
            char challenge[512] = {0};
            err = farmkeeper_device_get_challenge(challenge, sizeof(challenge));
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "挑战内容: %s", challenge);
                
                // 签名并验证挑战
                err = farmkeeper_device_verify_challenge(challenge);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "挑战验证成功! 设备状态已更新");
                    
                    // 重置挑战标志，防止重复挑战
                    err = farmkeeper_device_reset_challenge_flag();
                    if (err == ESP_OK) {
                        ESP_LOGI(TAG, "成功重置挑战标志");
                    } else {
                        ESP_LOGW(TAG, "重置挑战标志失败: %s", esp_err_to_name(err));
                    }
                    
                    // 任务完成，退出循环
                    ESP_LOGI(TAG, "设备挑战响应完成，退出监听任务");
                    break;
                } else {
                    ESP_LOGE(TAG, "挑战验证失败: %s", esp_err_to_name(err));
                }
            } else {
                ESP_LOGE(TAG, "获取挑战内容失败: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGI(TAG, "设备ID %d 无挑战，等待下次检查...", device_config.device_id);
        }
        
        // 等待下次检查
        vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL_MS));
    }
    
    // 清理资源并退出
    ESP_LOGI(TAG, "设备挑战监听任务结束");
    web3_cleanup(&context);
    vTaskDelete(NULL);
}

void ethereum_test_task(void *pvParameter)
{   
    const char* eth_url = "http://192.168.1.100:8545"; // Hardhat/Ganache RPC URL
    
    // 先测试网络连接
    ESP_LOGI(TAG, "测试到以太坊节点的网络连接...");
    esp_err_t conn_err = test_url_connection(eth_url, 5000);
    if (conn_err != ESP_OK) {
        ESP_LOGE(TAG, "网络连接测试失败: %s", esp_err_to_name(conn_err));
        ESP_LOGE(TAG, "请检查:");
        ESP_LOGE(TAG, "1. 以太坊节点是否在 %s 上运行", eth_url);
        ESP_LOGE(TAG, "2. 防火墙是否允许连接到此地址/端口");
        ESP_LOGE(TAG, "3. 节点是否配置为接受外部连接 (--rpc-external 或 --host 0.0.0.0)");
        vTaskDelete(NULL);
        return;
    }
    
    /* 初始化web3上下文 */
    web3_context_t context;
    ESP_LOGI(TAG, "网络连接测试成功，初始化Web3...");
    esp_err_t err = web3_init(&context, eth_url);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "初始化web3失败: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    
    
    /* 获取以太坊客户端版本 */
    char client_version[128] = {0};
    err = eth_get_client_version(&context, client_version, sizeof(client_version));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "以太坊客户端版本: %s", client_version);
    } else {
        ESP_LOGE(TAG, "获取客户端版本失败: %s", esp_err_to_name(err));
    }

    /* 获取网络ID */
    char network_id[32] = {0};
    err = eth_get_net_version(&context, network_id, sizeof(network_id));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "网络ID: %s", network_id);
    } else {
        ESP_LOGE(TAG, "获取网络ID失败: %s", esp_err_to_name(err));
    }
    
    // /* 测试交易签名功能 */
    // test_transaction_signing(&context);
    
    // /* 测试ERC20代币转账签名 (如果有相应合约) */
    // // test_erc20_transfer(&context);
    
    // /* 测试ABI编码 */
    // test_abi_encoding(&context);

    // /* 增加延迟，避免连续的RPC调用可能导致的内存或同步问题 */
    // vTaskDelay(pdMS_TO_TICKS(500));

    // /* 测试调用合约函数 */
    // test_get_author_info(&context);
    
    /* 增加延迟 */
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // /* 测试调用addFarm合约方法 */
    // test_add_farm(&context);
    
    // /* 测试设备挑战功能 */
    // test_device_challenge(&context);
    
    /* 清理web3上下文 */
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
    
    /* 创建设备挑战监听任务 */
    static farmkeeper_device_config_t device_config = {
        // CRITICAL: Contract address must be exact match for latest deployment
        .contract_address = "0x8C8e61E4705D1dbEe6DeADb39E67AC77650b0704", // From log
        .device_private_key = "2a871d0798f97d79848a013d4936a73bf4cc922c825d33c1cf7073dff6d409c6", 
        .device_address = "0xa0Ee7A142d267C1f36714E4a8F75612F20a79720", 
        .device_id = 0,
        .poll_interval_ms = 3000
    };
    
    // Create a higher-priority task for device monitoring to ensure it gets CPU time
    xTaskCreatePinnedToCore(&device_challenge_monitor_task, "device_monitor", 16384, (void*)&device_config, 5, NULL, 1);
    
    printf("Ethereum RPC test 开始\n");
}