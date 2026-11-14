#include <medusa/editor/dialogs/render_setup_dialog.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <medusa/ui/medusa_icons.hpp>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace medusa {

namespace {

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalized(Vec3 v) {
    const float l = length(v);
    return l > 1.0e-6f ? v * (1.0f / l) : Vec3{0.0f, 0.0f, -1.0f};
}

} // namespace

RenderSetupDialog::RenderSetupDialog(EditorState& state)
    : state_(state),
      samples_("Samples", static_cast<float>(state.max_samples_per_pixel), "spp", Theme::kTextSecondary,
               1.0f, 4096.0f, 8.0f, [this](float value) {
                   state_.max_samples_per_pixel = std::clamp(static_cast<int>(std::round(value)), 1, 4096);
               }),
      firefly_("Clamp", state.firefly_clamp, "", Theme::kTextSecondary, 0.0f, 100.0f, 1.0f,
               [this](float value) { state_.firefly_clamp = value; }),
      adaptive_thresh_("Threshold", state.adaptive_sampling_threshold, "", Theme::kTextSecondary, 0.001f, 0.20f, 0.005f,
                       [this](float value) { state_.adaptive_sampling_threshold = std::clamp(value, 0.001f, 0.20f); }) {}

Rect RenderSetupDialog::panel_bounds(const Rect& bounds) {
    layout(bounds);
    return panel_;
}

