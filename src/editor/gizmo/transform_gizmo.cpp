#include "medusa/editor/gizmo/transform_gizmo.hpp"
#include "medusa/core/medusa_theme.hpp"
#include <cmath>
#include <algorithm>

namespace medusa {

bool ViewportTransformGizmo::is_available(const EditorState& state) const {
    if (state.selected_scene_node < 0 || static_cast<size_t>(state.selected_scene_node) >= state.scene_nodes.size()) {
        return false;
    }
    if (state.active_tool != "tool_move" && state.active_tool != "tool_rotate" && state.active_tool != "tool_scale") {
        return false;
    }
    const auto& node = state.scene_nodes[static_cast<size_t>(state.selected_scene_node)];
    if (node.locked) return false;
    if (node.type == zegfx::ProductionNodeType::Light || node.type == zegfx::ProductionNodeType::Camera) {
        return state.active_tool == "tool_move" || state.active_tool == "tool_rotate";
    }
    return node.type == zegfx::ProductionNodeType::Mesh || node.type == zegfx::ProductionNodeType::Group;
}

GizmoVec3 ViewportTransformGizmo::get_selected_pivot(const EditorState& state) const {
    if (state.camera_selected()) {
        return {state.camera_position[0], state.camera_position[1], state.camera_position[2]};
    }
    if (state.selected_scene_node >= 0 && static_cast<size_t>(state.selected_scene_node) < state.scene_nodes.size()) {
        const auto& node = state.scene_nodes[static_cast<size_t>(state.selected_scene_node)];
        return {node.worldTransform[12], node.worldTransform[13], node.worldTransform[14]};
    }
    return {};
}

float ViewportTransformGizmo::point_segment_distance(Point p, Point a, Point b) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    const float t = lengthSquared > 1.0e-4f
        ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared, 0.0f, 1.0f) : 0.0f;
    return std::hypot(p.x - (a.x + dx * t), p.y - (a.y + dy * t));
}

GizmoVec3 ViewportTransformGizmo::world_axis(int axis) {
    return axis == 0 ? GizmoVec3{1.0f, 0.0f, 0.0f} : axis == 1 ? GizmoVec3{0.0f, 1.0f, 0.0f} : GizmoVec3{0.0f, 0.0f, 1.0f};
}

void ViewportTransformGizmo::draw_axis(Graphics& gfx, GizmoVec3 pivot, int axis, float world_length,
                                       std::function<bool(const GizmoVec3&, Point&)> project_fn) const {
    Point start, end;
    if (!project_fn(pivot, start) || !project_fn(pivot + world_axis(axis) * world_length, end)) return;
    const Color colors[] = {Theme::kAxisX, Theme::kAxisY, Theme::kAxisZ};
    gfx.draw_line(start, end, colors[axis], 2.0f);
    const float angle = std::atan2(end.y - start.y, end.x - start.x);
    const Point a(end.x - std::cos(angle - 0.48f) * 8.0f, end.y - std::sin(angle - 0.48f) * 8.0f);
    const Point b(end.x - std::cos(angle + 0.48f) * 8.0f, end.y - std::sin(angle + 0.48f) * 8.0f);
    gfx.draw_line(end, a, colors[axis], 2.0f);
    gfx.draw_line(end, b, colors[axis], 2.0f);
    gfx.draw_text(axis == 0 ? "X" : axis == 1 ? "Y" : "Z",
                  Rect(end.x - 8.0f, end.y - 20.0f, 16.0f, 16.0f), colors[axis],
                  Theme::kSmallFont, true, true, true, Theme::kUiFontFamily);
}

