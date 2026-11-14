#pragma once

#include <vector>
#include <utility>
#include <functional>
#include "medusa/core/medusa_types.hpp"
#include "medusa/graphics/medusa_graphics.hpp"
#include "medusa/editor/editor_state.hpp"
#include "medusa/editor/gizmo/gizmo_types.hpp"

namespace medusa {

class ViewportOrientationGizmo {
public:
    ViewportOrientationGizmo() = default;

    void set_bounds(const Rect& bounds) { bounds_ = bounds; }
    const Rect& get_bounds() const { return bounds_; }
    bool contains(Point p) const { return bounds_.contains(p); }

    void draw(Graphics& gfx, const GizmoViewBasis& view);

    bool snap_to_axis(Point mouse, EditorState& state,
                      std::function<void()> on_view_initialized,
                      std::function<void()> on_view_updated);

private:
    Rect bounds_{0.0f, 0.0f, 64.0f, 64.0f};
    std::vector<std::pair<Point, GizmoVec3>> orientation_endpoints_;
};

} // namespace medusa
