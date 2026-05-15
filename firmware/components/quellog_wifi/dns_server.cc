#include "dns_server.h"

#include <esp_log.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#include <cerrno>
#include <cstring>

namespace {

constexpr char kTag[] = "DnsServer";

}  // namespace

DnsServer::~DnsServer() {
    Stop();
}

void DnsServer::Start(esp_ip4_addr_t gateway) {
    Stop();

    gateway_ = gateway;
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_fd_ < 0) {
        ESP_LOGE(kTag, "failed to create socket");
        return;
    }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<uint16_t>(port_));
    if (bind(socket_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ESP_LOGE(kTag, "failed to bind dns port: errno=%d", errno);
        close(socket_fd_);
        socket_fd_ = -1;
        return;
    }

    running_.store(true, std::memory_order_release);
    xTaskCreate(
        [](void* arg) {
            static_cast<DnsServer*>(arg)->Run();
            vTaskDelete(nullptr);
        },
        "quellog_dns",
        4096,
        this,
        5,
        &task_handle_);
}

void DnsServer::Stop() {
    running_.store(false, std::memory_order_release);

    if (socket_fd_ >= 0) {
        shutdown(socket_fd_, SHUT_RDWR);
        close(socket_fd_);
        socket_fd_ = -1;
    }

    if (task_handle_ != nullptr) {
        vTaskDelay(pdMS_TO_TICKS(100));
        task_handle_ = nullptr;
    }
}

void DnsServer::Run() {
    char buffer[512];
    while (running_.load(std::memory_order_acquire)) {
        sockaddr_in client = {};
        socklen_t client_len = sizeof(client);
        const int received = recvfrom(
            socket_fd_,
            buffer,
            sizeof(buffer),
            0,
            reinterpret_cast<sockaddr*>(&client),
            &client_len);
        if (received < 0) {
            if (!running_.load(std::memory_order_acquire)) {
                break;
            }
            ESP_LOGW(kTag, "recvfrom failed: errno=%d", errno);
            continue;
        }

        if (received < 12) {
            continue;
        }

        buffer[2] |= 0x80;
        buffer[3] |= 0x80;
        buffer[7] = 1;

        int length = received;
        static const uint8_t answer_header[] = {
            0xc0, 0x0c,
            0x00, 0x01,
            0x00, 0x01,
            0x00, 0x00, 0x00, 0x1c,
            0x00, 0x04,
        };
        if (length + static_cast<int>(sizeof(answer_header)) + 4 > static_cast<int>(sizeof(buffer))) {
            continue;
        }
        memcpy(buffer + length, answer_header, sizeof(answer_header));
        length += sizeof(answer_header);
        memcpy(buffer + length, &gateway_.addr, 4);
        length += 4;

        sendto(socket_fd_, buffer, length, 0, reinterpret_cast<sockaddr*>(&client), client_len);
    }

    task_handle_ = nullptr;
}
