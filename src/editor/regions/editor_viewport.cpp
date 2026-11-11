#include <medusa/editor/regions/editor_viewport.hpp>
#include <medusa/medusa_theme.hpp>
#include <medusa/medusa_icons.hpp>
#include <algorithm>
#include <array>
#include <cmath>

namespace medusa {

ViewportRegion::ViewportRegion(EditorState &state) : state_(state) {
    init_menus();
}

bool ViewportRegion::accepts_input(Point p) const {
    return bounds_.contains(p) || popup_open_;
}

Rect ViewportRegion::clip_bounds() const {
    return bounds_;
}

void ViewportRegion::layout(const Rect &bounds) {
    bounds_ = bounds;
    header_rect_ = Rect(bounds.x, bounds.y, bounds.width, Theme::kAreaHeaderHeight);
    canvas_rect_ = Rect(bounds.x, bounds.y + Theme::kAreaHeaderHeight, bounds.width, bounds.height - Theme::kAreaHeaderHeight);
    float mx = header_rect_.x + 28.0f;
    for (auto &button : menu_buttons_) {
        const float mw = std::max(48.0f, static_cast<float>(button.get_label().length()) * 6.6f + 18.0f);
        button.set_bounds(Rect(mx, header_rect_.y + 2.0f, mw, header_rect_.height - 4.0f));
        mx += mw + 6.0f;
    }
    const float shadingWidth = std::min(132.0f, std::max(112.0f, header_rect_.width * 0.16f));
    shading_rect_ = Rect(std::max(mx + 122.0f, header_rect_.right() - shadingWidth - 8.0f),
                         header_rect_.y + 2.0f, shadingWidth, header_rect_.height - 4.0f);
    camera_rect_ = Rect(mx + 8.0f, header_rect_.y + 2.0f,
                        std::max(100.0f, std::min(270.0f, shading_rect_.x - mx - 16.0f)),
                        header_rect_.height - 4.0f);
    orientation_rect_ = Rect(canvas_rect_.right() - 74.0f, canvas_rect_.y + 6.0f, 64.0f, 64.0f);
}

void ViewportRegion::draw(Graphics &gfx) {
    // 1. Viewport Header Bar (Blender 3D Viewport header style)
    gfx.fill_rect(header_rect_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(header_rect_.x, header_rect_.bottom()), Point(header_rect_.right(), header_rect_.bottom()), Theme::kBorderSubtle, 1.0f);

    // Editor Icon
    Rect icon_r(header_rect_.x + 8.0f, header_rect_.y + 5.0f, 14.0f, 14.0f);
    IconRenderer::draw_icon(gfx, "camera_actor", icon_r, Theme::kTextSecondary);

    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        menu_buttons_[i].set_active(popup_open_ && active_menu_ == static_cast<int>(i));
        menu_buttons_[i].draw(gfx);
    }

    // Camera Switcher (Aligned dedicated icon + clean label)
    if (camera_rect_.contains(last_mouse_pos_) || (popup_open_ && active_menu_ == 4)) {
        gfx.fill_rect(camera_rect_, Theme::kControlHover);
        gfx.draw_rect(camera_rect_, Theme::kBorderSubtle, 1.0f);
    }
    float cam_icon_sz = 14.0f;
    float cam_x = camera_rect_.x + 8.0f;
    float cam_icon_y = header_rect_.y + (header_rect_.height - cam_icon_sz) * 0.5f;
    const bool cameraPilotActive = state_.camera_pilot_active;
    const bool shotWorkspace = state_.active_workspace == "Shot";
    const bool isScene = (state_.active_workspace == "Scene");
    IconRenderer::draw_icon(gfx, shotWorkspace ? "camera_actor" : "view_grid",
                            Rect(cam_x, cam_icon_y, cam_icon_sz, cam_icon_sz), Theme::kTextSecondary);
    cam_x += cam_icon_sz + 6.0f;
    const std::string camera_label = shotWorkspace ? "SHOT CAMERA  ·  WASD MOVE  ·  RMB LOOK" :
        cameraPilotActive ? "FREE VIEW  ·  WASD MOVE  ·  RMB LOOK" :
        isScene ? "User Perspective  ▾" :
        state_.active_camera.empty() ? "No active camera" :
        state_.active_camera + " (" + std::to_string(static_cast<int>(std::lround(state_.focal_length))) + "mm)  ▾";
    gfx.draw_text(camera_label, Rect(cam_x, camera_rect_.y, camera_rect_.right() - cam_x, camera_rect_.height),
                  (cameraPilotActive || shotWorkspace) ? Theme::kAccent : Theme::kTextPrimary,
                  Theme::kUiFont, cameraPilotActive || shotWorkspace, false, true, Theme::kUiFontFamily);

    const bool shadingHovered = shading_rect_.contains(last_mouse_pos_) || (popup_open_ && active_menu_ == 5);
    if (shadingHovered) gfx.fill_rect(shading_rect_, Theme::kControlHover);
    gfx.draw_rect(shading_rect_, shadingHovered ? Theme::kBorderBright : Theme::kBorderSubtle, 1.0f);
    const char* shadingLabel = state_.viewport_shading == ViewportShadingMode::Wireframe ? "Wireframe" :
        state_.viewport_shading == ViewportShadingMode::Solid ? "Solid" : "Rendered";
    gfx.draw_text(std::string(shadingLabel) + "  ▾",
                  Rect(shading_rect_.x + 8.0f, shading_rect_.y,
                       shading_rect_.width - 16.0f, shading_rect_.height),
                  Theme::kTextPrimary, Theme::kUiFont, false, false, true, Theme::kUiFontFamily);

    // 2. Viewport 3D Canvas
    gfx.fill_rect(canvas_rect_, Theme::kViewportBg);
    if (state_.show_grid) {
        gfx.push_clip(canvas_rect_);
        draw_world_grid(gfx);
        draw_camera_motion_trail(gfx);
        draw_starter_cube(gfx);
        if (state_.scene_nodes.empty()) draw_empty_scene_label(gfx);
        gfx.pop_clip();
    }

