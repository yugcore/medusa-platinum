#pragma once

#include <string>
#include <functional>
#include "medusa/core/medusa_types.hpp"
#include "medusa/graphics/medusa_graphics.hpp"
#include "medusa/ui/medusa_widgets.hpp"

namespace medusa {

class DCCTransformTriplet {
public:
    DCCTransformTriplet() = default;
    DCCTransformTriplet(const std::string& label, const std::string& unit,
                        float min_val, float max_val, float step,
                        std::function<void(int axis, float value)> on_change = nullptr);

    void set_bounds(const Rect& bounds);
    const Rect& get_bounds() const { return bounds_; }

    void set_label(const std::string& label) { label_ = label; }
    const std::string& get_label() const { return label_; }

    void set_show_label(bool show) { show_label_ = show; }
    bool get_show_label() const { return show_label_; }

    void set_visible(bool visible) { visible_ = visible; }
    bool is_visible() const { return visible_; }

    void set_values(float x, float y, float z);
    void set_value(int axis, float val);
    float get_value(int axis) const;

    void set_on_change(std::function<void(int axis, float value)> on_change);

    void draw(Graphics& gfx);
    bool handle_mouse_down(const MouseEvent& e);
    bool handle_mouse_move(const MouseEvent& e);
    bool handle_mouse_up(const MouseEvent& e);

    static float preferred_height(bool has_label = true);

private:
    void update_layout();

    Rect bounds_;
    std::string label_;
    std::string unit_;
    bool show_label_ = true;
    bool visible_ = true;

    ScrubbableNumberWidget widget_x_;
    ScrubbableNumberWidget widget_y_;
    ScrubbableNumberWidget widget_z_;

    std::function<void(int axis, float value)> on_change_;
};

} // namespace medusa