void ViewportTransformGizmo::draw(Graphics& gfx, const EditorState& state, const GizmoViewBasis& view,
                                  std::function<bool(const GizmoVec3&, Point&)> project_fn) const {
    if (!is_available(state) || state.active_tool == "tool_select") return;
    const GizmoVec3 pivot = get_selected_pivot(state);
    const float depth = std::max(0.1f, dot(pivot - view.position, view.forward));
    const float world_length = std::max(0.15f, depth * 0.13f);
    Point center;
    if (!project_fn(pivot, center)) return;

    if (state.active_tool == "tool_rotate") {
        const Color colors[] = {Theme::kAxisX, Theme::kAxisY, Theme::kAxisZ};
        for (int axis = 0; axis < 3; ++axis) {
            const GizmoVec3 u = world_axis((axis + 1) % 3), v = world_axis((axis + 2) % 3);
            Point previous;
            bool hasPrevious = false;
            for (int step = 0; step <= 36; ++step) {
                const float angle = 6.28318530718f * static_cast<float>(step) / 36.0f;
                Point current;
                const GizmoVec3 position = pivot + (u * std::cos(angle) + v * std::sin(angle)) * world_length;
                const bool projected = project_fn(position, current);
                if (projected && hasPrevious) {
                    gfx.draw_line(previous, current, colors[axis], 1.7f);
                }
                previous = current;
                hasPrevious = projected;
            }
        }
    } else {
        for (int axis = 0; axis < 3; ++axis) {
            draw_axis(gfx, pivot, axis, world_length, project_fn);
        }
        if (state.active_tool == "tool_move") {
            const int planes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
            const Color planeColors[] = {Theme::kAxisY, Theme::kAxisZ, Theme::kAxisX};
            for (int i = 0; i < 3; ++i) {
                Point a, b;
                if (!project_fn(pivot + world_axis(planes[i][0]) * world_length * 0.26f, a) ||
                    !project_fn(pivot + world_axis(planes[i][1]) * world_length * 0.26f, b)) continue;
                const Point c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
                gfx.fill_rounded_rect(Rect(c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f), 2.0f,
                                      Color(planeColors[i].r, planeColors[i].g, planeColors[i].b, 0.55f));
            }
        }
    }
    gfx.fill_rounded_rect(Rect(center.x - 4.0f, center.y - 4.0f, 8.0f, 8.0f), 4.0f, Theme::kTextPrimary);
}

void ViewportTransformGizmo::draw_selection_outline(Graphics& gfx, const EditorState& state, const GizmoViewBasis& view,
                                                    std::function<bool(const GizmoVec3&, Point&)> project_fn) const {
    if (state.selected_scene_node < 0 || static_cast<size_t>(state.selected_scene_node) >= state.scene_nodes.size() ||
        static_cast<size_t>(state.selected_scene_node) >= state.scene_node_bounds.size()) return;
    const size_t index = static_cast<size_t>(state.selected_scene_node);
    const auto& node = state.scene_nodes[index];
    const auto& bounds = state.scene_node_bounds[index];
    if (node.type != zegfx::ProductionNodeType::Mesh || !bounds.valid) return;
    const GizmoVec3 center{node.worldTransform[12] + bounds.centerOffset[0],
                           node.worldTransform[13] + bounds.centerOffset[1],
                           node.worldTransform[14] + bounds.centerOffset[2]};
    const float radius = std::max(0.05f, bounds.radius * std::max({node.scaleFactor[0], node.scaleFactor[1], node.scaleFactor[2]}));
    Point centerScreen, rightScreen, upScreen;
    if (!project_fn(center, centerScreen) || !project_fn(center + view.right * radius, rightScreen) ||
        !project_fn(center + view.up * radius, upScreen)) return;
    const float halfWidth = std::max(8.0f, std::abs(rightScreen.x - centerScreen.x));
    const float halfHeight = std::max(8.0f, std::abs(upScreen.y - centerScreen.y));
    const Rect outline(centerScreen.x - halfWidth, centerScreen.y - halfHeight,
                       halfWidth * 2.0f, halfHeight * 2.0f);
    gfx.draw_rect(outline, Color(0.78f, 0.81f, 0.85f, 0.72f), 1.0f);
}

