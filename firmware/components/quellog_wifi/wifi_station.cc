#include "wifi_station.h"

#include <esp_log.h>
#include <esp_wifi.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "ssid_manager.h"

namespace {

constexpr char kTag[] = "WifiStation";

std::string FormatIp(esp_ip4_addr_t address) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&address));
    return std::string(buffer);
}

}  // namespace

WifiStation::WifiStation() = default;

WifiStation::~WifiStation() {
    Stop();
}

void WifiStation::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }

    station_netif_ = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiStation::WifiEventHandler, this, &wifi_event_handler_));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiStation::IpEventHandler, this, &got_ip_handler_));

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &WifiStation::ScanTimerCallback;
    timer_args.arg = this;
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.name = "quellog_sta_scan";
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &scan_timer_));

    wifi_config_t wifi_config = {};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    running_ = true;
    connected_ = false;
    connecting_ = false;
    current_ssid_.clear();
    connecting_ssid_.clear();
    ip_address_.clear();
    StartScan();
}

void WifiStation::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) {
        return;
    }

    if (scan_timer_ != nullptr) {
        esp_timer_stop(scan_timer_);
        esp_timer_delete(scan_timer_);
        scan_timer_ = nullptr;
    }
    if (got_ip_handler_ != nullptr) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_handler_);
        got_ip_handler_ = nullptr;
    }
    if (wifi_event_handler_ != nullptr) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_);
        wifi_event_handler_ = nullptr;
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
    if (station_netif_ != nullptr) {
        esp_netif_destroy_default_wifi(station_netif_);
        station_netif_ = nullptr;
    }

    running_ = false;
    connected_ = false;
    connecting_ = false;
    scan_in_progress_ = false;
    current_ssid_.clear();
    connecting_ssid_.clear();
    ip_address_.clear();
}

bool WifiStation::IsConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

std::string WifiStation::GetSsid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_ssid_;
}

std::string WifiStation::GetIpAddress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ip_address_;
}

int WifiStation::GetRssi() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) {
        return 0;
    }
    wifi_ap_record_t ap_info = {};
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK ? ap_info.rssi : 0;
}

int WifiStation::GetChannel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) {
        return 0;
    }
    wifi_ap_record_t ap_info = {};
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK ? static_cast<int>(ap_info.primary) : 0;
}

void WifiStation::SetScanIntervalSeconds(int scan_interval_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    scan_interval_seconds_ = std::max(5, scan_interval_seconds);
}

void WifiStation::OnScanBegin(std::function<void()> on_scan_begin) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_scan_begin_ = std::move(on_scan_begin);
}

void WifiStation::OnConnect(std::function<void(const std::string& ssid)> on_connect) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_connect_ = std::move(on_connect);
}

void WifiStation::OnConnected(std::function<void(const std::string& ssid)> on_connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_connected_ = std::move(on_connected);
}

void WifiStation::OnDisconnected(std::function<void()> on_disconnected) {
    std::lock_guard<std::mutex> lock(mutex_);
    on_disconnected_ = std::move(on_disconnected);
}

void WifiStation::StartScan() {
    if (!running_ || scan_in_progress_ || connecting_) {
        return;
    }

    const std::vector<SsidItem>& credentials = SsidManager::GetInstance().GetSsidList();
    if (credentials.empty()) {
        ScheduleScan();
        return;
    }

    scan_in_progress_ = true;
    if (on_scan_begin_) {
        on_scan_begin_();
    }
    esp_wifi_scan_start(nullptr, false);
}

void WifiStation::ScheduleScan() {
    if (scan_timer_ == nullptr) {
        return;
    }
    esp_timer_stop(scan_timer_);
    esp_timer_start_once(scan_timer_, static_cast<uint64_t>(scan_interval_seconds_) * 1000ULL * 1000ULL);
}

