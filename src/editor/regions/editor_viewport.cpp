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
    return bounds_.contains(p) || (popup_menu_.is_open() && popup_menu_.contains(p));
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
    const float shadingWidth = std::min(148.0f, std::max(128.0f, header_rect_.width * 0.16f));
    const float denoiseWidth = 100.0f;
    shading_rect_ = Rect(header_rect_.right() - shadingWidth - 8.0f,
                         header_rect_.y + 2.0f, shadingWidth, header_rect_.height - 4.0f);
    if (state_.viewport_shading == ViewportShadingMode::PathTracer) {
        const float aovWidth = 138.0f;
        aov_rect_ = Rect(shading_rect_.x - aovWidth - 6.0f,
                         header_rect_.y + 2.0f, aovWidth, header_rect_.height - 4.0f);
        denoise_rect_ = Rect(aov_rect_.x - denoiseWidth - 6.0f,
                             header_rect_.y + 2.0f, denoiseWidth, header_rect_.height - 4.0f);
    } else {
        aov_rect_ = Rect{};
        denoise_rect_ = Rect(shading_rect_.x - denoiseWidth - 6.0f,
                             header_rect_.y + 2.0f, denoiseWidth, header_rect_.height - 4.0f);
    }
    camera_rect_ = Rect(mx + 8.0f, header_rect_.y + 2.0f,
                        std::max(100.0f, std::min(270.0f, denoise_rect_.x - mx - 14.0f)),
                        header_rect_.height - 4.0f);
    orientation_gizmo_.set_bounds(Rect(canvas_rect_.right() - 74.0f, canvas_rect_.y + 6.0f, 64.0f, 64.0f));
}

