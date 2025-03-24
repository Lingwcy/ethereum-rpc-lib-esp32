#include "net_test.h"
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>

static const char *TAG = "NET_TEST";

esp_err_t test_tcp_connection(const char* host, int port, int timeout_ms) {
    ESP_LOGI(TAG, "Testing TCP connection to %s:%d", host, port);
    
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    
    // 解析主机名
    int err = getaddrinfo(host, NULL, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed for %s: %d", host, err);
        return ESP_FAIL;
    }
    
    // 创建Socket
    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: %d", errno);
        freeaddrinfo(res);
        return ESP_FAIL;
    }
    
    // 设置非阻塞
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // 设置端口
    ((struct sockaddr_in *)res->ai_addr)->sin_port = htons(port);
    
    // 连接
    err = connect(sock, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    
    if (err < 0 && errno != EINPROGRESS) {
        ESP_LOGE(TAG, "Socket connect failed: %d", errno);
        close(sock);
        return ESP_FAIL;
    }
    
    // 等待连接或超时
    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    err = select(sock + 1, NULL, &fdset, NULL, &tv);
    if (err < 0) {
        ESP_LOGE(TAG, "Select failed: %d", errno);
        close(sock);
        return ESP_FAIL;
    } else if (err == 0) {
        ESP_LOGE(TAG, "Connection timeout");
        close(sock);
        return ESP_ERR_TIMEOUT;
    }
    
    // 检查连接是否成功
    int optval;
    socklen_t optlen = sizeof(optval);
    err = getsockopt(sock, SOL_SOCKET, SO_ERROR, &optval, &optlen);
    
    if (err < 0 || optval != 0) {
        ESP_LOGE(TAG, "Socket connect error: %d", optval);
        close(sock);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "TCP connection successful to %s:%d", host, port);
    close(sock);
    return ESP_OK;
}

esp_err_t test_url_connection(const char* url, int timeout_ms) {
    if (url == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 提取主机名和端口
    char proto[16] = {0};
    char host[128] = {0};
    int port = 80; // 默认HTTP端口
    
    // 解析协议
    if (sscanf(url, "%15[^:]://%127[^:/]:%d", proto, host, &port) != 3) {
        // 尝试不带端口的格式
        if (sscanf(url, "%15[^:]://%127[^:/]", proto, host) != 2) {
            ESP_LOGE(TAG, "Failed to parse URL: %s", url);
            return ESP_ERR_INVALID_ARG;
        }
        
        // 设置默认端口
        if (strcmp(proto, "https") == 0) {
            port = 443;
        } else {
            port = 80;
        }
    }
    
    ESP_LOGI(TAG, "Parsed URL: proto=%s, host=%s, port=%d", proto, host, port);
    return test_tcp_connection(host, port, timeout_ms);
}
