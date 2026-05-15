#include "settings_page.h"

PageModel SettingsPage::BuildModel(const AppContext& context) const {
    PageModel model;
    model.title = "Quellog / Settings";
    model.text_blocks.push_back({"Alias  " + context.device_alias});
    model.text_blocks.push_back({"Board  " + context.board_type});
    model.text_blocks.push_back({"UUID   " + context.device_uuid});
    model.text_blocks.push_back({"Refresh " + std::string(context.refresh_policy == RefreshPolicy::Timed ? "Timed" : "Manual")});
    if (context.battery_known) {
        model.text_blocks.push_back({"Battery " + std::to_string(context.battery_level) + "%"});
    } else {
        model.text_blocks.push_back({"Battery unavailable"});
    }
    model.footer = "OK toggles refresh mode";
    return model;
}
