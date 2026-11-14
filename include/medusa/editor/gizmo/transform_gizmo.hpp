#pragma once

#include <functional>
#include "medusa/core/medusa_types.hpp"
#include "medusa/graphics/medusa_graphics.hpp"
#include "medusa/editor/editor_state.hpp"
#include "medusa/editor/gizmo/gizmo_types.hpp"

namespace medusa {

class ViewportTransformGizmo {
public:
    struct DragState {
        bool active = false;
        int handle = -1; // 0: X, 1: Y, 2: Z, 3: XY plane, 4: XZ plane, 5: YZ plane, 6: Uniform Scale
        int node = -1;
        GizmoVec3 pivot;
        float position[3] = {0.0f, 0.0f, 0.0f};
        float rotation[3] = {0.0f, 0.0f, 0.0f};
        float scale[3] = {1.0f, 1.0f, 1.0f};
        Point drag_start;
    };

    ViewportTransformGizmo() = default;

    bool is_available(const EditorState& state) const;
    GizmoVec3 get_selected_pivot(const EditorState& state) const;

    static float point_segment_distance(Point p, Point a, Point b);
    static GizmoVec3 world_axis(int axis);

    void draw(Graphics& gfx, const EditorState& state, const GizmoViewBasis& view,
              std::function<bool(const GizmoVec3&, Point&)> project_fn) const;

    void draw_selection_outline(Graphics& gfx, const EditorState& state, const GizmoViewBasis& view,
                                std::function<bool(const GizmoVec3&, Point&)> project_fn) const;

    int hit_test(Point point, const EditorState& state, const GizmoViewBasis& view,
                 std::function<bool(const GizmoVec3&, Point&)> project_fn) const;

    bool begin_drag(int handle, Point mouse, EditorState& state, const GizmoViewBasis& view);

    void update_drag(Point mouse, EditorState& state, const GizmoViewBasis& view,
                     float canvas_height, std::function<bool(const GizmoVec3&, Point&)> project_fn);

    void end_drag();

    bool is_dragging() const { return drag_.active; }
    const DragState& get_drag_state() const { return drag_; }

private:
    void draw_axis(Graphics& gfx, GizmoVec3 pivot, int axis, float world_length,
                   std::function<bool(const GizmoVec3&, Point&)> project_fn) const;

    DragState drag_;
};

} // namespace medusa
