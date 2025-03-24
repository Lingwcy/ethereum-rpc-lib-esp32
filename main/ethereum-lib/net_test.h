#ifndef NET_TEST_H
#define NET_TEST_H

#include <esp_err.h>

/**
 * @brief 测试与指定主机和端口的TCP连接
 * 
 * @param host 目标主机名或IP地址
 * @param port 目标端口号
 * @param timeout_ms 连接超时时间（毫秒）
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t test_tcp_connection(const char* host, int port, int timeout_ms);

/**
 * @brief 解析URL并测试连接
 * 
 * @param url 完整的URL（如 http://example.com:8080）
 * @param timeout_ms 连接超时时间（毫秒）
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t test_url_connection(const char* url, int timeout_ms);

#endif /* NET_TEST_H */
