#include <medusa/editor/drawers/medusa_python_console.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <medusa/ui/medusa_icons.hpp>
#include <algorithm>
#include <cmath>
#include <windows.h>

namespace medusa {

DCCPythonConsoleWidget::DCCPythonConsoleWidget() {
    last_blink_ = std::chrono::steady_clock::now();
}

void DCCPythonConsoleWidget::layout(const Rect& bounds) {
    bounds_ = bounds;
    toolbar_rect_ = Rect(bounds.x, bounds.y, bounds.width, 28.0f);
    input_rect_ = Rect(bounds.x, bounds.bottom() - 28.0f, bounds.width, 28.0f);
    log_rect_ = Rect(bounds.x, toolbar_rect_.bottom(), bounds.width,
                     std::max(0.0f, input_rect_.y - toolbar_rect_.bottom()));

    // Right toolbar buttons
    const float btn_w = 88.0f;
    const float btn_h = 22.0f;
    const float btn_y = toolbar_rect_.y + 3.0f;
    copy_btn_rect_ = Rect(toolbar_rect_.right() - btn_w - 8.0f, btn_y, btn_w, btn_h);
    run_btn_rect_ = Rect(copy_btn_rect_.x - btn_w - 6.0f, btn_y, btn_w, btn_h);
    clear_btn_rect_ = Rect(run_btn_rect_.x - 68.0f - 6.0f, btn_y, 68.0f, btn_h);
}

void DCCPythonConsoleWidget::draw(Graphics& gfx) {
    if (bounds_.width <= 0.0f || bounds_.height <= 0.0f) return;

    // 1. Terminal Background (Obsidian chassis)
    gfx.fill_rect(bounds_, Color::from_hex(0x131417));

    // 2. Toolbar Header
    gfx.fill_rect(toolbar_rect_, Color::from_hex(0x1A1B20));
    gfx.draw_line(Point(toolbar_rect_.x, toolbar_rect_.bottom()),
                  Point(toolbar_rect_.right(), toolbar_rect_.bottom()),
                  Color::from_hex(0x2A2C34), 1.0f);

    // Python Status Badge
    const Rect badge_r(toolbar_rect_.x + 8.0f, toolbar_rect_.y + 4.0f, 290.0f, 20.0f);
    gfx.fill_rounded_rect(badge_r, 3.0f, Color::from_hex(0x21232B));
    gfx.draw_rounded_rect(badge_r, 3.0f, Color::from_hex(0x353844), 1.0f);
    Point dot_center(badge_r.x + 10.0f, badge_r.y + 10.0f);
    gfx.fill_rounded_rect(Rect(dot_center.x - 3.0f, dot_center.y - 3.0f, 6.0f, 6.0f), 3.0f, Color::from_hex(0x98C379)); // green status dot
    gfx.draw_text("Python 3.14 · Medusa Platinum Core API",
                  Rect(badge_r.x + 20.0f, badge_r.y, badge_r.width - 24.0f, badge_r.height),
                  Color::from_hex(0x61AFEF), 9.5f, true, false, true, Theme::kUiFontFamily);

    // Toolbar action buttons
    auto draw_btn = [&](const Rect& r, const std::string& label, const std::string& icon, int id) {
        bool hov = (hovered_btn_ == id);
        gfx.fill_rounded_rect(r, 2.0f, hov ? Color::from_hex(0x2B2E38) : Color::from_hex(0x21232B));
        gfx.draw_rounded_rect(r, 2.0f, hov ? Color::from_hex(0x4C5161) : Color::from_hex(0x323540), 1.0f);
        if (!icon.empty()) {
            IconRenderer::draw_icon(gfx, icon, Rect(r.x + 6.0f, r.y + (r.height - 12.0f) * 0.5f, 12.0f, 12.0f),
                                    hov ? Color::from_hex(0xFFFFFF) : Theme::kTextSecondary);
        }
        const float text_x = icon.empty() ? r.x : r.x + 22.0f;
        gfx.draw_text(label, Rect(text_x, r.y, r.right() - text_x, r.height),
                      hov ? Color::from_hex(0xFFFFFF) : Theme::kTextSecondary,
                      9.5f, false, true, true, Theme::kUiFontFamily);
    };

    draw_btn(clear_btn_rect_, "Clear", "edit_undo", 0);
    draw_btn(run_btn_rect_, "Run Script...", "file_open", 1);
    draw_btn(copy_btn_rect_, "Copy Log", "file_save", 2);

    // 3. Terminal Log Output Lines
    gfx.push_clip(log_rect_);
    const auto& logs = scripting::PythonEngine::instance().get_logs();
    const float line_h = 18.0f;
    const float total_content_h = static_cast<float>(logs.size()) * line_h + 12.0f;

    if (auto_scroll_) {
        float max_scroll = std::max(0.0f, total_content_h - log_rect_.height);
        log_scroll_ = max_scroll;
    }

    float y = log_rect_.y + 6.0f - log_scroll_;
    for (const auto& entry : logs) {
        if (y + line_h >= log_rect_.y && y <= log_rect_.bottom()) {
            Color col = Theme::kTextPrimary;
            bool bold = false;
            switch (entry.level) {
                case scripting::LogLevel::Prompt:
                    col = Color::from_hex(0x56B6C2); // Cyan
                    bold = true;
                    break;
                case scripting::LogLevel::Command:
                    col = Color::from_hex(0xECEFF4); // Crisp white
                    break;
                case scripting::LogLevel::Output:
                    col = Color::from_hex(0xABB2BF); // Silver slate
                    break;
                case scripting::LogLevel::Result:
                    col = Color::from_hex(0xE5C07B); // Gold
                    break;
                case scripting::LogLevel::Error:
                    col = Color::from_hex(0xE06C75); // Coral crimson
                    bold = true;
                    break;
                case scripting::LogLevel::Notice:
                    col = Color::from_hex(0xC678DD); // Violet
                    break;
            }
            gfx.draw_text(entry.text, Rect(log_rect_.x + 12.0f, y, log_rect_.width - 24.0f, line_h),
                          col, 9.5f, bold, false, true, Theme::kNumericFontFamily);
        }
        y += line_h;
    }

    // Scrollbar if content overflows
    if (total_content_h > log_rect_.height) {
        const float track_h = log_rect_.height;
        const float thumb_h = std::max(20.0f, track_h * track_h / total_content_h);
        const float max_scroll = total_content_h - track_h;
        const float thumb_y = log_rect_.y + (track_h - thumb_h) * (log_scroll_ / std::max(1.0f, max_scroll));
        const Rect track(log_rect_.right() - 5.0f, log_rect_.y, 4.0f, track_h);
        gfx.fill_rect(track, Color(0.08f, 0.09f, 0.11f, 0.6f));
        gfx.fill_rounded_rect(Rect(track.x, thumb_y, track.width, thumb_h), 2.0f, Color::from_hex(0x3E424C));
    }
    gfx.pop_clip();

    // 4. Command Input Bar
    gfx.fill_rect(input_rect_, Color::from_hex(0x16171B));
    gfx.draw_line(Point(input_rect_.x, input_rect_.y), Point(input_rect_.right(), input_rect_.y),
                  Color::from_hex(0x282B35), 1.0f);

    // Prompt Indicator
    const Rect prompt_r(input_rect_.x + 10.0f, input_rect_.y, 28.0f, input_rect_.height);
    gfx.draw_text(">>>", prompt_r, Color::from_hex(0x56B6C2), 10.0f, true, false, true, Theme::kNumericFontFamily);

    // Input text field
    const float input_x = input_rect_.x + 40.0f;
    const Rect text_r(input_x, input_rect_.y, input_rect_.width - 50.0f, input_rect_.height);
    if (!input_buffer_.empty()) {
        gfx.draw_text(input_buffer_, text_r, Color::from_hex(0xFFFFFF), 10.0f, false, false, true, Theme::kNumericFontFamily);
    } else {
        gfx.draw_text("Enter Python command or expression (e.g. medusa.scene.get_nodes())...",
                      text_r, Color::from_hex(0x5A606E), 9.5f, false, false, true, Theme::kUiFontFamily);
    }

    // Blinking Cursor
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_blink_).count() > 500) {
        cursor_visible_ = !cursor_visible_;
        last_blink_ = now;
    }
    if (cursor_visible_) {
        const float char_w = 7.2f;
        const float cursor_x = input_x + static_cast<float>(cursor_pos_) * char_w;
        const float cursor_y = input_rect_.y + (input_rect_.height - 14.0f) * 0.5f;
        gfx.fill_rect(Rect(cursor_x, cursor_y, 1.8f, 14.0f), Color::from_hex(0x61AFEF));
    }
}

