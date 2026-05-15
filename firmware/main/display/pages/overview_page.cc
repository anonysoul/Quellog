#include "overview_page.h"

namespace {

std::string FormatAmount(int64_t cents) {
    const long long whole = static_cast<long long>(cents / 100);
    const long long fraction = static_cast<long long>(cents % 100);
    const long long abs_fraction = fraction < 0 ? -fraction : fraction;
    return "CNY " + std::to_string(whole) + "." + (abs_fraction < 10 ? "0" : "") + std::to_string(abs_fraction);
}

}  // namespace

PageModel OverviewPage::BuildModel(const AppContext& context) const {
    PageModel model;
    model.title = "Quellog E-Ink / Overview";
    model.text_blocks.push_back({"Today  " + FormatAmount(context.dashboard.today_expense_cents)});
    model.text_blocks.push_back({"Month  " + FormatAmount(context.dashboard.month_expense_cents)});
    model.text_blocks.push_back({"Budget " + std::to_string(context.dashboard.budget_used_percent) + "% used"});
    model.text_blocks.push_back({"Sync   " + context.dashboard.sync_status});
    model.text_blocks.push_back({"Seen   " + context.last_refresh_label});
    model.footer = "UP/DN page  OK refresh";
    return model;
}
