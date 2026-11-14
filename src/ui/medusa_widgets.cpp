#include <medusa/medusa_widgets.hpp>
#include <cmath>
#include <cstdio>
#include <limits>

namespace medusa {

// ==============================================================================
// 1. ButtonWidget
// ==============================================================================

ButtonWidget::ButtonWidget(const std::string &label, const std::string &icon,
                           std::function<void()> on_click)
    : label_(label), icon_(icon), on_click_(std::move(on_click)) {}

void ButtonWidget::draw(Graphics &gfx) {
    Color bg = is_primary_ ? Theme::kAccentBg : Theme::kControlBg;
    if (is_primary_ && is_hovered_) bg = Theme::kAccentHover;
    else if (is_pressed_) bg = Theme::kControlActive;
    else if (is_active_) bg = Theme::kControlActive;
    else if (is_hovered_) bg = Theme::kControlHover;

    if (is_primary_) {
        gfx.fill_rounded_rect(bounds_, 3.0f, bg);
        gfx.draw_rounded_rect(bounds_, 3.0f, Theme::kAccentBorder, 1.0f);
    } else if (!is_flat_) {
        gfx.fill_rounded_rect(bounds_, 3.0f, bg);
        gfx.draw_rounded_rect(bounds_, 3.0f, is_active_ ? Theme::kAccentBorder : Theme::kBorderBright, 1.0f);
    } else if (is_active_) {
        // Active tool pill in Blender style: rounded dark container with subtle outline
        gfx.fill_rounded_rect(bounds_, 4.0f, Theme::kControlActive);
        gfx.draw_rounded_rect(bounds_, 4.0f, Theme::kBorderBright, 1.0f);
    } else if (is_hovered_ || is_pressed_) {
        gfx.fill_rounded_rect(bounds_, 3.0f, bg);
    }

    if (!icon_.empty() && !label_.empty()) {
        float sz = std::min(bounds_.height - 8.0f, 16.0f);
        Rect icon_rect(bounds_.x + 9.0f, bounds_.y + (bounds_.height - sz) * 0.5f, sz, sz);
        Color icon_col = is_primary_ ? Theme::kAccentText : (is_active_ || is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary);
        IconRenderer::draw_icon(gfx, icon_, icon_rect, icon_col);
        Rect label_rect(bounds_.x + 31.0f, bounds_.y, bounds_.width - 36.0f, bounds_.height);
        Color txt_col = is_primary_ ? Theme::kAccentText : (is_active_ || is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary);
        gfx.draw_text(label_, label_rect, txt_col, Theme::kUiFont, is_primary_ || is_active_, true, true, Theme::kUiFontFamily);
    } else if (!icon_.empty()) {
        float sz = std::min(bounds_.width, bounds_.height) * 0.58f;
        Rect icon_rect(bounds_.x + (bounds_.width - sz) * 0.5f,
                       bounds_.y + (bounds_.height - sz) * 0.5f,
                       sz, sz);
        Color icon_col = is_primary_ ? Theme::kAccentText : (is_active_ || is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary);
        IconRenderer::draw_icon(gfx, icon_, icon_rect, icon_col);
    } else if (!label_.empty()) {
        Color txt_col = is_primary_ ? Theme::kAccentText : (is_active_ || is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary);
        gfx.draw_text(label_, bounds_, txt_col, Theme::kUiFont, is_primary_ || is_active_, true, true, Theme::kUiFontFamily);
    }

}

void ButtonWidget::draw_tooltip(Graphics &gfx) {
    if (!is_hovered_ || tooltip_.empty() ||
        std::chrono::steady_clock::now() - hover_started_ < std::chrono::milliseconds(1000)) return;
    const float tip_w = std::max(150.0f, static_cast<float>(tooltip_.size()) * 6.5f + 20.0f);
    Rect tip(bounds_.right() + 8.0f, bounds_.y + (bounds_.height - 32.0f) * 0.5f, tip_w, 32.0f);
    gfx.fill_rect(tip, Theme::kPanelHeaderBg);
    gfx.draw_rect(tip, Theme::kBorderBright, 1.0f);
    gfx.draw_text(tooltip_, Rect(tip.x + 10.0f, tip.y, tip.width - 20.0f, tip.height),
                  Theme::kTextPrimary, Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);
}

bool ButtonWidget::handle_mouse_down(const MouseEvent &e) {
    if (bounds_.contains(e.pos) && e.button == MouseButton::Left) {
        is_pressed_ = true;
        return true;
    }
    return false;
}

bool ButtonWidget::handle_mouse_move(const MouseEvent &e) {
    bool was_hovered = is_hovered_;
    is_hovered_ = bounds_.contains(e.pos);
    if (is_hovered_ && !was_hovered) hover_started_ = std::chrono::steady_clock::now();
    return was_hovered != is_hovered_;
}

bool ButtonWidget::handle_mouse_up(const MouseEvent &e) {
    if (is_pressed_ && bounds_.contains(e.pos) && e.button == MouseButton::Left) {
        is_pressed_ = false;
        if (on_click_) on_click_();
        return true;
    }
    is_pressed_ = false;
    return false;
}

// ==============================================================================
// 2. ScrubbableNumberWidget (Neutral Blender-Style Slider)
// ==============================================================================

ScrubbableNumberWidget::ScrubbableNumberWidget(const std::string &axis_label, float value,
                                               const std::string &unit, Color axis_color,
                                               float min_val, float max_val, float step,
                                               std::function<void(float)> on_change)
    : axis_label_(axis_label), value_(value), unit_(unit), axis_color_(axis_color),
      min_val_(min_val), max_val_(max_val), step_(step), on_change_(std::move(on_change)) {}

void ScrubbableNumberWidget::draw(Graphics &gfx) {
    Color bg = is_dragging_ ? Theme::kControlActive : (is_hovered_ ? Theme::kControlHover : Theme::kControlBg);
    gfx.fill_rounded_rect(bounds_, 3.0f, bg);
    gfx.draw_rounded_rect(bounds_, 3.0f, is_dragging_ ? Theme::kAccentBorder : Theme::kBorderBright, 1.0f);

    // If bounded slider, draw a subtle progressive scrub fill bar (Blender style)
    if (min_val_ < max_val_ && std::isfinite(min_val_) && std::isfinite(max_val_)) {
        float pct = std::clamp((value_ - min_val_) / (max_val_ - min_val_), 0.0f, 1.0f);
        if (pct > 0.005f) {
            Rect fill_rect(bounds_.x + 1.0f, bounds_.y + 1.0f, (bounds_.width - 2.0f) * pct, bounds_.height - 2.0f);
            gfx.fill_rounded_rect(fill_rect, 2.0f, is_dragging_ ? Color(0.25f, 0.45f, 0.70f, 0.35f) : Color(0.35f, 0.38f, 0.45f, 0.20f));
        }
    }

    // Left Axis / Property Label Indicator
    float tag_w = 0.0f;
    if (!axis_label_.empty()) {
        tag_w = std::max(20.0f, static_cast<float>(axis_label_.length()) * 7.2f + 10.0f);
        Rect tag_rect(bounds_.x, bounds_.y, tag_w, bounds_.height);
        Color strip_col = axis_color_;
        if (strip_col.a < 0.1f) {
            strip_col = Theme::kTextSecondary;
            gfx.fill_rounded_rect(tag_rect, 3.0f, Color::from_hex(0x22242B));
        } else {
            gfx.fill_rounded_rect(tag_rect, 3.0f, Color(strip_col.r, strip_col.g, strip_col.b, 0.22f));
        }
        gfx.draw_text(axis_label_, tag_rect, strip_col,
                      Theme::kSmallFont, true, true, true, Theme::kUiFontFamily);
    }

    // Formatted Value Display (Neutral white/gray numbers)
    char value_text[64];
    std::snprintf(value_text, sizeof(value_text), "%.3f%s%s", value_,
                  unit_.empty() ? "" : " ", unit_.c_str());
    Rect val_rect(bounds_.x + tag_w + 6.0f, bounds_.y, bounds_.width - tag_w - 12.0f, bounds_.height);
    gfx.draw_text(value_text, val_rect, Theme::kTextPrimary, Theme::kUiFont, false, true, true, Theme::kNumericFontFamily);
}

bool ScrubbableNumberWidget::handle_mouse_down(const MouseEvent &e) {
    if (bounds_.contains(e.pos) && e.button == MouseButton::Left) {
        is_dragging_ = true;
        drag_start_x_ = e.pos.x;
        drag_start_val_ = value_;
        return true;
    }
    return false;
}

bool ScrubbableNumberWidget::handle_mouse_move(const MouseEvent &e) {
    bool was_hovered = is_hovered_;
    is_hovered_ = bounds_.contains(e.pos);

    if (is_dragging_) {
        float dx = e.pos.x - drag_start_x_;
        float speed = step_;
        if (e.shift) speed *= 0.1f; // Shift: micro-step
        if (e.ctrl) speed *= 5.0f;  // Ctrl: coarse step

        float new_val = drag_start_val_ + dx * speed * 0.1f;
        if (e.ctrl) {
            new_val = std::round(new_val / step_) * step_; // Ctrl: snap
        }
        if (new_val < min_val_) new_val = min_val_;
        if (new_val > max_val_) new_val = max_val_;

        if (new_val != value_) {
            value_ = new_val;
            if (on_change_) on_change_(value_);
        }
        return true;
    }
    return was_hovered != is_hovered_;
}

bool ScrubbableNumberWidget::handle_mouse_up(const MouseEvent &e) {
    if (is_dragging_) {
        is_dragging_ = false;
        return true;
    }
    return false;
}

// ==============================================================================
// 3. CollapsibleSectionWidget (Blender Properties Panel Header)
// ==============================================================================

CollapsibleSectionWidget::CollapsibleSectionWidget(const std::string &title, bool expanded,
                                                   std::function<void(bool)> on_toggle)
    : title_(title), is_expanded_(expanded), on_toggle_(std::move(on_toggle)) {}

void CollapsibleSectionWidget::draw(Graphics &gfx) {
    Color bg = is_hovered_ ? Theme::kControlHover : Theme::kPanelHeaderBg;
    gfx.fill_rounded_rect(Rect(bounds_.x + 4.0f, bounds_.y, bounds_.width - 8.0f, bounds_.height), 3.0f, bg);
    gfx.draw_line(Point(bounds_.x + 4.0f, bounds_.bottom()), Point(bounds_.right() - 4.0f, bounds_.bottom()), Theme::kBorderSubtle, 1.0f);

    // Chevron arrow
    Rect arrow_rect(bounds_.x + 8.0f, bounds_.y, 14.0f, bounds_.height);
    gfx.draw_text(is_expanded_ ? "▾" : "▸", arrow_rect, Theme::kTextSecondary, 11.0f, false, true, true);

    // Section Title
    Rect title_rect(bounds_.x + 24.0f, bounds_.y, bounds_.width - 32.0f, bounds_.height);
    gfx.draw_text(title_, title_rect, Theme::kTextPrimary, Theme::kUiFont, true, false, true, Theme::kUiFontFamily);
}

bool CollapsibleSectionWidget::handle_mouse_down(const MouseEvent &e) {
    if (bounds_.contains(e.pos) && e.button == MouseButton::Left) {
        is_expanded_ = !is_expanded_;
        if (on_toggle_) on_toggle_(is_expanded_);
        return true;
    }
    return false;
}

bool CollapsibleSectionWidget::handle_mouse_move(const MouseEvent &e) {
    bool was_hovered = is_hovered_;
    is_hovered_ = bounds_.contains(e.pos);
    return was_hovered != is_hovered_;
}

bool CollapsibleSectionWidget::handle_mouse_up(const MouseEvent &e) {
    return false;
}

// ==============================================================================
// 4. OutlinerRowWidget (Blender Hierarchy Tree)
// ==============================================================================

OutlinerRowWidget::OutlinerRowWidget(const std::string &name, const std::string &icon,
                                     int indent_level, bool has_children, bool is_expanded,
                                     bool is_selected, bool is_visible, bool is_locked, bool is_renderable,
                                     std::function<void()> on_select,
                                     std::function<void(bool)> on_toggle_expand,
                                     std::function<void(bool)> on_toggle_vis,
                                     std::function<void(bool)> on_toggle_lock,
                                     std::function<void(bool)> on_toggle_render)
    : name_(name), icon_(icon), indent_level_(indent_level),
      has_children_(has_children), is_expanded_(is_expanded),
      is_selected_(is_selected), is_visible_(is_visible),
      is_locked_(is_locked), is_renderable_(is_renderable),
      on_select_(std::move(on_select)),
      on_toggle_expand_(std::move(on_toggle_expand)),
      on_toggle_vis_(std::move(on_toggle_vis)),
      on_toggle_lock_(std::move(on_toggle_lock)),
      on_toggle_render_(std::move(on_toggle_render)) {}

void OutlinerRowWidget::draw(Graphics &gfx) {
    // Modern DCC selection pill: inset rounded rect with accent indicator
    if (is_selected_) {
        gfx.fill_rounded_rect(Rect(bounds_.x + 4.0f, bounds_.y + 1.0f, bounds_.width - 8.0f, bounds_.height - 2.0f), 3.0f, Theme::kSelectionBg);
        gfx.fill_rounded_rect(Rect(bounds_.x + 4.0f, bounds_.y + 3.0f, 2.5f, bounds_.height - 6.0f), 1.0f, Theme::kAccent);
    } else if (is_hovered_) {
        gfx.fill_rounded_rect(Rect(bounds_.x + 4.0f, bounds_.y + 1.0f, bounds_.width - 8.0f, bounds_.height - 2.0f), 3.0f, Theme::kControlHover);
    }

    float indent_x = bounds_.x + 8.0f + static_cast<float>(indent_level_ * 16);

    // Tree hierarchy guide lines
    if (indent_level_ > 0) {
        float parent_line_x = indent_x - 10.0f;
        float mid_y = bounds_.y + bounds_.height * 0.5f;
        gfx.draw_line(Point(parent_line_x, bounds_.y), Point(parent_line_x, bounds_.bottom()), Theme::kBorderBright, 0.75f);
        gfx.draw_line(Point(parent_line_x, mid_y), Point(indent_x - 2.0f, mid_y), Theme::kBorderBright, 0.75f);
    }

    // Disclosure Chevron
    if (has_children_) {
        Rect arrow_rect(indent_x, bounds_.y, 12.0f, bounds_.height);
        gfx.draw_text(is_expanded_ ? "▾" : "▸", arrow_rect, Theme::kTextSecondary, 10.0f, false, true, true);
    }

    // Icon (14x14)
    Rect icon_rect(indent_x + 14.0f, bounds_.y + (bounds_.height - 14.0f) * 0.5f, 14.0f, 14.0f);
    Color icon_col = is_selected_ ? Theme::kTextPrimary : Theme::kTextSecondary;
    IconRenderer::draw_icon(gfx, icon_, icon_rect, icon_col);

    // Label
    Rect label_rect(indent_x + 32.0f, bounds_.y, bounds_.right() - indent_x - 90.0f, bounds_.height);
    Color txt_col = is_selected_ ? Theme::kTextPrimary : (is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary);
    gfx.draw_text(name_, label_rect, txt_col, Theme::kUiFont, false, false, true, Theme::kUiFontFamily);

    // Right Restriction Columns (Eye, Lock, Render Camera)
    float col_x = bounds_.right() - 56.0f;

    // 1. Viewport Visibility (Eye)
    Rect eye_rect(col_x, bounds_.y + (bounds_.height - 14.0f) * 0.5f, 14.0f, 14.0f);
    IconRenderer::draw_icon(gfx, is_visible_ ? "toggle_eye_on" : "toggle_eye_off",
                            eye_rect, is_visible_ ? Theme::kTextSecondary : Theme::kTextMuted);
    col_x += 18.0f;

    // 2. Selection Lock
    Rect lock_rect(col_x, bounds_.y + (bounds_.height - 14.0f) * 0.5f, 14.0f, 14.0f);
    IconRenderer::draw_icon(gfx, is_locked_ ? "toggle_lock_on" : "toggle_lock_off",
                            lock_rect, is_locked_ ? Theme::kAccent : Theme::kTextMuted);
    col_x += 18.0f;

    // 3. Render Visibility (Camera indicator)
    Rect render_rect(col_x, bounds_.y + (bounds_.height - 14.0f) * 0.5f, 14.0f, 14.0f);
    IconRenderer::draw_icon(gfx, is_renderable_ ? "camera_actor" : "toggle_render_off",
                            render_rect, is_renderable_ ? Theme::kTextSecondary : Theme::kTextMuted);
}

bool OutlinerRowWidget::handle_mouse_down(const MouseEvent &e) {
    if (bounds_.contains(e.pos) && e.button == MouseButton::Left) {
        // Check restriction column clicks
        float eye_x = bounds_.right() - 56.0f;
        if (e.pos.x >= eye_x && e.pos.x < eye_x + 16.0f) {
            is_visible_ = !is_visible_;
            if (on_toggle_vis_) on_toggle_vis_(is_visible_);
            return true;
        }
        float lock_x = eye_x + 18.0f;
        if (e.pos.x >= lock_x && e.pos.x < lock_x + 16.0f) {
            is_locked_ = !is_locked_;
            if (on_toggle_lock_) on_toggle_lock_(is_locked_);
            return true;
        }
        float render_x = lock_x + 18.0f;
        if (e.pos.x >= render_x && e.pos.x < render_x + 16.0f) {
            is_renderable_ = !is_renderable_;
            if (on_toggle_render_) on_toggle_render_(is_renderable_);
            return true;
        }

        // Chevron click toggles expand
        float indent_x = bounds_.x + 8.0f + static_cast<float>(indent_level_ * 16);
        if (has_children_ && e.pos.x >= indent_x && e.pos.x <= indent_x + 14.0f) {
            is_expanded_ = !is_expanded_;
            if (on_toggle_expand_) on_toggle_expand_(is_expanded_);
            return true;
        }

        // Row selection
        if (on_select_) on_select_();
        return true;
    }
    return false;
}

bool OutlinerRowWidget::handle_mouse_move(const MouseEvent &e) {
    bool was_hovered = is_hovered_;
    is_hovered_ = bounds_.contains(e.pos);
    return was_hovered != is_hovered_;
}

bool OutlinerRowWidget::handle_mouse_up(const MouseEvent &e) {
    return false;
}

// ==============================================================================
// 5. SequencerWidget (Blender Dope Sheet & Keyframe Sequencer)
// ==============================================================================

SequencerWidget::SequencerWidget(int current_frame, int total_frames,
                                 std::function<void(int)> on_frame_change)
    : current_frame_(current_frame), total_frames_(total_frames),
      on_frame_change_(std::move(on_frame_change)) {
    start_frame_ = 1;
    total_frames_ = std::max(1, total_frames);
    end_frame_ = total_frames_;
    view_start_frame_ = start_frame_;
    view_end_frame_ = end_frame_;
    view_initialized_ = true;
}

void SequencerWidget::set_frame_range(int startFrame, int endFrame, int totalFrames) {
    const int newStart = std::max(1, startFrame);
    const int newEnd = std::max(newStart, endFrame);
    const bool playbackRangeChanged = newStart != start_frame_ || newEnd != end_frame_;
    start_frame_ = newStart;
    end_frame_ = newEnd;
    total_frames_ = std::max(end_frame_, totalFrames);
    current_frame_ = std::clamp(current_frame_, start_frame_, end_frame_);
    if (!view_initialized_ || playbackRangeChanged) {
        view_start_frame_ = start_frame_;
        view_end_frame_ = end_frame_;
        view_initialized_ = true;
    } else {
        view_start_frame_ = std::clamp(view_start_frame_, start_frame_, end_frame_);
        view_end_frame_ = std::clamp(view_end_frame_, view_start_frame_, end_frame_);
        if (view_end_frame_ - view_start_frame_ < 1) view_end_frame_ = std::min(end_frame_, view_start_frame_ + 1);
    }
}

void SequencerWidget::set_frame(int frame) {
    current_frame_ = std::clamp(frame, start_frame_, end_frame_);
    if (current_frame_ < view_start_frame_) {
        const int span = std::max(1, view_end_frame_ - view_start_frame_);
        view_start_frame_ = current_frame_;
        view_end_frame_ = std::min(end_frame_, view_start_frame_ + span);
        view_start_frame_ = std::max(start_frame_, view_end_frame_ - span);
    } else if (current_frame_ > view_end_frame_) {
        const int span = std::max(1, view_end_frame_ - view_start_frame_);
        view_end_frame_ = current_frame_;
        view_start_frame_ = std::max(start_frame_, view_end_frame_ - span);
        view_end_frame_ = std::min(end_frame_, view_start_frame_ + span);
    }
}

void SequencerWidget::set_channels(std::vector<DopeSheetChannel> channels) {
    channels_ = std::move(channels);
    const float visibleRows = std::max(1.0f, (bounds_.height - ruler_height_ - 7.0f) / row_height_);
    const float maxScroll = std::max(0.0f, static_cast<float>(channels_.size()) * row_height_ -
                                            visibleRows * row_height_);
    channel_scroll_ = std::clamp(channel_scroll_, 0.0f, maxScroll);
}

float SequencerWidget::frame_to_x(int frame) const {
    const float left = bounds_.x + channel_width_;
    const float width = std::max(1.0f, bounds_.right() - left);
    const float span = static_cast<float>(std::max(1, view_end_frame_ - view_start_frame_));
    return left + (static_cast<float>(frame - view_start_frame_) / span) * width;
}

int SequencerWidget::x_to_frame(float x) const {
    const float left = bounds_.x + channel_width_;
    const float width = std::max(1.0f, bounds_.right() - left);
    const float span = static_cast<float>(std::max(1, view_end_frame_ - view_start_frame_));
    return std::clamp(view_start_frame_ + static_cast<int>(std::round((x - left) / width * span)),
                      view_start_frame_, view_end_frame_);
}

void SequencerWidget::zoom_at(float factor, float anchor_x) {
    if (end_frame_ <= start_frame_) return;
    const int oldSpan = std::max(1, view_end_frame_ - view_start_frame_);
    const int playbackSpan = std::max(1, end_frame_ - start_frame_);
    const int newSpan = std::clamp(static_cast<int>(std::round(oldSpan / factor)),
                                   std::min(10, playbackSpan), playbackSpan);
    const float left = bounds_.x + channel_width_;
    const float width = std::max(1.0f, bounds_.right() - left);
    const float anchor = std::clamp((anchor_x - left) / width, 0.0f, 1.0f);
    const float anchorFrame = static_cast<float>(view_start_frame_) + anchor * oldSpan;
    int newStart = static_cast<int>(std::round(anchorFrame - anchor * newSpan));
    newStart = std::clamp(newStart, start_frame_, std::max(start_frame_, end_frame_ - newSpan));
    view_start_frame_ = newStart;
    view_end_frame_ = newStart + newSpan;
}

void SequencerWidget::draw(Graphics &gfx) {
    channel_width_ = std::clamp(bounds_.width * 0.18f, 112.0f, 180.0f);
    const float plot_left = bounds_.x + channel_width_;
    const float plot_width = std::max(1.0f, bounds_.right() - plot_left);
    const float scrollbar_h = 7.0f;
    const float body_top = bounds_.y + ruler_height_;
    const float body_bottom = std::max(body_top, bounds_.bottom() - scrollbar_h);
    const Rect channel_header(bounds_.x, bounds_.y, channel_width_, ruler_height_);
    const Rect ruler(plot_left, bounds_.y, plot_width, ruler_height_);
    const Rect channel_body(bounds_.x, body_top, channel_width_, body_bottom - body_top);

    gfx.fill_rect(bounds_, Theme::kPanelBg);
    gfx.fill_rect(channel_header, Theme::kPanelHeaderBg);
    gfx.fill_rect(ruler, Theme::kPanelHeaderBg);
    gfx.fill_rect(channel_body, Theme::kChannelBg);
    gfx.draw_line(Point(plot_left, bounds_.y), Point(plot_left, body_bottom), Theme::kBorderSubtle, 1.0f);
    gfx.draw_line(Point(bounds_.x, body_top), Point(bounds_.right(), body_top), Theme::kBorderSubtle, 1.0f);
    gfx.draw_text("CHANNEL", Rect(bounds_.x + 8.0f, bounds_.y, channel_width_ - 16.0f, ruler_height_),
                  Theme::kTextMuted, 9.5f, true, false, true, Theme::kUiFontFamily);

    // Choose a readable ruler cadence from screen-space density instead of
    // hard-coding 12-frame ticks. Dense views get frame-level marks; wide views
    // get seconds-like groupings while keeping labels legible.
    const int visibleFrames = std::max(1, view_end_frame_ - view_start_frame_);
    const float pixelsPerFrame = plot_width / static_cast<float>(visibleFrames);
    constexpr int tickSteps[] = {1, 2, 5, 10, 20, 25, 50, 100, 200, 500, 1000, 2000, 5000,
                                 10000, 20000, 50000, 100000, 200000, 500000, 1000000,
                                 2000000, 5000000, 10000000, 20000000, 50000000,
                                 100000000, 200000000, 500000000, 1000000000, 2000000000};
    int majorStep = tickSteps[0];
    for (int step : tickSteps) {
        majorStep = step;
        if (pixelsPerFrame * static_cast<float>(step) >= 56.0f) break;
    }
    const int minorStep = std::max(1, majorStep / 5);
    const std::int64_t firstMinor = (static_cast<std::int64_t>(view_start_frame_) + minorStep - 1) /
                                    minorStep * minorStep;
    for (std::int64_t frameValue = firstMinor; frameValue <= view_end_frame_; frameValue += minorStep) {
        const int frame = static_cast<int>(frameValue);
        const float x = frame_to_x(frame);
        const bool major = frame % majorStep == 0;
        if (major) {
            gfx.draw_line(Point(x, bounds_.y + 15.0f), Point(x, body_bottom), Theme::kTrackGrid, 0.8f);
            const float labelX = std::clamp(x - 20.0f, plot_left, std::max(plot_left, bounds_.right() - 40.0f));
            gfx.draw_text(std::to_string(frame), Rect(labelX, bounds_.y + 1.0f, 40.0f, 14.0f),
                          Theme::kTextSecondary, Theme::kSmallFont, false, true, false, Theme::kNumericFontFamily);
            gfx.draw_line(Point(x, bounds_.y + 15.0f), Point(x, bounds_.y + ruler_height_),
                          Theme::kTextMuted, 0.8f);
        } else {
            gfx.draw_line(Point(x, bounds_.y + 18.0f), Point(x, bounds_.y + ruler_height_),
                          Theme::kBorderBright, 0.7f);
        }
        if (frameValue > std::numeric_limits<int>::max() - static_cast<std::int64_t>(minorStep)) break;
    }

    const size_t visibleRows = static_cast<size_t>(std::ceil(channel_body.height / row_height_)) + 1;
    const size_t firstRow = std::min(channels_.size(), static_cast<size_t>(channel_scroll_ / row_height_));
    const float rowOffset = std::fmod(channel_scroll_, row_height_);
    gfx.push_clip(Rect(bounds_.x, body_top, bounds_.width, std::max(0.0f, body_bottom - body_top)));
    for (size_t visible = 0; visible < visibleRows && firstRow + visible < channels_.size(); ++visible) {
        const auto& channel = channels_[firstRow + visible];
        const float y = body_top + static_cast<float>(visible) * row_height_ - rowOffset;
        const Rect row(bounds_.x, y, bounds_.width, row_height_);
        if (row.bottom() <= body_top || row.y >= body_bottom) continue;
        if (visible % 2 == 0) gfx.fill_rect(Rect(bounds_.x, y, channel_width_, row_height_), Theme::kTrackLaneEven);
        else gfx.fill_rect(Rect(bounds_.x, y, channel_width_, row_height_), Theme::kTrackLaneOdd);
        if (channel.indent == 0) gfx.fill_rect(Rect(bounds_.x, y, channel_width_, row_height_), Theme::kPanelHeaderBg);
        gfx.draw_line(Point(bounds_.x, row.bottom()), Point(bounds_.right(), row.bottom()), Theme::kBorderSubtle, 0.65f);

        const float indent = static_cast<float>(channel.indent) * 13.0f;
        const float iconX = bounds_.x + 7.0f + indent;
        if (channel.indent > 0)
            gfx.draw_text("·", Rect(bounds_.x + indent - 1.0f, y, 9.0f, row_height_), Theme::kTextMuted,
                          12.0f, false, true, true, Theme::kUiFontFamily);
        if (!channel.icon.empty())
            IconRenderer::draw_icon(gfx, channel.icon, Rect(iconX, y + 4.0f, 12.0f, 12.0f),
                                    channel.is_muted ? Theme::kTextMuted : Theme::kTextSecondary);
        gfx.draw_text(channel.name,
                      Rect(iconX + 18.0f, y, std::max(0.0f, channel_width_ - indent - 48.0f), row_height_),
                      channel.is_muted ? Theme::kTextMuted : Theme::kTextSecondary,
                      Theme::kSmallFont, channel.indent == 0, false, true, Theme::kUiFontFamily);

        if (channel.indent > 0) {
            const float laneY = y + row_height_ * 0.5f;
            const auto firstKey = std::lower_bound(channel.keyframes.begin(), channel.keyframes.end(), view_start_frame_);
            for (auto key = firstKey; key != channel.keyframes.end() && *key <= view_end_frame_; ++key) {
                const int frame = *key;
                const float keyX = frame_to_x(frame);
                if (keyX < plot_left || keyX > bounds_.right()) continue;
                const Point diamond[] = {{keyX, laneY - 4.5f}, {keyX + 4.5f, laneY},
                                         {keyX, laneY + 4.5f}, {keyX - 4.5f, laneY}};
                gfx.fill_polygon(diamond, 4, channel.accent_color);
            }
        }
    }
    gfx.pop_clip();

    // Frame marks stay aligned across all channels; the playhead is warm and
    // easy to follow against the blue selection system.
    const float playheadX = frame_to_x(current_frame_);
    if (playheadX >= plot_left && playheadX <= bounds_.right()) {
        const float badgeX = std::clamp(playheadX - 6.0f, plot_left, std::max(plot_left, bounds_.right() - 12.0f));
        gfx.fill_rect(Rect(badgeX, bounds_.y, 12.0f, ruler_height_), Theme::kPlayheadBlue);
        const float labelX = std::clamp(playheadX - 20.0f, plot_left, std::max(plot_left, bounds_.right() - 40.0f));
        gfx.draw_text(std::to_string(current_frame_), Rect(labelX, bounds_.y + 1.0f, 40.0f, 14.0f),
                      Theme::kAccentText, Theme::kSmallFont, true, true, false, Theme::kNumericFontFamily);
        gfx.draw_line(Point(playheadX, bounds_.y + ruler_height_), Point(playheadX, body_bottom),
                      Theme::kPlayheadBlue, 1.2f);
    }
    // Compact horizontal range indicator communicates the visible window and
    // keeps zoomed timelines grounded in the full playback range.
    const Rect scrollTrack(plot_left, bounds_.bottom() - scrollbar_h, plot_width, scrollbar_h);
    gfx.fill_rect(scrollTrack, Theme::kScrollbarTrack);
    const float playbackSpan = static_cast<float>(std::max(1, end_frame_ - start_frame_));
    const float viewSpan = static_cast<float>(std::max(1, view_end_frame_ - view_start_frame_));
    const float thumbW = std::min(plot_width, std::max(18.0f, plot_width * viewSpan / playbackSpan));
    const float thumbT = static_cast<float>(view_start_frame_ - start_frame_) /
                         static_cast<float>(std::max(1, end_frame_ - start_frame_));
    const float thumbX = plot_left + std::clamp(thumbT, 0.0f, 1.0f) * std::max(0.0f, plot_width - thumbW);
    gfx.fill_rect(Rect(thumbX, scrollTrack.y + 1.0f, thumbW, scrollbar_h - 2.0f), Theme::kScrollbarThumb);
}

bool SequencerWidget::handle_mouse_down(const MouseEvent &e) {
    if (e.button == MouseButton::Middle && bounds_.contains(e.pos) && e.pos.x >= bounds_.x + channel_width_) {
        is_panning_ = true;
        pan_start_x_ = e.pos.x;
        pan_start_frame_ = view_start_frame_;
        pan_end_frame_ = view_end_frame_;
        return true;
    }
    if (e.button == MouseButton::Left && bounds_.contains(e.pos) && e.pos.x >= bounds_.x + channel_width_) {
        is_scrubbing_ = true;
        const int frame = x_to_frame(e.pos.x);
        if (frame != current_frame_) {
            current_frame_ = frame;
            if (on_frame_change_) on_frame_change_(current_frame_);
        }
        return true;
    }
    return false;
}

bool SequencerWidget::handle_mouse_move(const MouseEvent &e) {
    if (is_panning_) {
        const float plotWidth = std::max(1.0f, bounds_.width - channel_width_);
        const float framesPerPixel = static_cast<float>(pan_end_frame_ - pan_start_frame_) / plotWidth;
        const int deltaFrames = static_cast<int>(std::round((pan_start_x_ - e.pos.x) * framesPerPixel));
        const int span = std::max(1, pan_end_frame_ - pan_start_frame_);
        const int maxStart = std::max(start_frame_, end_frame_ - span);
        view_start_frame_ = std::clamp(pan_start_frame_ + deltaFrames, start_frame_, maxStart);
        view_end_frame_ = std::min(end_frame_, view_start_frame_ + span);
        return true;
    }
    if (is_scrubbing_) {
        int frame = x_to_frame(e.pos.x);
        if (frame != current_frame_) {
            current_frame_ = frame;
            if (on_frame_change_) on_frame_change_(current_frame_);
        }
        return true;
    }
    return false;
}

bool SequencerWidget::handle_mouse_up(const MouseEvent &e) {
    if (is_panning_) {
        is_panning_ = false;
        return true;
    }
    if (is_scrubbing_) {
        is_scrubbing_ = false;
        return true;
    }
    return false;
}

bool SequencerWidget::handle_mouse_wheel(float delta, Point p) {
    if (!bounds_.contains(p) || std::abs(delta) < 1.0e-4f) return false;
    if (p.x < bounds_.x + channel_width_) {
        const float rowCount = static_cast<float>(channels_.size());
        const float visibleRows = std::max(1.0f, (bounds_.height - ruler_height_ - 7.0f) / row_height_);
        channel_scroll_ = std::clamp(channel_scroll_ - (delta > 0.0f ? 2.0f : -2.0f) * row_height_,
                                     0.0f, std::max(0.0f, rowCount * row_height_ - visibleRows * row_height_));
        return true;
    }
    zoom_at(delta > 0.0f ? 1.25f : 0.8f, p.x);
    return true;
}

} // namespace medusa