int ViewportTransformGizmo::hit_test(Point point, const EditorState& state, const GizmoViewBasis& view,
                                     std::function<bool(const GizmoVec3&, Point&)> project_fn) const {
    if (!is_available(state) || state.active_tool == "tool_select") return -1;
    const GizmoVec3 pivot = get_selected_pivot(state);
    const float depth = std::max(0.1f, dot(pivot - view.position, view.forward));
    const float world_length = std::max(0.15f, depth * 0.13f);
    Point center;
    if (!project_fn(pivot, center)) return -1;

    if (state.active_tool == "tool_rotate") {
        int bestAxis = -1;
        float bestDistance = 10.0f;
        for (int axis = 0; axis < 3; ++axis) {
            const GizmoVec3 u = world_axis((axis + 1) % 3), v = world_axis((axis + 2) % 3);
            Point previous;
            bool hasPrevious = false;
            for (int step = 0; step <= 36; ++step) {
                const float angle = 6.28318530718f * static_cast<float>(step) / 36.0f;
                Point current;
                const GizmoVec3 position = pivot + (u * std::cos(angle) + v * std::sin(angle)) * world_length;
                const bool projected = project_fn(position, current);
                if (projected && hasPrevious) {
                    const float distance = point_segment_distance(point, previous, current);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        bestAxis = axis;
                    }
                }
                previous = current;
                hasPrevious = projected;
            }
        }
        return bestAxis;
    }
    if (state.active_tool == "tool_scale" && std::hypot(point.x - center.x, point.y - center.y) <= 9.0f) {
        return 6; // Uniform scale
    }
    if (state.active_tool == "tool_move") {
        const int planes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
        for (int i = 0; i < 3; ++i) {
            Point a, b;
            if (project_fn(pivot + world_axis(planes[i][0]) * world_length * 0.26f, a) &&
                project_fn(pivot + world_axis(planes[i][1]) * world_length * 0.26f, b)) {
                const float cx = (a.x + b.x) * 0.5f, cy = (a.y + b.y) * 0.5f;
                if (std::abs(point.x - cx) <= 8.0f && std::abs(point.y - cy) <= 8.0f) {
                    return 3 + i;
                }
            }
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        Point end;
        if (project_fn(pivot + world_axis(axis) * world_length, end) &&
            point_segment_distance(point, center, end) <= 8.0f) {
            return axis;
        }
    }
    return -1;
}

bool ViewportTransformGizmo::begin_drag(int handle, Point mouse, EditorState& state, const GizmoViewBasis& /*view*/) {
    if (!is_available(state)) return false;
    drag_ = {};
    drag_.active = true;
    drag_.handle = handle;
    drag_.node = state.selected_scene_node;
    drag_.pivot = get_selected_pivot(state);
    const auto& node = state.scene_nodes[static_cast<size_t>(drag_.node)];
    for (int axis = 0; axis < 3; ++axis) {
        drag_.position[axis] = state.camera_selected() ? state.camera_position[axis] : node.worldTransform[12 + axis];
        drag_.rotation[axis] = state.camera_selected() ? state.camera_rotation[axis] : node.rotationOffset[axis];
        drag_.scale[axis] = node.scaleFactor[axis];
    }
    drag_.drag_start = mouse;
    return true;
}