bool DCCPythonConsoleWidget::handle_mouse_down(const MouseEvent& e) {
    if (clear_btn_rect_.contains(e.pos)) {
        clear();
        return true;
    }
    if (run_btn_rect_.contains(e.pos)) {
        // Open file dialog for .py
        std::vector<wchar_t> path(32768, L'\0');
        OPENFILENAMEW dialog = {};
        dialog.lStructSize = sizeof(dialog);
        dialog.lpstrFilter = L"Python Scripts (*.py)\0*.py\0All files (*.*)\0*.*\0";
        dialog.lpstrFile = path.data();
        dialog.nMaxFile = static_cast<DWORD>(path.size());
        dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        dialog.lpstrTitle = L"Run Python Script in Medusa Platinum";
        if (GetOpenFileNameW(&dialog)) {
            char u8[1024] = {};
            WideCharToMultiByte(CP_UTF8, 0, path.data(), -1, u8, sizeof(u8), nullptr, nullptr);
            scripting::PythonEngine::instance().execute_file(u8);
            scroll_to_bottom();
        }
        return true;
    }
    if (copy_btn_rect_.contains(e.pos)) {
        // Copy entire log to clipboard
        std::string fullLog;
        for (const auto& entry : scripting::PythonEngine::instance().get_logs()) {
            fullLog += entry.text + "\r\n";
        }
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, fullLog.size() + 1);
            if (hMem) {
                memcpy(GlobalLock(hMem), fullLog.c_str(), fullLog.size() + 1);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
            CloseClipboard();
        }
        return true;
    }

    if (input_rect_.contains(e.pos)) {
        const float input_x = input_rect_.x + 40.0f;
        float relX = e.pos.x - input_x;
        cursor_pos_ = std::clamp(static_cast<size_t>(std::max(0.0f, relX / 7.2f)),
                                 size_t{0}, input_buffer_.length());
        cursor_visible_ = true;
        last_blink_ = std::chrono::steady_clock::now();
        return true;
    }

    return bounds_.contains(e.pos);
}