    // 3. Cinematic Film Safe Frame (Letterbox Matte & Guides) - Only active in Shot workspace
    const bool isShotWorkspace = (state_.active_workspace == "Shot");
    if (isShotWorkspace && state_.show_safe_frame && canvas_rect_.width > 10.0f && canvas_rect_.height > 10.0f) {
        const float target_aspect = 2.39f;
        const float margin = 16.0f;
        const float avail_w = canvas_rect_.width - margin * 2.0f;
        const float avail_h = canvas_rect_.height - margin * 2.0f;

        float frame_w = avail_w;
        float frame_h = frame_w / target_aspect;
        if (frame_h > avail_h) {
            frame_h = avail_h;
            frame_w = frame_h * target_aspect;
        }

        const float fx0 = canvas_rect_.x + (canvas_rect_.width - frame_w) * 0.5f;
        const float fy0 = canvas_rect_.y + (canvas_rect_.height - frame_h) * 0.5f;
        const float fx1 = fx0 + frame_w;
        const float fy1 = fy0 + frame_h;

        // Dark cinema matte (Passepartout mask)
        Color matte(0.04f, 0.04f, 0.05f, 0.65f);
        if (fy0 > canvas_rect_.y)
            gfx.fill_rect(Rect(canvas_rect_.x, canvas_rect_.y, canvas_rect_.width, fy0 - canvas_rect_.y), matte);
        if (fy1 < canvas_rect_.bottom())
            gfx.fill_rect(Rect(canvas_rect_.x, fy1, canvas_rect_.width, canvas_rect_.bottom() - fy1), matte);
        if (fx0 > canvas_rect_.x)
            gfx.fill_rect(Rect(canvas_rect_.x, fy0, fx0 - canvas_rect_.x, frame_h), matte);
        if (fx1 < canvas_rect_.right())
            gfx.fill_rect(Rect(fx1, fy0, canvas_rect_.right() - fx1, frame_h), matte);

        // Safe frame outline
        gfx.draw_rect(Rect(fx0, fy0, frame_w, frame_h), Theme::kAccent, 1.0f);

        // Thirds Composition Guides (subtle crossbars)
        Color guide_col(Theme::kAccent.r, Theme::kAccent.g, Theme::kAccent.b, 0.22f);
        const float x_third = frame_w / 3.0f;
        const float y_third = frame_h / 3.0f;
        gfx.draw_line(Point(fx0 + x_third, fy0), Point(fx0 + x_third, fy1), guide_col, 1.0f);
        gfx.draw_line(Point(fx0 + x_third * 2.0f, fy0), Point(fx0 + x_third * 2.0f, fy1), guide_col, 1.0f);
        gfx.draw_line(Point(fx0, fy0 + y_third), Point(fx1, fy0 + y_third), guide_col, 1.0f);
        gfx.draw_line(Point(fx0, fy0 + y_third * 2.0f), Point(fx1, fy0 + y_third * 2.0f), guide_col, 1.0f);

        // Center crosshair (subtle micro cross)
        const float cx = fx0 + frame_w * 0.5f;
        const float cy = fy0 + frame_h * 0.5f;
        gfx.draw_line(Point(cx - 6.0f, cy), Point(cx + 6.0f, cy), guide_col, 1.0f);
        gfx.draw_line(Point(cx, cy - 6.0f), Point(cx, cy + 6.0f), guide_col, 1.0f);

        // 4. Camera HUD Readout (Clean studio overlay)
        Rect hud_rect(fx0 + 10.0f, fy1 - 22.0f, frame_w - 20.0f, 18.0f);
        const std::string camera_hud = state_.active_camera.empty() ? "No active camera" :
            state_.active_camera + " · " + std::to_string(static_cast<int>(std::lround(state_.focal_length))) + "mm · Production Preview";
        gfx.draw_text(camera_hud, hud_rect, Theme::kTextSecondary, Theme::kSmallFont, false, false, true, Theme::kNumericFontFamily);
    }
    draw_light_markers(gfx);
    draw_selection_outline(gfx);
    draw_transform_gizmo(gfx);
    draw_orientation_gizmo(gfx);

    draw_popup(gfx);
    for (auto &button : menu_buttons_) button.draw_tooltip(gfx);
}

bool ViewportRegion::handle_mouse_down(const MouseEvent &e) {
    if (popup_open_) {
        for (size_t i = 0; i < popup_items_.size(); ++i) {
            if (!popup_item_rect(i).contains(e.pos)) continue;
            if (popup_items_[i].enabled && popup_items_[i].action) popup_items_[i].action();
            popup_open_ = false;
            state_.viewport_popup_open = false;
            active_menu_ = -1;
            return true;
        }
        popup_open_ = false;
        state_.viewport_popup_open = false;
        active_menu_ = -1;
        if (!bounds_.contains(e.pos)) return false;
    }
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        if (!menu_buttons_[i].handle_mouse_down(e)) continue;
        active_menu_ = static_cast<int>(i);
        populate_popup(active_menu_);
        popup_open_ = true;
        state_.viewport_popup_open = true;
        return true;
    }
    if (camera_rect_.contains(e.pos) && e.button == MouseButton::Left) {
        active_menu_ = 4;
        populate_popup(active_menu_);
        popup_open_ = true;
        state_.viewport_popup_open = true;
        return true;
    }
    if (shading_rect_.contains(e.pos) && e.button == MouseButton::Left) {
        active_menu_ = 5;
        populate_popup(active_menu_);
        popup_open_ = true;
        state_.viewport_popup_open = true;
        return true;
    }
    if (!canvas_rect_.contains(e.pos)) return false;
    if (e.button == MouseButton::Left && orientation_rect_.contains(e.pos)) {
        if (state_.active_workspace == "Shot") return true;
        snap_view_to_axis(e.pos);
        return true;
    }
    if (e.button == MouseButton::Right && !e.alt) {
        initialize_view_navigation();
        if (state_.active_workspace != "Shot") state_.viewport_navigation_active = true;
        state_.free_camera_look_active = true;
        navigation_drag_ = NavigationDrag::FreeLook;
        drag_start_ = e.pos;
        last_drag_ = e.pos;
        return true;
    }
    if (e.alt && (e.button == MouseButton::Left || e.button == MouseButton::Middle || e.button == MouseButton::Right)) {
        initialize_view_navigation();
        if (state_.active_workspace != "Shot") state_.viewport_navigation_active = true;
        navigation_drag_ = e.button == MouseButton::Left ? NavigationDrag::Orbit :
            e.button == MouseButton::Middle ? (e.shift ? NavigationDrag::Dolly : NavigationDrag::Pan) : NavigationDrag::Dolly;
        drag_start_ = e.pos;
        last_drag_ = e.pos;
        return true;
    }
    if (e.button == MouseButton::Middle) {
        initialize_view_navigation();
        if (state_.active_workspace != "Shot") state_.viewport_navigation_active = true;
        navigation_drag_ = e.shift ? NavigationDrag::Pan : NavigationDrag::Orbit;
        drag_start_ = e.pos;
        last_drag_ = e.pos;
        return true;
    }
    if (e.button == MouseButton::Left) {
        if (state_.active_workspace == "Shot" && !state_.camera_selected()) return true;
        const int lightNode = hit_test_light_marker(e.pos);
        if (lightNode >= 0) {
            state_.select_scene_node(static_cast<uint32_t>(lightNode));
            return true;
        }
        const int handle = hit_test_gizmo(e.pos);
        if (handle >= 0 && begin_gizmo_drag(handle, e.pos)) return true;
        if (state_.active_workspace == "Shot") return true;
        float origin[3], direction[3];
        if (make_pick_ray(e.pos, origin, direction)) {
            std::copy(origin, origin + 3, state_.pending_pick_origin);
            std::copy(direction, direction + 3, state_.pending_pick_direction);
            state_.pending_command = "viewport_pick";
        }
        return true;
    }
    return false;
}

bool ViewportRegion::handle_mouse_move(const MouseEvent &e) {
    last_mouse_pos_ = e.pos;
    int hovered_menu = shading_rect_.contains(e.pos) ? 5 : camera_rect_.contains(e.pos) ? 4 : -1;
    bool changed = false;
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        if (menu_buttons_[i].handle_mouse_move(e)) changed = true;
        if (menu_buttons_[i].get_bounds().contains(e.pos)) hovered_menu = static_cast<int>(i);
    }
    if (navigation_drag_ != NavigationDrag::None) {
        update_view_navigation(e.pos);
        return true;
    }
    if (gizmo_drag_.active) {
        update_gizmo_drag(e.pos);
        return true;
    }
    if (popup_open_) {
        if (hovered_menu >= 0 && hovered_menu != active_menu_) {
            active_menu_ = hovered_menu;
            populate_popup(active_menu_);
            changed = true;
        } else if (hovered_menu < 0 && !popup_rect_.contains(e.pos)) {
            popup_open_ = false;
            state_.viewport_popup_open = false;
            active_menu_ = -1;
            changed = true;
        }
    }
    return changed;
}

bool ViewportRegion::handle_mouse_up(const MouseEvent &e) {
    const bool wasFreeLooking = navigation_drag_ == NavigationDrag::FreeLook;
    const bool wasDragging = navigation_drag_ != NavigationDrag::None || gizmo_drag_.active;
    navigation_drag_ = NavigationDrag::None;
    gizmo_drag_.active = false;
    if (wasFreeLooking) state_.free_camera_look_active = false;
    if (wasDragging) return true;
    for (auto &button : menu_buttons_) if (button.handle_mouse_up(e)) return true;
    return false;
}