void RenderSetupDialog::draw(Graphics& gfx, const Rect& bounds) {
    layout(bounds);

    // 1. Dark cinema modal backdrop dimming
    gfx.fill_rect(bounds, Color(0.0f, 0.0f, 0.0f, 0.72f));

    // 2. Main Studio Modal Frame
    gfx.fill_rounded_rect(panel_, 5.0f, Theme::kPanelBg);
    gfx.draw_rounded_rect(panel_, 5.0f, Theme::kBorderBright, 1.0f);

    // 3. Top Title Bar
    gfx.fill_rect(header_rect_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(panel_.x, header_rect_.bottom()), Point(panel_.right(), header_rect_.bottom()), Theme::kBorderSubtle, 1.0f);

    Rect iconRect(header_rect_.x + 12.0f, header_rect_.y + 9.0f, 16.0f, 16.0f);
    IconRenderer::draw_icon(gfx, "action_render", iconRect, Theme::kAccent);

    Rect headerTitleRect(header_rect_.x + 36.0f, header_rect_.y + 8.0f, 340.0f, 20.0f);
    gfx.draw_text("Render Sequence  —  Output Preview & Setup", headerTitleRect, Theme::kTextPrimary, Theme::kUiFont, true, false, true, Theme::kUiFontFamily);

    // Close Button "✕"
    const bool closeHovered = close_btn_rect_.contains(last_mouse_);
    if (closeHovered) gfx.fill_rounded_rect(close_btn_rect_, 3.0f, Color(0.85f, 0.22f, 0.22f, 0.25f));
    gfx.draw_text("✕", close_btn_rect_, closeHovered ? Color(1.0f, 0.45f, 0.45f, 1.0f) : Theme::kTextSecondary, Theme::kUiFont, true, true, true, Theme::kUiFontFamily);

    // 4. Left 70%: Render Preview Window
    draw_render_preview(gfx, preview_panel_);

    // 5. Vertical Divider between 70% Preview and 30% Settings
    gfx.draw_line(Point(settings_panel_.x, settings_panel_.y), Point(settings_panel_.x, settings_panel_.bottom()), Theme::kBorderSubtle, 1.0f);

    // 6. Right 30%: Render Settings & Options Panel
    gfx.draw_text("RENDER SETTINGS", title_rect_, Theme::kTextPrimary, Theme::kUiFont, true, false, true, Theme::kUiFontFamily);
    gfx.draw_text("Review output format, range and quality before rendering.", subtitle_rect_,
                  Theme::kTextMuted, 9.5f, false, false, true, Theme::kUiFontFamily);

    // Section 1: RENDERER
    draw_section(gfx, engine_card_, "RENDERER");
    draw_button(gfx, renderer_rect_, "ZeGFX  ▾", true);

    // Section 2: OUTPUT
    draw_section(gfx, output_card_, "OUTPUT & FORMAT");
    draw_button(gfx, scene_range_rect_, "Scene Range", state_.render_frame_mode == EditorState::RenderFrameMode::SceneRange);
    draw_button(gfx, current_frame_rect_, "Current Frame", state_.render_frame_mode == EditorState::RenderFrameMode::CurrentFrame);
    draw_button(gfx, custom_range_rect_, "Custom Range", state_.render_frame_mode == EditorState::RenderFrameMode::CustomRange);
    if (state_.render_frame_mode == EditorState::RenderFrameMode::CustomRange) {
        draw_number_field(gfx, custom_start_rect_, "Start", displayed_number(Field::Start, state_.render_custom_start), field_ == Field::Start);
        draw_number_field(gfx, custom_end_rect_, "End", displayed_number(Field::End, state_.render_custom_end), field_ == Field::End);
    } else {
        const auto range = selected_range();
        const std::string summary = "Frames " + std::to_string(range.first) + " – " + std::to_string(range.second) +
            "   ·   " + std::to_string(range.second - range.first + 1) + " frame" + (range.second == range.first ? "" : "s");
        gfx.draw_text(summary, range_summary_rect_, Theme::kTextSecondary, Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);
    }

    gfx.draw_text("Resolution", resolution_label_rect_, Theme::kTextSecondary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
    draw_button(gfx, hd_rect_, "HD 720p", state_.render_output_width == 1280 && state_.render_output_height == 720);
    draw_button(gfx, full_hd_rect_, "Full HD 1080p", state_.render_output_width == 1920 && state_.render_output_height == 1080);
    draw_button(gfx, custom_resolution_rect_, "Custom", !is_preset_resolution());
    draw_number_field(gfx, width_rect_, "W", displayed_number(Field::Width, state_.render_output_width), field_ == Field::Width);
    draw_number_field(gfx, height_rect_, "H", displayed_number(Field::Height, state_.render_output_height), field_ == Field::Height);

    gfx.draw_text("Format", format_label_rect_, Theme::kTextSecondary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
    draw_button(gfx, png_rect_, "PNG", !state_.render_output_exr);
    draw_button(gfx, exr_rect_, "OpenEXR", state_.render_output_exr);

    gfx.draw_text("Destination", path_label_rect_, Theme::kTextSecondary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
    std::string outputPathText = "Choose a filename when rendering";
    if (!state_.render_output_path.empty()) {
        std::filesystem::path outputPath = std::filesystem::u8path(state_.render_output_path);
        outputPath.replace_extension(state_.render_output_exr ? ".exr" : ".png");
        outputPathText = outputPath.u8string();
    }
    gfx.draw_text(outputPathText, path_text_rect_, Theme::kTextMuted, Theme::kSmallFont, false, false, true, Theme::kNumericFontFamily);
    draw_button(gfx, browse_rect_, "Browse…", false);

    // Section 3: QUALITY
    draw_section(gfx, quality_card_, "SAMPLING QUALITY");
    samples_.set_value(static_cast<float>(state_.max_samples_per_pixel));
    samples_.set_bounds(samples_rect_);
    samples_.draw(gfx);
    firefly_.set_value(state_.firefly_clamp);
    firefly_.set_bounds(firefly_rect_);
    firefly_.draw(gfx);
    draw_button(gfx, light_tree_toggle_rect_,
                state_.light_tree_enabled ? "Hierarchical Light Tree (O(log N))  ·  On" : "Hierarchical Light Tree  ·  Off",
                state_.light_tree_enabled);
    draw_button(gfx, adaptive_toggle_rect_,
                state_.adaptive_sampling_enabled ? "Adaptive  ·  On" : "Adaptive  ·  Off",
                state_.adaptive_sampling_enabled);
    adaptive_thresh_.set_value(state_.adaptive_sampling_threshold);
    adaptive_thresh_.set_bounds(adaptive_thresh_rect_);
    adaptive_thresh_.draw(gfx);
    draw_button(gfx, denoise_toggle_rect_,
                state_.denoiser_enabled ? "Wavelet Denoising (EAW)  ·  Enabled" : "Wavelet Denoising (EAW)  ·  Disabled",
                state_.denoiser_enabled);
    gfx.draw_text("ZeGFX Light Tree BVH · Welford Adaptive Sampler · Multiscale Wavelet Denoising.",
                  quality_note_rect_, Theme::kTextMuted, 9.5f, false, false, true, Theme::kUiFontFamily);

    // Section 4: Footer Action Bar
    gfx.draw_line(Point(settings_panel_.x + 16.0f, footer_rect_.y - 6.0f), Point(settings_panel_.right() - 16.0f, footer_rect_.y - 6.0f), Theme::kBorderSubtle, 1.0f);
    gfx.draw_text("Output will be saved sequentially to the selected folder.", footer_hint_rect_,
                  Theme::kTextMuted, 9.0f, false, false, true, Theme::kUiFontFamily);
    draw_button(gfx, cancel_rect_, "Cancel", false);
    draw_button(gfx, render_rect_, "Render Sequence", true, true);

    if (renderer_menu_.is_open()) {
        renderer_menu_.draw(gfx);
    }
}

void RenderSetupDialog::draw_render_preview(Graphics& gfx, const Rect& previewRect) {
    // Fill preview panel background
    gfx.fill_rect(previewRect, Color(0.08f, 0.08f, 0.09f, 1.0f));

    // 1. Preview Sub-Header Bar
    const Rect headerRect(previewRect.x + 16.0f, previewRect.y + 10.0f, previewRect.width - 32.0f, 22.0f);
    const std::string camName = state_.active_camera.empty() ? "Render Camera" : state_.active_camera;
    const std::string camBadge = "● SHOT CAMERA: " + camName + " (" +
        std::to_string(static_cast<int>(std::lround(state_.camera_focal_length))) + "mm)";
    gfx.draw_text(camBadge, headerRect, Theme::kAccent, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);

    const int w = state_.render_output_width;
    const int h = state_.render_output_height;
    std::string aspectStr = "16:9";
    if (w > 0 && h > 0) {
        const float r = static_cast<float>(w) / static_cast<float>(h);
        if (std::abs(r - 16.0f / 9.0f) < 0.02f) aspectStr = "16:9";
        else if (std::abs(r - 4.0f / 3.0f) < 0.02f) aspectStr = "4:3";
        else if (std::abs(r - 2.39f) < 0.05f) aspectStr = "2.39:1 Cinema";
        else if (std::abs(r - 1.0f) < 0.02f) aspectStr = "1:1 Square";
        else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f:1", r);
            aspectStr = buf;
        }
    }
    const std::string badge = std::to_string(w) + " × " + std::to_string(h) + " (" + aspectStr + ")   ·   Frame " +
        std::to_string(state_.current_frame) + " of " + std::to_string(state_.end_frame);
    gfx.draw_text(badge, headerRect, Theme::kTextSecondary, Theme::kSmallFont, false, true, true, Theme::kNumericFontFamily);

    // 2. Preview Sub-Footer Bar
    const Rect footerRect(previewRect.x + 16.0f, previewRect.bottom() - 26.0f, previewRect.width - 32.0f, 20.0f);
    const std::string engineStatus = "● Ready to Render   ·   ZeGFX Production Pipeline   ·   Color: Linear sRGB";
    gfx.draw_text(engineStatus, footerRect, Theme::kTextMuted, 9.5f, false, false, true, Theme::kUiFontFamily);

    // 3. Canvas Frame Area
    const Rect canvasRect(previewRect.x + 16.0f, headerRect.bottom() + 8.0f, previewRect.width - 32.0f, footerRect.y - headerRect.bottom() - 14.0f);
    if (canvasRect.width <= 20.0f || canvasRect.height <= 20.0f) return;

    gfx.fill_rect(canvasRect, Color(0.04f, 0.04f, 0.05f, 1.0f));

    // Calculate aspect-ratio fitted camera frame
    const float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : (16.0f / 9.0f);
    const float avail_w = canvasRect.width - 16.0f;
    const float avail_h = canvasRect.height - 16.0f;
    float frame_w = avail_w;
    float frame_h = frame_w / aspect;
    if (frame_h > avail_h) {
        frame_h = avail_h;
        frame_w = frame_h * aspect;
    }
    const float fx0 = canvasRect.x + (canvasRect.width - frame_w) * 0.5f;
    const float fy0 = canvasRect.y + (canvasRect.height - frame_h) * 0.5f;
    const Rect frameRect(fx0, fy0, frame_w, frame_h);

    // Dark matte for letterboxing/pillarboxing
    Color matte(0.02f, 0.02f, 0.03f, 0.95f);
    if (fy0 > canvasRect.y)
        gfx.fill_rect(Rect(canvasRect.x, canvasRect.y, canvasRect.width, fy0 - canvasRect.y), matte);
    if (frameRect.bottom() < canvasRect.bottom())
        gfx.fill_rect(Rect(canvasRect.x, frameRect.bottom(), canvasRect.width, canvasRect.bottom() - frameRect.bottom()), matte);
    if (fx0 > canvasRect.x)
        gfx.fill_rect(Rect(canvasRect.x, fy0, fx0 - canvasRect.x, frame_h), matte);
    if (frameRect.right() < canvasRect.right())
        gfx.fill_rect(Rect(frameRect.right(), fy0, canvasRect.right() - frameRect.right(), frame_h), matte);

    // Frame canvas background
    gfx.fill_rect(frameRect, Theme::kViewportBg);
    gfx.draw_rect(frameRect, Theme::kAccent, 1.0f);

    // Render Scene Geometry inside the frame
    gfx.push_clip(frameRect);

    draw_preview_geometry(gfx, frameRect);

    // Rule-of-thirds composition guides
    Color guideCol(Theme::kAccent.r, Theme::kAccent.g, Theme::kAccent.b, 0.20f);
    const float x_third = frame_w / 3.0f;
    const float y_third = frame_h / 3.0f;
    gfx.draw_line(Point(fx0 + x_third, fy0), Point(fx0 + x_third, fy0 + frame_h), guideCol, 1.0f);
    gfx.draw_line(Point(fx0 + x_third * 2.0f, fy0), Point(fx0 + x_third * 2.0f, fy0 + frame_h), guideCol, 1.0f);
    gfx.draw_line(Point(fx0, fy0 + y_third), Point(fx0 + frame_w, fy0 + y_third), guideCol, 1.0f);
    gfx.draw_line(Point(fx0, fy0 + y_third * 2.0f), Point(fx0 + frame_w, fy0 + y_third * 2.0f), guideCol, 1.0f);

    // Center Crosshair
    const float cx = fx0 + frame_w * 0.5f;
    const float cy = fy0 + frame_h * 0.5f;
    gfx.draw_line(Point(cx - 5.0f, cy), Point(cx + 5.0f, cy), guideCol, 1.0f);
    gfx.draw_line(Point(cx, cy - 5.0f), Point(cx, cy + 5.0f), guideCol, 1.0f);

    // Studio Cinema HUD overlay at bottom-left of the framed render
    const Rect hudRect(fx0 + 8.0f, frameRect.bottom() - 20.0f, frame_w - 16.0f, 16.0f);
    const std::string hudText = camName + "  ·  " +
        std::to_string(static_cast<int>(std::lround(state_.camera_focal_length))) + "mm  ·  Production Output Preview";
    gfx.draw_text(hudText, hudRect, Theme::kTextSecondary, Theme::kSmallFont, false, false, true, Theme::kNumericFontFamily);

    gfx.pop_clip();
}

void RenderSetupDialog::draw_preview_geometry(Graphics& gfx, const Rect& frameRect) const {
    if (frameRect.width <= 1.0f || frameRect.height <= 1.0f) return;

    // Camera view basis
    const float radians = 0.017453292519943295f;
    const float pitch = state_.camera_rotation[0] * radians;
    const float yaw = state_.camera_rotation[1] * radians;
    const float roll = state_.camera_rotation[2] * radians;
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float sy = std::sin(yaw), cy = std::cos(yaw);
    const Vec3 pos = {state_.camera_position[0], state_.camera_position[1], state_.camera_position[2]};
    const Vec3 fwd = normalized({sy * cp, sp, -cy * cp});
    const Vec3 rgt = normalized({cy, 0.0f, sy});
    Vec3 up = normalized({-sy * sp, cp, cy * sp});
    const float cr = std::cos(roll), sr = std::sin(roll);
    up = normalized(up * cr + rgt * sr);
    const float focal = std::clamp(state_.camera_focal_length, 10.0f, 500.0f);
    const float vfov = 2.0f * std::atan(18.0f / focal);
    const float focalPixels = frameRect.height / (2.0f * std::tan(vfov * 0.5f));

    auto project_cam = [&](Vec3 w, Point& s, float* out_d) -> bool {
        const Vec3 rel = w - pos;
        const float z = dot(rel, fwd);
        if (out_d) *out_d = z;
        if (z <= 0.05f) return false;
        s.x = frameRect.x + frameRect.width * 0.5f + dot(rel, rgt) * focalPixels / z;
        s.y = frameRect.y + frameRect.height * 0.5f - dot(rel, up) * focalPixels / z;
        return std::isfinite(s.x) && std::isfinite(s.y);
    };

    // Draw subtle ground grid
    constexpr int halfGrid = 6;
    constexpr float step = 1.0f;
    const Color gridColor(0.22f, 0.22f, 0.25f, 0.50f);
    for (int i = -halfGrid; i <= halfGrid; ++i) {
        Point p0, p1;
        float d0 = 0, d1 = 0;
        Vec3 a{static_cast<float>(i) * step, 0.0f, -static_cast<float>(halfGrid) * step};
        Vec3 b{static_cast<float>(i) * step, 0.0f, static_cast<float>(halfGrid) * step};
        if (project_cam(a, p0, &d0) && project_cam(b, p1, &d1)) {
            const Color col = (i == 0) ? Theme::kAxisZ : gridColor;
            gfx.draw_line(p0, p1, col, (i == 0) ? 1.4f : 0.6f);
        }
        a = {-static_cast<float>(halfGrid) * step, 0.0f, static_cast<float>(i) * step};
        b = {static_cast<float>(halfGrid) * step, 0.0f, static_cast<float>(i) * step};
        if (project_cam(a, p0, &d0) && project_cam(b, p1, &d1)) {
            const Color col = (i == 0) ? Theme::kAxisX : gridColor;
            gfx.draw_line(p0, p1, col, (i == 0) ? 1.4f : 0.6f);
        }
    }

    // Draw triangles if present
    if (!state_.scene_triangles.empty()) {
        const Vec3 studioDirection = normalized(Vec3{-0.45f, 0.82f, -0.36f});
        struct TriDraw {
            Point points[3];
            float depth = 0.0f;
            Color color;
            bool selected = false;
        };
        std::vector<TriDraw> draws;
        const size_t total = state_.scene_triangles.size();
        const size_t stride = (total > 40000) ? (total / 20000) : 1;
        draws.reserve(std::min<size_t>(total / stride + 16, 45000));

        for (size_t i = 0; i < total; i += stride) {
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
                if (!project_cam(w[v], sp[v], &d[v])) {
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
            float facing = dot(normal, fwd * -1.0f);
            if (facing <= 0.001f) {
                if (total <= 4 || state_.viewport_shading == ViewportShadingMode::Wireframe) {
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
            td.selected = isSelected;
            draws.push_back(td);
        }

        if (!draws.empty()) {
            std::sort(draws.begin(), draws.end(), [](const TriDraw& a, const TriDraw& b) {
                return a.depth > b.depth;
            });
            for (const auto& td : draws) {
                gfx.fill_polygon(td.points, 3, td.color);
            }
            for (const auto& td : draws) {
                if (td.selected && total <= 6000) {
                    gfx.draw_line(td.points[0], td.points[1], Theme::kAccent, 1.0f);
                    gfx.draw_line(td.points[1], td.points[2], Theme::kAccent, 1.0f);
                    gfx.draw_line(td.points[2], td.points[0], Theme::kAccent, 1.0f);
                }
            }
        }
    }
}

bool RenderSetupDialog::handle_mouse_down(const MouseEvent& event, const Rect& bounds) {
    layout(bounds);
    if (event.button != MouseButton::Left) return true;

    // Check click on close button
    if (close_btn_rect_.contains(event.pos)) {
        finish_field(false);
        renderer_menu_.close();
        state_.render_setup_open = false;
        return true;
    }

    // Clicking outside modal panel dismisses it
    if (!panel_.contains(event.pos)) {
        finish_field(false);
        renderer_menu_.close();
        state_.render_setup_open = false;
        return true;
    }

    if (renderer_menu_.is_open()) {
        if (renderer_rect_.contains(event.pos)) {
            renderer_menu_.close();
            return true;
        }
        if (renderer_menu_.contains(event.pos)) {
            return renderer_menu_.handle_mouse_down(event);
        }
        renderer_menu_.close();
    }

    // Only handle setting controls if inside settings_panel_
    if (!settings_panel_.contains(event.pos)) return true;

    if (renderer_rect_.contains(event.pos)) {
        renderer_menu_.clear();
        renderer_menu_.add_check("ZeGFX Production Renderer", "space_render", true, nullptr);
        renderer_menu_.add_check("DirectX 12 Accelerated", "", true, nullptr);
        renderer_menu_.add_separator("ENGINE FEATURES");
        renderer_menu_.add_item("BVH Light Tree (O(log N))", "light_point", "", nullptr, false);
        renderer_menu_.add_item("Welford Adaptive Sampler", "action_render", "", nullptr, false);
        renderer_menu_.add_item("Wavelet Denoiser (EAW)", "render_denoise", "", nullptr, false);
        renderer_menu_.open(renderer_rect_, Rect(0.0f, 0.0f, 4000.0f, 3000.0f), 240.0f, 340.0f);
        return true;
    }

    if (scene_range_rect_.contains(event.pos)) {
        finish_field(true);
        state_.render_frame_mode = EditorState::RenderFrameMode::SceneRange;
        return true;
    }
    if (current_frame_rect_.contains(event.pos)) {
        finish_field(true);
        state_.render_frame_mode = EditorState::RenderFrameMode::CurrentFrame;
        return true;
    }
    if (custom_range_rect_.contains(event.pos)) {
        const bool enteringCustom = state_.render_frame_mode != EditorState::RenderFrameMode::CustomRange;
        finish_field(true);
        state_.render_frame_mode = EditorState::RenderFrameMode::CustomRange;
        if (enteringCustom) {
            state_.render_custom_start = state_.start_frame;
            state_.render_custom_end = state_.end_frame;
        }
        state_.render_custom_start = std::max(1, state_.render_custom_start);
        state_.render_custom_end = std::max(state_.render_custom_start, state_.render_custom_end);
        return true;
    }
    if (state_.render_frame_mode == EditorState::RenderFrameMode::CustomRange) {
        if (custom_start_rect_.contains(event.pos)) { begin_field(Field::Start); return true; }
        if (custom_end_rect_.contains(event.pos)) { begin_field(Field::End); return true; }
    }
    if (hd_rect_.contains(event.pos)) { finish_field(true); set_resolution(1280, 720); return true; }
    if (full_hd_rect_.contains(event.pos)) { finish_field(true); set_resolution(1920, 1080); return true; }
    if (custom_resolution_rect_.contains(event.pos)) { finish_field(true); return true; }
    if (width_rect_.contains(event.pos)) { begin_field(Field::Width); return true; }
    if (height_rect_.contains(event.pos)) { begin_field(Field::Height); return true; }
    if (png_rect_.contains(event.pos)) { finish_field(true); state_.render_output_exr = false; return true; }
    if (exr_rect_.contains(event.pos)) { finish_field(true); state_.render_output_exr = true; return true; }
    if (browse_rect_.contains(event.pos)) {
        finish_field(true);
        renderer_menu_.close();
        state_.render_setup_open = false;
        state_.pending_command = "render_output_browse";
        return true;
    }
    if (samples_.handle_mouse_down(event) || firefly_.handle_mouse_down(event) || adaptive_thresh_.handle_mouse_down(event)) return true;
    if (light_tree_toggle_rect_.contains(event.pos)) {
        state_.light_tree_enabled = !state_.light_tree_enabled;
        return true;
    }
    if (adaptive_toggle_rect_.contains(event.pos)) {
        state_.adaptive_sampling_enabled = !state_.adaptive_sampling_enabled;
        return true;
    }
    if (denoise_toggle_rect_.contains(event.pos)) {
        state_.denoiser_enabled = !state_.denoiser_enabled;
        return true;
    }
    if (cancel_rect_.contains(event.pos)) {
        finish_field(false);
        renderer_menu_.close();
        state_.render_setup_open = false;
        return true;
    }
    if (render_rect_.contains(event.pos)) {
        finish_field(true);
        renderer_menu_.close();
        state_.render_setup_open = false;
        state_.pending_command = "render_sequence";
        return true;
    }
    finish_field(true);
    return true;
}

bool RenderSetupDialog::handle_mouse_move(const MouseEvent& event, const Rect& bounds) {
    layout(bounds);
    bool changed = samples_.handle_mouse_move(event) | firefly_.handle_mouse_move(event) | adaptive_thresh_.handle_mouse_move(event);
    if (renderer_menu_.is_open()) {
        if (renderer_menu_.handle_mouse_move(event)) changed = true;
    }
    const bool moved = event.pos.x != last_mouse_.x || event.pos.y != last_mouse_.y;
    last_mouse_ = event.pos;
    return changed || moved;
}

bool RenderSetupDialog::handle_mouse_up(const MouseEvent& event, const Rect& bounds) {
    layout(bounds);
    samples_.handle_mouse_up(event);
    firefly_.handle_mouse_up(event);
    adaptive_thresh_.handle_mouse_up(event);
    return true;
}

bool RenderSetupDialog::handle_key_down(const KeyEvent& event) {
    const uint32_t key = event.key_code;
    if (key == 27) { // Escape closes dialog
        finish_field(false);
        renderer_menu_.close();
        state_.render_setup_open = false;
        return true;
    }
    if (field_ != Field::None) {
        if (key >= '0' && key <= '9') {
            if (replace_field_buffer_) field_buffer_.clear();
            if (field_buffer_.size() < 7) field_buffer_.push_back(static_cast<char>(key));
            replace_field_buffer_ = false;
            return true;
        }
        if (key == 8) {
            if (replace_field_buffer_) field_buffer_.clear();
            else if (!field_buffer_.empty()) field_buffer_.pop_back();
            replace_field_buffer_ = false;
            return true;
        }
        if (key == 9) {
            const Field next = field_ == Field::Start ? Field::End : field_ == Field::End ? Field::Start :
                field_ == Field::Width ? Field::Height : Field::Width;
            finish_field(true);
            begin_field(next);
            return true;
        }
        if (key == 13) {
            finish_field(true);
            return true;
        }
        if (key == 0x25 || key == 0x27) {
            apply_field_value(current_field_value() + (key == 0x27 ? 1 : -1));
            field_buffer_ = std::to_string(current_field_value());
            replace_field_buffer_ = false;
            return true;
        }
        return true;
    }
    if (key == 13) {
        renderer_menu_.close();
        state_.render_setup_open = false;
        state_.pending_command = "render_sequence";
    } else if (key == 9) {
        begin_field(state_.render_frame_mode == EditorState::RenderFrameMode::CustomRange ? Field::Start : Field::Width);
    }
    return true;
}

void RenderSetupDialog::layout(const Rect& bounds) {
    // 1. Studio Modal Outer Frame: generous, comfortable sizing (up to 95% width, 92% height)
    const float width = std::min(bounds.width - 40.0f, std::max(880.0f, bounds.width * 0.94f));
    const float height = std::min(bounds.height - 36.0f, std::max(560.0f, bounds.height * 0.91f));
    panel_ = Rect(bounds.x + (bounds.width - width) * 0.5f, bounds.y + (bounds.height - height) * 0.5f, width, height);

    // 2. Top Title Bar
    header_rect_ = Rect(panel_.x, panel_.y, panel_.width, 36.0f);
    close_btn_rect_ = Rect(panel_.right() - 32.0f, panel_.y + 5.0f, 26.0f, 26.0f);

    // 3. 70% Left Preview / 30% Right Settings Split
    const float content_y = header_rect_.bottom();
    const float content_h = panel_.bottom() - content_y;
    const float split_ratio = 0.70f;
    const float preview_w = std::floor(panel_.width * split_ratio);
    const float settings_w = panel_.width - preview_w;

    preview_panel_ = Rect(panel_.x, content_y, preview_w, content_h);
    settings_panel_ = Rect(preview_panel_.right(), content_y, settings_w, content_h);

    // 4. Right 30% Settings Panel Card Layout
    const float pad = 16.0f;
    const float left = settings_panel_.x + pad;
    const float inner = settings_panel_.width - pad * 2.0f;

    title_rect_ = Rect(left, settings_panel_.y + 12.0f, inner, 20.0f);
    subtitle_rect_ = Rect(left, settings_panel_.y + 32.0f, inner, 16.0f);

    engine_card_ = Rect(left, settings_panel_.y + 54.0f, inner, 48.0f);
    renderer_rect_ = Rect(left + 10.0f, engine_card_.y + 20.0f, 130.0f, 22.0f);
    renderer_menu_rect_ = Rect(renderer_rect_.x, renderer_rect_.bottom() + 2.0f, 150.0f, 28.0f);
    renderer_active_item_rect_ = Rect(renderer_menu_rect_.x, renderer_menu_rect_.y, 150.0f, 25.0f);

    output_card_ = Rect(left, settings_panel_.y + 110.0f, inner, 226.0f);
    const float ol = output_card_.x + 10.0f;
    const float oright = output_card_.right() - 10.0f;
    const float ow = oright - ol;
    const float gap = 5.0f;
    const float optionW = (ow - gap * 2.0f) / 3.0f;

    scene_range_rect_ = Rect(ol, output_card_.y + 22.0f, optionW, 22.0f);
    current_frame_rect_ = Rect(scene_range_rect_.right() + gap, scene_range_rect_.y, optionW, 22.0f);
    custom_range_rect_ = Rect(current_frame_rect_.right() + gap, scene_range_rect_.y, optionW, 22.0f);

    custom_start_rect_ = Rect(ol, output_card_.y + 49.0f, ow * 0.49f, 22.0f);
    custom_end_rect_ = Rect(custom_start_rect_.right() + 6.0f, custom_start_rect_.y, ow - custom_start_rect_.width - 6.0f, 22.0f);
    range_summary_rect_ = Rect(ol, output_card_.y + 49.0f, ow, 22.0f);

    resolution_label_rect_ = Rect(ol, output_card_.y + 76.0f, 68.0f, 22.0f);
    const float rx = resolution_label_rect_.right() + 4.0f;
    const float rg = 5.0f;
    const float rw = (oright - rx - 76.0f - rg * 2.0f) / 2.0f;
    hd_rect_ = Rect(rx, resolution_label_rect_.y, rw, 22.0f);
    full_hd_rect_ = Rect(hd_rect_.right() + rg, hd_rect_.y, rw, 22.0f);
    custom_resolution_rect_ = Rect(full_hd_rect_.right() + rg, hd_rect_.y, 76.0f, 22.0f);

    width_rect_ = Rect(ol, output_card_.y + 103.0f, ow * 0.49f, 22.0f);
    height_rect_ = Rect(width_rect_.right() + 6.0f, width_rect_.y, ow - width_rect_.width - 6.0f, 22.0f);

    format_label_rect_ = Rect(ol, output_card_.y + 130.0f, 52.0f, 22.0f);
    png_rect_ = Rect(format_label_rect_.right() + 4.0f, format_label_rect_.y, 76.0f, 22.0f);
    exr_rect_ = Rect(png_rect_.right() + 5.0f, png_rect_.y, 94.0f, 22.0f);

    path_label_rect_ = Rect(ol, output_card_.y + 158.0f, 80.0f, 18.0f);
    browse_rect_ = Rect(oright - 86.0f, output_card_.y + 178.0f, 86.0f, 24.0f);
    path_text_rect_ = Rect(ol, browse_rect_.y, browse_rect_.x - ol - 8.0f, 24.0f);

    quality_card_ = Rect(left, settings_panel_.y + 344.0f, inner, 156.0f);
    samples_rect_ = Rect(quality_card_.x + 10.0f, quality_card_.y + 22.0f, (inner - 26.0f) * 0.5f, 24.0f);
    firefly_rect_ = Rect(samples_rect_.right() + 6.0f, samples_rect_.y, samples_rect_.width, 24.0f);
    light_tree_toggle_rect_ = Rect(quality_card_.x + 10.0f, quality_card_.y + 50.0f, inner - 20.0f, 22.0f);
    const float adw = (inner - 26.0f) * 0.52f;
    adaptive_toggle_rect_ = Rect(quality_card_.x + 10.0f, quality_card_.y + 76.0f, adw, 22.0f);
    adaptive_thresh_rect_ = Rect(adaptive_toggle_rect_.right() + 6.0f, adaptive_toggle_rect_.y, inner - 20.0f - adw - 6.0f, 22.0f);
    denoise_toggle_rect_ = Rect(quality_card_.x + 10.0f, quality_card_.y + 102.0f, inner - 20.0f, 22.0f);
    quality_note_rect_ = Rect(quality_card_.x + 10.0f, quality_card_.y + 128.0f, inner - 20.0f, 24.0f);

    footer_rect_ = Rect(left, settings_panel_.bottom() - 60.0f, inner, 50.0f);
    footer_hint_rect_ = Rect(left, footer_rect_.y, inner, 15.0f);
    cancel_rect_ = Rect(left + inner - 226.0f, footer_rect_.y + 18.0f, 84.0f, 28.0f);
    render_rect_ = Rect(cancel_rect_.right() + 8.0f, cancel_rect_.y, 134.0f, 28.0f);
}

void RenderSetupDialog::draw_section(Graphics& gfx, const Rect& rect, const char* title) {
    gfx.fill_rounded_rect(rect, 3.0f, Theme::kPanelHeaderBg);
    gfx.draw_rounded_rect(rect, 3.0f, Theme::kBorderSubtle, 1.0f);
    gfx.draw_text(title, Rect(rect.x + 10.0f, rect.y + 4.0f, rect.width - 20.0f, 15.0f),
                  Theme::kTextMuted, 9.0f, true, false, true, Theme::kUiFontFamily);
}

void RenderSetupDialog::draw_button(Graphics& gfx, const Rect& rect, const std::string& label, bool active, bool primary) {
    const bool hovered = rect.contains(last_mouse_);
    if (primary) {
        const Color bg = hovered ? Color(Theme::kAccent.r * 1.15f, Theme::kAccent.g * 1.15f, Theme::kAccent.b * 1.15f, 1.0f) : Theme::kAccent;
        gfx.fill_rounded_rect(rect, 3.0f, bg);
        gfx.draw_rounded_rect(rect, 3.0f, Color(1.0f, 1.0f, 1.0f, 0.40f), 1.0f);
        gfx.draw_text(label, Rect(rect.x + 4.0f, rect.y, rect.width - 8.0f, rect.height),
                      Color(1.0f, 1.0f, 1.0f, 1.0f), Theme::kUiFont, true, true, true, Theme::kUiFontFamily);
    } else {
        gfx.fill_rounded_rect(rect, 2.0f, active ? Theme::kSelectionBg : (hovered ? Theme::kControlHover : Theme::kControlBg));
        gfx.draw_rounded_rect(rect, 2.0f, active ? Theme::kAccentBorder : Theme::kBorderSubtle, 1.0f);
        gfx.draw_text(label, Rect(rect.x + 3.0f, rect.y, rect.width - 6.0f, rect.height),
                      active ? Theme::kAccentText : Theme::kTextSecondary,
                      Theme::kSmallFont, active, true, true, Theme::kUiFontFamily);
    }
}

void RenderSetupDialog::draw_number_field(Graphics& gfx, const Rect& rect, const char* label, const std::string& value, bool focused) {
    gfx.fill_rect(rect, focused ? Theme::kSelectionBg : Theme::kControlBg);
    gfx.draw_rect(rect, focused ? Theme::kAccentBorder : Theme::kBorderSubtle, 1.0f);
    const float labelW = label[1] == '\0' ? 24.0f : 42.0f;
    gfx.draw_text(label, Rect(rect.x + 5.0f, rect.y, labelW, rect.height),
                  Theme::kTextMuted, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
    gfx.draw_text(value, Rect(rect.x + labelW, rect.y, rect.width - labelW - 6.0f, rect.height),
                  Theme::kTextPrimary, Theme::kSmallFont, false, false, true, Theme::kNumericFontFamily);
}

std::pair<int, int> RenderSetupDialog::selected_range() const {
    if (state_.render_frame_mode == EditorState::RenderFrameMode::CurrentFrame)
        return {state_.current_frame, state_.current_frame};
    if (state_.render_frame_mode == EditorState::RenderFrameMode::CustomRange)
        return {std::max(1, state_.render_custom_start), std::max(state_.render_custom_start, state_.render_custom_end)};
    return {state_.start_frame, state_.end_frame};
}

bool RenderSetupDialog::is_preset_resolution() const {
    return (state_.render_output_width == 1280 && state_.render_output_height == 720) ||
           (state_.render_output_width == 1920 && state_.render_output_height == 1080);
}

void RenderSetupDialog::set_resolution(int width, int height) {
    state_.render_output_width = width;
    state_.render_output_height = height;
}

int RenderSetupDialog::current_field_value() const {
    switch (field_) {
        case Field::Start: return state_.render_custom_start;
        case Field::End: return state_.render_custom_end;
        case Field::Width: return state_.render_output_width;
        case Field::Height: return state_.render_output_height;
        case Field::None: default: return 0;
    }
}

std::string RenderSetupDialog::displayed_number(Field field, int value) const {
    return field_ == field ? field_buffer_ : std::to_string(value);
}

void RenderSetupDialog::begin_field(Field field) {
    finish_field(true);
    field_ = field;
    field_buffer_ = std::to_string(current_field_value());
    replace_field_buffer_ = true;
}

void RenderSetupDialog::apply_field_value(int value) {
    switch (field_) {
        case Field::Start:
            state_.render_custom_start = std::clamp(value, 1, std::max(1, state_.total_frames));
            state_.render_custom_end = std::max(state_.render_custom_start, state_.render_custom_end);
            break;
        case Field::End:
            state_.render_custom_end = std::clamp(value, state_.render_custom_start, std::max(state_.render_custom_start, state_.total_frames));
            break;
        case Field::Width:
            state_.render_output_width = std::clamp(value, 320, 1920);
            break;
        case Field::Height:
            state_.render_output_height = std::clamp(value, 180, 1080);
            break;
        case Field::None:
            break;
    }
}

void RenderSetupDialog::finish_field(bool accept) {
    if (field_ == Field::None) return;
    if (accept && !field_buffer_.empty()) {
        int value = 0;
        for (char digit : field_buffer_) {
            if (digit >= '0' && digit <= '9') value = std::min(1000000, value * 10 + digit - '0');
        }
        apply_field_value(value);
    }
    field_ = Field::None;
    field_buffer_.clear();
    replace_field_buffer_ = true;
}

} // namespace medusa