void ViewportRegion::draw(Graphics &gfx) {
    // 1. Viewport Header Bar (Blender 3D Viewport header style)
    gfx.fill_rect(header_rect_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(header_rect_.x, header_rect_.bottom()), Point(header_rect_.right(), header_rect_.bottom()), Theme::kDockDivider, 1.0f);

    // Editor Icon
    const bool shotWorkspace = (state_.active_workspace == "Shot");
    Rect icon_r(header_rect_.x + 8.0f, header_rect_.y + 5.0f, 14.0f, 14.0f);
    IconRenderer::draw_icon(gfx, shotWorkspace ? "camera_actor" : "view_grid", icon_r, Theme::kTextSecondary);

    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        menu_buttons_[i].set_active(popup_menu_.is_open() && active_menu_ == static_cast<int>(i));
        menu_buttons_[i].draw(gfx);
    }

    // Camera Switcher (Aligned dedicated icon + clean label)
    if (camera_rect_.contains(last_mouse_pos_) || (popup_menu_.is_open() && active_menu_ == 4)) {
        gfx.fill_rounded_rect(camera_rect_, 3.0f, Theme::kControlHover);
        gfx.draw_rounded_rect(camera_rect_, 3.0f, Theme::kBorderBright, 1.0f);
    }
    float cam_icon_sz = 14.0f;
    float cam_x = camera_rect_.x + 8.0f;
    float cam_icon_y = header_rect_.y + (header_rect_.height - cam_icon_sz) * 0.5f;
    const bool cameraPilotActive = state_.camera_pilot_active;
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

    // Wavelet Denoiser live toggle pill
    const bool denoiseHovered = denoise_rect_.contains(last_mouse_pos_);
    const bool denoiseActive = state_.denoiser_enabled;
    if (denoiseActive) {
        gfx.fill_rounded_rect(denoise_rect_, 3.0f, denoiseHovered ? Theme::kControlHover : Theme::kControlActive);
        gfx.draw_rounded_rect(denoise_rect_, 3.0f, Theme::kBorderBright, 1.0f);
        gfx.draw_text("AI Denoise", Rect(denoise_rect_.x, denoise_rect_.y, denoise_rect_.width, denoise_rect_.height),
                      Theme::kTextPrimary, Theme::kUiFont, false, true, true, Theme::kUiFontFamily);
    } else {
        if (denoiseHovered) gfx.fill_rounded_rect(denoise_rect_, 3.0f, Theme::kControlHover);
        gfx.draw_rounded_rect(denoise_rect_, 3.0f, Theme::kBorderSubtle, 1.0f);
        gfx.draw_text("AI Denoise", Rect(denoise_rect_.x, denoise_rect_.y, denoise_rect_.width, denoise_rect_.height),
                      Theme::kTextMuted, Theme::kUiFont, false, true, true, Theme::kUiFontFamily);
    }

    // AOV Channel Selector dropdown (PathTracer mode only)
    if (state_.viewport_shading == ViewportShadingMode::PathTracer && aov_rect_.width > 0.0f) {
        const bool aovHovered = aov_rect_.contains(last_mouse_pos_) || (popup_menu_.is_open() && active_menu_ == 6);
        if (aovHovered) gfx.fill_rounded_rect(aov_rect_, 3.0f, Theme::kControlHover);
        gfx.draw_rounded_rect(aov_rect_, 3.0f, aovHovered ? Theme::kBorderBright : Theme::kBorderSubtle, 1.0f);
        std::string aovLabel = "Pass: " + state_.active_aov_name + "  ▾";
        gfx.draw_text(aovLabel,
                      Rect(aov_rect_.x + 8.0f, aov_rect_.y,
                           aov_rect_.width - 16.0f, aov_rect_.height),
                      Theme::kTextPrimary, Theme::kUiFont, false, false, true, Theme::kUiFontFamily);
    }

    const bool shadingHovered = shading_rect_.contains(last_mouse_pos_) || (popup_menu_.is_open() && active_menu_ == 5);
    if (shadingHovered) gfx.fill_rounded_rect(shading_rect_, 3.0f, Theme::kControlHover);
    gfx.draw_rounded_rect(shading_rect_, 3.0f, shadingHovered ? Theme::kBorderBright : Theme::kBorderSubtle, 1.0f);
    const char* shadingLabel =
        state_.viewport_shading == ViewportShadingMode::Wireframe ? "Wireframe" :
        state_.viewport_shading == ViewportShadingMode::Solid ? "Solid (MatCap)" :
        state_.viewport_shading == ViewportShadingMode::RealTime ? "RealTime (120 FPS)" :
        "PathTracer";
    gfx.draw_text(std::string(shadingLabel) + "  ▾",
                  Rect(shading_rect_.x + 8.0f, shading_rect_.y,
                       shading_rect_.width - 16.0f, shading_rect_.height),
                  Theme::kTextPrimary, Theme::kUiFont, false, false, true, Theme::kUiFontFamily);

    // 2. Viewport 3D Canvas
    gfx.fill_rect(canvas_rect_, Theme::kViewportBg);
    gfx.push_clip(canvas_rect_);

    if (state_.viewport_shading == ViewportShadingMode::PathTracer) {
        // Path Tracer Mode: Blit progressive path-traced Beauty or selected AOV pass
        const bool drawn = gfx.draw_pathtracer_viewport(canvas_rect_, state_.denoiser_enabled, state_.active_aov);
        if (!drawn) {
            draw_scene_geometry(gfx);
        }
        if (state_.show_grid) {
            draw_world_grid(gfx);
        }
        draw_camera_motion_trail(gfx);

        // Progressive Stats Overlay Pill
        const auto progress = gfx.production_render_progress();
        const int convPercent = static_cast<int>(progress.convergenceRate * 100.0f + 0.5f);
        std::string sppText = "✦ PathTracer: " + std::to_string(progress.currentSample) + " / " +
                              std::to_string(state_.max_samples_per_pixel) + " spp · Pass: " +
                              state_.active_aov_name;
        if (state_.light_tree_enabled) {
            sppText += " · Light Tree: On";
        }
        if (state_.adaptive_sampling_enabled) {
            sppText += " · " + std::to_string(convPercent) + "% Converged (Welford)";
        } else if (progress.isConverged) {
            sppText += " · Converged";
        }
        if (state_.denoiser_enabled && progress.currentSample >= 4) {
            sppText += " · EAW Denoised";
        }
        const float badgeW = std::max(200.0f, static_cast<float>(sppText.length()) * 6.5f + 20.0f);
        Rect badgeRect(canvas_rect_.x + 12.0f, canvas_rect_.bottom() - 32.0f, badgeW, 22.0f);
        gfx.fill_rounded_rect(badgeRect, 3.0f, Color(0.10f, 0.12f, 0.15f, 0.88f));
        gfx.draw_rounded_rect(badgeRect, 3.0f, Theme::kBorderSubtle, 1.0f);
        gfx.draw_text(sppText, badgeRect, progress.isConverged ? Theme::kAccent : Theme::kTextPrimary,
                      Theme::kSmallFont, false, true, true, Theme::kUiFontFamily);
    } else {
        // RealTime, Solid, or Wireframe Mode: 120 FPS ZeGFX D3D12 Rasterizer
        std::vector<Rect> overlays;
        overlays.push_back(orientation_gizmo_.get_bounds());
        if (popup_menu_.is_open() && popup_menu_.get_bounds().width > 0.0f && popup_menu_.get_bounds().height > 0.0f) {
            overlays.push_back(popup_menu_.get_cutout_bounds());
        }
        if (state_.menu_popup_open && state_.active_topbar_popup_rect.width > 0.0f && state_.active_topbar_popup_rect.height > 0.0f) {
            overlays.push_back(Rect(state_.active_topbar_popup_rect.x - 4.0f,
                                    state_.active_topbar_popup_rect.y,
                                    state_.active_topbar_popup_rect.width + 8.0f,
                                    state_.active_topbar_popup_rect.height + 5.0f));
        }
        if (state_.active_modal_rect.width > 0.0f && state_.active_modal_rect.height > 0.0f) {
            overlays.push_back(state_.active_modal_rect);
        }
        if (state_.active_drawer_rect.width > 0.0f && state_.active_drawer_rect.height > 0.0f) {
            overlays.push_back(state_.active_drawer_rect);
        }
        gfx.draw_realtime_viewport(canvas_rect_, overlays);
        if (state_.show_grid) {
            draw_world_grid(gfx);
        }
        draw_camera_motion_trail(gfx);
        draw_scene_geometry(gfx);
    }

    if (state_.scene_nodes.empty()) draw_empty_scene_label(gfx);
    gfx.pop_clip();

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
    const auto proj = [this](const Vec3& w, Point& s) { return project(w, s); };
    if (state_.viewport_shading != ViewportShadingMode::PathTracer) {
        transform_gizmo_.draw_selection_outline(gfx, state_, view_basis(), proj);
        transform_gizmo_.draw(gfx, state_, view_basis(), proj);
    }
    orientation_gizmo_.draw(gfx, view_basis());

    for (auto &button : menu_buttons_) button.draw_tooltip(gfx);
}