bool ViewportRegion::handle_mouse_wheel(float delta, Point p) {
    if (!canvas_rect_.contains(p) || std::abs(delta) < 1.0e-4f) return false;
    initialize_view_navigation();
    const float factor = std::pow(0.82f, delta);
    if (state_.active_workspace == "Shot") {
        state_.focus_distance = std::clamp(state_.focus_distance * factor, 0.02f, 100000.0f);
        state_.update_camera_position_from_orbit();
        return true;
    }
    state_.viewport_navigation_active = true;
    state_.viewport_distance = std::clamp(state_.viewport_distance * factor, 0.02f, 100000.0f);
    update_view_position_from_orbit();
    return true;
}

bool ViewportRegion::handle_raw_mouse_delta(int deltaX, int deltaY) {
    if (navigation_drag_ != NavigationDrag::FreeLook || (deltaX == 0 && deltaY == 0)) return false;
    apply_free_look_delta(static_cast<float>(deltaX), static_cast<float>(deltaY));
    return true;
}

ViewportRegion::ViewBasis ViewportRegion::view_basis() const {
    const float* position = state_.viewport_navigation_active ? state_.viewport_position : state_.camera_position;
    const float* rotation = state_.viewport_navigation_active ? state_.viewport_rotation : state_.camera_rotation;
    const float radians = 0.017453292519943295f;
    const float pitch = rotation[0] * radians;
    const float yaw = rotation[1] * radians;
    const float roll = rotation[2] * radians;
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float sy = std::sin(yaw), cy = std::cos(yaw);
    ViewBasis result;
    result.position = {position[0], position[1], position[2]};
    result.forward = normalized({sy * cp, sp, -cy * cp});
    result.right = normalized({cy, 0.0f, sy});
    result.up = normalized({-sy * sp, cp, cy * sp});
    const float cr = std::cos(roll), sr = std::sin(roll);
    result.up = normalized(result.up * cr + result.right * sr);
    const float focal = (state_.active_workspace == "Shot")
        ? std::clamp(state_.camera_focal_length, 10.0f, 500.0f)
        : 50.0f;
    result.verticalFov = 2.0f * std::atan(18.0f / focal);
    return result;
}

