#include "overview_page.h"

#include <cstdio>

namespace {

std::string FormatAmount(int64_t cents) {
    char buffer[32];
    const long long whole = static_cast<long long>(cents / 100);
    const long long fraction = static_cast<long long>(cents % 100);
    snprintf(buffer, sizeof(buffer), "CNY %lld.%02lld", whole, fraction < 0 ? -fraction : fraction);
    return std::string(buffer);
}

}  // namespace

PageModel OverviewPage::BuildModel(const AppContext& context) const {
    PageModel model;
    model.title = "Quellog / Overview";
    model.lines.push_back("Today  " + FormatAmount(context.dashboard.today_expense_cents));
    model.lines.push_back("Month  " + FormatAmount(context.dashboard.month_expense_cents));
    model.lines.push_back("Budget " + std::to_string(context.dashboard.budget_used_percent) + "% used");
    model.lines.push_back("Sync   " + context.dashboard.sync_status);
    model.lines.push_back("Seen   " + context.last_refresh_label);
    model.footer = "UP/DN page  OK refresh";
    return model;
}