void WifiStation::HandleScanDone() {
    std::function<void(const std::string&)> on_connect_callback;
    std::string target_ssid;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        scan_in_progress_ = false;

        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        std::vector<wifi_ap_record_t> access_points(ap_count);
        if (ap_count > 0) {
            esp_wifi_scan_get_ap_records(&ap_count, access_points.data());
            access_points.resize(ap_count);
        }

        const std::vector<SsidItem>& credentials = SsidManager::GetInstance().GetSsidList();
        const SsidItem* selected = nullptr;
        int best_rssi = INT32_MIN;
        for (const wifi_ap_record_t& access_point : access_points) {
            const char* scanned_ssid = reinterpret_cast<const char*>(access_point.ssid);
            auto match = std::find_if(credentials.begin(), credentials.end(), [scanned_ssid](const SsidItem& credential) {
                return credential.ssid == scanned_ssid;
            });
            if (match != credentials.end() && (selected == nullptr || access_point.rssi > best_rssi)) {
                selected = &(*match);
                best_rssi = access_point.rssi;
            }
        }

        if (selected == nullptr) {
            ScheduleScan();
            return;
        }

        wifi_config_t wifi_config = {};
        strlcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), selected->ssid.c_str(), sizeof(wifi_config.sta.ssid));
        strlcpy(reinterpret_cast<char*>(wifi_config.sta.password), selected->password.c_str(), sizeof(wifi_config.sta.password));
        wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

        connecting_ = true;
        connecting_ssid_ = selected->ssid;
        target_ssid = connecting_ssid_;
        on_connect_callback = on_connect_;
    }

    if (on_connect_callback) {
        on_connect_callback(target_ssid);
    }
    if (esp_wifi_connect() != ESP_OK) {
        HandleDisconnected();
    }
}

void WifiStation::HandleDisconnected() {
    std::function<void()> on_disconnected_callback;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool was_active = connected_ || connecting_;
        connected_ = false;
        connecting_ = false;
        current_ssid_.clear();
        ip_address_.clear();
        scan_in_progress_ = false;
        if (!was_active) {
            ScheduleScan();
            return;
        }
        on_disconnected_callback = on_disconnected_;
        ScheduleScan();
    }

    if (on_disconnected_callback) {
        on_disconnected_callback();
    }
}

void WifiStation::WifiEventHandler(void* arg,
                                   esp_event_base_t event_base,
                                   int32_t event_id,
                                   void* event_data) {
    (void)event_base;
    (void)event_data;
    auto* self = static_cast<WifiStation*>(arg);
    if (event_id == WIFI_EVENT_STA_START) {
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->StartScan();
    } else if (event_id == WIFI_EVENT_SCAN_DONE) {
        self->HandleScanDone();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        self->HandleDisconnected();
    }
}

void WifiStation::IpEventHandler(void* arg,
                                 esp_event_base_t event_base,
                                 int32_t event_id,
                                 void* event_data) {
    (void)event_base;
    (void)event_id;
    auto* self = static_cast<WifiStation*>(arg);
    auto* got_ip = static_cast<ip_event_got_ip_t*>(event_data);

    std::function<void(const std::string&)> on_connected_callback;
    std::string ssid;
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->connected_ = true;
        self->connecting_ = false;
        self->current_ssid_ = self->connecting_ssid_;
        self->ip_address_ = FormatIp(got_ip->ip_info.ip);
        self->scan_in_progress_ = false;
        self->connecting_ssid_.clear();
        ssid = self->current_ssid_;
        on_connected_callback = self->on_connected_;
        if (self->scan_timer_ != nullptr) {
            esp_timer_stop(self->scan_timer_);
        }
    }

    if (on_connected_callback) {
        on_connected_callback(ssid);
    }
}

void WifiStation::ScanTimerCallback(void* arg) {
    auto* self = static_cast<WifiStation*>(arg);
    std::lock_guard<std::mutex> lock(self->mutex_);
    self->StartScan();
}
