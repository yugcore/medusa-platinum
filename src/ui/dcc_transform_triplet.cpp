#include "medusa/ui/dcc_transform_triplet.hpp"
#include "medusa/core/medusa_theme.hpp"
#include <algorithm>

namespace medusa {

DCCTransformTriplet::DCCTransformTriplet(const std::string& label, const std::string& unit,
                                         float min_val, float max_val, float step,
                                         std::function<void(int axis, float value)> on_change)
    : label_(label), unit_(unit), on_change_(std::move(on_change)) {
    widget_x_ = ScrubbableNumberWidget("X", 0.0f, unit_, Theme::kAxisX, min_val, max_val, step,
        [this](float val) {
            if (on_change_) on_change_(0, val);
        });
    widget_y_ = ScrubbableNumberWidget("Y", 0.0f, unit_, Theme::kAxisY, min_val, max_val, step,
        [this](float val) {
            if (on_change_) on_change_(1, val);
        });
    widget_z_ = ScrubbableNumberWidget("Z", 0.0f, unit_, Theme::kAxisZ, min_val, max_val, step,
        [this](float val) {
            if (on_change_) on_change_(2, val);
        });
}

float DCCTransformTriplet::preferred_height(bool has_label) {
    return has_label ? (14.0f + Theme::kControlHeight) : Theme::kControlHeight;
}

void DCCTransformTriplet::set_bounds(const Rect& bounds) {
    bounds_ = bounds;
    update_layout();
}

void DCCTransformTriplet::update_layout() {
    float y = bounds_.y;
    if (show_label_) {
        y += 14.0f;
    }
    const float total_w = bounds_.width;
    const float gap = 6.0f;
    const float scrubber_w = std::max(20.0f, (total_w - gap * 2.0f) / 3.0f);

    widget_x_.set_bounds(Rect(bounds_.x, y, scrubber_w, Theme::kControlHeight));
    widget_y_.set_bounds(Rect(bounds_.x + scrubber_w + gap, y, scrubber_w, Theme::kControlHeight));
    widget_z_.set_bounds(Rect(bounds_.x + (scrubber_w + gap) * 2.0f, y, scrubber_w, Theme::kControlHeight));
}

void DCCTransformTriplet::set_values(float x, float y, float z) {
    widget_x_.set_value(x);
    widget_y_.set_value(y);
    widget_z_.set_value(z);
}

void DCCTransformTriplet::set_value(int axis, float val) {
    if (axis == 0) widget_x_.set_value(val);
    else if (axis == 1) widget_y_.set_value(val);
    else if (axis == 2) widget_z_.set_value(val);
}

float DCCTransformTriplet::get_value(int axis) const {
    if (axis == 0) return widget_x_.get_value();
    if (axis == 1) return widget_y_.get_value();
    if (axis == 2) return widget_z_.get_value();
    return 0.0f;
}

void DCCTransformTriplet::set_on_change(std::function<void(int axis, float value)> on_change) {
    on_change_ = std::move(on_change);
}

void DCCTransformTriplet::draw(Graphics& gfx) {
    if (!visible_) return;

    if (show_label_ && !label_.empty()) {
        const Rect label_r(bounds_.x, bounds_.y, bounds_.width, 13.0f);
        gfx.draw_text(label_, label_r, Theme::kTextMuted, Theme::kSmallFont,
                      false, false, true, Theme::kUiFontFamily);
    }

    widget_x_.draw(gfx);
    widget_y_.draw(gfx);
    widget_z_.draw(gfx);
}

bool DCCTransformTriplet::handle_mouse_down(const MouseEvent& e) {
    if (!visible_) return false;
    if (widget_x_.handle_mouse_down(e)) return true;
    if (widget_y_.handle_mouse_down(e)) return true;
    if (widget_z_.handle_mouse_down(e)) return true;
    return false;
}

bool DCCTransformTriplet::handle_mouse_move(const MouseEvent& e) {
    if (!visible_) return false;
    bool handled = false;
    if (widget_x_.handle_mouse_move(e)) handled = true;
    if (widget_y_.handle_mouse_move(e)) handled = true;
    if (widget_z_.handle_mouse_move(e)) handled = true;
    return handled;
}

bool DCCTransformTriplet::handle_mouse_up(const MouseEvent& e) {
    if (!visible_) return false;
    bool handled = false;
    if (widget_x_.handle_mouse_up(e)) handled = true;
    if (widget_y_.handle_mouse_up(e)) handled = true;
    if (widget_z_.handle_mouse_up(e)) handled = true;
    return handled;
}

} // namespace medusa
