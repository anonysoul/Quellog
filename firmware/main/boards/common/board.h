#ifndef QUELLOG_BOARD_H_
#define QUELLOG_BOARD_H_

#include <string>

class Display;

enum class InputKey {
    None = 0,
    Up,
    Down,
    Confirm
};

struct InputEvent {
    InputKey key = InputKey::None;
};

void* create_board();

class Board {
public:
    static Board& GetInstance() {
        static Board* instance = static_cast<Board*>(create_board());
        return *instance;
    }

    Board(const Board&) = delete;
    Board& operator=(const Board&) = delete;
    virtual ~Board() = default;

    virtual std::string GetBoardType() = 0;
    virtual std::string GetUuid() const { return uuid_; }
    virtual Display* GetDisplay();
    virtual bool PollInput(InputEvent& event) = 0;
    virtual bool GetBatteryLevel(int& level) { (void)level; return false; }
    virtual std::string GetSystemInfoJson();

protected:
    Board();
    std::string GenerateUuid();

private:
    std::string uuid_;
};

#define DECLARE_BOARD(BOARD_CLASS_NAME) \
void* create_board() { \
    return new BOARD_CLASS_NAME(); \
}

#endif  // QUELLOG_BOARD_H_
