#include "sdkconfig.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <array>

#include "board.h"
#include "config.h"
#include "display/display.h"

namespace {

constexpr char kTag[] = "ZectrixBoard";

class PlaceholderEpaperDisplay : public Display {
public:
    PlaceholderEpaperDisplay() {
        width_ = QUELLOG_DISPLAY_WIDTH;
        height_ = QUELLOG_DISPLAY_HEIGHT;
    }

    void BeginPage() override {
        ESP_LOGI(kTag, "[EPD %dx%d] begin page", width_, height_);
    }

    void EndPage() override {
        ESP_LOGI(kTag, "[EPD] end page");
    }

    void DrawText(const Rect& rect, const char* text, TextAlign align) override {
        ESP_LOGI(kTag, "[EPD] text [%d,%d,%d,%d] align=%d %s",
                 rect.x, rect.y, rect.w, rect.h, static_cast<int>(align), text);
    }

    void DrawLine(int x1, int y1, int x2, int y2) override {
        ESP_LOGI(kTag, "[EPD] line (%d,%d)-(%d,%d)", x1, y1, x2, y2);
    }

    void DrawRect(const Rect& rect) override {
        ESP_LOGI(kTag, "[EPD] rect [%d,%d,%d,%d]", rect.x, rect.y, rect.w, rect.h);
    }

    void FillRect(const Rect& rect) override {
        ESP_LOGI(kTag, "[EPD] fill [%d,%d,%d,%d]", rect.x, rect.y, rect.w, rect.h);
    }
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
};

}  // namespace

DECLARE_BOARD(ZectrixBoard);
