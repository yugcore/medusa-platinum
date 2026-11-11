#pragma once
#include <medusa/medusa_types.hpp>
#include <medusa/medusa_graphics.hpp>
#include <medusa/medusa_theme.hpp>
#include <medusa/medusa_icons.hpp>
#include <functional>
#include <chrono>
#include <string>
#include <vector>

namespace medusa {

class ButtonWidget {
public:
    ButtonWidget(const std::string &label = "", const std::string &icon = "",
                 std::function<void()> on_click = nullptr);

    void set_bounds(const Rect &r) { bounds_ = r; }
    const Rect& get_bounds() const { return bounds_; }
    const std::string& get_label() const { return label_; }
    void set_label(const std::string &label) { label_ = label; }
    void set_active(bool active) { is_active_ = active; }
    void set_flat(bool flat) { is_flat_ = flat; }
    void set_flat_active_fill(bool enabled) { flat_active_fill_ = enabled; }
    void set_primary(bool primary) { is_primary_ = primary; }
    void set_tooltip(const std::string &tip) { tooltip_ = tip; }

    void draw(Graphics &gfx);
    void draw_tooltip(Graphics &gfx);
    bool handle_mouse_down(const MouseEvent &e);
    bool handle_mouse_move(const MouseEvent &e);
    bool handle_mouse_up(const MouseEvent &e);

private:
    Rect bounds_;
    std::string label_;
    std::string icon_;
    std::string tooltip_;
    std::function<void()> on_click_;
    bool is_hovered_ = false;
    bool is_pressed_ = false;
    bool is_active_ = false;
    bool is_flat_ = false;
    bool flat_active_fill_ = true;
    bool is_primary_ = false;
    std::chrono::steady_clock::time_point hover_started_{};
};

class ScrubbableNumberWidget {
public:
    ScrubbableNumberWidget() = default;
    ScrubbableNumberWidget(const std::string &axis_label, float value,
                           const std::string &unit, Color axis_color,
                           float min_val, float max_val, float step,
                           std::function<void(float)> on_change = nullptr);

    void set_bounds(const Rect &r) { bounds_ = r; }
    const Rect& get_bounds() const { return bounds_; }
    float get_value() const { return value_; }
    void set_value(float v) { value_ = v; }

    void draw(Graphics &gfx);
    bool handle_mouse_down(const MouseEvent &e);
    bool handle_mouse_move(const MouseEvent &e);
    bool handle_mouse_up(const MouseEvent &e);

private:
    Rect bounds_;
    std::string axis_label_;
    float value_ = 0.0f;
    std::string unit_;
    Color axis_color_;
    float min_val_ = -1000.0f;
    float max_val_ = 1000.0f;
    float step_ = 1.0f;
    std::function<void(float)> on_change_;

    bool is_hovered_ = false;
    bool is_dragging_ = false;
    float drag_start_x_ = 0.0f;
    float drag_start_val_ = 0.0f;
};

class CollapsibleSectionWidget {
public:
    CollapsibleSectionWidget(const std::string &title = "", bool expanded = true,
                             std::function<void(bool)> on_toggle = nullptr);

    void set_bounds(const Rect &r) { bounds_ = r; }
    const Rect& get_bounds() const { return bounds_; }
    bool is_expanded() const { return is_expanded_; }
    void set_expanded(bool exp) { is_expanded_ = exp; }

    void draw(Graphics &gfx);
    bool handle_mouse_down(const MouseEvent &e);
    bool handle_mouse_move(const MouseEvent &e);
    bool handle_mouse_up(const MouseEvent &e);

private:
    Rect bounds_;
    std::string title_;
    bool is_expanded_ = true;
    bool is_hovered_ = false;
    std::function<void(bool)> on_toggle_;
};

class OutlinerRowWidget {
public:
    OutlinerRowWidget(const std::string &name, const std::string &icon,
                      int indent_level, bool has_children, bool is_expanded,
                      bool is_selected, bool is_visible, bool is_locked, bool is_renderable = true,
                      std::function<void()> on_select = nullptr,
                      std::function<void(bool)> on_toggle_expand = nullptr,
                      std::function<void(bool)> on_toggle_vis = nullptr,
                      std::function<void(bool)> on_toggle_lock = nullptr,
                      std::function<void(bool)> on_toggle_render = nullptr);

    void set_bounds(const Rect &r) { bounds_ = r; }
    const Rect& get_bounds() const { return bounds_; }
    void set_selected(bool selected) { is_selected_ = selected; }
    void set_visible(bool visible) { is_visible_ = visible; }
    void set_locked(bool locked) { is_locked_ = locked; }
    void set_renderable(bool renderable) { is_renderable_ = renderable; }
    void set_expanded(bool expanded) { is_expanded_ = expanded; }

    void draw(Graphics &gfx);
    bool handle_mouse_down(const MouseEvent &e);
    bool handle_mouse_move(const MouseEvent &e);
    bool handle_mouse_up(const MouseEvent &e);

private:
    Rect bounds_;
    std::string name_;
    std::string icon_;
    int indent_level_ = 0;
    bool has_children_ = false;
    bool is_expanded_ = false;
    bool is_selected_ = false;
    bool is_visible_ = true;
    bool is_locked_ = false;
    bool is_renderable_ = true;

    std::function<void()> on_select_;
    std::function<void(bool)> on_toggle_expand_;
    std::function<void(bool)> on_toggle_vis_;
    std::function<void(bool)> on_toggle_lock_;
    std::function<void(bool)> on_toggle_render_;

    bool is_hovered_ = false;
};

struct DopeSheetChannel {
    std::string name;
    std::string icon = "channel_summary";
    int indent = 0;
    bool is_expanded = true;
    bool is_muted = false;
    bool is_locked = false;
    Color accent_color = Theme::kKeyframeGold;
    std::vector<int> keyframes;
};

class SequencerWidget {
public:
    SequencerWidget(int current_frame = 1, int total_frames = 250,
                    std::function<void(int)> on_frame_change = nullptr);

    void set_bounds(const Rect &r) { bounds_ = r; }
    const Rect& get_bounds() const { return bounds_; }
    void set_frame(int f);
    int get_frame() const { return current_frame_; }
    void set_frame_range(int startFrame, int endFrame, int totalFrames);
    void set_channels(std::vector<DopeSheetChannel> channels);

    void draw(Graphics &gfx);
    bool handle_mouse_down(const MouseEvent &e);
    bool handle_mouse_move(const MouseEvent &e);
    bool handle_mouse_up(const MouseEvent &e);
    bool handle_mouse_wheel(float delta, Point p);

private:
    float frame_to_x(int frame) const;
    int x_to_frame(float x) const;
    void zoom_at(float factor, float anchor_x);

    Rect bounds_;
    int current_frame_ = 1;
    int total_frames_ = 250;
    int start_frame_ = 1;
    int end_frame_ = 250;
    int view_start_frame_ = 1;
    int view_end_frame_ = 250;
    bool view_initialized_ = false;
    float channel_width_ = 152.0f;
    float ruler_height_ = 24.0f;
    float row_height_ = 20.0f;
    float channel_scroll_ = 0.0f;
    std::function<void(int)> on_frame_change_;
    bool is_scrubbing_ = false;
    bool is_panning_ = false;
    float pan_start_x_ = 0.0f;
    int pan_start_frame_ = 1;
    int pan_end_frame_ = 250;
    std::vector<DopeSheetChannel> channels_;
};

} // namespace medusa
