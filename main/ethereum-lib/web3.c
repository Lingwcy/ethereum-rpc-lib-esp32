#include "web3.h"
#include <string.h>
#include <esp_log.h>
#include <cJSON.h>
#include <stdlib.h>

static const char *TAG = "WEB3";

// 全局响应缓冲区用于事件处理器中存储数据
typedef struct {
    char *buffer;
    size_t buffer_size;
    size_t data_length;
} http_response_buffer_t;

// HTTP事件处理函数
esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_buffer_t *buffer = (http_response_buffer_t *)evt->user_data;
    
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            // 如果接收到的数据可以放入缓冲区
            if (buffer->data_length + evt->data_len < buffer->buffer_size) {
                // 将数据复制到用户提供的缓冲区
                memcpy(buffer->buffer + buffer->data_length, evt->data, evt->data_len);
                buffer->data_length += evt->data_len;
                buffer->buffer[buffer->data_length] = 0; // 确保结尾有null字符
                ESP_LOGI(TAG, "Received %d bytes, total: %d", evt->data_len, buffer->data_length);
            } else {
                ESP_LOGE(TAG, "Response too large for buffer! buffer_size=%zu, data_len=%zu", 
                         buffer->buffer_size, buffer->data_length + evt->data_len);
            }
            return ESP_OK;
            
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP request completed");
            return ESP_OK;
            
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP error");
            return ESP_FAIL;
            
        default:
            return ESP_OK;
    }
}

esp_err_t web3_init(web3_context_t* context, const char* url) {
    if (!context || !url) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing web3 with URL: %s", url);
    
    context->url = strdup(url);
    if (!context->url) {
        return ESP_ERR_NO_MEM;
    }
    
    // 判断是否为HTTPS连接
    bool is_https = (strncmp(url, "https://", 8) == 0);
    
    context->config = (esp_http_client_config_t) {
        .url = context->url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        // 对于HTTPS连接，跳过证书验证
        .skip_cert_common_name_check = is_https,
        .crt_bundle_attach = NULL, // 不使用证书捆绑
        .cert_pem = NULL,
        .client_cert_pem = NULL,
        .client_key_pem = NULL,
        // 添加事件处理器
        .event_handler = http_event_handler,
    };
    
    context->client = esp_http_client_init(&context->config);
    if (!context->client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(context->url);
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(context->client, "Content-Type", "application/json");
    ESP_LOGI(TAG, "Web3 initialized successfully");
    
    return ESP_OK;
}

esp_err_t web3_send_request(web3_context_t* context, const char* method, 
                           const char* params, char* result, size_t result_len) {
    if (!context || !method || !result) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check if buffer size is reasonable
    if (result_len < 128) {
        ESP_LOGW(TAG, "Response buffer size %zu might be too small for RPC responses", result_len);
    }
    
    // 清空结果缓冲区
    memset(result, 0, result_len);
    
    // 创建响应缓冲区结构体
    http_response_buffer_t response_buffer = {
        .buffer = result,
        .buffer_size = result_len,
        .data_length = 0
    };
    
    // 设置事件处理器的用户数据为响应缓冲区
    esp_http_client_set_user_data(context->client, &response_buffer);
    
    static int request_id = 1;
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    cJSON_AddStringToObject(root, "method", method);
    
    if (params) {
        cJSON *params_json = cJSON_Parse(params);
        if (params_json) {
            cJSON_AddItemToObject(root, "params", params_json);
        } else {
            // 如果解析失败，添加一个空数组
            cJSON_AddArrayToObject(root, "params");
            ESP_LOGW(TAG, "Failed to parse params: %s, using empty array", params);
        }
    } else {
        // 添加空数组作为参数
        cJSON_AddArrayToObject(root, "params");
    }
    
    cJSON_AddNumberToObject(root, "id", request_id++);
    
    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!post_data) {
        return ESP_ERR_NO_MEM;
    }
    
    // 记录完整URL和请求内容
    char full_url[256];
    strncpy(full_url, context->url, sizeof(full_url) - 1);
    full_url[sizeof(full_url) - 1] = '\0';
    
    ESP_LOGI(TAG, "发送请求到 %s: %s", full_url, post_data);
    esp_http_client_set_url(context->client, full_url); // 确保URL设置正确
    esp_http_client_set_post_field(context->client, post_data, strlen(post_data));
    
    // 执行请求
    esp_err_t err = esp_http_client_perform(context->client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST 请求发送失败: %s", esp_err_to_name(err));
        free(post_data);
        return err;
    }
    
    int status_code = esp_http_client_get_status_code(context->client);
    ESP_LOGI(TAG, "HTTP 状态 = %d", status_code);
    
    if (status_code != 200) {
        ESP_LOGE(TAG, "HTTP 状态异常 %d", status_code);
        free(post_data);
        return ESP_FAIL;
    }
    
    free(post_data);
    
    // 如果事件处理器成功收集了响应数据
    if (response_buffer.data_length > 0) {
        // 确保字符串以null字符结尾
        if (response_buffer.data_length < result_len) {
            result[response_buffer.data_length] = '\0';
        } else {
            result[result_len - 1] = '\0';
        }
        
        ESP_LOGI(TAG, "响应: %s", result);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "没有接收到响应数据");
    return ESP_FAIL;
}

esp_err_t web3_cleanup(web3_context_t* context) {
    if (!context) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (context->client) {
        esp_http_client_cleanup(context->client);
    }
    
    if (context->url) {
        free(context->url);
    }
    
    return ESP_OK;
}
