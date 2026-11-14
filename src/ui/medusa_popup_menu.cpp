#include <medusa/ui/medusa_popup_menu.hpp>
#include <cmath>
#include <algorithm>

namespace medusa {

void DCCPopupMenu::clear() {
    items_.clear();
    item_rects_.clear();
    is_open_ = false;
    hovered_index_ = -1;
    bounds_ = Rect{};
    anchor_ = Rect{};
}

void DCCPopupMenu::add_item(const std::string& label, const std::string& icon,
                            const std::string& shortcut, bool enabled,
                            std::function<void()> action) {
    items_.push_back({label, icon, shortcut, enabled, false, false, false, std::move(action)});
}

void DCCPopupMenu::add_item(const std::string& label, const std::string& icon,
                            const std::string& shortcut, std::function<void()> action,
                            bool enabled) {
    items_.push_back({label, icon, shortcut, enabled, false, false, false, std::move(action)});
}

void DCCPopupMenu::add_check(const std::string& label, bool checked,
                             std::function<void()> action) {
    items_.push_back({label, "", "", true, false, true, checked, std::move(action)});
}

void DCCPopupMenu::add_check(const std::string& label, const std::string& icon, bool checked,
                             std::function<void()> action, const std::string& shortcut) {
    items_.push_back({label, icon, shortcut, true, false, true, checked, std::move(action)});
}

void DCCPopupMenu::add_separator(const std::string& section_title) {
    items_.push_back({section_title, "", "", false, true, false, false, nullptr});
}

void DCCPopupMenu::open(const Rect& anchor, const Rect& screen_bounds, float min_width, float max_width, bool instant) {
    anchor_ = anchor;
    instant_ = instant;
    open_time_ = std::chrono::steady_clock::now();
    is_open_ = true;
    hovered_index_ = -1;

    // Safety: ensure min_width and max_width are generous and valid DCC bounds
    min_width = std::max(min_width, 200.0f);
    if (max_width <= min_width) {
        max_width = std::max(min_width + 120.0f, 340.0f);
    }

    // Compute dynamic width to comfortably fit longest label and shortcut
    float width = min_width;
    for (const auto& item : items_) {
        if (item.is_separator) continue;
        const bool has_cb = item.is_checkable;
        const bool has_icon = !item.icon.empty();
        const float gutter = (has_cb && has_icon) ? 42.0f : (has_cb || has_icon) ? 30.0f : 16.0f;
        float w = gutter + static_cast<float>(item.label.length()) * 7.5f + 16.0f;
        if (!item.shortcut.empty()) {
            w += static_cast<float>(item.shortcut.length()) * 7.0f + 24.0f;
        }
        if (w > width) width = w;
    }
    width = std::clamp(width, min_width, max_width);

    // Compute total height (26px per item row, exactly matching Blender's clean compact feel)
    float total_h = 8.0f; // 4px top + 4px bottom padding
    for (const auto& item : items_) {
        total_h += item.is_separator ? (!item.label.empty() ? 18.0f : 8.0f) : 26.0f;
    }

    // Horizontal placement: align to anchor.x, clamped within screen_bounds
    float popup_x = std::clamp(anchor.x, screen_bounds.x + 4.0f,
                               std::max(screen_bounds.x + 4.0f, screen_bounds.right() - width - 4.0f));

    // Vertical placement: default below anchor. If overflow bottom, place above anchor!
    float popup_y = anchor.bottom() + 1.0f;
    if (popup_y + total_h > screen_bounds.bottom() - 4.0f && anchor.y - total_h >= screen_bounds.y) {
        popup_y = anchor.y - total_h;
    }

    bounds_ = Rect(popup_x, popup_y, width, total_h);

    // Layout each row
    item_rects_.clear();
    item_rects_.reserve(items_.size());
    float cur_y = bounds_.y + 4.0f;
    for (const auto& item : items_) {
        const float h = item.is_separator ? (!item.label.empty() ? 18.0f : 8.0f) : 26.0f;
        item_rects_.push_back(Rect(bounds_.x, cur_y, width, h));
        cur_y += h;
    }
}

void DCCPopupMenu::close() {
    is_open_ = false;
    hovered_index_ = -1;
}

bool DCCPopupMenu::is_animating() const {
    if (!is_open_ || instant_) return false;
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - open_time_).count();
    return elapsed < 65;
}

