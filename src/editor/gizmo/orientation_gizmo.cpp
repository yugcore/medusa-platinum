#include "medusa/editor/gizmo/orientation_gizmo.hpp"
#include "medusa/core/medusa_theme.hpp"
#include <cmath>
#include <algorithm>
#include <array>

namespace medusa {

void ViewportOrientationGizmo::draw(Graphics& gfx, const GizmoViewBasis& view) {
    const Point center(bounds_.x + bounds_.width * 0.5f,
                       bounds_.y + bounds_.height * 0.5f);

    // Circular, opaque compass keeping the viewport background clean
    const Rect compass(center.x - 28.0f, center.y - 28.0f, 56.0f, 56.0f);
    gfx.fill_rounded_rect(compass, 28.0f, Theme::kPanelBg);
    gfx.draw_rounded_rect(compass, 28.0f, Color(0.30f, 0.30f, 0.30f, 1.0f), 1.0f);

    orientation_endpoints_.clear();
    const Color colors[] = {Theme::kAxisX, Theme::kAxisY, Theme::kAxisZ};
    const char* names[] = {"X", "Y", "Z"};

    struct AxisEnd {
        Point point;
        GizmoVec3 direction;
        float depth;
        int axis;
        int sign;
    };
    std::array<AxisEnd, 6> ends{};
    size_t endCount = 0;

    for (int axis = 0; axis < 3; ++axis) {
        const GizmoVec3 direction = (axis == 0) ? GizmoVec3{1.0f, 0.0f, 0.0f} :
                                    (axis == 1) ? GizmoVec3{0.0f, 1.0f, 0.0f} :
                                                  GizmoVec3{0.0f, 0.0f, 1.0f};
        for (int sign : {-1, 1}) {
            const GizmoVec3 signedAxis = direction * static_cast<float>(sign);
            const Point endpoint(center.x + dot(signedAxis, view.right) * 18.0f,
                                 center.y - dot(signedAxis, view.up) * 18.0f);
            ends[endCount++] = {endpoint, signedAxis, dot(signedAxis, view.forward), axis, sign};
        }
    }

    std::sort(ends.begin(), ends.end(), [](const AxisEnd& a, const AxisEnd& b) {
        return a.depth > b.depth;
    });

    for (const AxisEnd& end : ends) {
        const bool front = end.depth < 0.0f;
        const Color axisColor = front ? colors[end.axis] :
            Color(colors[end.axis].r * 0.58f, colors[end.axis].g * 0.58f,
                  colors[end.axis].b * 0.58f, 0.62f);
        gfx.draw_line(center, end.point, axisColor, front ? 2.2f : 1.4f);

        const float nodeSize = front ? 9.0f : 7.0f;
        const Rect node(end.point.x - nodeSize * 0.5f, end.point.y - nodeSize * 0.5f, nodeSize, nodeSize);
        gfx.fill_rounded_rect(node, nodeSize * 0.5f, axisColor);
        gfx.draw_rounded_rect(node, nodeSize * 0.5f, Color(0.06f, 0.06f, 0.07f, 0.94f), 1.0f);

        const float screenX = end.point.x - center.x;
        const float screenY = end.point.y - center.y;
        const float screenLength = std::hypot(screenX, screenY);
        if (end.sign > 0 && screenLength > 5.0f) {
            const float labelX = center.x + screenX / screenLength * 24.0f;
            const float labelY = center.y + screenY / screenLength * 24.0f;
            gfx.draw_text(names[end.axis], Rect(labelX - 6.0f, labelY - 5.0f, 12.0f, 10.0f),
                          front ? Theme::kTextPrimary : Theme::kTextMuted,
                          8.5f, true, true, true, Theme::kUiFontFamily);
        }
        orientation_endpoints_.push_back({end.point, end.direction});
    }

    const Rect centerNode(center.x - 4.0f, center.y - 4.0f, 8.0f, 8.0f);
    gfx.fill_rounded_rect(centerNode, 4.0f, Color(0.68f, 0.70f, 0.73f, 1.0f));
    gfx.draw_rounded_rect(centerNode, 4.0f, Color(0.05f, 0.05f, 0.06f, 1.0f), 1.0f);
}

bool ViewportOrientationGizmo::snap_to_axis(Point mouse, EditorState& state,
                                            std::function<void()> on_view_initialized,
                                            std::function<void()> on_view_updated) {
    if (orientation_endpoints_.empty()) return false;
    auto closest = std::min_element(orientation_endpoints_.begin(), orientation_endpoints_.end(),
        [mouse](const auto& a, const auto& b) {
            return std::hypot(mouse.x - a.first.x, mouse.y - a.first.y) <
                   std::hypot(mouse.x - b.first.x, mouse.y - b.first.y);
        });
    if (closest == orientation_endpoints_.end() ||
        std::hypot(mouse.x - closest->first.x, mouse.y - closest->first.y) > 12.0f) {
        return false;
    }

    if (on_view_initialized) on_view_initialized();
    state.viewport_navigation_active = true;
    const GizmoVec3 forward = closest->second * -1.0f;
    state.viewport_rotation[0] = std::asin(std::clamp(forward.y, -1.0f, 1.0f)) * 57.2957795f;
    state.viewport_rotation[1] = std::atan2(forward.x, -forward.z) * 57.2957795f;
    if (on_view_updated) on_view_updated();
    return true;
}

} // namespace medusa