void ViewportTransformGizmo::update_drag(Point mouse, EditorState& state, const GizmoViewBasis& view,
                                        float canvas_height, std::function<bool(const GizmoVec3&, Point&)> project_fn) {
    const int handle = drag_.handle;
    const int nodeIndex = drag_.node;
    if (!drag_.active || nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= state.scene_nodes.size()) return;

    const float dx = mouse.x - drag_.drag_start.x;
    const float dy = mouse.y - drag_.drag_start.y;
    float position[3] = {drag_.position[0], drag_.position[1], drag_.position[2]};
    float rotation[3] = {drag_.rotation[0], drag_.rotation[1], drag_.rotation[2]};
    float scale[3] = {drag_.scale[0], drag_.scale[1], drag_.scale[2]};

    const GizmoVec3 pivot = drag_.pivot;
    const float current_distance = std::max(0.1f, state.viewport_navigation_active ? state.viewport_distance : state.focus_distance);
    const float unitsPerPixel = 2.0f * current_distance * std::tan(view.verticalFov * 0.5f) / std::max(1.0f, canvas_height);
    auto snapped = [&state](float val) { return state.snap_enabled ? std::round(val * 10.0f) * 0.1f : val; };

    if (state.active_tool == "tool_move") {
        GizmoVec3 delta{};
        if (handle <= 2) {
            Point center, endpoint;
            const float world_length = std::max(0.15f, current_distance * 0.13f);
            if (project_fn(pivot, center) && project_fn(pivot + world_axis(handle) * world_length, endpoint)) {
                const float sx = endpoint.x - center.x, sy = endpoint.y - center.y;
                const float pixels = std::hypot(sx, sy);
                const float along = pixels > 0.01f ? (dx * sx + dy * sy) / pixels : -dy;
                const float amount = along * world_length / std::max(1.0f, pixels);
                delta = world_axis(handle) * amount;
            }
        } else {
            const int planes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
            const int planeIndex = std::clamp(handle - 3, 0, 2);
            GizmoVec3 movement = view.right * (dx * unitsPerPixel) + view.up * (-dy * unitsPerPixel);
            const int a = planes[planeIndex][0], b = planes[planeIndex][1];
            if (a != 0) movement.x = 0.0f;
            if (a != 1 && b != 1) movement.y = 0.0f;
            if (b != 2) movement.z = 0.0f;
            delta = movement;
        }
        position[0] = snapped(position[0] + delta.x);
        position[1] = snapped(position[1] + delta.y);
        position[2] = snapped(position[2] + delta.z);
    } else if (state.active_tool == "tool_rotate") {
        const int axis = handle == 0 ? 0 : handle == 1 ? 1 : 2;
        float amount = (dx - dy) * 0.55f;
        if (state.snap_enabled) amount = std::round(amount / 15.0f) * 15.0f;
        rotation[axis] += amount;
    } else if (state.active_tool == "tool_scale") {
        const float factor = std::exp((dx - dy) * 0.008f);
        if (handle == 6) {
            for (int axis = 0; axis < 3; ++axis) {
                scale[axis] = std::clamp(drag_.scale[axis] * factor, 0.001f, 1000.0f);
            }
        } else {
            const int axis = std::clamp(handle, 0, 2);
            scale[axis] = std::clamp(drag_.scale[axis] * factor, 0.001f, 1000.0f);
        }
        if (state.snap_enabled) {
            for (float& value : scale) {
                value = std::clamp(std::round(value * 10.0f) * 0.1f, 0.001f, 1000.0f);
            }
        }
    }

    const auto type = state.scene_nodes[static_cast<size_t>(nodeIndex)].type;
    if (type == zegfx::ProductionNodeType::Camera) {
        for (int axis = 0; axis < 3; ++axis) {
            state.camera_position[axis] = position[axis];
            state.camera_rotation[axis] = rotation[axis];
            state.scene_nodes[static_cast<size_t>(nodeIndex)].worldTransform[12 + axis] = position[axis];
        }
        state.pos_x = position[0]; state.pos_y = position[1]; state.pos_z = position[2];
        state.rot_x = rotation[0]; state.rot_y = rotation[1]; state.rot_z = rotation[2];
        state.update_camera_orbit_target();
        state.record_keyframe_if_enabled();
    } else if (type == zegfx::ProductionNodeType::Light) {
        if (state.active_tool == "tool_move") {
            state.key_light_x = position[0]; state.key_light_y = position[1]; state.key_light_z = position[2];
            state.set_scene_node_translation(static_cast<uint32_t>(nodeIndex), position);
        } else {
            state.set_scene_node_rotation_scale(static_cast<uint32_t>(nodeIndex), rotation, scale);
        }
        state.record_keyframe_if_enabled();
    } else {
        state.pos_x = position[0]; state.pos_y = position[1]; state.pos_z = position[2];
        state.rot_x = rotation[0]; state.rot_y = rotation[1]; state.rot_z = rotation[2];
        state.scale_x = scale[0]; state.scale_y = scale[1]; state.scale_z = scale[2];
        if (state.active_tool == "tool_move") {
            state.set_scene_node_translation(static_cast<uint32_t>(nodeIndex), position);
        } else {
            state.set_scene_node_rotation_scale(static_cast<uint32_t>(nodeIndex), rotation, scale);
        }
    }
}

void ViewportTransformGizmo::end_drag() {
    drag_.active = false;
    drag_.handle = -1;
}

} // namespace medusa
