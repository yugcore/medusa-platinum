#include <medusa/editor/regions/editor_toolshelf.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <algorithm>
#include <utility>

namespace medusa {

ToolshelfRegion::ToolshelfRegion(EditorState &state) : state_(state) {
    init_tools();
}

Rect ToolshelfRegion::clip_bounds() const {
    return Rect(bounds_.x, bounds_.y, 500.0f, bounds_.height);
}

void ToolshelfRegion::layout(const Rect &bounds) {
    bounds_ = bounds;
    const float size = std::max(24.0f, std::min(32.0f, bounds.width - 8.0f));
    const float x = bounds.x + (bounds.width - size) * 0.5f;
    const float first_y = bounds.y + 8.0f;
    const float row_gap = 4.0f;
    float y = first_y;
    separator_ys_.clear();
    for (size_t i = 0; i < tool_buttons_.size(); ++i) {
        if (i == 4 || i == 6) {
            separator_ys_.push_back(y + 3.5f);
            y += 7.0f;
        }
        tool_buttons_[i].set_bounds(Rect(x, y, size, size));
        y += size + row_gap;
    }
}

void ToolshelfRegion::draw(Graphics &gfx) {
    gfx.fill_rect(bounds_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(bounds_.right(), bounds_.y), Point(bounds_.right(), bounds_.bottom()), Theme::kBorderSubtle, 1.0f);
    for (float y : separator_ys_)
        gfx.draw_line(Point(bounds_.x + 9.0f, y), Point(bounds_.right() - 9.0f, y), Theme::kBorderSubtle, 1.0f);

    for (size_t i = 0; i < tool_buttons_.size(); ++i) {
        tool_buttons_[i].set_active(tool_ids_[i] == state_.active_tool ||
            (tool_ids_[i] == "tool_snapping" && state_.snap_enabled) ||
            (tool_ids_[i] == "tool_camera_pilot" && state_.camera_pilot_active));
        tool_buttons_[i].draw(gfx);
    }
    for (auto &btn : tool_buttons_) btn.draw_tooltip(gfx);
}

bool ToolshelfRegion::handle_mouse_down(const MouseEvent &e) {
    for (size_t i = 0; i < tool_buttons_.size(); ++i) {
        if (tool_buttons_[i].handle_mouse_down(e)) {
            if (tool_ids_[i] == "tool_snapping") {
                state_.snap_enabled = !state_.snap_enabled;
                return true;
            }
            if (tool_ids_[i] == "tool_camera_pilot") {
                state_.camera_pilot_active = !state_.camera_pilot_active;
                if (state_.camera_pilot_active && state_.active_workspace != "Shot")
                    state_.enter_viewport_camera();
                return true;
            }
            if (state_.active_workspace == "Shot" &&
                tool_ids_[i] != "tool_move" && tool_ids_[i] != "tool_rotate" &&
                tool_ids_[i] != "tool_select") return true;
            if (tool_ids_[i] == "tool_pivot") return true;
            state_.active_tool = tool_ids_[i];
            return true;
        }
    }
    return false;
}

bool ToolshelfRegion::handle_mouse_move(const MouseEvent &e) {
    bool changed = false;
    for (auto &btn : tool_buttons_) {
        if (btn.handle_mouse_move(e)) changed = true;
    }
    return changed;
}

bool ToolshelfRegion::handle_mouse_up(const MouseEvent &e) {
    for (auto &btn : tool_buttons_) {
        if (btn.handle_mouse_up(e)) return true;
    }
    return false;
}

void ToolshelfRegion::init_tools() {
    std::vector<std::pair<std::string, std::string>> tools = {
        { "tool_select", "Select imported scene items from the Scene Collection" },
        { "tool_move", "Move the selected object with axis and plane handles" },
        { "tool_rotate", "Rotate the selected object around an axis" },
        { "tool_scale", "Scale the selected object along an axis" },
        { "tool_snapping", "Toggle transform snapping" },
        { "tool_pivot", "Frame the selected object in the viewport" },
        { "tool_camera_pilot", "Camera Pilot: right-drag to look, WASD to move, Q/E for vertical movement. Shift speeds up; Ctrl slows down. Shift+F enters, Esc exits." }
    };

    for (size_t i = 0; i < tools.size(); ++i) {
        std::function<void()> action;
        if (tools[i].first == "tool_pivot")
            action = [this]() { state_.pending_command = "frame_selected"; };
        ButtonWidget btn("", tools[i].first, std::move(action));
        btn.set_flat(true);
        btn.set_flat_active_fill(true);
        btn.set_tooltip(tools[i].second);
        tool_ids_.push_back(tools[i].first);
        if (tools[i].first == state_.active_tool) btn.set_active(true);
        tool_buttons_.push_back(btn);
    }
}

} // namespace medusa
