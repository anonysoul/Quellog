#include "display.h"

#include <esp_log.h>

namespace {

constexpr char kTag[] = "Display";

}  // namespace

void Display::RenderPage(const PageModel& model) {
    ESP_LOGI(kTag, "render page: %s", model.title.c_str());
    for (const std::string& line : model.lines) {
        ESP_LOGI(kTag, "  %s", line.c_str());
    }
    if (!model.footer.empty()) {
        ESP_LOGI(kTag, "footer: %s", model.footer.c_str());
    }
}

void Display::SetStatus(const char* status) {
    ESP_LOGI(kTag, "status: %s", status);
}

void Display::ShowNotification(const char* notification) {
    ESP_LOGI(kTag, "notice: %s", notification);
}

void Display::RequestFullRefresh() {
    ESP_LOGI(kTag, "full refresh requested");
}

void Display::RequestPartialRefresh() {
    ESP_LOGI(kTag, "partial refresh requested");
}