Rect DCCPopupMenu::get_cutout_bounds() const {
    if (!is_open_ || bounds_.width <= 0.0f || bounds_.height <= 0.0f) return Rect{};
    return Rect(bounds_.x - 4.0f, bounds_.y, bounds_.width + 8.0f, bounds_.height + 5.0f);
}

void DCCPopupMenu::draw(Graphics& gfx) {
    if (!is_open_ || items_.empty()) return;

    // Calculate snappy ease-out roll-down animation progress
    float progress = 1.0f;
    if (!instant_) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - open_time_).count();
        if (elapsed < 65) {
            float t = static_cast<float>(elapsed) / 65.0f;
            float inv = 1.0f - t;
            progress = std::clamp(1.0f - inv * inv * inv, 0.15f, 1.0f);
        }
    }

    const float anim_h = bounds_.height * progress;
    const Rect anim_rect(bounds_.x, bounds_.y, bounds_.width, anim_h);

    // 1. Soft Multi-Pass Drop Shadow (elevated modern DCC depth)
    gfx.fill_rounded_rect(Rect(anim_rect.x - 4.0f, anim_rect.y + 2.0f, anim_rect.width + 8.0f, anim_rect.height + 5.0f), 6.0f, Color(0.0f, 0.0f, 0.0f, 0.16f));
    gfx.fill_rounded_rect(Rect(anim_rect.x - 2.0f, anim_rect.y + 1.0f, anim_rect.width + 4.0f, anim_rect.height + 3.0f), 5.0f, Color(0.0f, 0.0f, 0.0f, 0.28f));
    gfx.fill_rounded_rect(Rect(anim_rect.x - 1.0f, anim_rect.y + 1.0f, anim_rect.width + 2.0f, anim_rect.height + 1.0f), 4.0f, Color(0.0f, 0.0f, 0.0f, 0.42f));

    // 2. 100% Solid Opaque Obsidian Chassis Background matching animated height (Never empty black void)
    gfx.fill_rounded_rect(anim_rect, 4.0f, Color::from_hex(0x16171A));
    gfx.draw_rounded_rect(anim_rect, 4.0f, Color::from_hex(0x363942), 1.0f);

    // 3. Render Items clipped within anim_rect
    gfx.push_clip(anim_rect);
    for (size_t i = 0; i < items_.size() && i < item_rects_.size(); ++i) {
        const auto& item = items_[i];
        const Rect row = item_rects_[i];

        if (item.is_separator) {
            if (!item.label.empty()) {
                gfx.draw_text(item.label, Rect(row.x + 10.0f, row.y + 1.0f, row.width - 20.0f, row.height - 2.0f),
                              Color::from_hex(0x767B8A), 8.5f, true, false, true, Theme::kUiFontFamily);
            } else {
                const float line_y = std::round(row.y + row.height * 0.5f);
                gfx.draw_line(Point(row.x + 8.0f, line_y), Point(row.right() - 8.0f, line_y), Color::from_hex(0x282A31), 1.0f);
            }
            continue;
        }

        const bool is_hovered = (static_cast<int>(i) == hovered_index_);

        // Inset Selection / Hover Pill
        if (is_hovered && item.enabled) {
            const Rect pill(row.x + 3.0f, row.y + 1.0f, row.width - 6.0f, row.height - 2.0f);
            gfx.fill_rounded_rect(pill, 3.0f, Color::from_hex(0x272B35));
        }

        // Left Gutter: Checkbox, Icon, or Both
        const bool has_cb = item.is_checkable;
        const bool has_icon = !item.icon.empty();
        const float gutter_w = (has_cb && has_icon) ? 40.0f : (has_cb || has_icon) ? 26.0f : 14.0f;

        if (has_cb) {
            const Rect cb(row.x + 6.0f, row.y + (row.height - 12.0f) * 0.5f, 12.0f, 12.0f);
            if (item.checked) {
                gfx.fill_rounded_rect(cb, 2.0f, Theme::kAccent);
                gfx.draw_rounded_rect(cb, 2.0f, Theme::kAccentBorder, 1.0f);
                Point p0(cb.x + 2.5f, cb.y + 6.0f);
                Point p1(cb.x + 5.0f, cb.y + 9.0f);
                Point p2(cb.x + 9.5f, cb.y + 3.5f);
                gfx.draw_line(p0, p1, Color(1.0f, 1.0f, 1.0f, 1.0f), 1.6f);
                gfx.draw_line(p1, p2, Color(1.0f, 1.0f, 1.0f, 1.0f), 1.6f);
            } else {
                gfx.fill_rounded_rect(cb, 2.0f, Color::from_hex(0x1E2025));
                gfx.draw_rounded_rect(cb, 2.0f, Color::from_hex(0x3E424C), 1.0f);
            }
        }

        if (has_icon) {
            const float icon_x = has_cb ? row.x + 22.0f : row.x + 6.0f;
            const float sz = 14.0f;
            const Rect icon_r(icon_x, row.y + (row.height - sz) * 0.5f, sz, sz);
            const Color icon_col = !item.enabled ? Theme::kTextMuted
                                  : is_hovered ? Color::from_hex(0xFFFFFF)
                                  : Theme::kTextSecondary;
            IconRenderer::draw_icon(gfx, item.icon, icon_r, icon_col);
        }

        // Label Column
        const Color text_col = !item.enabled ? Theme::kTextMuted
                             : is_hovered ? Color::from_hex(0xFFFFFF)
                             : Theme::kTextPrimary;
        const float label_x = row.x + gutter_w;
        const float shortcut_space = item.shortcut.empty() ? 10.0f : 85.0f;
        const float label_w = std::max(20.0f, row.width - gutter_w - shortcut_space);
        const Rect label_r(label_x, row.y, label_w, row.height);
        gfx.draw_text(item.label, label_r, text_col, Theme::kUiFont, false, false, true, Theme::kUiFontFamily);

        // Right Shortcut Column
        if (!item.shortcut.empty()) {
            const Rect sc_r(row.right() - 85.0f, row.y, 76.0f, row.height);
            const Color sc_col = is_hovered ? Color::from_hex(0x9EA3AD) : Theme::kTextMuted;
            gfx.draw_text(item.shortcut, sc_r, sc_col, Theme::kSmallFont, false, true, true, Theme::kUiFontFamily);
        }
    }
    gfx.pop_clip();
}

bool DCCPopupMenu::handle_mouse_down(const MouseEvent& e) {
    if (!is_open_) return false;

    for (size_t i = 0; i < items_.size() && i < item_rects_.size(); ++i) {
        if (items_[i].is_separator) continue;
        if (item_rects_[i].contains(e.pos)) {
            auto action = items_[i].action;
            close();
            if (items_[i].enabled && action) {
                action();
            }
            return true;
        }
    }

    // Clicked outside
    close();
    return bounds_.contains(e.pos);
}

bool DCCPopupMenu::handle_mouse_move(const MouseEvent& e) {
    if (!is_open_) return false;
    last_mouse_pos_ = e.pos;

    int prev_hover = hovered_index_;
    hovered_index_ = -1;
    for (size_t i = 0; i < item_rects_.size(); ++i) {
        if (items_[i].is_separator) continue;
        if (item_rects_[i].contains(e.pos)) {
            hovered_index_ = static_cast<int>(i);
            break;
        }
    }
    return hovered_index_ != prev_hover;
}

} // namespace medusa
