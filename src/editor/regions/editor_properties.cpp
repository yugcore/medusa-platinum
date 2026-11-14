#include <medusa/editor/regions/editor_properties.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <medusa/ui/medusa_icons.hpp>
#include <algorithm>
#include <cmath>

namespace medusa {

AttributeRegion::AttributeRegion(EditorState &state) : state_(state) {
    init_panels();
}

void AttributeRegion::layout(const Rect &bounds) {
    bounds_ = bounds;
    header_rect_ = Rect(bounds.x, bounds.y, bounds.width, Theme::kAreaHeaderHeight);

    float y = bounds.y + 34.0f;
    float w = (bounds.width - 24.0f) / 3.0f;

    const float triplet_w = bounds.width - 16.0f;
    const float triplet_h = DCCTransformTriplet::preferred_height(true);

    if (show_transform()) {
        transform_header_.set_bounds(Rect(bounds.x, y, bounds.width, 22.0f));
        y += 28.0f;

        if (transform_header_.is_expanded()) {
            location_triplet_.set_bounds(Rect(bounds.x + 8.0f, y, triplet_w, triplet_h));
            y += triplet_h + 8.0f;

            const bool show_rot = state_.camera_selected() || state_.transformable_selected();
            rotation_triplet_.set_visible(show_rot);
            if (show_rot) {
                rotation_triplet_.set_bounds(Rect(bounds.x + 8.0f, y, triplet_w, triplet_h));
                y += triplet_h + 8.0f;
            }

            const bool show_scale = state_.transformable_selected();
            scale_triplet_.set_visible(show_scale);
            if (show_scale) {
                scale_triplet_.set_bounds(Rect(bounds.x + 8.0f, y, triplet_w, triplet_h));
                y += triplet_h + 8.0f;
            }
        }
    }

    if (show_lighting()) {
        light_header_.set_bounds(Rect(bounds.x, y, bounds.width, 22.0f));
        y += 28.0f;
        if (light_header_.is_expanded()) {
            light_pos_triplet_.set_bounds(Rect(bounds.x + 8.0f, y, triplet_w, triplet_h));
            y += triplet_h + 8.0f;
            light_energy_.set_bounds(Rect(bounds.x + 8.0f, y, bounds.width - 16.0f, Theme::kControlHeight));
            y += 28.0f;
        }
    }

    if (show_optics()) {
        optics_header_.set_bounds(Rect(bounds.x, y, bounds.width, 22.0f));
        y += 28.0f;

        if (optics_header_.is_expanded()) {
            focal_len_.set_bounds(Rect(bounds.x + 8.0f, y, bounds.width - 16.0f, Theme::kControlHeight));
            y += 28.0f;
        }
    }

    if (show_render()) {
        render_header_.set_bounds(Rect(bounds.x, y, bounds.width, 22.0f));
        y += 28.0f;

        if (render_header_.is_expanded()) {
            float dw = (bounds.width - 20.0f) / 2.0f;
            max_samples_.set_bounds(Rect(bounds.x + 8.0f, y, dw, Theme::kControlHeight));
            firefly_clamp_.set_bounds(Rect(bounds.x + 8.0f + dw + 6.0f, y, dw, Theme::kControlHeight));
            y += 28.0f;
        }
    }

    if (show_material()) {
        material_header_.set_bounds(Rect(bounds.x, y, bounds.width, 22.0f));
        y += 28.0f;

        if (material_header_.is_expanded()) {
            // Row 1: Base Color Swatch (left) + R, G, B inputs (right)
            const float swatchW = 34.0f;
            mat_color_swatch_rect_ = Rect(bounds.x + 8.0f, y, swatchW, Theme::kControlHeight);
            const float rgbW = (bounds.width - 24.0f - swatchW - 12.0f) / 3.0f;
            mat_col_r_.set_bounds(Rect(bounds.x + 8.0f + swatchW + 6.0f, y, rgbW, Theme::kControlHeight));
            mat_col_g_.set_bounds(Rect(bounds.x + 8.0f + swatchW + 6.0f + rgbW + 4.0f, y, rgbW, Theme::kControlHeight));
            mat_col_b_.set_bounds(Rect(bounds.x + 8.0f + swatchW + 6.0f + (rgbW + 4.0f) * 2.0f, y, rgbW, Theme::kControlHeight));
            y += 28.0f;

            // Row 2: Roughness & Metallic
            const float halfW = (bounds.width - 22.0f) / 2.0f;
            mat_roughness_.set_bounds(Rect(bounds.x + 8.0f, y, halfW, Theme::kControlHeight));
            mat_metallic_.set_bounds(Rect(bounds.x + 8.0f + halfW + 6.0f, y, halfW, Theme::kControlHeight));
            y += 28.0f;

            // Row 3: Transmission & Specular IOR
            mat_transmission_.set_bounds(Rect(bounds.x + 8.0f, y, halfW, Theme::kControlHeight));
            mat_ior_.set_bounds(Rect(bounds.x + 8.0f + halfW + 6.0f, y, halfW, Theme::kControlHeight));
            y += 28.0f;

            // Row 4: Coat Weight & Coat Roughness
            mat_coat_weight_.set_bounds(Rect(bounds.x + 8.0f, y, halfW, Theme::kControlHeight));
            mat_coat_roughness_.set_bounds(Rect(bounds.x + 8.0f + halfW + 6.0f, y, halfW, Theme::kControlHeight));
            y += 28.0f;

            // Row 5: Sheen Weight & Sheen Roughness
            mat_sheen_weight_.set_bounds(Rect(bounds.x + 8.0f, y, halfW, Theme::kControlHeight));
            mat_sheen_roughness_.set_bounds(Rect(bounds.x + 8.0f + halfW + 6.0f, y, halfW, Theme::kControlHeight));
            y += 28.0f;

            // Row 6: Emission Luminance
            mat_emission_.set_bounds(Rect(bounds.x + 8.0f, y, bounds.width - 16.0f, Theme::kControlHeight));
            y += 28.0f;
        }
    }
    status_rect_ = Rect(bounds.x + 12.0f, y + 6.0f, bounds.width - 24.0f, 60.0f);
}

void AttributeRegion::draw(Graphics &gfx) {
    gfx.fill_rect(bounds_, Theme::kPanelBg);

    // Header (Blender Properties header style)
    gfx.fill_rect(header_rect_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(header_rect_.x, header_rect_.bottom()), Point(header_rect_.right(), header_rect_.bottom()), Theme::kDockDivider, 1.0f);

    Rect icon_r(header_rect_.x + 8.0f, header_rect_.y + 5.0f, 14.0f, 14.0f);
    IconRenderer::draw_icon(gfx, "tool_pivot", icon_r, Theme::kTextSecondary);

    std::string selected_name = state_.active_workspace + " Properties";
    if (state_.selected_scene_node >= 0 && static_cast<size_t>(state_.selected_scene_node) < state_.scene_nodes.size())
        selected_name = state_.scene_nodes[static_cast<size_t>(state_.selected_scene_node)].name + " Properties";
    gfx.draw_text(selected_name, Rect(header_rect_.x + 28.0f, header_rect_.y, header_rect_.width - 36.0f, header_rect_.height),
                  Theme::kTextPrimary, Theme::kPanelTitleFont, true, false, true, Theme::kUiFontFamily);

    if (show_transform()) {
        location_triplet_.set_values(state_.pos_x, state_.pos_y, state_.pos_z);
        rotation_triplet_.set_values(state_.rot_x, state_.rot_y, state_.rot_z);
        scale_triplet_.set_values(state_.scale_x, state_.scale_y, state_.scale_z);
        transform_header_.draw(gfx);
        if (transform_header_.is_expanded()) {
            location_triplet_.draw(gfx);
            if (state_.camera_selected() || state_.transformable_selected()) {
                rotation_triplet_.draw(gfx);
            }
            if (state_.transformable_selected()) {
                scale_triplet_.draw(gfx);
            }
        }
    }

    if (show_lighting()) {
        light_header_.draw(gfx);
        if (light_header_.is_expanded()) {
            light_pos_triplet_.set_values(state_.key_light_x, state_.key_light_y, state_.key_light_z);
            light_pos_triplet_.draw(gfx);
            light_energy_.set_value(state_.key_light_energy);
            light_energy_.draw(gfx);
        }
    }

    if (show_optics()) {
        optics_header_.draw(gfx);
        if (optics_header_.is_expanded()) {
            focal_len_.set_value(state_.focal_length);
            focal_len_.draw(gfx);
        }
    }

    if (show_render()) {
        render_header_.draw(gfx);
        if (render_header_.is_expanded()) {
            max_samples_.set_value(static_cast<float>(state_.max_samples_per_pixel));
            firefly_clamp_.set_value(state_.firefly_clamp);
            max_samples_.draw(gfx);
            firefly_clamp_.draw(gfx);
        }
    }

    if (show_material()) {
        material_header_.draw(gfx);
        if (material_header_.is_expanded()) {
            // Draw Color Swatch
            Color swatchCol(state_.mat_base_color[0], state_.mat_base_color[1], state_.mat_base_color[2], 1.0f);
            gfx.fill_rounded_rect(mat_color_swatch_rect_, 3.0f, swatchCol);
            gfx.draw_rounded_rect(mat_color_swatch_rect_, 3.0f, Theme::kBorderBright, 1.0f);

            // Draw RGB
            mat_col_r_.set_value(state_.mat_base_color[0]);
            mat_col_g_.set_value(state_.mat_base_color[1]);
            mat_col_b_.set_value(state_.mat_base_color[2]);
            mat_col_r_.draw(gfx);
            mat_col_g_.draw(gfx);
            mat_col_b_.draw(gfx);

            // Draw Sliders
            mat_roughness_.set_value(state_.mat_roughness);
            mat_metallic_.set_value(state_.mat_metallic);
            mat_roughness_.draw(gfx);
            mat_metallic_.draw(gfx);

            mat_transmission_.set_value(state_.mat_transmission);
            mat_ior_.set_value(state_.mat_ior);
            mat_transmission_.draw(gfx);
            mat_ior_.draw(gfx);

            mat_coat_weight_.set_value(state_.mat_coat_weight);
            mat_coat_roughness_.set_value(state_.mat_coat_roughness);
            mat_coat_weight_.draw(gfx);
            mat_coat_roughness_.draw(gfx);

            mat_sheen_weight_.set_value(state_.mat_sheen_weight);
            mat_sheen_roughness_.set_value(state_.mat_sheen_roughness);
            mat_sheen_weight_.draw(gfx);
            mat_sheen_roughness_.draw(gfx);

            mat_emission_.set_value(state_.mat_emission_energy);
            mat_emission_.draw(gfx);
        }
    }
    if (state_.mesh_selected() || (state_.transformable_selected() && show_transform())) {
        gfx.fill_rounded_rect(status_rect_, 4.0f, Theme::kControlBg);
        gfx.draw_rounded_rect(status_rect_, 4.0f, Theme::kBorderSubtle, 1.0f);

        Rect info_icon_r(status_rect_.x + 8.0f, status_rect_.y + 8.0f, 14.0f, 14.0f);
        IconRenderer::draw_icon(gfx, "view_grid", info_icon_r, Theme::kTextMuted);

        float text_y = status_rect_.y + 6.0f;
        if (state_.mesh_selected()) {
            const std::string render_state = effectively_renderable(state_.selected_scene_node)
                ? "Included in production render" : "Excluded from production render";
            gfx.draw_text(render_state, Rect(status_rect_.x + 28.0f, text_y, status_rect_.width - 34.0f, 16.0f),
                          Theme::kTextPrimary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
            text_y += 18.0f;
        }
        if (state_.transformable_selected() && show_transform()) {
            gfx.draw_text("World translation · Rotation & Scale active",
                          Rect(status_rect_.x + 28.0f, text_y, status_rect_.width - 34.0f, 16.0f),
                          Theme::kTextSecondary, Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);
        }
    }
    if (state_.scene_nodes.empty() && state_.selected_scene_node < 0) {
        const float centerY = bounds_.y + std::max(110.0f, bounds_.height * 0.42f);
        gfx.draw_text("Nothing selected", Rect(bounds_.x + 12.0f, centerY, bounds_.width - 24.0f, 22.0f),
                      Theme::kTextSecondary, 12.0f, true, true, true, Theme::kUiFontFamily);
        gfx.draw_text("Import a model to see its transform and render controls.",
                      Rect(bounds_.x + 20.0f, centerY + 24.0f, bounds_.width - 40.0f, 44.0f),
                      Theme::kTextMuted, Theme::kSmallFont, false, true, true, Theme::kUiFontFamily);
    }
}

bool AttributeRegion::handle_mouse_down(const MouseEvent &e) {
    if (show_transform() && transform_header_.handle_mouse_down(e)) { layout(bounds_); return true; }
    if (show_lighting() && light_header_.handle_mouse_down(e)) { layout(bounds_); return true; }
    if (show_optics() && optics_header_.handle_mouse_down(e)) { layout(bounds_); return true; }
    if (show_render() && render_header_.handle_mouse_down(e)) { layout(bounds_); return true; }
    if (show_material() && material_header_.handle_mouse_down(e)) { layout(bounds_); return true; }

    if (show_transform() && transform_header_.is_expanded()) {
        if (location_triplet_.handle_mouse_down(e)) return true;
        if ((state_.camera_selected() || state_.transformable_selected()) && rotation_triplet_.handle_mouse_down(e)) return true;
        if (state_.transformable_selected() && scale_triplet_.handle_mouse_down(e)) return true;
    }

    if (show_lighting() && light_header_.is_expanded()) {
        if (light_pos_triplet_.handle_mouse_down(e)) return true;
        if (light_energy_.handle_mouse_down(e)) return true;
    }

    if (show_optics() && optics_header_.is_expanded()) {
        if (focal_len_.handle_mouse_down(e)) return true;
    }

    if (show_render() && render_header_.is_expanded()) {
        if (max_samples_.handle_mouse_down(e)) return true;
        if (firefly_clamp_.handle_mouse_down(e)) return true;
    }

    if (show_material() && material_header_.is_expanded()) {
        if (mat_col_r_.handle_mouse_down(e)) return true;
        if (mat_col_g_.handle_mouse_down(e)) return true;
        if (mat_col_b_.handle_mouse_down(e)) return true;
        if (mat_roughness_.handle_mouse_down(e)) return true;
        if (mat_metallic_.handle_mouse_down(e)) return true;
        if (mat_transmission_.handle_mouse_down(e)) return true;
        if (mat_ior_.handle_mouse_down(e)) return true;
        if (mat_coat_weight_.handle_mouse_down(e)) return true;
        if (mat_coat_roughness_.handle_mouse_down(e)) return true;
        if (mat_sheen_weight_.handle_mouse_down(e)) return true;
        if (mat_sheen_roughness_.handle_mouse_down(e)) return true;
        if (mat_emission_.handle_mouse_down(e)) return true;
    }

    return false;
}

bool AttributeRegion::handle_mouse_move(const MouseEvent &e) {
    bool changed = false;
    if (transform_header_.handle_mouse_move(e)) changed = true;
    if (light_header_.handle_mouse_move(e)) changed = true;
    if (optics_header_.handle_mouse_move(e)) changed = true;
    if (render_header_.handle_mouse_move(e)) changed = true;
    if (material_header_.handle_mouse_move(e)) changed = true;

    if (transform_header_.is_expanded()) {
        if (location_triplet_.handle_mouse_move(e)) changed = true;
        if ((state_.camera_selected() || state_.transformable_selected()) && rotation_triplet_.handle_mouse_move(e)) changed = true;
        if (state_.transformable_selected() && scale_triplet_.handle_mouse_move(e)) changed = true;
    }

    if (light_header_.is_expanded()) {
        if (light_pos_triplet_.handle_mouse_move(e)) changed = true;
        if (light_energy_.handle_mouse_move(e)) changed = true;
    }

    if (optics_header_.is_expanded()) {
        if (focal_len_.handle_mouse_move(e)) changed = true;
    }

    if (render_header_.is_expanded()) {
        if (max_samples_.handle_mouse_move(e)) changed = true;
        if (firefly_clamp_.handle_mouse_move(e)) changed = true;
    }

    if (material_header_.is_expanded()) {
        if (mat_col_r_.handle_mouse_move(e)) changed = true;
        if (mat_col_g_.handle_mouse_move(e)) changed = true;
        if (mat_col_b_.handle_mouse_move(e)) changed = true;
        if (mat_roughness_.handle_mouse_move(e)) changed = true;
        if (mat_metallic_.handle_mouse_move(e)) changed = true;
        if (mat_transmission_.handle_mouse_move(e)) changed = true;
        if (mat_ior_.handle_mouse_move(e)) changed = true;
        if (mat_coat_weight_.handle_mouse_move(e)) changed = true;
        if (mat_coat_roughness_.handle_mouse_move(e)) changed = true;
        if (mat_sheen_weight_.handle_mouse_move(e)) changed = true;
        if (mat_sheen_roughness_.handle_mouse_move(e)) changed = true;
        if (mat_emission_.handle_mouse_move(e)) changed = true;
    }

    return changed;
}

bool AttributeRegion::handle_mouse_up(const MouseEvent &e) {
    bool changed = false;
    bool positionChanged = location_triplet_.handle_mouse_up(e);
    changed = positionChanged || changed;
    if (positionChanged && state_.transformable_selected() && !state_.camera_selected() &&
        state_.selected_scene_node >= 0 && !state_.scene_nodes[static_cast<size_t>(state_.selected_scene_node)].locked) {
        const float position[3] = {state_.pos_x, state_.pos_y, state_.pos_z};
        state_.set_scene_node_translation(static_cast<uint32_t>(state_.selected_scene_node), position);
    }
    bool rotationChanged = rotation_triplet_.handle_mouse_up(e);
    bool scaleChanged = scale_triplet_.handle_mouse_up(e);
    changed = rotationChanged || scaleChanged || changed;
    if ((rotationChanged || scaleChanged) && state_.transformable_selected() &&
        state_.selected_scene_node >= 0 && static_cast<size_t>(state_.selected_scene_node) < state_.scene_nodes.size() &&
        !state_.scene_nodes[static_cast<size_t>(state_.selected_scene_node)].locked) {
        const float rotation[3] = {state_.rot_x, state_.rot_y, state_.rot_z};
        const float scale[3] = {state_.scale_x, state_.scale_y, state_.scale_z};
        state_.set_scene_node_rotation_scale(static_cast<uint32_t>(state_.selected_scene_node), rotation, scale);
    }
    if (light_pos_triplet_.handle_mouse_up(e)) changed = true;
    if (light_energy_.handle_mouse_up(e)) changed = true;
    if (focal_len_.handle_mouse_up(e)) changed = true;
    if (max_samples_.handle_mouse_up(e)) changed = true;
    if (firefly_clamp_.handle_mouse_up(e)) changed = true;
    if (mat_col_r_.handle_mouse_up(e)) changed = true;
    if (mat_col_g_.handle_mouse_up(e)) changed = true;
    if (mat_col_b_.handle_mouse_up(e)) changed = true;
    if (mat_roughness_.handle_mouse_up(e)) changed = true;
    if (mat_metallic_.handle_mouse_up(e)) changed = true;
    if (mat_transmission_.handle_mouse_up(e)) changed = true;
    if (mat_ior_.handle_mouse_up(e)) changed = true;
    if (mat_coat_weight_.handle_mouse_up(e)) changed = true;
    if (mat_coat_roughness_.handle_mouse_up(e)) changed = true;
    if (mat_sheen_weight_.handle_mouse_up(e)) changed = true;
    if (mat_sheen_roughness_.handle_mouse_up(e)) changed = true;
    if (mat_emission_.handle_mouse_up(e)) changed = true;
    return changed;
}

bool AttributeRegion::effectively_renderable(int index) const {
    size_t depth = 0;
    while (index >= 0 && static_cast<size_t>(index) < state_.scene_nodes.size() &&
           depth++ < state_.scene_nodes.size()) {
        const auto& node = state_.scene_nodes[static_cast<size_t>(index)];
        if (!node.visible || !node.renderable) return false;
        index = node.parentIndex;
    }
    return true;
}

bool AttributeRegion::show_transform() const {
    if (state_.active_workspace == "Shot") {
        return state_.camera_selected() && (state_.active_camera_node < 0 ||
            (static_cast<size_t>(state_.active_camera_node) < state_.scene_nodes.size() &&
             !state_.scene_nodes[static_cast<size_t>(state_.active_camera_node)].locked));
    }
    return state_.active_workspace == "Scene" && state_.transformable_selected() &&
        state_.selected_scene_node >= 0 &&
        static_cast<size_t>(state_.selected_scene_node) < state_.scene_nodes.size() &&
        !state_.scene_nodes[static_cast<size_t>(state_.selected_scene_node)].locked;
}

bool AttributeRegion::show_lighting() const {
    return state_.active_workspace == "Scene" && state_.light_selected();
}

bool AttributeRegion::show_optics() const {
    return state_.active_workspace == "Shot" && state_.camera_selected();
}

bool AttributeRegion::show_render() const {
    return state_.active_workspace == "Production";
}

bool AttributeRegion::show_material() const {
    return (state_.active_workspace == "Scene" || state_.active_workspace == "Production") &&
           (state_.mesh_selected() || !state_.scene_materials.empty());
}

void AttributeRegion::init_panels() {
    Color neutral = Theme::kTextSecondary;
    transform_header_ = CollapsibleSectionWidget("Transform", true);
    optics_header_ = CollapsibleSectionWidget("Camera Lens", true);
    light_header_ = CollapsibleSectionWidget("Key Light", true);
    render_header_ = CollapsibleSectionWidget("Production Sampling", true);
    material_header_ = CollapsibleSectionWidget("OpenPBR Material", true);

    auto updateMaterial = [this]() {
        state_.sync_fields_to_active_material();
        const uint32_t activeIdx = state_.active_material_index();
        state_.pending_command = "material_update:" + std::to_string(activeIdx);
    };

    mat_col_r_ = ScrubbableNumberWidget("R", state_.mat_base_color[0], "", Theme::kAxisX, 0.0f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_base_color[0] = v; updateMaterial(); });
    mat_col_g_ = ScrubbableNumberWidget("G", state_.mat_base_color[1], "", Theme::kAxisY, 0.0f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_base_color[1] = v; updateMaterial(); });
    mat_col_b_ = ScrubbableNumberWidget("B", state_.mat_base_color[2], "", Theme::kAxisZ, 0.0f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_base_color[2] = v; updateMaterial(); });

    mat_roughness_ = ScrubbableNumberWidget("Roughness", state_.mat_roughness, "", neutral, 0.0f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_roughness = v; updateMaterial(); });
    mat_metallic_ = ScrubbableNumberWidget("Metallic", state_.mat_metallic, "", neutral, 0.0f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_metallic = v; updateMaterial(); });

    mat_transmission_ = ScrubbableNumberWidget("Transmission", state_.mat_transmission, "", neutral, 0.0f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_transmission = v; updateMaterial(); });
    mat_ior_ = ScrubbableNumberWidget("Specular IOR", state_.mat_ior, "", neutral, 1.0f, 3.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_ior = v; updateMaterial(); });

    mat_coat_weight_ = ScrubbableNumberWidget("Coat Weight", state_.mat_coat_weight, "", neutral, 0.0f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_coat_weight = v; updateMaterial(); });
    mat_coat_roughness_ = ScrubbableNumberWidget("Coat Roughness", state_.mat_coat_roughness, "", neutral, 0.001f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_coat_roughness = v; updateMaterial(); });

    mat_sheen_weight_ = ScrubbableNumberWidget("Sheen Weight", state_.mat_sheen_weight, "", neutral, 0.0f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_sheen_weight = v; updateMaterial(); });
    mat_sheen_roughness_ = ScrubbableNumberWidget("Sheen Roughness", state_.mat_sheen_roughness, "", neutral, 0.01f, 1.0f, 0.01f,
        [this, updateMaterial](float v) { state_.mat_sheen_roughness = v; updateMaterial(); });

    mat_emission_ = ScrubbableNumberWidget("Emission", state_.mat_emission_energy, "", neutral, 0.0f, 100.0f, 0.1f,
        [this, updateMaterial](float v) { state_.mat_emission_energy = v; updateMaterial(); });

    location_triplet_ = DCCTransformTriplet("Location", "m", -1000.0f, 1000.0f, 0.1f,
        [this](int axis, float v) {
            if (axis == 0) state_.pos_x = v;
            else if (axis == 1) state_.pos_y = v;
            else if (axis == 2) state_.pos_z = v;
            if (state_.camera_selected()) {
                state_.camera_position[axis] = v;
                state_.update_camera_pose_fields();
            }
        });

    rotation_triplet_ = DCCTransformTriplet("Rotation", "°", -360.0f, 360.0f, 1.0f,
        [this](int axis, float v) {
            if (axis == 0) state_.rot_x = v;
            else if (axis == 1) state_.rot_y = v;
            else if (axis == 2) state_.rot_z = v;
            if (state_.camera_selected()) {
                state_.camera_rotation[axis] = v;
                state_.update_camera_pose_fields();
            }
        });

    scale_triplet_ = DCCTransformTriplet("Scale", "x", 0.001f, 1000.0f, 0.01f,
        [this](int axis, float v) {
            if (axis == 0) state_.scale_x = v;
            else if (axis == 1) state_.scale_y = v;
            else if (axis == 2) state_.scale_z = v;
        });

    light_pos_triplet_ = DCCTransformTriplet("Position", "m", -10000.0f, 10000.0f, 0.1f,
        [this](int axis, float v) {
            if (axis == 0) state_.key_light_x = v;
            else if (axis == 1) state_.key_light_y = v;
            else if (axis == 2) state_.key_light_z = v;
            if (state_.key_light_node >= 0 && static_cast<size_t>(state_.key_light_node) < state_.scene_nodes.size()) {
                const float position[3] = {state_.key_light_x, state_.key_light_y, state_.key_light_z};
                state_.set_scene_node_translation(static_cast<uint32_t>(state_.key_light_node), position);
            }
            state_.record_keyframe_if_enabled();
        });

    light_energy_ = ScrubbableNumberWidget("Energy", state_.key_light_energy, "x", neutral, 0.0f, 100.0f, 0.1f,
        [this](float v) { state_.key_light_energy = v; if (state_.key_light_node >= 0 && static_cast<size_t>(state_.key_light_node) < state_.scene_node_ids.size() && state_.scene_node_ids[static_cast<size_t>(state_.key_light_node)].rfind("medusa-object-light-", 0) == 0) state_.set_scene_node_light_energy(static_cast<uint32_t>(state_.key_light_node), v); state_.record_keyframe_if_enabled(); });

    focal_len_ = ScrubbableNumberWidget("FL", state_.focal_length, "mm", neutral, 10.0f, 500.0f, 1.0f,
        [this](float v) { state_.focal_length = v; state_.camera_focal_length = v; state_.record_keyframe_if_enabled(); });

    max_samples_ = ScrubbableNumberWidget("Samples", static_cast<float>(state_.max_samples_per_pixel), "spp", neutral, 1.0f, 4096.0f, 16.0f,
        [this](float v) { state_.max_samples_per_pixel = std::max(1, static_cast<int>(std::round(v / 16.0f) * 16.0f)); });
    firefly_clamp_ = ScrubbableNumberWidget("Clamp", state_.firefly_clamp, "", neutral, 0.0f, 1000.0f, 1.0f,
        [this](float v) { state_.firefly_clamp = v; });
}

} // namespace medusa
