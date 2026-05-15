#ifndef QUELLOG_OVERVIEW_PAGE_H_
#define QUELLOG_OVERVIEW_PAGE_H_

#include "display/ui_page.h"

class OverviewPage : public UiPage {
public:
    const char* GetId() const override { return "overview"; }
    const char* GetTitle() const override { return "Overview"; }
    PageModel BuildModel(const AppContext& context) const override;
};

#endif  // QUELLOG_OVERVIEW_PAGE_H_
