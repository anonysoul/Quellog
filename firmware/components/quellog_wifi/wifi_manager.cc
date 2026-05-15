#include "wifi_manager.h"

#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>

#include <utility>

#include "wifi_configuration_ap.h"
#include "wifi_station.h"

namespace {

constexpr char kTag[] = "WifiManager";

}  // namespace

WifiManager& WifiManager::GetInstance() {
    static WifiManager instance;
    return instance;
}

WifiManager::~WifiManager() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (station_active_ && station_) {
        station_->Stop();
    }
    if (config_mode_active_ && config_ap_) {
        config_ap_->Stop();
    }
    if (initialized_) {
        esp_wifi_deinit();
    }
}

bool WifiManager::Initialize(const WifiManagerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        return true;
    }

    config_ = config;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "nvs init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "netif init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(kTag, "event loop init failed: %s", esp_err_to_name(ret));
        return false;
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config.nvs_enable = false;
    ret = esp_wifi_init(&wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(kTag, "wifi init failed: %s", esp_err_to_name(ret));
        return false;
    }

    station_ = std::make_unique<WifiStation>();
    config_ap_ = std::make_unique<WifiConfigurationAp>();
    initialized_ = true;
    return true;
}

bool WifiManager::IsInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

void WifiManager::StartStation() {
    bool notify_config_exit = false;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!initialized_ || station_active_ || station_ == nullptr) {
            return;
        }
        if (config_mode_active_ && config_ap_ != nullptr) {
            config_ap_->Stop();
            config_mode_active_ = false;
            notify_config_exit = true;
        }

        station_->SetScanIntervalSeconds(config_.station_scan_interval_seconds);
        station_->OnScanBegin([this]() { NotifyEvent(WifiEvent::Scanning); });
        station_->OnConnect([this](const std::string&) { NotifyEvent(WifiEvent::Connecting); });
        station_->OnConnected([this](const std::string&) { NotifyEvent(WifiEvent::Connected); });
        station_->OnDisconnected([this]() { NotifyEvent(WifiEvent::Disconnected); });
        station_->Start();
        station_active_ = true;
    }

    if (notify_config_exit) {
        NotifyEvent(WifiEvent::ConfigModeExit);
    }
}

void WifiManager::StopStation() {
    bool notify_disconnected = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!station_active_ || station_ == nullptr) {
            return;
        }
        station_->Stop();
        station_active_ = false;
        notify_disconnected = true;
    }

    if (notify_disconnected) {
        NotifyEvent(WifiEvent::Disconnected);
    }
}

bool WifiManager::IsConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return station_active_ && station_ != nullptr && station_->IsConnected();
}

std::string WifiManager::GetSsid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return station_ != nullptr ? station_->GetSsid() : "";
}

std::string WifiManager::GetIpAddress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return station_ != nullptr ? station_->GetIpAddress() : "";
}

int WifiManager::GetRssi() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return station_ != nullptr ? station_->GetRssi() : 0;
}

int WifiManager::GetChannel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return station_ != nullptr ? station_->GetChannel() : 0;
}

void WifiManager::StartConfigAp() {
    bool notify_disconnected = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ || config_mode_active_ || config_ap_ == nullptr) {
            return;
        }
        if (station_active_ && station_ != nullptr) {
            station_->Stop();
            station_active_ = false;
            notify_disconnected = true;
        }

        config_ap_->SetSsidPrefix(config_.ssid_prefix);
        config_ap_->SetPassword(config_.ap_password);
        config_ap_->SetLanguage(config_.language);
        config_ap_->OnExitRequested([this]() { HandleConfigExitRequested(); });
        config_ap_->Start();
        config_mode_active_ = true;
    }

    if (notify_disconnected) {
        NotifyEvent(WifiEvent::Disconnected);
    }
    NotifyEvent(WifiEvent::ConfigModeEnter);
}

void WifiManager::StopConfigAp() {
    bool notify_exit = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!config_mode_active_ || config_ap_ == nullptr) {
            return;
        }
        config_ap_->Stop();
        config_mode_active_ = false;
        notify_exit = true;
    }

    if (notify_exit) {
        NotifyEvent(WifiEvent::ConfigModeExit);
    }
}

bool WifiManager::IsConfigMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_mode_active_;
}

std::string WifiManager::GetApSsid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_ap_ != nullptr ? config_ap_->GetSsid() : "";
}

std::string WifiManager::GetApPassword() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_ap_ != nullptr ? config_ap_->GetPassword() : "";
}

std::string WifiManager::GetApWebUrl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_ap_ != nullptr ? config_ap_->GetWebServerUrl() : "";
}

void WifiManager::SetEventCallback(std::function<void(WifiEvent)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    event_callback_ = std::move(callback);
}

void WifiManager::NotifyEvent(WifiEvent event) {
    std::function<void(WifiEvent)> callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = event_callback_;
    }
    if (callback) {
        callback(event);
    }
}

void WifiManager::HandleConfigExitRequested() {
    StopConfigAp();
    StartStation();
}
