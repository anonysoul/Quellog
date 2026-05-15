#include "recent_records_page.h"

#include <cstdio>

namespace {

std::string FormatRecordLine(const RecordSummary& record) {
    char amount[24];
    snprintf(amount, sizeof(amount), "CNY %lld.%02lld",
             static_cast<long long>(record.amount_cents / 100),
             static_cast<long long>(record.amount_cents % 100));
    return record.title + " | " + record.category + " | " + amount;
}

}  // namespace

PageModel RecentRecordsPage::BuildModel(const AppContext& context) const {
    PageModel model;
    model.title = "Quellog / Recent";
    if (context.dashboard.recent_records.empty()) {
        model.lines.push_back("No local records yet.");
    } else {
        for (const RecordSummary& record : context.dashboard.recent_records) {
            model.lines.push_back(FormatRecordLine(record));
        }
    }
    model.footer = "UP/DN page";
    return model;
}