bool ViewportRegion::project(Vec3 world, Point& screen, float* depth) const {
    const ViewBasis view = view_basis();
    const Vec3 relative = world - view.position;
    const float z = dot(relative, view.forward);
    if (depth) *depth = z;
    if (z <= 0.001f || canvas_rect_.height <= 1.0f) return false;
    const float focalPixels = canvas_rect_.height / (2.0f * std::tan(view.verticalFov * 0.5f));
    screen.x = canvas_rect_.x + canvas_rect_.width * 0.5f + dot(relative, view.right) * focalPixels / z;
    screen.y = canvas_rect_.y + canvas_rect_.height * 0.5f - dot(relative, view.up) * focalPixels / z;
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

bool ViewportRegion::project_segment(Vec3 a, Vec3 b, Point& screenA, Point& screenB) const {
    const ViewBasis view = view_basis();
    float depthA = dot(a - view.position, view.forward);
    float depthB = dot(b - view.position, view.forward);
    constexpr float nearPlane = 0.02f;
    if (depthA <= nearPlane && depthB <= nearPlane) return false;
    if (depthA <= nearPlane) {
        const float t = (nearPlane - depthA) / (depthB - depthA);
        a = a + (b - a) * t;
    } else if (depthB <= nearPlane) {
        const float t = (nearPlane - depthB) / (depthA - depthB);
        b = b + (a - b) * t;
    }
    return project(a, screenA) && project(b, screenB);
}

void ViewportRegion::draw_camera_motion_trail(Graphics& gfx) const {
    if (state_.keyframes.size() < 2 || canvas_rect_.width <= 1.0f || canvas_rect_.height <= 1.0f) return;
    const bool shotView = state_.active_workspace == "Shot";
    const Color trailColor = Theme::kAccent;
    auto pathPoint = [&](const EditorState::Keyframe& a, const EditorState::Keyframe& b, float t) {
        Vec3 position{
            a.camera[0] + (b.camera[0] - a.camera[0]) * t,
            a.camera[1] + (b.camera[1] - a.camera[1]) * t,
            a.camera[2] + (b.camera[2] - a.camera[2]) * t};
        if (!shotView) return position;
        const float pitch = (a.camera[3] + (b.camera[3] - a.camera[3]) * t) * 0.017453292519943295f;
        const float yaw = (a.camera[4] + (b.camera[4] - a.camera[4]) * t) * 0.017453292519943295f;
        const float cp = std::cos(pitch);
        const Vec3 forward{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
        return position + forward * std::max(0.1f, state_.focus_distance);
    };

    gfx.push_clip(canvas_rect_);
    for (size_t segment = 1; segment < state_.keyframes.size(); ++segment) {
        const auto& a = state_.keyframes[segment - 1];
        const auto& b = state_.keyframes[segment];
        Point previous{};
        bool previousVisible = false;
        constexpr int subdivisions = 24;
        for (int step = 0; step <= subdivisions; ++step) {
            const float t = static_cast<float>(step) / subdivisions;
            Point current{};
            const bool currentVisible = project(pathPoint(a, b, t), current);
            if (currentVisible && previousVisible)
                gfx.draw_line(previous, current, trailColor, 2.0f);
            previous = current;
            previousVisible = currentVisible;
        }
    }
    for (const auto& key : state_.keyframes) {
        Point marker{};
        const Vec3 point = shotView
            ? [&]() {
                const float pitch = key.camera[3] * 0.017453292519943295f;
                const float yaw = key.camera[4] * 0.017453292519943295f;
                const float cp = std::cos(pitch);
                const Vec3 forward{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
                return Vec3{key.camera[0], key.camera[1], key.camera[2]} +
                       forward * std::max(0.1f, state_.focus_distance);
              }()
            : Vec3{key.camera[0], key.camera[1], key.camera[2]};
        if (!project(point, marker)) continue;
        const float size = key.frame == state_.current_frame ? 8.0f : 5.0f;
        const Rect markerRect(marker.x - size * 0.5f, marker.y - size * 0.5f, size, size);
        gfx.fill_rect(markerRect, trailColor);
        if (key.frame == state_.current_frame) gfx.draw_rect(markerRect, Theme::kTextPrimary, 1.0f);
    }
    gfx.pop_clip();
}

void ViewportRegion::draw_world_grid(Graphics& gfx) const {
    if (canvas_rect_.height <= 1.0f) return;
    const ViewBasis view = view_basis();
    const float distance = std::max(0.1f, state_.viewport_navigation_active
        ? state_.viewport_distance : state_.focus_distance);
    const float worldPerPixel = 2.0f * distance * std::tan(view.verticalFov * 0.5f) /
                                std::max(1.0f, canvas_rect_.height);
    const float requestedStep = std::max(1.0e-4f, worldPerPixel * 52.0f);
    const float decade = std::pow(10.0f, std::floor(std::log10(requestedStep)));
    const float normalizedStep = requestedStep / decade;
    const float minorStep = decade * (normalizedStep < 1.5f ? 1.0f :
                                      normalizedStep < 3.5f ? 2.0f : 5.0f);
    const int lineCount = std::clamp(static_cast<int>(std::ceil(distance * 3.5f / minorStep)), 24, 52);
    const float extent = lineCount * minorStep;

    for (int i = -lineCount; i <= lineCount; ++i) {
        if (i == 0) continue;
        const bool major = (std::abs(i) % 5) == 0;
        const float fade = static_cast<float>(std::abs(i)) / static_cast<float>(lineCount);
        const float alpha = fade < 0.35f ? 0.72f : fade < 0.72f ? 0.50f : 0.29f;
        const Color color = major
            ? Color(Theme::kViewportGridMajor.r, Theme::kViewportGridMajor.g,
                    Theme::kViewportGridMajor.b, alpha + 0.10f)
            : Color(Theme::kViewportGrid.r, Theme::kViewportGrid.g,
                    Theme::kViewportGrid.b, alpha);
        const float offset = static_cast<float>(i) * minorStep;
        Point a, b;
        if (project_segment({-extent, 0.0f, offset}, {extent, 0.0f, offset}, a, b))
            gfx.draw_line(a, b, color, major ? 1.0f : 0.7f);
        if (project_segment({offset, 0.0f, -extent}, {offset, 0.0f, extent}, a, b))
            gfx.draw_line(a, b, color, major ? 1.0f : 0.7f);
    }

    Point a, b;
    if (project_segment({-extent, 0.0f, 0.0f}, {extent, 0.0f, 0.0f}, a, b))
        gfx.draw_line(a, b, Theme::kAxisX, 1.8f);
    if (project_segment({0.0f, 0.0f, -extent}, {0.0f, 0.0f, extent}, a, b))
        gfx.draw_line(a, b, Theme::kAxisZ, 1.8f);
}

void ViewportRegion::draw_starter_cube(Graphics& gfx) const {
    if (state_.scene_nodes.size() != 1 || !state_.scene_nodes[0].visible ||
        !state_.scene_nodes[0].renderable) return;
    const auto& node = state_.scene_nodes[0];
    static constexpr float localVertices[8][3] = {
        {-0.5f,-0.5f,-0.5f}, {0.5f,-0.5f,-0.5f},
        {-0.5f,0.5f,-0.5f},  {0.5f,0.5f,-0.5f},
        {-0.5f,-0.5f,0.5f},  {0.5f,-0.5f,0.5f},
        {-0.5f,0.5f,0.5f},   {0.5f,0.5f,0.5f}
    };
    static constexpr int faceIndices[6][4] = {
        {1,3,7,5}, {0,4,6,2}, {2,6,7,3},
        {0,1,5,4}, {4,5,7,6}, {0,2,3,1}
    };
    std::array<Vec3, 8> worldVertices{};
    std::array<Point, 8> screenVertices{};
    for (size_t i = 0; i < worldVertices.size(); ++i) {
        const float x = localVertices[i][0];
        const float y = localVertices[i][1];
        const float z = localVertices[i][2];
        const float* m = node.worldTransform;
        worldVertices[i] = {
            m[0] * x + m[4] * y + m[8] * z + m[12],
            m[1] * x + m[5] * y + m[9] * z + m[13],
            m[2] * x + m[6] * y + m[10] * z + m[14]
        };
        if (!project(worldVertices[i], screenVertices[i])) return;
    }

    struct FaceDraw {
        Point points[4];
        float depth = 0.0f;
        Color color;
    };
    const ViewBasis view = view_basis();
    const Vec3 studioDirection = normalized(Vec3{-0.45f, 0.82f, -0.36f});
    std::array<FaceDraw, 6> faces{};
    bool visibleEdges[8][8]{};
    size_t visibleCount = 0;
    for (int face = 0; face < 6; ++face) {
        const Vec3 a = worldVertices[faceIndices[face][0]];
        const Vec3 b = worldVertices[faceIndices[face][1]];
        const Vec3 c = worldVertices[faceIndices[face][2]];
        const Vec3 edgeA = b - a;
        const Vec3 edgeB = c - a;
        const Vec3 normal = normalized({edgeA.y * edgeB.z - edgeA.z * edgeB.y,
                                        edgeA.z * edgeB.x - edgeA.x * edgeB.z,
                                        edgeA.x * edgeB.y - edgeA.y * edgeB.x});
        if (dot(normal, view.forward * -1.0f) <= 0.001f) continue;
        FaceDraw& draw = faces[visibleCount++];
        Vec3 center{};
        for (int corner = 0; corner < 4; corner++) {
            const int index = faceIndices[face][corner];
            draw.points[corner] = screenVertices[index];
            center = center + worldVertices[index] * 0.25f;
            const int next = faceIndices[face][(corner + 1) % 4];
            visibleEdges[index][next] = true;
            visibleEdges[next][index] = true;
        }
        draw.depth = dot(center - view.position, view.forward);
        const float shade = std::clamp(0.56f + std::max(0.0f, dot(normal, studioDirection)) * 0.30f,
                                       0.34f, 0.86f);
        draw.color = Color(shade, shade, shade, 1.0f);
    }
    for (size_t i = 1; i < visibleCount; ++i) {
        const FaceDraw face = faces[i];
        size_t j = i;
        while (j > 0 && faces[j - 1].depth < face.depth) {
            faces[j] = faces[j - 1];
            --j;
        }
        faces[j] = face;
    }
    for (size_t i = 0; i < visibleCount; ++i) gfx.fill_polygon(faces[i].points, 4, faces[i].color);

    const Color edgeColor(0.16f, 0.16f, 0.16f, 0.90f);
    for (int vertex = 0; vertex < 8; ++vertex) {
        for (int axis = 0; axis < 3; ++axis) {
            const int adjacent = vertex ^ (1 << axis);
            if (adjacent <= vertex || !visibleEdges[vertex][adjacent]) continue;
            gfx.draw_line(screenVertices[vertex], screenVertices[adjacent], edgeColor, 1.0f);
        }
    }
}

void ViewportRegion::draw_empty_scene_label(Graphics& gfx) const {
    const float width = std::min(380.0f, canvas_rect_.width - 28.0f);
    if (width < 150.0f) return;
    const float x = canvas_rect_.x + 14.0f;
    const float y = canvas_rect_.y + 11.0f;
    const char* heading = state_.active_workspace == "Shot" ? "Ready to frame a shot" :
        state_.active_workspace == "Production" ? "Ready to prepare output" : "Start building your scene";
    const char* instruction = state_.active_workspace == "Shot"
        ? "Add a model, choose a camera, then set the shot timing"
        : state_.active_workspace == "Production"
            ? "Add a model to preview and render your scene"
            : "Add a model with File > Add Model, then arrange it in the set";
    gfx.draw_text(heading, Rect(x, y, width, 18.0f),
                  Theme::kTextPrimary, Theme::kUiFont, true, false, true, Theme::kUiFontFamily);
    gfx.draw_text(instruction,
                  Rect(x, y + 19.0f, width, 28.0f), Theme::kTextSecondary,
                  Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);
}

ViewportRegion::Vec3 ViewportRegion::selected_pivot() const {
    if (state_.camera_selected())
        return {state_.camera_position[0], state_.camera_position[1], state_.camera_position[2]};
    if (state_.selected_scene_node >= 0 && static_cast<size_t>(state_.selected_scene_node) < state_.scene_nodes.size()) {
        const auto& node = state_.scene_nodes[static_cast<size_t>(state_.selected_scene_node)];
        return {node.worldTransform[12], node.worldTransform[13], node.worldTransform[14]};
    }
    return {};
}

bool ViewportRegion::gizmo_available() const {
    if (state_.selected_scene_node < 0 || static_cast<size_t>(state_.selected_scene_node) >= state_.scene_nodes.size()) return false;
    if (state_.active_tool != "tool_move" && state_.active_tool != "tool_rotate" &&
        state_.active_tool != "tool_scale") return false;
    const auto& node = state_.scene_nodes[static_cast<size_t>(state_.selected_scene_node)];
    if (node.locked) return false;
    if (node.type == zegfx::ProductionNodeType::Light)
        return state_.active_tool == "tool_move" || state_.active_tool == "tool_rotate";
    if (node.type == zegfx::ProductionNodeType::Camera)
        return state_.active_tool == "tool_move" || state_.active_tool == "tool_rotate";
    return node.type == zegfx::ProductionNodeType::Mesh || node.type == zegfx::ProductionNodeType::Group;
}

float ViewportRegion::point_segment_distance(Point p, Point a, Point b) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float lengthSquared = dx * dx + dy * dy;
    const float t = lengthSquared > 1.0e-4f
        ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared, 0.0f, 1.0f) : 0.0f;
    return std::hypot(p.x - (a.x + dx * t), p.y - (a.y + dy * t));
}

ViewportRegion::Vec3 ViewportRegion::world_axis(int axis) const {
    return axis == 0 ? Vec3{1.0f, 0.0f, 0.0f} : axis == 1 ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{0.0f, 0.0f, 1.0f};
}

void ViewportRegion::draw_axis(Graphics& gfx, Vec3 pivot, int axis, float worldLength) const {
    Point start, end;
    if (!project(pivot, start) || !project(pivot + world_axis(axis) * worldLength, end)) return;
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

void ViewportRegion::draw_transform_gizmo(Graphics& gfx) const {
    if (!gizmo_available() || state_.active_tool == "tool_select") return;
    const Vec3 pivot = selected_pivot();
    const ViewBasis view = view_basis();
    const float depth = std::max(0.1f, dot(pivot - view.position, view.forward));
    const float worldLength = std::max(0.15f, depth * 0.13f);
    Point center;
    if (!project(pivot, center)) return;
    if (state_.active_tool == "tool_rotate") {
        const Color colors[] = {Theme::kAxisX, Theme::kAxisY, Theme::kAxisZ};
        for (int axis = 0; axis < 3; ++axis) {
            const Vec3 u = world_axis((axis + 1) % 3), v = world_axis((axis + 2) % 3);
            Point previous;
            bool hasPrevious = false;
            for (int step = 0; step <= 36; ++step) {
                const float angle = 6.28318530718f * static_cast<float>(step) / 36.0f;
                Point current;
                const Vec3 position = pivot + (u * std::cos(angle) + v * std::sin(angle)) * worldLength;
                const bool projected = project(position, current);
                if (projected && hasPrevious)
                    gfx.draw_line(previous, current, colors[axis], 1.7f);
                previous = current;
                hasPrevious = projected;
            }
        }
    } else {
        for (int axis = 0; axis < 3; ++axis) draw_axis(gfx, pivot, axis, worldLength);
        if (state_.active_tool == "tool_move") {
            const int planes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
            const Color planeColors[] = {Theme::kAxisY, Theme::kAxisZ, Theme::kAxisX};
            for (int i = 0; i < 3; ++i) {
                Point a, b;
                if (!project(pivot + world_axis(planes[i][0]) * worldLength * 0.26f, a) ||
                    !project(pivot + world_axis(planes[i][1]) * worldLength * 0.26f, b)) continue;
                const Point c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
                gfx.fill_rounded_rect(Rect(c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f), 2.0f, Color(planeColors[i].r, planeColors[i].g, planeColors[i].b, 0.55f));
            }
        }
    }
    gfx.fill_rounded_rect(Rect(center.x - 4.0f, center.y - 4.0f, 8.0f, 8.0f), 4.0f, Theme::kTextPrimary);
}

void ViewportRegion::draw_selection_outline(Graphics& gfx) const {
    if (state_.selected_scene_node < 0 || static_cast<size_t>(state_.selected_scene_node) >= state_.scene_nodes.size() ||
        static_cast<size_t>(state_.selected_scene_node) >= state_.scene_node_bounds.size()) return;
    const size_t index = static_cast<size_t>(state_.selected_scene_node);
    const auto& node = state_.scene_nodes[index];
    const auto& bounds = state_.scene_node_bounds[index];
    if (node.type != zegfx::ProductionNodeType::Mesh || !bounds.valid) return;
    const Vec3 center{node.worldTransform[12] + bounds.centerOffset[0],
                      node.worldTransform[13] + bounds.centerOffset[1],
                      node.worldTransform[14] + bounds.centerOffset[2]};
    const float radius = std::max(0.05f, bounds.radius * std::max({node.scaleFactor[0], node.scaleFactor[1], node.scaleFactor[2]}));
    const ViewBasis view = view_basis();
    Point centerScreen, rightScreen, upScreen;
    if (!project(center, centerScreen) || !project(center + view.right * radius, rightScreen) ||
        !project(center + view.up * radius, upScreen)) return;
    const float halfWidth = std::max(8.0f, std::abs(rightScreen.x - centerScreen.x));
    const float halfHeight = std::max(8.0f, std::abs(upScreen.y - centerScreen.y));
    const Rect outline(centerScreen.x - halfWidth, centerScreen.y - halfHeight,
                       halfWidth * 2.0f, halfHeight * 2.0f);
    gfx.draw_rect(outline, Color(0.78f, 0.81f, 0.85f, 0.72f), 1.0f);
}

const char* ViewportRegion::light_icon(const zegfx::ProductionSceneNode& node) {
    if (node.areaLight) return "light_area";
    if (node.lightType == zegfx::LightType::Directional) return "light_directional";
    if (node.lightType == zegfx::LightType::Spot) return "light_spot";
    return "light_point";
}

void ViewportRegion::draw_light_markers(Graphics& gfx) const {
    if (state_.active_workspace == "Shot") return;
    gfx.push_clip(canvas_rect_);
    for (size_t index = 0; index < state_.scene_nodes.size(); ++index) {
        const auto& node = state_.scene_nodes[index];
        if (node.type != zegfx::ProductionNodeType::Light || !node.visible) continue;
        Point screen;
        if (!project({node.worldTransform[12], node.worldTransform[13], node.worldTransform[14]}, screen) ||
            !canvas_rect_.contains(screen)) continue;
        const bool selected = state_.selected_scene_node == static_cast<int>(index);
        const float diameter = selected ? 28.0f : 24.0f;
        const Rect marker(screen.x - diameter * 0.5f, screen.y - diameter * 0.5f, diameter, diameter);
        gfx.fill_rounded_rect(marker, diameter * 0.5f,
                              selected ? Color(Theme::kAccent.r, Theme::kAccent.g, Theme::kAccent.b, 0.92f)
                                       : Color(0.055f, 0.060f, 0.068f, 0.94f));
        gfx.draw_rounded_rect(marker, diameter * 0.5f,
                              selected ? Theme::kTextPrimary : Color(0.38f, 0.40f, 0.43f, 0.95f), 1.0f);
        IconRenderer::draw_icon(gfx, light_icon(node),
                                Rect(screen.x - 8.0f, screen.y - 8.0f, 16.0f, 16.0f),
                                selected ? Theme::kTextPrimary : Color(0.84f, 0.86f, 0.89f, 1.0f));
    }
    gfx.pop_clip();
}

int ViewportRegion::hit_test_light_marker(Point point) const {
    if (state_.active_workspace == "Shot" || !canvas_rect_.contains(point)) return -1;
    int hit = -1;
    float nearest = 15.0f;
    for (size_t index = 0; index < state_.scene_nodes.size(); ++index) {
        const auto& node = state_.scene_nodes[index];
        if (node.type != zegfx::ProductionNodeType::Light || !node.visible || node.locked) continue;
        Point screen;
        if (!project({node.worldTransform[12], node.worldTransform[13], node.worldTransform[14]}, screen) ||
            !canvas_rect_.contains(screen)) continue;
        const float distance = std::hypot(point.x - screen.x, point.y - screen.y);
        if (distance < nearest) {
            nearest = distance;
            hit = static_cast<int>(index);
        }
    }
    return hit;
}

void ViewportRegion::draw_orientation_gizmo(Graphics& gfx) {
    const Point center(orientation_rect_.x + orientation_rect_.width * 0.5f,
                       orientation_rect_.y + orientation_rect_.height * 0.5f);
    // A circular, opaque compass keeps the viewport grid from showing through
    // while avoiding the old square tile around the orientation control.
    const Rect compass(center.x - 28.0f, center.y - 28.0f, 56.0f, 56.0f);
    gfx.fill_rounded_rect(compass, 28.0f, Theme::kPanelBg);
    gfx.draw_rounded_rect(compass, 28.0f, Color(0.30f, 0.30f, 0.30f, 1.0f), 1.0f);
    orientation_endpoints_.clear();
    const ViewBasis view = view_basis();
    const Color colors[] = {Theme::kAxisX, Theme::kAxisY, Theme::kAxisZ};
    const char* names[] = {"X", "Y", "Z"};
    struct AxisEnd { Point point; Vec3 direction; float depth; int axis; int sign; };
    std::array<AxisEnd, 6> ends{};
    size_t endCount = 0;
    for (int axis = 0; axis < 3; ++axis) {
        const Vec3 direction = world_axis(axis);
        for (int sign : {-1, 1}) {
            const Vec3 signedAxis = direction * static_cast<float>(sign);
            const Point endpoint(center.x + dot(signedAxis, view.right) * 18.0f,
                                 center.y - dot(signedAxis, view.up) * 18.0f);
            ends[endCount++] = {endpoint, signedAxis, dot(signedAxis, view.forward), axis, sign};
        }
    }
    std::sort(ends.begin(), ends.end(), [](const AxisEnd& a, const AxisEnd& b) { return a.depth > b.depth; });
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

int ViewportRegion::hit_test_gizmo(Point point) const {
    if (!gizmo_available() || state_.active_tool == "tool_select") return -1;
    const Vec3 pivot = selected_pivot();
    const ViewBasis view = view_basis();
    const float depth = std::max(0.1f, dot(pivot - view.position, view.forward));
    const float worldLength = std::max(0.15f, depth * 0.13f);
    Point center;
    if (!project(pivot, center)) return -1;
    if (state_.active_tool == "tool_rotate") {
        int bestAxis = -1;
        float bestDistance = 10.0f;
        for (int axis = 0; axis < 3; ++axis) {
            const Vec3 u = world_axis((axis + 1) % 3), v = world_axis((axis + 2) % 3);
            Point previous;
            bool hasPrevious = false;
            for (int step = 0; step <= 36; ++step) {
                const float angle = 6.28318530718f * static_cast<float>(step) / 36.0f;
                Point current;
                const Vec3 position = pivot + (u * std::cos(angle) + v * std::sin(angle)) * worldLength;
                const bool projected = project(position, current);
                if (projected && hasPrevious) {
                    const float distance = point_segment_distance(point, previous, current);
                    if (distance < bestDistance) { bestDistance = distance; bestAxis = axis; }
                }
                previous = current;
                hasPrevious = projected;
            }
        }
        return bestAxis;
    }
    if (state_.active_tool == "tool_scale" && std::hypot(point.x - center.x, point.y - center.y) <= 9.0f) return 6;
    if (state_.active_tool == "tool_move") {
        const int planes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
        for (int i = 0; i < 3; ++i) {
            Point a, b;
            if (project(pivot + world_axis(planes[i][0]) * worldLength * 0.26f, a) &&
                project(pivot + world_axis(planes[i][1]) * worldLength * 0.26f, b)) {
                const float cx = (a.x + b.x) * 0.5f, cy = (a.y + b.y) * 0.5f;
                if (std::abs(point.x - cx) <= 8.0f && std::abs(point.y - cy) <= 8.0f) return 3 + i;
            }
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        Point end;
        if (project(pivot + world_axis(axis) * worldLength, end) && point_segment_distance(point, center, end) <= 8.0f)
            return axis;
    }
    return -1;
}

bool ViewportRegion::make_pick_ray(Point point, float origin[3], float direction[3]) const {
    const ViewBasis view = view_basis();
    if (canvas_rect_.height <= 1.0f) return false;
    const float nx = (point.x - (canvas_rect_.x + canvas_rect_.width * 0.5f)) /
        (canvas_rect_.height * 0.5f);
    const float ny = ((canvas_rect_.y + canvas_rect_.height * 0.5f) - point.y) /
        (canvas_rect_.height * 0.5f);
    const Vec3 ray = normalized(view.forward + view.right * (nx * std::tan(view.verticalFov * 0.5f)) +
                                view.up * (ny * std::tan(view.verticalFov * 0.5f)));
    origin[0] = view.position.x; origin[1] = view.position.y; origin[2] = view.position.z;
    direction[0] = ray.x; direction[1] = ray.y; direction[2] = ray.z;
    return std::isfinite(ray.x) && std::isfinite(ray.y) && std::isfinite(ray.z);
}

void ViewportRegion::initialize_view_navigation() {
    if (state_.active_workspace == "Shot") return;
    if (state_.viewport_navigation_active && state_.viewport_distance > 0.02f) return;
    state_.enter_viewport_camera();
}

void ViewportRegion::update_view_position_from_orbit() {
    const float pitch = std::clamp(state_.viewport_rotation[0], -89.0f, 89.0f) * 0.017453292519943295f;
    const float yaw = state_.viewport_rotation[1] * 0.017453292519943295f;
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const Vec3 forward{std::sin(yaw) * cp, sp, -std::cos(yaw) * cp};
    for (int axis = 0; axis < 3; ++axis) {
        const float f = axis == 0 ? forward.x : axis == 1 ? forward.y : forward.z;
        state_.viewport_position[axis] = state_.viewport_target[axis] - f * state_.viewport_distance;
    }
}

void ViewportRegion::apply_free_look_delta(float deltaX, float deltaY) {
    constexpr float degreesPerMouseCount = 0.15f;
    if (state_.active_workspace == "Shot") {
        state_.camera_rotation[1] += deltaX * degreesPerMouseCount;
        state_.camera_rotation[0] = std::clamp(
            state_.camera_rotation[0] - deltaY * degreesPerMouseCount, -89.0f, 89.0f);
        state_.update_camera_pose_fields();
        return;
    }
    state_.viewport_rotation[1] += deltaX * degreesPerMouseCount;
    state_.viewport_rotation[0] = std::clamp(
        state_.viewport_rotation[0] - deltaY * degreesPerMouseCount, -89.0f, 89.0f);
    const float pitch = state_.viewport_rotation[0] * 0.017453292519943295f;
    const float yaw = state_.viewport_rotation[1] * 0.017453292519943295f;
    const Vec3 forward{std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                       -std::cos(yaw) * std::cos(pitch)};
    for (int axis = 0; axis < 3; ++axis) {
        const float component = axis == 0 ? forward.x : axis == 1 ? forward.y : forward.z;
        state_.viewport_target[axis] = state_.viewport_position[axis] + component * state_.viewport_distance;
    }
}

void ViewportRegion::update_view_navigation(Point point) {
    const float dx = point.x - last_drag_.x;
    const float dy = point.y - last_drag_.y;
    const ViewBasis view = view_basis();
    if (state_.active_workspace == "Shot") {
        if (navigation_drag_ == NavigationDrag::Orbit) {
            state_.camera_rotation[1] += dx * 0.35f;
            state_.camera_rotation[0] = std::clamp(state_.camera_rotation[0] + dy * 0.35f, -89.0f, 89.0f);
            state_.update_camera_position_from_orbit();
        } else if (navigation_drag_ == NavigationDrag::FreeLook) {
            apply_free_look_delta(dx * (0.25f / 0.15f), dy * (0.25f / 0.15f));
        } else if (navigation_drag_ == NavigationDrag::Pan) {
            const float unitsPerPixel = 2.0f * state_.focus_distance * std::tan(view.verticalFov * 0.5f) /
                std::max(1.0f, canvas_rect_.height);
            const Vec3 movement = view.right * (-dx * unitsPerPixel) + view.up * (dy * unitsPerPixel);
            for (int axis = 0; axis < 3; ++axis) {
                const float value = axis == 0 ? movement.x : axis == 1 ? movement.y : movement.z;
                state_.camera_position[axis] += value;
                state_.viewport_target[axis] += value;
            }
            state_.update_camera_pose_fields();
        } else if (navigation_drag_ == NavigationDrag::Dolly) {
            state_.focus_distance = std::clamp(state_.focus_distance * std::exp(dy * 0.012f),
                                               0.02f, 100000.0f);
            state_.update_camera_position_from_orbit();
        }
        last_drag_ = point;
        return;
    }
    if (navigation_drag_ == NavigationDrag::Orbit) {
        state_.viewport_rotation[1] += dx * 0.35f;
        state_.viewport_rotation[0] = std::clamp(state_.viewport_rotation[0] + dy * 0.35f, -89.0f, 89.0f);
    } else if (navigation_drag_ == NavigationDrag::FreeLook) {
        apply_free_look_delta(dx * (0.25f / 0.15f), dy * (0.25f / 0.15f));
    } else if (navigation_drag_ == NavigationDrag::Pan) {
        const float unitsPerPixel = 2.0f * state_.viewport_distance * std::tan(view.verticalFov * 0.5f) /
            std::max(1.0f, canvas_rect_.height);
        Vec3 movement = view.right * (-dx * unitsPerPixel) + view.up * (dy * unitsPerPixel);
        for (int axis = 0; axis < 3; ++axis) {
            const float value = axis == 0 ? movement.x : axis == 1 ? movement.y : movement.z;
            state_.viewport_target[axis] += value;
        }
    } else if (navigation_drag_ == NavigationDrag::Dolly) {
        state_.viewport_distance = std::clamp(state_.viewport_distance * std::exp(dy * 0.012f), 0.02f, 100000.0f);
    }
    last_drag_ = point;
    if (navigation_drag_ != NavigationDrag::FreeLook) update_view_position_from_orbit();
}

bool ViewportRegion::begin_gizmo_drag(int handle, Point point) {
    if (!gizmo_available()) return false;
    gizmo_drag_ = {};
    gizmo_drag_.active = true;
    gizmo_drag_.handle = handle;
    gizmo_drag_.node = state_.selected_scene_node;
    gizmo_drag_.pivot = selected_pivot();
    const auto& node = state_.scene_nodes[static_cast<size_t>(gizmo_drag_.node)];
    for (int axis = 0; axis < 3; ++axis) {
        gizmo_drag_.position[axis] = state_.camera_selected() ? state_.camera_position[axis] :
            node.worldTransform[12 + axis];
        gizmo_drag_.rotation[axis] = state_.camera_selected() ? state_.camera_rotation[axis] : node.rotationOffset[axis];
        gizmo_drag_.scale[axis] = node.scaleFactor[axis];
    }
    drag_start_ = point;
    return true;
}

void ViewportRegion::update_gizmo_drag(Point point) {
    const int handle = gizmo_drag_.handle;
    const int nodeIndex = gizmo_drag_.node;
    if (!gizmo_drag_.active || nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= state_.scene_nodes.size()) return;
    const float dx = point.x - drag_start_.x, dy = point.y - drag_start_.y;
    float position[3] = {gizmo_drag_.position[0], gizmo_drag_.position[1], gizmo_drag_.position[2]};
    float rotation[3] = {gizmo_drag_.rotation[0], gizmo_drag_.rotation[1], gizmo_drag_.rotation[2]};
    float scale[3] = {gizmo_drag_.scale[0], gizmo_drag_.scale[1], gizmo_drag_.scale[2]};
    const ViewBasis view = view_basis();
    const Vec3 pivot = gizmo_drag_.pivot;
    const float unitsPerPixel = 2.0f * std::max(0.1f, state_.viewport_navigation_active
        ? state_.viewport_distance : state_.focus_distance) * std::tan(view.verticalFov * 0.5f) /
        std::max(1.0f, canvas_rect_.height);
    auto snapped = [this](float value) { return state_.snap_enabled ? std::round(value * 10.0f) * 0.1f : value; };
    if (state_.active_tool == "tool_move") {
        Vec3 delta{};
        if (handle <= 2) {
            Point center, endpoint;
            const float worldLength = std::max(0.15f, (state_.viewport_navigation_active ? state_.viewport_distance : state_.focus_distance) * 0.13f);
            if (project(pivot, center) && project(pivot + world_axis(handle) * worldLength, endpoint)) {
                const float sx = endpoint.x - center.x, sy = endpoint.y - center.y;
                const float pixels = std::hypot(sx, sy);
                const float along = pixels > 0.01f ? (dx * sx + dy * sy) / pixels : -dy;
                const float amount = along * worldLength / std::max(1.0f, pixels);
                delta = world_axis(handle) * amount;
            }
        } else {
            const int planes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
            const int planeIndex = std::clamp(handle - 3, 0, 2);
            Vec3 movement = view.right * (dx * unitsPerPixel) + view.up * (-dy * unitsPerPixel);
            const int a = planes[planeIndex][0], b = planes[planeIndex][1];
            if (a != 0) movement.x = 0.0f;
            if (a != 1 && b != 1) movement.y = 0.0f;
            if (b != 2) movement.z = 0.0f;
            delta = movement;
        }
        position[0] = snapped(position[0] + delta.x);
        position[1] = snapped(position[1] + delta.y);
        position[2] = snapped(position[2] + delta.z);
    } else if (state_.active_tool == "tool_rotate") {
        const int axis = handle == 0 ? 0 : handle == 1 ? 1 : 2;
        float amount = (dx - dy) * 0.55f;
        if (state_.snap_enabled) amount = std::round(amount / 15.0f) * 15.0f;
        rotation[axis] += amount;
    } else if (state_.active_tool == "tool_scale") {
        const float factor = std::exp((dx - dy) * 0.008f);
        if (handle == 6) {
            for (int axis = 0; axis < 3; ++axis)
                scale[axis] = std::clamp(gizmo_drag_.scale[axis] * factor, 0.001f, 1000.0f);
        } else {
            const int axis = std::clamp(handle, 0, 2);
            scale[axis] = std::clamp(gizmo_drag_.scale[axis] * factor, 0.001f, 1000.0f);
        }
        if (state_.snap_enabled) {
            for (float& value : scale) value = std::clamp(std::round(value * 10.0f) * 0.1f, 0.001f, 1000.0f);
        }
    }

    const auto type = state_.scene_nodes[static_cast<size_t>(nodeIndex)].type;
    if (type == zegfx::ProductionNodeType::Camera) {
        for (int axis = 0; axis < 3; ++axis) {
            state_.camera_position[axis] = position[axis];
            state_.camera_rotation[axis] = rotation[axis];
            state_.scene_nodes[static_cast<size_t>(nodeIndex)].worldTransform[12 + axis] = position[axis];
        }
        state_.pos_x = position[0]; state_.pos_y = position[1]; state_.pos_z = position[2];
        state_.rot_x = rotation[0]; state_.rot_y = rotation[1]; state_.rot_z = rotation[2];
        state_.update_camera_orbit_target();
        state_.record_keyframe_if_enabled();
    } else if (type == zegfx::ProductionNodeType::Light) {
        if (state_.active_tool == "tool_move") {
            state_.key_light_x = position[0]; state_.key_light_y = position[1]; state_.key_light_z = position[2];
            state_.set_scene_node_translation(static_cast<uint32_t>(nodeIndex), position);
        } else {
            state_.set_scene_node_rotation_scale(static_cast<uint32_t>(nodeIndex), rotation, scale);
        }
        state_.record_keyframe_if_enabled();
    } else {
        state_.pos_x = position[0]; state_.pos_y = position[1]; state_.pos_z = position[2];
        state_.rot_x = rotation[0]; state_.rot_y = rotation[1]; state_.rot_z = rotation[2];
        state_.scale_x = scale[0]; state_.scale_y = scale[1]; state_.scale_z = scale[2];
        if (state_.active_tool == "tool_move") state_.set_scene_node_translation(static_cast<uint32_t>(nodeIndex), position);
        else state_.set_scene_node_rotation_scale(static_cast<uint32_t>(nodeIndex), rotation, scale);
    }
}

void ViewportRegion::snap_view_to_axis(Point point) {
    if (orientation_endpoints_.empty()) return;
    auto closest = std::min_element(orientation_endpoints_.begin(), orientation_endpoints_.end(), [point](const auto& a, const auto& b) {
        return std::hypot(point.x - a.first.x, point.y - a.first.y) < std::hypot(point.x - b.first.x, point.y - b.first.y);
    });
    if (closest == orientation_endpoints_.end() || std::hypot(point.x - closest->first.x, point.y - closest->first.y) > 12.0f) return;
    initialize_view_navigation();
    state_.viewport_navigation_active = true;
    const Vec3 forward = closest->second * -1.0f;
    state_.viewport_rotation[0] = std::asin(std::clamp(forward.y, -1.0f, 1.0f)) * 57.2957795f;
    state_.viewport_rotation[1] = std::atan2(forward.x, -forward.z) * 57.2957795f;
    update_view_position_from_orbit();
}

void ViewportRegion::init_menus() {
    const std::vector<std::string> labels = {"View", "Select", "Add Object", "Object"};
    const std::vector<std::string> tips = {
        "Viewport display, grid and camera framing",
        "Select an imported scene item",
        "Add scene objects; use File or Assets to import models",
        "Choose the active transform tool"
    };
    for (size_t i = 0; i < labels.size(); ++i) {
        const char* label = labels[i].c_str();
        ButtonWidget button(label, "", nullptr);
        button.set_flat(true);
        button.set_tooltip(tips[i]);
        menu_buttons_.push_back(button);
    }
}

Rect ViewportRegion::popup_item_rect(size_t index) const {
    constexpr float item_height = 28.0f;
    return Rect(popup_rect_.x, popup_rect_.y + 4.0f + static_cast<float>(index) * item_height,
                popup_rect_.width, item_height);
}

void ViewportRegion::populate_popup(int menu) {
    popup_items_.clear();
    auto add = [this](const char* label, const char* icon, bool enabled,
                      std::function<void()> action = {}, bool separator = false) {
        popup_items_.push_back({label, icon, enabled, separator, std::move(action)});
    };
    switch (menu) {
        case 0:
            add("Show Ground Grid", "view_grid", true, [this]() { state_.show_grid = !state_.show_grid; });
            add("Show Safe Frame", "camera_safe_frame", true, [this]() { state_.show_safe_frame = !state_.show_safe_frame; });
            add("Rendered Preview", "action_render", true, [this]() { state_.pending_command = "render"; });
            break;
        case 1:
            for (size_t i = 0; i < state_.scene_nodes.size(); ++i) {
                const auto type = state_.scene_nodes[i].type;
                const char* icon = type == zegfx::ProductionNodeType::Camera ? "camera_actor" :
                    type == zegfx::ProductionNodeType::Light ? "light_directional" :
                    type == zegfx::ProductionNodeType::Mesh ? "outliner_mesh" : "outliner_collection";
                add(state_.scene_nodes[i].name.c_str(), icon, true, [this, i]() {
                    state_.select_scene_node(static_cast<uint32_t>(i));
                });
            }
            break;
        case 2:
            add("Shapes", "", false, {}, true);
            add("Plane", "outliner_mesh", true, [this]() { state_.pending_command = "add_primitive_plane"; });
            add("Cube", "outliner_mesh", true, [this]() { state_.pending_command = "add_primitive_cube"; });
            add("Circle", "outliner_mesh", true, [this]() { state_.pending_command = "add_primitive_circle"; });
            add("Cylinder", "outliner_mesh", true, [this]() { state_.pending_command = "add_primitive_cylinder"; });
            add("Lights", "", false, {}, true);
            add("Point Light", "light_point", true, [this]() { state_.pending_command = "add_light_point"; });
            add("Sun Light", "light_directional", true, [this]() { state_.pending_command = "add_light_sun"; });
            add("Spot Light", "light_spot", true, [this]() { state_.pending_command = "add_light_spot"; });
            add("Area Light", "light_area", true, [this]() { state_.pending_command = "add_light_area"; });
            add("Camera", "camera_actor", true, [this]() { state_.pending_command = "add_camera"; }, true);
            add("Volume", "outliner_collection", true, [this]() { state_.pending_command = "add_volume"; }, true);
            break;
        case 3:
            add("Select Tool", "tool_select", true, [this]() { state_.active_tool = "tool_select"; });
            add("Move", "tool_move", true, [this]() { state_.active_tool = "tool_move"; });
            add("Rotate", "tool_rotate", true, [this]() { state_.active_tool = "tool_rotate"; });
            add("Scale", "tool_scale", true, [this]() { state_.active_tool = "tool_scale"; });
            break;
        case 4:
            add("User Perspective", "view_grid", state_.active_workspace == "Scene", [this]() {
                state_.activate_workspace("Scene");
            });
            for (size_t i = 0; i < state_.scene_nodes.size(); ++i) {
                if (state_.scene_nodes[i].type != zegfx::ProductionNodeType::Camera) continue;
                add(state_.scene_nodes[i].name.c_str(), "camera_actor", true, [this, i]() {
                    if (i >= state_.scene_nodes.size()) return;
                    state_.active_camera_node = static_cast<int>(i);
                    state_.select_active_camera();
                });
            }
            if (state_.active_camera_node >= 0 && static_cast<size_t>(state_.active_camera_node) < state_.scene_nodes.size())
                add("Camera Properties", "camera_aperture", true, [this]() {
                    state_.select_active_camera();
                });
            break;
        case 5: {
            const char* wireframeLabel = state_.viewport_shading == ViewportShadingMode::Wireframe
                ? "Wireframe  ✓" : "Wireframe";
            const char* solidLabel = state_.viewport_shading == ViewportShadingMode::Solid
                ? "Solid  ✓" : "Solid";
            const char* renderedLabel = state_.viewport_shading == ViewportShadingMode::Rendered
                ? "Rendered  ✓" : "Rendered";
            add(wireframeLabel, "", true, [this]() {
                state_.viewport_shading = ViewportShadingMode::Wireframe;
            });
            add(solidLabel, "", true, [this]() {
                state_.viewport_shading = ViewportShadingMode::Solid;
            });
            add(renderedLabel, "", true, [this]() {
                state_.viewport_shading = ViewportShadingMode::Rendered;
            });
            break;
        }
    }
    Rect anchor = active_menu_ < 4 ? menu_buttons_[static_cast<size_t>(active_menu_)].get_bounds() :
        active_menu_ == 5 ? shading_rect_ : camera_rect_;
    const float width = 210.0f;
    popup_rect_ = Rect(std::clamp(anchor.x, bounds_.x + 4.0f, std::max(bounds_.x + 4.0f, bounds_.right() - width - 4.0f)),
                       header_rect_.bottom() - 4.0f, width, static_cast<float>(popup_items_.size()) * 28.0f + 8.0f);
}

void ViewportRegion::draw_popup(Graphics &gfx) {
    if (!popup_open_) return;
    gfx.fill_rect(popup_rect_, Theme::kPanelBg);
    gfx.draw_rect(popup_rect_, Theme::kBorderBright, 1.0f);
    for (size_t i = 0; i < popup_items_.size(); ++i) {
        Rect row = popup_item_rect(i);
        if (popup_items_[i].separator_before)
            gfx.draw_line(Point(row.x + 8.0f, row.y + 1.0f), Point(row.right() - 8.0f, row.y + 1.0f),
                          Theme::kBorderSubtle, 1.0f);
        if (row.contains(last_mouse_pos_)) gfx.fill_rect(row, Theme::kControlHover);
        if (!popup_items_[i].icon.empty())
            IconRenderer::draw_icon(gfx, popup_items_[i].icon, Rect(row.x + 8.0f, row.y + 6.0f, 16.0f, 16.0f),
                                    popup_items_[i].enabled ? Theme::kTextSecondary : Theme::kTextMuted);
        gfx.draw_text(popup_items_[i].label, Rect(row.x + 32.0f, row.y, row.width - 40.0f, row.height),
                      popup_items_[i].enabled ? Theme::kTextPrimary : Theme::kTextMuted,
                      Theme::kUiFont, false, false, true, Theme::kUiFontFamily);
    }
}

} // namespace medusa
