#pragma once
#include <medusa/core/medusa_types.hpp>
#include <medusa/graphics/medusa_graphics.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <medusa/ui/medusa_icons.hpp>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace medusa {

struct PopupMenuItem {
    std::string label;
    std::string icon;
    std::string shortcut;
    bool enabled = true;
    bool is_separator = false;
    bool is_checkable = false;
    bool checked = false;
    std::function<void()> action;
};

class DCCPopupMenu {
public:
    DCCPopupMenu() = default;

    void clear();

    void add_item(const std::string& label, const std::string& icon = "",
                  const std::string& shortcut = "", bool enabled = true,
                  std::function<void()> action = nullptr);
    void add_item(const std::string& label, const std::string& icon,
                  const std::string& shortcut, std::function<void()> action,
                  bool enabled = true);

    void add_check(const std::string& label, bool checked,
                   std::function<void()> action = nullptr);
    void add_check(const std::string& label, const std::string& icon, bool checked,
                   std::function<void()> action = nullptr,
                   const std::string& shortcut = "");

    void add_separator(const std::string& section_title = "");

    void open(const Rect& anchor, const Rect& screen_bounds, float min_width = 200.0f, float max_width = 340.0f, bool instant = false);
    void close();

    bool is_open() const { return is_open_; }
    bool is_animating() const;
    const Rect& get_bounds() const { return bounds_; }
    Rect get_cutout_bounds() const;
    const Rect& get_anchor() const { return anchor_; }
    bool contains(Point p) const { return is_open_ && bounds_.contains(p); }

    void draw(Graphics& gfx);
    bool handle_mouse_down(const MouseEvent& e);
    bool handle_mouse_move(const MouseEvent& e);

private:
    std::vector<PopupMenuItem> items_;
    std::vector<Rect> item_rects_;
    Rect bounds_{};
    Rect anchor_{};
    bool is_open_ = false;
    bool instant_ = false;
    int hovered_index_ = -1;
    Point last_mouse_pos_{};
    std::chrono::steady_clock::time_point open_time_{};
};

} // namespace medusa
