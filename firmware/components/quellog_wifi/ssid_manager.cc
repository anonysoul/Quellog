#include "ssid_manager.h"

#include <esp_log.h>
#include <nvs.h>

#include <algorithm>

namespace {

constexpr char kTag[] = "SsidManager";
constexpr char kNamespace[] = "wifi";
constexpr int kMaxWifiSsidCount = 5;

std::string MakeIndexedKey(const char* base, int index) {
    return index == 0 ? std::string(base) : std::string(base) + std::to_string(index);
}

}  // namespace

SsidManager& SsidManager::GetInstance() {
    static SsidManager instance;
    return instance;
}

SsidManager::SsidManager() {
    LoadFromNvs();
}

void SsidManager::AddSsid(const std::string& ssid, const std::string& password) {
    if (ssid.empty()) {
        return;
    }

    auto existing = std::find_if(ssid_list_.begin(), ssid_list_.end(), [&ssid](const SsidItem& item) {
        return item.ssid == ssid;
    });
    if (existing != ssid_list_.end()) {
        existing->password = password;
        SsidItem updated = *existing;
        ssid_list_.erase(existing);
        ssid_list_.insert(ssid_list_.begin(), updated);
    } else {
        ssid_list_.insert(ssid_list_.begin(), {ssid, password});
        if (static_cast<int>(ssid_list_.size()) > kMaxWifiSsidCount) {
            ssid_list_.resize(kMaxWifiSsidCount);
        }
    }
    SaveToNvs();
}

void SsidManager::RemoveSsid(const std::string& ssid) {
    if (ssid.empty()) {
        return;
    }

    auto existing = std::find_if(ssid_list_.begin(), ssid_list_.end(), [&ssid](const SsidItem& item) {
        return item.ssid == ssid;
    });
    if (existing == ssid_list_.end()) {
        return;
    }

    ssid_list_.erase(existing);
    SaveToNvs();
}

void SsidManager::Clear() {
    ssid_list_.clear();
    SaveToNvs();
}

void SsidManager::LoadFromNvs() {
    ssid_list_.clear();

    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGI(kTag, "no saved wifi credentials");
        return;
    }

    for (int i = 0; i < kMaxWifiSsidCount; ++i) {
        const std::string ssid_key = MakeIndexedKey("ssid", i);
        const std::string password_key = MakeIndexedKey("password", i);

        char ssid[33] = {};
        char password[65] = {};
        size_t ssid_length = sizeof(ssid);
        size_t password_length = sizeof(password);
        if (nvs_get_str(handle, ssid_key.c_str(), ssid, &ssid_length) != ESP_OK) {
            continue;
        }
        if (nvs_get_str(handle, password_key.c_str(), password, &password_length) != ESP_OK) {
            password[0] = '\0';
        }
        ssid_list_.push_back({ssid, password});
    }

    nvs_close(handle);
}

void SsidManager::SaveToNvs() {
    nvs_handle_t handle = 0;
    ESP_ERROR_CHECK(nvs_open(kNamespace, NVS_READWRITE, &handle));

    for (int i = 0; i < kMaxWifiSsidCount; ++i) {
        const std::string ssid_key = MakeIndexedKey("ssid", i);
        const std::string password_key = MakeIndexedKey("password", i);
        if (i < static_cast<int>(ssid_list_.size())) {
            ESP_ERROR_CHECK(nvs_set_str(handle, ssid_key.c_str(), ssid_list_[i].ssid.c_str()));
            ESP_ERROR_CHECK(nvs_set_str(handle, password_key.c_str(), ssid_list_[i].password.c_str()));
        } else {
            nvs_erase_key(handle, ssid_key.c_str());
            nvs_erase_key(handle, password_key.c_str());
        }
    }

    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}
