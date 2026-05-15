#ifndef QUELLOG_WIFI_SSID_MANAGER_H_
#define QUELLOG_WIFI_SSID_MANAGER_H_

#include <string>
#include <vector>

struct SsidItem {
    std::string ssid;
    std::string password;
};

class SsidManager {
public:
    static SsidManager& GetInstance();

    void AddSsid(const std::string& ssid, const std::string& password);
    void RemoveSsid(const std::string& ssid);
    void Clear();
    const std::vector<SsidItem>& GetSsidList() const { return ssid_list_; }

private:
    SsidManager();

    void LoadFromNvs();
    void SaveToNvs();

    std::vector<SsidItem> ssid_list_;
};

#endif  // QUELLOG_WIFI_SSID_MANAGER_H_
