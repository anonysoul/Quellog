#include "application.h"

#include "sdkconfig.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>

#include "settings.h"

namespace {

constexpr char kTag[] = "Application";

int64_t SecondsToMicros(int seconds) {
    return static_cast<int64_t>(seconds) * 1000000LL;
}

}  // namespace

Application::Application()
    : board_(Board::GetInstance()) {
}

void Application::Initialize() {
    display_ = board_.GetDisplay();
    pages_ = UiPageRegistry::CreateDefault();
    LoadSettings();
    SeedMockData();
    state_.store(kDeviceStateStarting, std::memory_order_release);
    last_refresh_us_ = esp_timer_get_time();
    RenderCurrentPage(true);
    state_.store(kDeviceStateIdle, std::memory_order_release);
}

void Application::Run() {
    while (true) {
        InputEvent event;
        if (board_.PollInput(event)) {
            HandleInput(event);
        }

        const int64_t now_us = esp_timer_get_time();
        if (ShouldAutoRefresh(now_us)) {
            TriggerRefresh();
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void Application::HandleInput(const InputEvent& event) {
    switch (event.key) {
        case InputKey::Up:
            PreviousPage();
            break;
        case InputKey::Down:
            NextPage();
            break;
        case InputKey::Confirm:
            if (current_page_index_ == pages_.Count() - 1) {
                refresh_policy_ = refresh_policy_ == RefreshPolicy::Manual
                                      ? RefreshPolicy::Timed
                                      : RefreshPolicy::Manual;
                SaveSettings();
                display_->ShowNotification(refresh_policy_ == RefreshPolicy::Timed
                                               ? "Refresh policy: timed"
                                               : "Refresh policy: manual");
                RenderCurrentPage(true);
            } else {
                TriggerRefresh();
            }
            break;
        case InputKey::None:
            break;
    }
}

void Application::RenderCurrentPage(bool full_refresh) {
    const UiPage* page = pages_.Get(current_page_index_);
    if (page == nullptr || display_ == nullptr) {
        state_.store(kDeviceStateError, std::memory_order_release);
        return;
    }

    if (full_refresh) {
        display_->RequestFullRefresh();
    } else {
        display_->RequestPartialRefresh();
    }

    const PageModel model = page->BuildModel(BuildContext());
    display_->RenderPage(model);
}

void Application::NextPage() {
    current_page_index_ = (current_page_index_ + 1) % std::max(1, pages_.Count());
    SaveSettings();
    RenderCurrentPage(false);
}

void Application::PreviousPage() {
    current_page_index_ = (current_page_index_ - 1 + std::max(1, pages_.Count())) % std::max(1, pages_.Count());
    SaveSettings();
    RenderCurrentPage(false);
}

void Application::TriggerRefresh() {
    state_.store(kDeviceStateRefreshing, std::memory_order_release);
    ++refresh_count_;
    dashboard_.sync_status = "local snapshot #" + std::to_string(refresh_count_);
    last_refresh_us_ = esp_timer_get_time();
    RenderCurrentPage(true);
    state_.store(current_page_index_ == pages_.Count() - 1 ? kDeviceStateSettings : kDeviceStateIdle,
                 std::memory_order_release);
}

void Application::LoadSettings() {
    Settings settings("app", true);
    current_page_index_ = settings.GetInt("page_index", 0);
    refresh_policy_ = settings.GetInt("refresh_policy", 0) == 1 ? RefreshPolicy::Timed : RefreshPolicy::Manual;
    device_alias_ = settings.GetString("device_alias", "Quellog E-Ink");
}

void Application::SaveSettings() {
    Settings settings("app", true);
    settings.SetInt("page_index", current_page_index_);
    settings.SetInt("refresh_policy", refresh_policy_ == RefreshPolicy::Timed ? 1 : 0);
    settings.SetString("device_alias", device_alias_);
}

void Application::SeedMockData() {
    dashboard_.today_expense_cents = 4680;
    dashboard_.month_expense_cents = 125430;
    dashboard_.budget_used_percent = 63;
    dashboard_.sync_status = "local placeholder";
    dashboard_.recent_records = {
        {"Lunch", "Food", 3200},
        {"Metro Top-up", "Transport", 2000},
        {"Coffee Beans", "Groceries", 6480},
    };
    dashboard_.categories = {
        {"Food", 34, 42500},
        {"Groceries", 27, 33800},
        {"Transport", 12, 15400},
        {"Home", 11, 13900},
    };
}

AppContext Application::BuildContext() const {
    AppContext context;
    context.device_state = state_.load(std::memory_order_acquire);
    context.page_index = current_page_index_;
    context.page_count = pages_.Count();
    context.refresh_policy = refresh_policy_;
    context.device_alias = device_alias_;
    context.board_type = board_.GetBoardType();
    context.device_uuid = board_.GetUuid();
    context.last_refresh_label = BuildRefreshLabel();
    context.dashboard = dashboard_;

    int battery_level = 0;
    context.battery_known = board_.GetBatteryLevel(battery_level);
    context.battery_level = battery_level;

    const UiPage* page = pages_.Get(current_page_index_);
    context.page_title = page == nullptr ? "" : page->GetTitle();
    return context;
}

std::string Application::BuildRefreshLabel() const {
    const int64_t age_sec = (esp_timer_get_time() - last_refresh_us_) / 1000000LL;
    return std::to_string(age_sec) + "s ago";
}

bool Application::ShouldAutoRefresh(int64_t now_us) const {
    if (refresh_policy_ != RefreshPolicy::Timed) {
        return false;
    }
    return (now_us - last_refresh_us_) >= SecondsToMicros(CONFIG_QUELLOG_AUTO_REFRESH_SECONDS);
}
