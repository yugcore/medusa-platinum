#pragma once
#include <medusa/medusa_types.hpp>
#include <medusa/medusa_graphics.hpp>
#include <medusa/scripting/medusa_python_engine.hpp>
#include <string>
#include <vector>

namespace medusa {

class DCCPythonConsoleWidget {
public:
    DCCPythonConsoleWidget();

    void layout(const Rect& bounds);
    void draw(Graphics& gfx);

    bool handle_mouse_down(const MouseEvent& e);
    bool handle_mouse_move(const MouseEvent& e);
    bool handle_mouse_up(const MouseEvent& e);
    bool handle_mouse_wheel(float delta, Point p);
    bool handle_key_down(int key, bool ctrl, bool shift);
    bool handle_char(uint32_t codepoint);

    void scroll_to_bottom();
    void clear();

    const Rect& get_bounds() const { return bounds_; }

private:
    Rect bounds_{};
    Rect toolbar_rect_{};
    Rect log_rect_{};
    Rect input_rect_{};

    Rect clear_btn_rect_{};
    Rect run_btn_rect_{};
    Rect copy_btn_rect_{};

    int hovered_btn_ = -1;

    std::string input_buffer_;
    size_t cursor_pos_ = 0;
    int history_index_ = -1;
    float log_scroll_ = 0.0f;
    bool auto_scroll_ = true;

    std::chrono::steady_clock::time_point last_blink_{};
    bool cursor_visible_ = true;
};

} // namespace medusa
