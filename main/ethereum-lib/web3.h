/*
    介绍：
    这是一个用于与以太坊节点进行通信的库，支持发送JSON-RPC请求。
    该库使用ESP-IDF的HTTP客户端库来发送请求和接收响应。
    它提供了初始化、发送请求和清理上下文的功能。
    该库支持的请求包括获取区块号、账户余额、交易收据、客户端版本和Keccak256哈希值。
    该库还提供了一个测试函数，用于测试与指定主机和端口的TCP连接。

    最后更新：2025年 3月 24日
    

*/

#include <esp_http_client.h>
#include <esp_err.h>
#include <stddef.h>

#ifndef WEB3_H
#define WEB3_H

typedef struct {
    char* url;
    esp_http_client_config_t config;
    esp_http_client_handle_t client;
} web3_context_t;

/**
 * @brief 初始化web3上下文
 * 
 * @param context web3上下文
 * @param url Ethereum节点的RPC URL
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t web3_init(web3_context_t* context, const char* url);

/**
 * @brief 发送JSON-RPC请求
 * 
 * @param context web3上下文
 * @param method RPC方法名
 * @param params JSON格式的参数
 * @param result 结果缓冲区
 * @param result_len 结果缓冲区长度
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t web3_send_request(web3_context_t* context, const char* method, 
                           const char* params, char* result, size_t result_len);

/**
 * @brief 清理web3上下文
 * 
 * @param context web3上下文
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t web3_cleanup(web3_context_t* context);

#endif /* WEB3_H */