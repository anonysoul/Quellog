#ifndef QUELLOG_WIFI_MANAGER_H_
#define QUELLOG_WIFI_MANAGER_H_

#include <functional>
#include <memory>
#include <mutex>
#include <string>

class WifiConfigurationAp;
class WifiStation;

enum class WifiEvent {
    Scanning,
    Connecting,
    Connected,
    Disconnected,
    ConfigModeEnter,
    ConfigModeExit,
};

struct WifiManagerConfig {
    std::string ssid_prefix = "Quellog";
    std::string ap_password;
    std::string language = "zh-CN";
    int station_scan_interval_seconds = 15;
};

class WifiManager {
public:
    static WifiManager& GetInstance();

    bool Initialize(const WifiManagerConfig& config = WifiManagerConfig{});
    bool IsInitialized() const;

    void StartStation();
    void StopStation();
    bool IsConnected() const;
    std::string GetSsid() const;
    std::string GetIpAddress() const;
    int GetRssi() const;
    int GetChannel() const;

    void StartConfigAp();
    void StopConfigAp();
    bool IsConfigMode() const;
    std::string GetApSsid() const;
    std::string GetApPassword() const;
    std::string GetApWebUrl() const;

    void SetEventCallback(std::function<void(WifiEvent)> callback);

    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

private:
    WifiManager() = default;
    ~WifiManager();

    void NotifyEvent(WifiEvent event);
    void HandleConfigExitRequested();

    WifiManagerConfig config_;
    std::unique_ptr<WifiStation> station_;
    std::unique_ptr<WifiConfigurationAp> config_ap_;

    mutable std::mutex mutex_;
    bool initialized_ = false;
    bool station_active_ = false;
    bool config_mode_active_ = false;
    std::function<void(WifiEvent)> event_callback_;
};

#endif  // QUELLOG_WIFI_MANAGER_H_
