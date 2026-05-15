#ifndef QUELLOG_WIFI_STATION_H_
#define QUELLOG_WIFI_STATION_H_

#include <functional>
#include <mutex>
#include <string>

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

class WifiStation {
public:
    WifiStation();
    ~WifiStation();

    WifiStation(const WifiStation&) = delete;
    WifiStation& operator=(const WifiStation&) = delete;

    void Start();
    void Stop();
    bool IsConnected() const;
    std::string GetSsid() const;
    std::string GetIpAddress() const;
    int GetRssi() const;
    int GetChannel() const;
    void SetScanIntervalSeconds(int scan_interval_seconds);

    void OnScanBegin(std::function<void()> on_scan_begin);
    void OnConnect(std::function<void(const std::string& ssid)> on_connect);
    void OnConnected(std::function<void(const std::string& ssid)> on_connected);
    void OnDisconnected(std::function<void()> on_disconnected);

private:
    static void WifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void IpEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void ScanTimerCallback(void* arg);

    void StartScan();
    void ScheduleScan();
    void HandleScanDone();
    void HandleDisconnected();

    mutable std::mutex mutex_;
    esp_netif_t* station_netif_ = nullptr;
    esp_event_handler_instance_t wifi_event_handler_ = nullptr;
    esp_event_handler_instance_t got_ip_handler_ = nullptr;
    esp_timer_handle_t scan_timer_ = nullptr;
    std::string current_ssid_;
    std::string connecting_ssid_;
    std::string ip_address_;
    bool running_ = false;
    bool connected_ = false;
    bool connecting_ = false;
    bool scan_in_progress_ = false;
    int scan_interval_seconds_ = 15;
    std::function<void()> on_scan_begin_;
    std::function<void(const std::string& ssid)> on_connect_;
    std::function<void(const std::string& ssid)> on_connected_;
    std::function<void()> on_disconnected_;
};

#endif  // QUELLOG_WIFI_STATION_H_