bool ViewportRegion::handle_mouse_down(const MouseEvent &e) {
    if (popup_menu_.is_open()) {
        int clicked_menu = -1;
        for (size_t i = 0; i < menu_buttons_.size(); ++i) {
            if (menu_buttons_[i].get_bounds().contains(e.pos)) { clicked_menu = static_cast<int>(i); break; }
        }
        if (camera_rect_.contains(e.pos)) clicked_menu = 4;
        else if (shading_rect_.contains(e.pos)) clicked_menu = 5;
        else if (state_.viewport_shading == ViewportShadingMode::PathTracer && aov_rect_.contains(e.pos)) clicked_menu = 6;

        if (clicked_menu >= 0) {
            if (active_menu_ == clicked_menu) {
                popup_menu_.close();
                state_.viewport_popup_open = false;
                state_.active_viewport_popup_rect = Rect{};
                active_menu_ = -1;
            } else {
                active_menu_ = clicked_menu;
                populate_popup(active_menu_);
                state_.viewport_popup_open = true;
                state_.active_viewport_popup_rect = popup_menu_.get_bounds();
            }
            return true;
        }

        if (popup_menu_.contains(e.pos)) {
            bool handled = popup_menu_.handle_mouse_down(e);
            state_.viewport_popup_open = popup_menu_.is_open();
            state_.active_viewport_popup_rect = popup_menu_.get_bounds();
            if (!popup_menu_.is_open()) active_menu_ = -1;
            return handled;
        }

        popup_menu_.close();
        state_.viewport_popup_open = false;
        state_.active_viewport_popup_rect = Rect{};
        active_menu_ = -1;
        if (!bounds_.contains(e.pos)) return false;
    }

    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        if (!menu_buttons_[i].handle_mouse_down(e)) continue;
        active_menu_ = static_cast<int>(i);
        populate_popup(active_menu_);
        state_.viewport_popup_open = true;
        state_.active_viewport_popup_rect = popup_menu_.get_bounds();
        return true;
    }
    if (camera_rect_.contains(e.pos) && e.button == MouseButton::Left) {
        active_menu_ = 4;
        populate_popup(active_menu_);
        state_.viewport_popup_open = true;
        state_.active_viewport_popup_rect = popup_menu_.get_bounds();
        return true;
    }
    if (denoise_rect_.contains(e.pos) && e.button == MouseButton::Left) {
        state_.denoiser_enabled = !state_.denoiser_enabled;
        return true;
    }
    if (state_.viewport_shading == ViewportShadingMode::PathTracer && aov_rect_.contains(e.pos) && e.button == MouseButton::Left) {
        active_menu_ = 6;
        populate_popup(active_menu_);
        state_.viewport_popup_open = true;
        state_.active_viewport_popup_rect = popup_menu_.get_bounds();
        return true;
    }
    if (shading_rect_.contains(e.pos) && e.button == MouseButton::Left) {
        active_menu_ = 5;
        populate_popup(active_menu_);
        state_.viewport_popup_open = true;
        state_.active_viewport_popup_rect = popup_menu_.get_bounds();
        return true;
    }
    if (!canvas_rect_.contains(e.pos)) return false;
    if (e.button == MouseButton::Left && orientation_gizmo_.contains(e.pos)) {
        if (state_.active_workspace == "Shot") return true;
        orientation_gizmo_.snap_to_axis(e.pos, state_,
            [this]() { initialize_view_navigation(); },
            [this]() { update_view_position_from_orbit(); });
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
        const auto proj = [this](const Vec3& w, Point& s) { return project(w, s); };
        const int handle = transform_gizmo_.hit_test(e.pos, state_, view_basis(), proj);
        if (handle >= 0 && transform_gizmo_.begin_drag(handle, e.pos, state_, view_basis())) return true;
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
    int hovered_menu = (state_.viewport_shading == ViewportShadingMode::PathTracer && aov_rect_.contains(e.pos)) ? 6 :
                       shading_rect_.contains(e.pos) ? 5 : camera_rect_.contains(e.pos) ? 4 : -1;
    bool changed = false;
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        if (menu_buttons_[i].handle_mouse_move(e)) changed = true;
        if (menu_buttons_[i].get_bounds().contains(e.pos)) hovered_menu = static_cast<int>(i);
    }
    if (navigation_drag_ != NavigationDrag::None) {
        update_view_navigation(e.pos);
        return true;
    }
    if (transform_gizmo_.is_dragging()) {
        const auto proj = [this](const Vec3& w, Point& s) { return project(w, s); };
        transform_gizmo_.update_drag(e.pos, state_, view_basis(), canvas_rect_.height, proj);
        return true;
    }
    if (popup_menu_.is_open()) {
        if (hovered_menu >= 0 && hovered_menu != active_menu_) {
            active_menu_ = hovered_menu;
            populate_popup(active_menu_, /*instant=*/true);
            state_.active_viewport_popup_rect = popup_menu_.get_bounds();
            changed = true;
        } else {
            if (popup_menu_.handle_mouse_move(e)) changed = true;
        }
    }
    return changed;
}

