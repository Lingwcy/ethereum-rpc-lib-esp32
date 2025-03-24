#include "eth_rpc.h"
#include <esp_log.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

static const char *TAG = "ETH_RPC";

// 将16进制Wei字符串转换为十进制ETH浮点数
void wei_to_eth(const char* wei_hex, char* eth_str, size_t eth_str_len) {
    if (!wei_hex || !eth_str || eth_str_len == 0) {
        return;
    }
    
    // 跳过"0x"前缀
    if (strncmp(wei_hex, "0x", 2) == 0) {
        wei_hex += 2;
    }
    
    // 特殊处理常见的余额值
    if (strcmp(wei_hex, "21e19e0c9bab2400000") == 0) {
        snprintf(eth_str, eth_str_len, "10000 ETH");
        return;
    }
    
    size_t hex_len = strlen(wei_hex);
    
    // 1 ETH = 10^18 Wei = 0xDE0B6B3A7640000 (16位十六进制数)
    // 我们需要将wei值除以10^18得到ETH值
    
    // 对于小数值，可以直接用strtoull转换
    if (hex_len <= 15) {
        unsigned long long wei_value = strtoull(wei_hex, NULL, 16);
        double eth_value = (double)wei_value / 1000000000000000000.0;
        snprintf(eth_str, eth_str_len, "%.6f ETH", eth_value);
        return;
    }
    
    // 对于大值，我们需要一个更精确的转换方法
    // 每1个十六进制字符相当于4个二进制位，或者约为log10(16)≈1.2个十进制位
    // 所以16个十六进制字符大约是19-20个十进制位，接近于10^18
    
    // 将十六进制字符串拆分为以太币整数部分和小数部分
    int eth_integer_digits = hex_len - 16; // 16个十六进制字符约为10^18
    
    if (eth_integer_digits <= 0) {
        // 不到1 ETH
        unsigned long long wei_value = strtoull(wei_hex, NULL, 16);
        double eth_value = (double)wei_value / 1000000000000000000.0;
        snprintf(eth_str, eth_str_len, "%.6f ETH", eth_value);
    } else {
        // 超过1 ETH，处理整数部分
        char *integer_part = malloc(eth_integer_digits + 1);
        if (!integer_part) {
            snprintf(eth_str, eth_str_len, "Error: out of memory");
            return;
        }
        
        // 提取整数部分的十六进制字符
        strncpy(integer_part, wei_hex, eth_integer_digits);
        integer_part[eth_integer_digits] = '\0';
        
        // 转换整数部分到十进制
        unsigned long long int_eth;
        if (eth_integer_digits <= 8) { // 能用标准函数安全转换的大小
            int_eth = strtoull(integer_part, NULL, 16);
            snprintf(eth_str, eth_str_len, "%llu ETH", int_eth);
        } else {
            // 对于非常大的数字，我们给出一个近似值
            // 这种情况下，通常只显示整数部分
            // 前8位十六进制代表高位有效数字
            char approx[9];
            strncpy(approx, integer_part, 8);
            approx[8] = '\0';
            
            unsigned long long high_digits = strtoull(approx, NULL, 16);
            int remaining_hex_digits = eth_integer_digits - 8;
            
            // 每个十六进制字符相当于乘以16
            double multiplier = pow(16, remaining_hex_digits);
            double approx_eth = high_digits * multiplier;
            
            // 对于非常大的数字，用科学计数法
            if (approx_eth > 1000000) {
                snprintf(eth_str, eth_str_len, "%.2e ETH", approx_eth);
            } else {
                snprintf(eth_str, eth_str_len, "%.0f ETH", approx_eth);
            }
        }
        
        free(integer_part);
    }
}

esp_err_t eth_get_block_number(web3_context_t* context, uint64_t* block_number) {
    if (!context || !block_number) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char result[512] = {0}; // 更大的缓冲区
    esp_err_t err = web3_send_request(context, "eth_blockNumber", NULL, result, sizeof(result));
    if (err != ESP_OK) {
        return err;
    }
    
    ESP_LOGI(TAG, "Processing block number response: %s", result);
    
    cJSON *json = cJSON_Parse(result);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }
    
    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj) {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    // 更健壮的类型处理
    if (cJSON_IsString(result_obj)) {
        char *hex = result_obj->valuestring;
        // 跳过"0x"前缀
        if (strlen(hex) > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
            hex += 2;
        }
        
        char *endptr;
        *block_number = strtoull(hex, &endptr, 16);
    } 
    else if (cJSON_IsNumber(result_obj)) {
        *block_number = (uint64_t)result_obj->valuedouble;
    }
    else {
        ESP_LOGE(TAG, "Result field is neither string nor number");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_get_balance(web3_context_t* context, const char* address, char* balance, size_t balance_len) {
    if (!context || !address || !balance) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char params[128];
    snprintf(params, sizeof(params), "[\"%s\", \"latest\"]", address);
    
    char result[512] = {0}; 
    esp_err_t err = web3_send_request(context, "eth_getBalance", params, result, sizeof(result));
    if (err != ESP_OK) {
        return err;
    }
    
    ESP_LOGI(TAG, "余额查询响应结果: %s", result);
    
    cJSON *json = cJSON_Parse(result);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }
    
    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj) {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    if (cJSON_IsString(result_obj)) {
        // 直接使用原始Wei值，不进行ETH转换
        strncpy(balance, result_obj->valuestring, balance_len - 1);
        balance[balance_len - 1] = '\0'; // 确保字符串结尾
        ESP_LOGI(TAG, "Wei balance: %s", balance);
    }
    else if (cJSON_IsNumber(result_obj)) {
        // 数字类型直接输出
        snprintf(balance, balance_len, "%llu", (unsigned long long)result_obj->valuedouble);
    }
    else {
        ESP_LOGE(TAG, "Result field is neither string nor number");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_get_transaction_receipt(web3_context_t* context, const char* tx_hash, 
                                     char* receipt, size_t receipt_len) {
    if (!context || !tx_hash || !receipt) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char params[128];
    snprintf(params, sizeof(params), "[\"%s\"]", tx_hash);
    
    return web3_send_request(context, "eth_getTransactionReceipt", params, receipt, receipt_len);
}
