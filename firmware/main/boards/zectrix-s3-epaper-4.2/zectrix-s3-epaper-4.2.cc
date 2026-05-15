#include "sdkconfig.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <vector>

#include "board.h"
#include "config.h"
#include "display/display.h"
#include "ssid_manager.h"
#include "wifi_manager.h"

namespace {

constexpr char kTag[] = "ZectrixBoard";

class PlaceholderEpaperDisplay : public Display {
public:
    PlaceholderEpaperDisplay() {
        width_ = QUELLOG_DISPLAY_WIDTH;
        height_ = QUELLOG_DISPLAY_HEIGHT;
        framebuffer_.assign(static_cast<size_t>(width_ * height_), 0);
    }

    void BeginPage() override {
        ESP_LOGI(kTag, "[EPD %dx%d] begin page", width_, height_);
        Clear(true);
    }

    void EndPage() override {
        size_t black_pixels = 0;
        for (uint8_t pixel : framebuffer_) {
            black_pixels += pixel == 0 ? 1U : 0U;
        }
        ESP_LOGI(kTag, "[EPD] end page, black_pixels=%lu", static_cast<unsigned long>(black_pixels));
    }

    void Clear(bool white = true) override {
        std::fill(framebuffer_.begin(), framebuffer_.end(), white ? 1U : 0U);
    }

    void SetPixel(int x, int y, bool black) override {
        if (x < 0 || y < 0 || x >= width_ || y >= height_) {
            return;
        }
        framebuffer_[static_cast<size_t>(y * width_ + x)] = black ? 0U : 1U;
    }

private:
    std::vector<uint8_t> framebuffer_;
};

struct ButtonState {
    gpio_num_t gpio = GPIO_NUM_NC;
    InputKey key = InputKey::None;
    int level = 1;
    int64_t last_change_us = 0;
};

class ZectrixBoard : public Board {
public:
    ZectrixBoard() {
        ConfigureButton(buttons_[0], static_cast<gpio_num_t>(CONFIG_QUELLOG_BUTTON_UP_GPIO), InputKey::Up);
        ConfigureButton(buttons_[1], static_cast<gpio_num_t>(CONFIG_QUELLOG_BUTTON_DOWN_GPIO), InputKey::Down);
        ConfigureButton(buttons_[2], static_cast<gpio_num_t>(CONFIG_QUELLOG_BUTTON_CONFIRM_GPIO), InputKey::Confirm);
    }

    std::string GetBoardType() override {
        return "zectrix-s3-epaper-4.2";
    }

    Display* GetDisplay() override {
        return &display_;
    }

    void StartNetwork() override {
        if (network_started_) {
            return;
        }

        WifiManagerConfig config;
        config.ssid_prefix = "Quellog";
        config.ap_password = "";
        config.language = "zh-CN";
        config.station_scan_interval_seconds = 15;
        if (!WifiManager::GetInstance().Initialize(config)) {
            ESP_LOGE(kTag, "wifi manager init failed");
            network_state_.store(NetworkState::Disconnected, std::memory_order_release);
            return;
        }

        WifiManager::GetInstance().SetEventCallback([this](WifiEvent event) {
            if (!network_event_callback_) {
                switch (event) {
                    case WifiEvent::Scanning:
                        network_state_.store(NetworkState::Scanning, std::memory_order_release);
                        return;
                    case WifiEvent::Connecting:
                        network_state_.store(NetworkState::Connecting, std::memory_order_release);
                        return;
                    case WifiEvent::Connected:
                        network_state_.store(NetworkState::Connected, std::memory_order_release);
                        return;
                    case WifiEvent::Disconnected:
                        network_state_.store(NetworkState::Disconnected, std::memory_order_release);
                        return;
                    case WifiEvent::ConfigModeEnter:
                        network_state_.store(NetworkState::ConfigMode, std::memory_order_release);
                        return;
                    case WifiEvent::ConfigModeExit:
                        network_state_.store(NetworkState::Disconnected, std::memory_order_release);
                        return;
                }
            }

            switch (event) {
                case WifiEvent::Scanning:
                    network_state_.store(NetworkState::Scanning, std::memory_order_release);
                    network_event_callback_(NetworkEvent::Scanning, "");
                    break;
                case WifiEvent::Connecting:
                    network_state_.store(NetworkState::Connecting, std::memory_order_release);
                    network_event_callback_(NetworkEvent::Connecting, WifiManager::GetInstance().GetSsid());
                    break;
                case WifiEvent::Connected:
                    network_state_.store(NetworkState::Connected, std::memory_order_release);
                    network_event_callback_(NetworkEvent::Connected, WifiManager::GetInstance().GetIpAddress());
                    break;
                case WifiEvent::Disconnected:
                    network_state_.store(NetworkState::Disconnected, std::memory_order_release);
                    network_event_callback_(NetworkEvent::Disconnected, "");
                    break;
                case WifiEvent::ConfigModeEnter:
                    network_state_.store(NetworkState::ConfigMode, std::memory_order_release);
                    network_event_callback_(
                        NetworkEvent::WifiConfigModeEnter,
                        WifiManager::GetInstance().GetApSsid() + " " + WifiManager::GetInstance().GetApWebUrl());
                    break;
                case WifiEvent::ConfigModeExit:
                    network_state_.store(NetworkState::Disconnected, std::memory_order_release);
                    network_event_callback_(NetworkEvent::WifiConfigModeExit, "");
                    break;
            }
        });

        if (SsidManager::GetInstance().GetSsidList().empty()) {
            network_state_.store(NetworkState::ConfigMode, std::memory_order_release);
            WifiManager::GetInstance().StartConfigAp();
        } else {
            network_state_.store(NetworkState::Connecting, std::memory_order_release);
            WifiManager::GetInstance().StartStation();
        }
        network_started_ = true;
    }