bool DCCPythonConsoleWidget::handle_mouse_move(const MouseEvent& e) {
    int prev = hovered_btn_;
    hovered_btn_ = -1;
    if (clear_btn_rect_.contains(e.pos)) hovered_btn_ = 0;
    else if (run_btn_rect_.contains(e.pos)) hovered_btn_ = 1;
    else if (copy_btn_rect_.contains(e.pos)) hovered_btn_ = 2;

    return hovered_btn_ != prev;
}

bool DCCPythonConsoleWidget::handle_mouse_up(const MouseEvent& e) {
    return false;
}

bool DCCPythonConsoleWidget::handle_mouse_wheel(float delta, Point p) {
    if (!log_rect_.contains(p)) return false;
    auto_scroll_ = false;
    log_scroll_ = std::max(0.0f, log_scroll_ - delta * 36.0f);
    return true;
}

bool DCCPythonConsoleWidget::handle_key_down(int key, bool ctrl, bool shift) {
    if (key == VK_RETURN) {
        if (!input_buffer_.empty()) {
            std::string cmd = input_buffer_;
            input_buffer_.clear();
            cursor_pos_ = 0;
            history_index_ = -1;
            scripting::PythonEngine::instance().execute(cmd);
            scroll_to_bottom();
            return true;
        }
        return true;
    }

    if (key == VK_UP) {
        const auto& hist = scripting::PythonEngine::instance().get_history();
        if (!hist.empty()) {
            if (history_index_ < 0) history_index_ = static_cast<int>(hist.size()) - 1;
            else if (history_index_ > 0) --history_index_;
            input_buffer_ = hist[static_cast<size_t>(history_index_)];
            cursor_pos_ = input_buffer_.length();
            return true;
        }
    }

    if (key == VK_DOWN) {
        const auto& hist = scripting::PythonEngine::instance().get_history();
        if (!hist.empty() && history_index_ >= 0) {
            if (history_index_ + 1 < static_cast<int>(hist.size())) {
                ++history_index_;
                input_buffer_ = hist[static_cast<size_t>(history_index_)];
            } else {
                history_index_ = -1;
                input_buffer_.clear();
            }
            cursor_pos_ = input_buffer_.length();
            return true;
        }
    }

    if (key == VK_BACK) {
        if (cursor_pos_ > 0 && !input_buffer_.empty()) {
            input_buffer_.erase(cursor_pos_ - 1, 1);
            --cursor_pos_;
            return true;
        }
    }

    if (key == VK_DELETE) {
        if (cursor_pos_ < input_buffer_.length()) {
            input_buffer_.erase(cursor_pos_, 1);
            return true;
        }
    }

    if (key == VK_LEFT) {
        if (cursor_pos_ > 0) { --cursor_pos_; return true; }
    }

    if (key == VK_RIGHT) {
        if (cursor_pos_ < input_buffer_.length()) { ++cursor_pos_; return true; }
    }

    if (key == VK_HOME) {
        cursor_pos_ = 0;
        return true;
    }

    if (key == VK_END) {
        cursor_pos_ = input_buffer_.length();
        return true;
    }

    if (ctrl && (key == 'L' || key == 'l')) {
        clear();
        return true;
    }

    if (ctrl && (key == 'V' || key == 'v')) {
        if (OpenClipboard(nullptr)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* text = static_cast<char*>(GlobalLock(hData));
                if (text) {
                    input_buffer_.insert(cursor_pos_, text);
                    cursor_pos_ += strlen(text);
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
            return true;
        }
    }

    return false;
}

bool DCCPythonConsoleWidget::handle_char(uint32_t codepoint) {
    if (codepoint >= 32 && codepoint != 127) {
        input_buffer_.insert(cursor_pos_, 1, static_cast<char>(codepoint));
        ++cursor_pos_;
        cursor_visible_ = true;
        last_blink_ = std::chrono::steady_clock::now();
        return true;
    }
    return false;
}

void DCCPythonConsoleWidget::scroll_to_bottom() {
    auto_scroll_ = true;
}

void DCCPythonConsoleWidget::clear() {
    scripting::PythonEngine::instance().clear_logs();
    log_scroll_ = 0.0f;
    auto_scroll_ = true;
}

} // namespace medusa
