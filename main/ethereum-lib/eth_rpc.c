#include "eth_rpc.h"
#include <esp_log.h>
#include <cJSON.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>

static const char *TAG = "ETH_RPC";

// 将16进制Wei字符串转换为十进制ETH浮点数
void wei_to_eth(const char *wei_hex, char *eth_str, size_t eth_str_len)
{
    if (!wei_hex || !eth_str || eth_str_len == 0)
    {
        return;
    }

    // 跳过"0x"前缀
    if (strncmp(wei_hex, "0x", 2) == 0)
    {
        wei_hex += 2;
    }

    // 特殊处理常见的余额值
    if (strcmp(wei_hex, "21e19e0c9bab2400000") == 0)
    {
        snprintf(eth_str, eth_str_len, "10000 ETH");
        return;
    }

    size_t hex_len = strlen(wei_hex);

    // 1 ETH = 10^18 Wei = 0xDE0B6B3A7640000 (16位十六进制数)
    // 我们需要将wei值除以10^18得到ETH值

    // 对于小数值，可以直接用strtoull转换
    if (hex_len <= 15)
    {
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

    if (eth_integer_digits <= 0)
    {
        // 不到1 ETH
        unsigned long long wei_value = strtoull(wei_hex, NULL, 16);
        double eth_value = (double)wei_value / 1000000000000000000.0;
        snprintf(eth_str, eth_str_len, "%.6f ETH", eth_value);
    }
    else
    {
        // 超过1 ETH，处理整数部分
        char *integer_part = malloc(eth_integer_digits + 1);
        if (!integer_part)
        {
            snprintf(eth_str, eth_str_len, "Error: out of memory");
            return;
        }

        // 提取整数部分的十六进制字符
        strncpy(integer_part, wei_hex, eth_integer_digits);
        integer_part[eth_integer_digits] = '\0';

        // 转换整数部分到十进制
        unsigned long long int_eth;
        if (eth_integer_digits <= 8)
        { // 能用标准函数安全转换的大小
            int_eth = strtoull(integer_part, NULL, 16);
            snprintf(eth_str, eth_str_len, "%llu ETH", int_eth);
        }
        else
        {
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
            if (approx_eth > 1000000)
            {
                snprintf(eth_str, eth_str_len, "%.2e ETH", approx_eth);
            }
            else
            {
                snprintf(eth_str, eth_str_len, "%.0f ETH", approx_eth);
            }
        }

        free(integer_part);
    }
}

// 辅助函数：将十六进制字符串转换为十进制字符串
void hex_to_decimal(const char *hex, char *decimal, size_t decimal_len)
{
    if (!hex || !decimal || decimal_len == 0)
    {
        return;
    }

    // 跳过"0x"前缀
    if (strncmp(hex, "0x", 2) == 0)
    {
        hex += 2;
    }

    // 特殊情况处理：0
    if (strcmp(hex, "0") == 0 || strlen(hex) == 0)
    {
        strncpy(decimal, "0", decimal_len);
        return;
    }

    // 初始化十进制结果字符串为0
    decimal[0] = '0';
    decimal[1] = '\0';

    // 对每一位十六进制数字进行处理
    for (size_t i = 0; i < strlen(hex); i++)
    {
        char hex_digit = hex[i];
        int digit_value;

        // 将十六进制字符转换为数值
        if (hex_digit >= '0' && hex_digit <= '9')
        {
            digit_value = hex_digit - '0';
        }
        else if (hex_digit >= 'a' && hex_digit <= 'f')
        {
            digit_value = hex_digit - 'a' + 10;
        }
        else if (hex_digit >= 'A' && hex_digit <= 'F')
        {
            digit_value = hex_digit - 'A' + 10;
        }
        else
        {
            // 无效字符，跳过
            continue;
        }

        // 将已有的十进制结果乘以16，然后加上当前位的值
        // 这里采用字符串计算的方式，适合处理任意长度的大数

        // 现有的十进制结果乘以16
        int carry = 0;
        for (size_t j = 0; j < strlen(decimal); j++)
        {
            int val = (decimal[j] - '0') * 16 + carry;
            decimal[j] = (val % 10) + '0';
            carry = val / 10;
        }

        // 处理进位
        char temp[2] = {0};
        while (carry > 0)
        {
            temp[0] = (carry % 10) + '0';
            strncat(decimal, temp, decimal_len - strlen(decimal) - 1);
            carry /= 10;
        }

        // 加上当前位的值
        int sum = (decimal[0] - '0') + digit_value;
        decimal[0] = (sum % 10) + '0';
        carry = sum / 10;

        // 处理进位
        size_t j = 1;
        while (carry > 0 && j < strlen(decimal))
        {
            sum = (decimal[j] - '0') + carry;
            decimal[j] = (sum % 10) + '0';
            carry = sum / 10;
            j++;
        }

        // 如果最后还有进位，添加到结果中
        while (carry > 0)
        {
            temp[0] = (carry % 10) + '0';
            strncat(decimal, temp, decimal_len - strlen(decimal) - 1);
            carry /= 10;
        }
    }

    // 反转字符串（因为我们是从低位到高位计算的）
    size_t len = strlen(decimal);
    for (size_t i = 0; i < len / 2; i++)
    {
        char temp = decimal[i];
        decimal[i] = decimal[len - 1 - i];
        decimal[len - 1 - i] = temp;
    }
}

esp_err_t eth_get_block_number(web3_context_t *context, uint64_t *block_number)
{
    if (!context || !block_number)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char result[512] = {0}; // 更大的缓冲区
    esp_err_t err = web3_send_request(context, "eth_blockNumber", NULL, result, sizeof(result));
    if (err != ESP_OK)
    {
        return err;
    }

    ESP_LOGI(TAG, "Processing block number response: %s", result);

    cJSON *json = cJSON_Parse(result);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // 更健壮的类型处理
    if (cJSON_IsString(result_obj))
    {
        char *hex = result_obj->valuestring;
        // 跳过"0x"前缀
        if (strlen(hex) > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X'))
        {
            hex += 2;
        }

        char *endptr;
        *block_number = strtoull(hex, &endptr, 16);
    }
    else if (cJSON_IsNumber(result_obj))
    {
        *block_number = (uint64_t)result_obj->valuedouble;
    }
    else
    {
        ESP_LOGE(TAG, "Result field is neither string nor number");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_get_balance(web3_context_t *context, const char *address, char *balance, size_t balance_len)
{
    if (!context || !address || !balance)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char params[128];
    snprintf(params, sizeof(params), "[\"%s\", \"latest\"]", address);

    char result[512] = {0};
    esp_err_t err = web3_send_request(context, "eth_getBalance", params, result, sizeof(result));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(result);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsString(result_obj))
    {
        // 获取原始十六进制Wei值
        const char *hex_wei = result_obj->valuestring;

        // 转换为十进制Wei值
        char decimal_wei[128] = {0};
        hex_to_decimal(hex_wei, decimal_wei, sizeof(decimal_wei));

        // 格式化输出，同时显示十六进制和十进制值
        snprintf(balance, balance_len, "%s (十进制: %s Wei)", hex_wei, decimal_wei);
    }
    else if (cJSON_IsNumber(result_obj))
    {
        // 数字类型直接输出
        snprintf(balance, balance_len, "%llu Wei", (unsigned long long)result_obj->valuedouble);
    }
    else
    {
        ESP_LOGE(TAG, "Result field is neither string nor number");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_get_transaction_receipt(web3_context_t *context, const char *tx_hash,
                                      char *receipt, size_t receipt_len)
{
    if (!context || !tx_hash || !receipt)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char params[128];
    snprintf(params, sizeof(params), "[\"%s\"]", tx_hash);

    return web3_send_request(context, "eth_getTransactionReceipt", params, receipt, receipt_len);
}

esp_err_t eth_get_client_version(web3_context_t *context, char* client_version, size_t version_len)
{
    if (!context || !client_version || version_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char result[512] = {0};
    esp_err_t err = web3_send_request(context, "web3_clientVersion", NULL, result, sizeof(result));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(result);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsString(result_obj))
    {
        strncpy(client_version, result_obj->valuestring, version_len - 1);
        client_version[version_len - 1] = '\0'; // 确保字符串结尾
    }
    else
    {
        ESP_LOGE(TAG, "Result field is not a string");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}


esp_err_t eth_get_web3_sha3(web3_context_t *context, const char *post_data, char *hash, size_t hash_len)
{
    if (!context || !post_data || !hash || hash_len < 70) { // Ensure buffer is large enough
        ESP_LOGE(TAG, "Invalid arguments or buffer too small for web3_sha3");
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate Keccak256 hash
    char params[512]; // Larger buffer for params
    snprintf(params, sizeof(params), "[\"%s\"]", post_data);

    char result[256] = {0}; // Much larger buffer for the response
    esp_err_t err = web3_send_request(context, "web3_sha3", params, result, sizeof(result));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "web3_sha3 request failed: %s", esp_err_to_name(err));
        return err;
    }

    // Parse result
    cJSON *json = cJSON_Parse(result);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj || !cJSON_IsString(result_obj)) {
        ESP_LOGE(TAG, "Invalid or missing 'result' field in response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // Copy result, ensuring we don't overflow the buffer
    strncpy(hash, result_obj->valuestring, hash_len - 1);
    hash[hash_len - 1] = '\0'; // Ensure null termination
    
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_get_net_version(web3_context_t *context, char *network_id, size_t network_id_len)
{
    if (!context || !network_id || network_id_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char result[512] = {0};
    esp_err_t err = web3_send_request(context, "net_version", NULL, result, sizeof(result));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(result);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsString(result_obj))
    {
        strncpy(network_id, result_obj->valuestring, network_id_len - 1);
        network_id[network_id_len - 1] = '\0'; // 确保字符串结尾
    }
    else
    {
        ESP_LOGE(TAG, "Result field is not a string");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_get_net_listening(web3_context_t *context, bool *result, size_t result_len)
{
    if (!context || !result || result_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char response[512] = {0};
    esp_err_t err = web3_send_request(context, "net_listening", NULL, response, sizeof(response));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(response);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsBool(result_obj))
    {
        *result = cJSON_IsTrue(result_obj);
    }
    else
    {
        ESP_LOGE(TAG, "Result field is not a boolean");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_get_net_peerCount(web3_context_t *context, char *quantity, size_t quantity_len)
{
    if (!context || !quantity || quantity_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char result[512] = {0};
    esp_err_t err = web3_send_request(context, "net_peerCount", NULL, result, sizeof(result));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(result);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsString(result_obj))
    {
        strncpy(quantity, result_obj->valuestring, quantity_len - 1);
        quantity[quantity_len - 1] = '\0'; // 确保字符串结尾
    }
    else
    {
        ESP_LOGE(TAG, "Result field is not a string");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_get_eth_protocolVersion(web3_context_t *context, char *result, size_t result_len)
{
    if (!context || !result || result_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char response[512] = {0};
    esp_err_t err = web3_send_request(context, "eth_protocolVersion", NULL, response, sizeof(response));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(response);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsString(result_obj))
    {
        strncpy(result, result_obj->valuestring, result_len - 1);
        result[result_len - 1] = '\0'; // 确保字符串结尾
    }
    else
    {
        ESP_LOGE(TAG, "Result field is not a string");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_get_eth_syncing(web3_context_t *context, char *result, size_t result_len)
{
    if (!context || !result || result_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char response[512] = {0};
    esp_err_t err = web3_send_request(context, "eth_syncing", NULL, response, sizeof(response));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(response);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsString(result_obj))
    {
        strncpy(result, result_obj->valuestring, result_len - 1);
        result[result_len - 1] = '\0'; // 确保字符串结尾
    }
    else
    {
        ESP_LOGE(TAG, "Result field is not a string");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t get_eth_gasPrice(web3_context_t *context, char *quantity, size_t quantity_len)
{
    if (!context || !quantity || quantity_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char result[512] = {0};
    esp_err_t err = web3_send_request(context, "eth_gasPrice", NULL, result, sizeof(result));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(result);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsString(result_obj))
    {
        // 获取原始十六进制Wei值
        const char *hex_wei = result_obj->valuestring;

        // 转换为十进制Wei值
        char decimal_wei[128] = {0};
        hex_to_decimal(hex_wei, decimal_wei, sizeof(decimal_wei));

        // 格式化输出，同时显示十六进制和十进制值
        snprintf(quantity, quantity_len, "%s (十进制: %s Wei)", hex_wei, decimal_wei);
    }
    else
    {
        ESP_LOGE(TAG, "Result field is not a string");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_getTransactionCount(web3_context_t *context, const char *address, char *quantity, size_t quantity_len)
{
    if (!context || !address || !quantity || quantity_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char params[128];
    snprintf(params, sizeof(params), "[\"%s\", \"latest\"]", address);

    char result[512] = {0};
    esp_err_t err = web3_send_request(context, "eth_getTransactionCount", params, result, sizeof(result));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(result);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsString(result_obj))
    {
        strncpy(quantity, result_obj->valuestring, quantity_len - 1);
        quantity[quantity_len - 1] = '\0'; // 确保字符串结尾
    }
    else
    {
        ESP_LOGE(TAG, "Result field is not a string");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_sign(web3_context_t *context, const char *address, const char *data,
                   char *signed_data, size_t signed_data_len)
{
    if (!context || !address || !data || !signed_data || signed_data_len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    char params[256];
    snprintf(params, sizeof(params), "[\"%s\", \"%s\"]", address, data);

    char result[512] = {0};
    esp_err_t err = web3_send_request(context, "eth_sign", params, result, sizeof(result));
    if (err != ESP_OK)
    {
        return err;
    }

    cJSON *json = cJSON_Parse(result);
    if (!json)
    {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj)
    {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    if (cJSON_IsString(result_obj))
    {
        strncpy(signed_data, result_obj->valuestring, signed_data_len - 1);
        signed_data[signed_data_len - 1] = '\0'; // 确保字符串结尾
    }
    else
    {
        ESP_LOGE(TAG, "Result field is not a string");
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_signTransaction(web3_context_t* context,
                             const char* from,
                             const char* to,
                             const char* gas,
                             const char* gas_price,
                             const char* value,
                             const char* data,
                             const char* nonce,
                             char* signed_tx,
                             size_t signed_tx_len)
{
    if (!context || !from || !signed_tx || signed_tx_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 创建交易对象
    cJSON* tx_obj = cJSON_CreateObject();
    if (!tx_obj) {
        ESP_LOGE(TAG, "Failed to create transaction object");
        return ESP_ERR_NO_MEM;
    }

    // 添加必需的from字段
    cJSON_AddStringToObject(tx_obj, "from", from);

    // 添加可选字段
    if (to) {
        cJSON_AddStringToObject(tx_obj, "to", to);
    }
    if (gas) {
        cJSON_AddStringToObject(tx_obj, "gas", gas);
    }
    if (gas_price) {
        cJSON_AddStringToObject(tx_obj, "gasPrice", gas_price);
    }
    if (value) {
        cJSON_AddStringToObject(tx_obj, "value", value);
    }
    if (data) {
        cJSON_AddStringToObject(tx_obj, "data", data);
    }
    if (nonce) {
        cJSON_AddStringToObject(tx_obj, "nonce", nonce);
    }

    // 将交易对象转换为JSON字符串
    char* tx_json = cJSON_PrintUnformatted(tx_obj);
    cJSON_Delete(tx_obj);
    
    if (!tx_json) {
        ESP_LOGE(TAG, "Failed to convert transaction to JSON");
        return ESP_ERR_NO_MEM;
    }

    // 构造参数 - 将交易对象包装在数组中
    char* params = NULL;
    asprintf(&params, "[%s]", tx_json);
    free(tx_json);
    
    if (!params) {
        ESP_LOGE(TAG, "Failed to create params string");
        return ESP_ERR_NO_MEM;
    }

    // 发送RPC请求
    char result[1024] = {0}; // 增大缓冲区以容纳可能的较大响应
    esp_err_t err = web3_send_request(context, "eth_signTransaction", params, result, sizeof(result));
    free(params);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sign transaction: %s", esp_err_to_name(err));
        return err;
    }

    // 解析响应
    cJSON* json = cJSON_Parse(result);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    // 获取签名后的交易
    cJSON* result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj) {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        
        // 检查是否有错误信息
        cJSON* error_obj = cJSON_GetObjectItem(json, "error");
        if (error_obj) {
            cJSON* error_message = cJSON_GetObjectItem(error_obj, "message");
            if (error_message && cJSON_IsString(error_message)) {
                ESP_LOGE(TAG, "Error from Ethereum node: %s", error_message->valuestring);
            }
        }
        
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // 如果结果是字符串
    if (cJSON_IsString(result_obj)) {
        strncpy(signed_tx, result_obj->valuestring, signed_tx_len - 1);
        signed_tx[signed_tx_len - 1] = '\0'; // 确保字符串以null字符结尾
    } 
    // 如果结果是对象，提取raw字段
    else if (cJSON_IsObject(result_obj)) {
        cJSON* raw = cJSON_GetObjectItem(result_obj, "raw");
        if (raw && cJSON_IsString(raw)) {
            strncpy(signed_tx, raw->valuestring, signed_tx_len - 1);
            signed_tx[signed_tx_len - 1] = '\0';
        } else {
            ESP_LOGE(TAG, "No 'raw' field in result object");
            cJSON_Delete(json);
            return ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "Result field is neither string nor object");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Transaction signed successfully");
    
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_sendRawTransaction(web3_context_t* context, const char* signed_data, char* tx_hash, size_t tx_hash_len)
{
    if (!context || !signed_data || !tx_hash) {
        return ESP_ERR_INVALID_ARG;
    }

    // 确保签名数据以0x开头
    char* data_to_send;
    if (strncmp(signed_data, "0x", 2) != 0) {
        // 添加0x前缀
        data_to_send = malloc(strlen(signed_data) + 3); // +3 for "0x" and null terminator
        if (!data_to_send) {
            ESP_LOGE(TAG, "Failed to allocate memory for signed data");
            return ESP_ERR_NO_MEM;
        }
        sprintf(data_to_send, "0x%s", signed_data);
    } else {
        data_to_send = strdup(signed_data);
        if (!data_to_send) {
            ESP_LOGE(TAG, "Failed to duplicate signed data");
            return ESP_ERR_NO_MEM;
        }
    }

    // 构造参数
    char params[1024]; // 足够大的缓冲区来容纳签名数据
    snprintf(params, sizeof(params), "[\"%s\"]", data_to_send);
    free(data_to_send); // 释放临时缓冲区

    // 发送RPC请求
    char result[512] = {0};
    esp_err_t err = web3_send_request(context, "eth_sendRawTransaction", params, result, sizeof(result));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send raw transaction: %s", esp_err_to_name(err));
        return err;
    }

    // 解析JSON响应
    cJSON* json = cJSON_Parse(result);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    // 获取交易哈希（结果字段）
    cJSON* result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj || !cJSON_IsString(result_obj)) {
        ESP_LOGE(TAG, "Invalid or missing 'result' field in response");
        
        // 检查是否有错误信息
        cJSON* error_obj = cJSON_GetObjectItem(json, "error");
        if (error_obj) {
            cJSON* error_message = cJSON_GetObjectItem(error_obj, "message");
            if (error_message && cJSON_IsString(error_message)) {
                ESP_LOGE(TAG, "Error from Ethereum node: %s", error_message->valuestring);
            }
        }
        
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // 复制交易哈希到输出参数
    strncpy(tx_hash, result_obj->valuestring, tx_hash_len - 1);
    tx_hash[tx_hash_len - 1] = '\0'; // 确保字符串以null字符结尾
    
    ESP_LOGI(TAG, "Transaction sent successfully, hash: %s", tx_hash);
    
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_getCode(web3_context_t* context, const char* address, 
                     const char* block_id, char* code, size_t code_len)
{
    if (!context || !address || !block_id || !code || code_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 构造参数: [address, blockId]
    char params[256];
    snprintf(params, sizeof(params), "[\"%s\", \"%s\"]", address, block_id);

    // 发送RPC请求
    char result[4096] = {0}; // 较大的缓冲区以容纳合约代码
    esp_err_t err = web3_send_request(context, "eth_getCode", params, result, sizeof(result));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth_getCode request failed: %s", esp_err_to_name(err));
        return err;
    }

    // 解析JSON响应
    cJSON* json = cJSON_Parse(result);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    // 获取返回的合约代码
    cJSON* result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj) {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        
        // 检查是否有错误信息
        cJSON* error_obj = cJSON_GetObjectItem(json, "error");
        if (error_obj) {
            cJSON* error_message = cJSON_GetObjectItem(error_obj, "message");
            if (error_message && cJSON_IsString(error_message)) {
                ESP_LOGE(TAG, "Error from Ethereum node: %s", error_message->valuestring);
            }
        }
        
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // 检查返回的代码
    if (cJSON_IsString(result_obj)) {
        // 复制合约代码到输出缓冲区
        if (strlen(result_obj->valuestring) < code_len) {
            strcpy(code, result_obj->valuestring);
        } else {
            ESP_LOGW(TAG, "Contract code truncated (too large for buffer)");
            strncpy(code, result_obj->valuestring, code_len - 1);
            code[code_len - 1] = '\0'; // 确保字符串以null字符结尾
        }
    } else if (cJSON_IsNull(result_obj)) {
        // 没有代码（可能是普通账户，不是合约）
        strcpy(code, "0x");
    } else {
        ESP_LOGE(TAG, "Result field is neither string nor null");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

esp_err_t eth_call(web3_context_t* context, const char* to_address, const char* data, 
                  const char* block, char* result, size_t result_len)
{
    if (!context || !to_address || !data || !result || result_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // 构造参数: [{"to":"合约地址", "data":"函数调用数据"}, "区块号"]
    char params[512];
    snprintf(params, sizeof(params), 
             "[{\"to\":\"%s\",\"data\":\"%s\"},\"%s\"]", 
             to_address, data, block ? block : "latest");

    // 发送RPC请求
    char response[4096] = {0}; // 较大的缓冲区以容纳可能的大型返回数据
    esp_err_t err = web3_send_request(context, "eth_call", params, response, sizeof(response));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "eth_call request failed: %s", esp_err_to_name(err));
        return err;
    }

    // 解析JSON响应
    cJSON* json = cJSON_Parse(response);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    // 获取返回值
    cJSON* result_obj = cJSON_GetObjectItem(json, "result");
    if (!result_obj) {
        ESP_LOGE(TAG, "No 'result' field in JSON response");
        
        // 检查是否有错误信息
        cJSON* error_obj = cJSON_GetObjectItem(json, "error");
        if (error_obj) {
            cJSON* error_message = cJSON_GetObjectItem(error_obj, "message");
            if (error_message && cJSON_IsString(error_message)) {
                ESP_LOGE(TAG, "Error from Ethereum node: %s", error_message->valuestring);
            }
        }
        
        cJSON_Delete(json);
        return ESP_FAIL;
    }

    // 复制返回值到输出缓冲区
    if (cJSON_IsString(result_obj)) {
        strncpy(result, result_obj->valuestring, result_len - 1);
        result[result_len - 1] = '\0'; // 确保字符串以null字符结尾
    } else {
        ESP_LOGE(TAG, "Result field is not a string");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}