    bool PollInput(InputEvent& event) override {
        const int64_t now_us = esp_timer_get_time();
        for (ButtonState& button : buttons_) {
            if (button.gpio == GPIO_NUM_NC) {
                continue;
            }

            const int level = gpio_get_level(button.gpio);
            if (level != button.level) {
                button.level = level;
                button.last_change_us = now_us;
                continue;
            }

            if (level == 0 &&
                button.last_change_us != 0 &&
                now_us - button.last_change_us >= static_cast<int64_t>(CONFIG_QUELLOG_INPUT_DEBOUNCE_MS) * 1000LL) {
                button.last_change_us = 0;
                event.key = button.key;
                return true;
            }
        }
        return false;
    }

    void EnterWifiConfigMode() override {
        if (!network_started_) {
            StartNetwork();
            return;
        }
        WifiManager::GetInstance().StartConfigAp();
    }

    bool IsWifiConnected() const override {
        return WifiManager::GetInstance().IsConnected();
    }

    bool IsWifiConfigMode() const override {
        return WifiManager::GetInstance().IsConfigMode();
    }

    NetworkState GetNetworkState() const override {
        return network_state_.load(std::memory_order_acquire);
    }

    std::string GetWifiSsid() const override {
        return WifiManager::GetInstance().GetSsid();
    }

    std::string GetWifiIpAddress() const override {
        return WifiManager::GetInstance().GetIpAddress();
    }

    std::string GetWifiConfigApSsid() const override {
        return WifiManager::GetInstance().GetApSsid();
    }

    std::string GetWifiConfigApUrl() const override {
        return WifiManager::GetInstance().GetApWebUrl();
    }

    void SetNetworkEventCallback(NetworkEventCallback callback) override {
        network_event_callback_ = std::move(callback);
    }

private:
    void ConfigureButton(ButtonState& state, gpio_num_t gpio, InputKey key) {
        state.gpio = gpio;
        state.key = key;
        if (gpio == GPIO_NUM_NC) {
            return;
        }

        gpio_config_t cfg = {};
        cfg.pin_bit_mask = 1ULL << gpio;
        cfg.mode = GPIO_MODE_INPUT;
        cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type = GPIO_INTR_DISABLE;
        ESP_ERROR_CHECK(gpio_config(&cfg));
        state.level = gpio_get_level(gpio);
        ESP_LOGI(kTag, "button gpio=%d mapped", static_cast<int>(gpio));
    }

    PlaceholderEpaperDisplay display_;
    std::array<ButtonState, 3> buttons_ = {};
    std::atomic<NetworkState> network_state_{NetworkState::Unknown};
    NetworkEventCallback network_event_callback_;
    bool network_started_ = false;
};

}  // namespace

DECLARE_BOARD(ZectrixBoard);
