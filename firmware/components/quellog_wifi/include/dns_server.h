#ifndef QUELLOG_WIFI_DNS_SERVER_H_
#define QUELLOG_WIFI_DNS_SERVER_H_

#include <atomic>

#include <esp_netif_ip_addr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class DnsServer {
public:
    DnsServer() = default;
    ~DnsServer();

    void Start(esp_ip4_addr_t gateway);
    void Stop();

private:
    void Run();

    int port_ = 53;
    int socket_fd_ = -1;
    esp_ip4_addr_t gateway_ = {};
    std::atomic<bool> running_{false};
    TaskHandle_t task_handle_ = nullptr;
};

#endif  // QUELLOG_WIFI_DNS_SERVER_H_
