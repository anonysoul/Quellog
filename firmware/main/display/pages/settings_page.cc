#include "settings_page.h"

PageModel SettingsPage::BuildModel(const AppContext& context) const {
    PageModel model;
    model.title = "Quellog / Settings";
    model.lines.push_back("Alias  " + context.device_alias);
    model.lines.push_back("Board  " + context.board_type);
    model.lines.push_back("UUID   " + context.device_uuid);
    model.lines.push_back("Refresh " + std::string(context.refresh_policy == RefreshPolicy::Timed ? "Timed" : "Manual"));
    if (context.battery_known) {
        model.lines.push_back("Battery " + std::to_string(context.battery_level) + "%");
    } else {
        model.lines.push_back("Battery unavailable");
    }
    model.footer = "OK toggles refresh mode";
    return model;
}