bool ViewportRegion::handle_mouse_up(const MouseEvent &e) {
    const bool wasFreeLooking = navigation_drag_ == NavigationDrag::FreeLook;
    const bool wasDragging = navigation_drag_ != NavigationDrag::None || transform_gizmo_.is_dragging();
    navigation_drag_ = NavigationDrag::None;
    transform_gizmo_.end_drag();
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

void ViewportRegion::draw_box_node(Graphics& gfx, const zegfx::ProductionSceneNode& node, bool selected) const {
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
        if (dot(normal, view.forward * -1.0f) <= 0.001f && state_.viewport_shading != ViewportShadingMode::Wireframe) continue;
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
        draw.color = selected
            ? Color(std::min(1.0f, shade * 1.15f + 0.1f), shade * 0.95f, shade * 0.80f, 1.0f)
            : Color(shade, shade, shade, 1.0f);
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
    if (state_.viewport_shading != ViewportShadingMode::Wireframe) {
        for (size_t i = 0; i < visibleCount; ++i) gfx.fill_polygon(faces[i].points, 4, faces[i].color);
    }

    const Color edgeColor = selected ? Theme::kAccent : Color(0.16f, 0.16f, 0.16f, 0.90f);
    for (int vertex = 0; vertex < 8; ++vertex) {
        for (int axis = 0; axis < 3; ++axis) {
            const int adjacent = vertex ^ (1 << axis);
            if (adjacent <= vertex || (!visibleEdges[vertex][adjacent] && state_.viewport_shading != ViewportShadingMode::Wireframe)) continue;
            gfx.draw_line(screenVertices[vertex], screenVertices[adjacent], edgeColor, selected ? 1.5f : 1.0f);
        }
    }
}

void ViewportRegion::draw_scene_geometry(Graphics& gfx) const {
    if (state_.scene_nodes.empty()) return;

    // Check if we have triangles from loaded models, the default scene, or primitives
    if (!state_.scene_triangles.empty()) {
        const ViewBasis view = view_basis();
        const Vec3 studioDirection = normalized(Vec3{-0.45f, 0.82f, -0.36f});

        struct TriDraw {
            Point points[3];
            float depth = 0.0f;
            Color color;
            Color edgeColor;
            bool selected = false;
        };

        std::vector<TriDraw> draws;
        const size_t totalTriangles = state_.scene_triangles.size();
        const size_t stride = (totalTriangles > 40000) ? (totalTriangles / 20000) : 1;
        draws.reserve(std::min<size_t>(totalTriangles / stride + 16, 45000));

        for (size_t i = 0; i < totalTriangles; i += stride) {
            const auto& tri = state_.scene_triangles[i];
            if (tri.nodeIndex < 0 || static_cast<size_t>(tri.nodeIndex) >= state_.scene_nodes.size()) continue;
            const auto& node = state_.scene_nodes[static_cast<size_t>(tri.nodeIndex)];
            if (!node.visible || !node.renderable || node.type != zegfx::ProductionNodeType::Mesh) continue;

            const float* m = node.worldTransform;
            Vec3 w[3];
            Point sp[3];
            float d[3]{};
            bool ok = true;
            for (int v = 0; v < 3; ++v) {
                const float lx = tri.vertices[v].position[0];
                const float ly = tri.vertices[v].position[1];
                const float lz = tri.vertices[v].position[2];
                w[v] = {
                    m[0] * lx + m[4] * ly + m[8] * lz + m[12],
                    m[1] * lx + m[5] * ly + m[9] * lz + m[13],
                    m[2] * lx + m[6] * ly + m[10] * lz + m[14]
                };
                if (!project(w[v], sp[v], &d[v]) || d[v] <= 0.05f) {
                    ok = false;
                    break;
                }
            }
            if (!ok) continue;

            const Vec3 eA = w[1] - w[0];
            const Vec3 eB = w[2] - w[0];
            const Vec3 rawN = {
                eA.y * eB.z - eA.z * eB.y,
                eA.z * eB.x - eA.x * eB.z,
                eA.x * eB.y - eA.y * eB.x
            };
            const float len = length(rawN);
            if (len < 1.0e-7f) continue;
            const Vec3 normal = rawN * (1.0f / len);

            Vec3 effNormal = normal;
            float facing = dot(normal, view.forward * -1.0f);
            if (facing <= 0.001f) {
                if (totalTriangles <= 4 || state_.viewport_shading == ViewportShadingMode::Wireframe) {
                    effNormal = normal * -1.0f;
                    facing = -facing;
                } else {
                    continue;
                }
            }

            const float avgD = (d[0] + d[1] + d[2]) * 0.33333333f;
            const float diffuse = std::max(0.0f, dot(effNormal, studioDirection));
            const float shade = std::clamp(0.50f + diffuse * 0.42f, 0.22f, 0.95f);

            Color baseCol(0.65f, 0.65f, 0.67f, 1.0f);
            if (tri.materialIndex < state_.scene_materials.size()) {
                const auto& mat = state_.scene_materials[tri.materialIndex];
                baseCol = Color(mat.baseColor[0], mat.baseColor[1], mat.baseColor[2], 1.0f);
            }

            const bool isSelected = (tri.nodeIndex == state_.selected_scene_node);
            Color faceCol(baseCol.r * shade, baseCol.g * shade, baseCol.b * shade, 1.0f);
            if (isSelected) {
                faceCol = Color(std::min(1.0f, faceCol.r * 1.15f + 0.08f),
                                faceCol.g * 0.98f,
                                faceCol.b * 0.88f, 1.0f);
            }

            TriDraw td;
            td.points[0] = sp[0];
            td.points[1] = sp[1];
            td.points[2] = sp[2];
            td.depth = avgD;
            td.color = faceCol;
            td.edgeColor = isSelected ? Theme::kAccent : Color(0.20f, 0.20f, 0.22f, 0.70f);
            td.selected = isSelected;
            draws.push_back(td);
        }

        if (!draws.empty()) {
            std::sort(draws.begin(), draws.end(), [](const TriDraw& a, const TriDraw& b) {
                return a.depth > b.depth;
            });

            if (state_.viewport_shading != ViewportShadingMode::Wireframe) {
                for (const auto& td : draws) {
                    gfx.fill_polygon(td.points, 3, td.color);
                }
            }

            for (const auto& td : draws) {
                if (state_.viewport_shading == ViewportShadingMode::Wireframe) {
                    gfx.draw_line(td.points[0], td.points[1], td.edgeColor, 1.0f);
                    gfx.draw_line(td.points[1], td.points[2], td.edgeColor, 1.0f);
                    gfx.draw_line(td.points[2], td.points[0], td.edgeColor, 1.0f);
                } else if (td.selected && totalTriangles <= 6000) {
                    gfx.draw_line(td.points[0], td.points[1], Theme::kAccent, 1.0f);
                    gfx.draw_line(td.points[1], td.points[2], Theme::kAccent, 1.0f);
                    gfx.draw_line(td.points[2], td.points[0], Theme::kAccent, 1.0f);
                }
            }
            return;
        }
    }

    // Fallback: draw box representation for each mesh node
    for (size_t i = 0; i < state_.scene_nodes.size(); ++i) {
        const auto& node = state_.scene_nodes[i];
        if (node.type != zegfx::ProductionNodeType::Mesh || !node.visible || !node.renderable) continue;
        draw_box_node(gfx, node, static_cast<int>(i) == state_.selected_scene_node);
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
        state_.camera_rotation[1] -= deltaX * degreesPerMouseCount;
        state_.camera_rotation[0] = std::clamp(
            state_.camera_rotation[0] - deltaY * degreesPerMouseCount, -89.0f, 89.0f);
        state_.update_camera_pose_fields();
        return;
    }
    state_.viewport_rotation[1] -= deltaX * degreesPerMouseCount;
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
            state_.camera_rotation[1] -= dx * 0.35f;
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
        state_.viewport_rotation[1] -= dx * 0.35f;
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

void ViewportRegion::populate_popup(int menu, bool instant) {
    popup_menu_.clear();
    switch (menu) {
        case 0:
            popup_menu_.add_item("Frame Selected", "camera_actor", "Numpad .", [this]() {
                state_.pending_command = "frame_selected";
            });
            popup_menu_.add_item("Frame All", "view_grid", "Home", [this]() {
                state_.pending_command = "frame_all";
            });
            popup_menu_.add_separator();
            popup_menu_.add_check("Show Ground Grid", "view_grid", state_.show_grid, [this]() {
                state_.show_grid = !state_.show_grid;
            });
            popup_menu_.add_check("Show Safe Frame", "camera_safe_frame", state_.show_safe_frame, [this]() {
                state_.show_safe_frame = !state_.show_safe_frame;
            });
            popup_menu_.add_check("Show Camera Trail", "camera_actor", state_.show_camera_trail, [this]() {
                state_.show_camera_trail = !state_.show_camera_trail;
            });
            popup_menu_.add_separator();
            popup_menu_.add_item("Rendered Preview", "action_render", "F12", [this]() {
                state_.pending_command = "render";
            });
            break;
        case 1:
            popup_menu_.add_item("Select All", "", "Ctrl+A", [this]() {
                if (!state_.scene_nodes.empty()) state_.select_scene_node(0);
            });
            popup_menu_.add_item("Deselect All", "", "Alt+A", [this]() {
                state_.selected_scene_node = -1;
            });
            popup_menu_.add_separator("SCENE OBJECTS");
            for (size_t i = 0; i < state_.scene_nodes.size(); ++i) {
                const auto type = state_.scene_nodes[i].type;
                const char* icon = type == zegfx::ProductionNodeType::Camera ? "camera_actor" :
                    type == zegfx::ProductionNodeType::Light ? "light_directional" :
                    type == zegfx::ProductionNodeType::Mesh ? "outliner_mesh" : "outliner_collection";
                const char* type_str = type == zegfx::ProductionNodeType::Camera ? "Camera" :
                    type == zegfx::ProductionNodeType::Light ? "Light" :
                    type == zegfx::ProductionNodeType::Mesh ? "Mesh" : "Volume";
                const bool is_sel = (state_.selected_scene_node == static_cast<int>(i));
                popup_menu_.add_check(state_.scene_nodes[i].name, icon, is_sel, [this, i]() {
                    state_.select_scene_node(static_cast<uint32_t>(i));
                }, type_str);
            }
            if (state_.scene_nodes.empty()) {
                popup_menu_.add_item("No Scene Objects", "", "", nullptr, false);
            }
            break;
        case 2:
            popup_menu_.add_separator("MESH OBJECTS");
            popup_menu_.add_item("Plane", "outliner_mesh", "Shift+A", [this]() { state_.pending_command = "add_primitive_plane"; });
            popup_menu_.add_item("Cube", "outliner_mesh", "", [this]() { state_.pending_command = "add_primitive_cube"; });
            popup_menu_.add_item("Circle", "outliner_mesh", "", [this]() { state_.pending_command = "add_primitive_circle"; });
            popup_menu_.add_item("Cylinder", "outliner_mesh", "", [this]() { state_.pending_command = "add_primitive_cylinder"; });
            popup_menu_.add_separator("LIGHTS");
            popup_menu_.add_item("Point Light", "light_point", "", [this]() { state_.pending_command = "add_light_point"; });
            popup_menu_.add_item("Sun Light", "light_directional", "", [this]() { state_.pending_command = "add_light_sun"; });
            popup_menu_.add_item("Spot Light", "light_spot", "", [this]() { state_.pending_command = "add_light_spot"; });
            popup_menu_.add_item("Area Light", "light_area", "", [this]() { state_.pending_command = "add_light_area"; });
            popup_menu_.add_separator("CAMERA & VOLUME");
            popup_menu_.add_item("Camera", "camera_actor", "", [this]() { state_.pending_command = "add_camera"; });
            popup_menu_.add_item("Volume", "outliner_collection", "", [this]() { state_.pending_command = "add_volume"; });
            break;
        case 3:
            popup_menu_.add_item("Select Tool", "tool_select", "Q", [this]() { state_.active_tool = "tool_select"; });
            popup_menu_.add_item("Move Tool", "tool_move", "W", [this]() { state_.active_tool = "tool_move"; });
            popup_menu_.add_item("Rotate Tool", "tool_rotate", "E", [this]() { state_.active_tool = "tool_rotate"; });
            popup_menu_.add_item("Scale Tool", "tool_scale", "R", [this]() { state_.active_tool = "tool_scale"; });
            popup_menu_.add_separator();
            popup_menu_.add_item("Duplicate", "", "Shift+D", [this]() { state_.pending_command = "duplicate"; });
            popup_menu_.add_item("Delete", "", "Del", [this]() { state_.pending_command = "delete_selected"; });
            break;
        case 4:
            popup_menu_.add_check("User Perspective", "view_grid", state_.active_workspace == "Scene", [this]() {
                state_.activate_workspace("Scene");
            }, "Numpad 5");
            popup_menu_.add_separator("SCENE CAMERAS");
            for (size_t i = 0; i < state_.scene_nodes.size(); ++i) {
                if (state_.scene_nodes[i].type != zegfx::ProductionNodeType::Camera) continue;
                popup_menu_.add_check(state_.scene_nodes[i].name, "camera_actor",
                                      state_.active_camera_node == static_cast<int>(i), [this, i]() {
                    if (i >= state_.scene_nodes.size()) return;
                    state_.active_camera_node = static_cast<int>(i);
                    state_.select_active_camera();
                });
            }
            if (state_.active_camera_node >= 0 && static_cast<size_t>(state_.active_camera_node) < state_.scene_nodes.size()) {
                popup_menu_.add_separator();
                popup_menu_.add_item("Camera Properties", "camera_aperture", "", [this]() {
                    state_.select_active_camera();
                });
            }
            break;
        case 5:
            popup_menu_.add_check("Wireframe", "", state_.viewport_shading == ViewportShadingMode::Wireframe, [this]() {
                state_.viewport_shading = ViewportShadingMode::Wireframe;
            }, "Shift+Z");
            popup_menu_.add_check("Solid (MatCap)", "", state_.viewport_shading == ViewportShadingMode::Solid, [this]() {
                state_.viewport_shading = ViewportShadingMode::Solid;
            }, "Alt+Z");
            popup_menu_.add_check("RealTime (120 FPS ZeGFX)", "", state_.viewport_shading == ViewportShadingMode::RealTime, [this]() {
                state_.viewport_shading = ViewportShadingMode::RealTime;
            });
            popup_menu_.add_check("PathTracer (Progressive)", "", state_.viewport_shading == ViewportShadingMode::PathTracer, [this]() {
                state_.viewport_shading = ViewportShadingMode::PathTracer;
            }, "Z");
            break;
        case 6: {
            struct AOVOption {
                zegfx::ProductionAOV aov;
                const char* name;
            };
            const AOVOption options[] = {
                { zegfx::ProductionAOV::Beauty, "Beauty" },
                { zegfx::ProductionAOV::Albedo, "Albedo" },
                { zegfx::ProductionAOV::WorldNormal, "World Normal" },
                { zegfx::ProductionAOV::LinearDepth, "Linear Depth" },
                { zegfx::ProductionAOV::DiffuseDirect, "Diffuse Direct" },
                { zegfx::ProductionAOV::DiffuseIndirect, "Diffuse Indirect" },
                { zegfx::ProductionAOV::SpecularDirect, "Specular Direct" },
                { zegfx::ProductionAOV::SpecularIndirect, "Specular Indirect" },
                { zegfx::ProductionAOV::Emission, "Emission" },
                { zegfx::ProductionAOV::Roughness, "Roughness" },
                { zegfx::ProductionAOV::Cryptomatte, "Cryptomatte (ID)" },
                { zegfx::ProductionAOV::VarianceMap, "Variance Heatmap" },
            };
            popup_menu_.add_separator("RENDER PASSES");
            for (const auto& opt : options) {
                popup_menu_.add_check(opt.name, "", state_.active_aov == opt.aov, [this, opt]() {
                    state_.active_aov = opt.aov;
                    state_.active_aov_name = opt.name;
                });
            }
            break;
        }
    }
    Rect anchor = active_menu_ < 4 ? menu_buttons_[static_cast<size_t>(active_menu_)].get_bounds() :
        active_menu_ == 5 ? shading_rect_ :
        active_menu_ == 6 ? aov_rect_ : camera_rect_;
    float width = (active_menu_ == 5) ? 230.0f : (active_menu_ == 6) ? 210.0f : (active_menu_ == 2) ? 210.0f : 210.0f;
    Rect screen_bounds(0.0f, 0.0f, 4000.0f, 3000.0f);
    popup_menu_.open(anchor, screen_bounds, width, 340.0f, instant);
    state_.active_viewport_popup_rect = popup_menu_.get_bounds();
}

void ViewportRegion::draw_popup(Graphics &gfx) {
    popup_menu_.draw(gfx);
}

} // namespace medusa
