#ifndef QUELLOG_DISPLAY_H_
#define QUELLOG_DISPLAY_H_

#include <string>
#include <vector>

struct PageModel {
    std::string title;
    std::vector<std::string> lines;
    std::string footer;
};

class Display {
public:
    Display() = default;
    virtual ~Display() = default;

    virtual void RenderPage(const PageModel& model);
    virtual void SetStatus(const char* status);
    virtual void ShowNotification(const char* notification);
    virtual void RequestFullRefresh();
    virtual void RequestPartialRefresh();

    int width() const { return width_; }
    int height() const { return height_; }

protected:
    int width_ = 0;
    int height_ = 0;
};

class NoDisplay : public Display {
};

#endif  // QUELLOG_DISPLAY_H_
