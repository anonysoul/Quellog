#include "settings_page.h"

PageModel SettingsPage::BuildModel(const AppContext& context) const {
    PageModel model;
    model.title = "泉流迹 / 设置";
    model.text_blocks.push_back({"设备别名  " + context.device_alias});
    model.text_blocks.push_back({"板型  " + context.board_type});
    model.text_blocks.push_back({"设备编号  " + context.device_uuid});
    model.text_blocks.push_back({"刷新模式  " + std::string(context.refresh_policy == RefreshPolicy::Timed ? "定时" : "手动")});
    if (context.battery_known) {
        model.text_blocks.push_back({"电量  " + std::to_string(context.battery_level) + "%"});
    } else {
        model.text_blocks.push_back({"电量暂不可用"});
    }
    model.footer = "确认切换刷新模式";
    return model;
}
