#include "stats_page.h"

#include <cstdio>

PageModel StatsPage::BuildModel(const AppContext& context) const {
    PageModel model;
    model.title = "Quellog / Stats";
    if (context.dashboard.categories.empty()) {
        model.lines.push_back("No category summary yet.");
    } else {
        for (const CategorySummary& category : context.dashboard.categories) {
            char line[96];
            snprintf(line, sizeof(line), "%s  %d%%  CNY %lld.%02lld",
                     category.category.c_str(),
                     category.percent,
                     static_cast<long long>(category.amount_cents / 100),
                     static_cast<long long>(category.amount_cents % 100));
            model.lines.push_back(line);
        }
    }
    model.footer = "UP/DN page";
    return model;
}
