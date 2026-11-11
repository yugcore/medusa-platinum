#include <medusa/medusa_editor.hpp>
#include <medusa/editor/drawers/asset_library_drawer.hpp>
#include <medusa/editor/dialogs/render_setup_dialog.hpp>
#include <medusa/medusa_production_scene.hpp>
#include <medusa/editor/regions/editor_topbar.hpp>
#include <medusa/editor/regions/editor_toolshelf.hpp>
#include <medusa/medusa_theme.hpp>
#include <medusa/editor/regions/editor_outliner.hpp>
#include <medusa/editor/regions/editor_properties.hpp>
#include <medusa/editor/regions/editor_timeline.hpp>
#include <medusa/editor/regions/editor_viewport.hpp>
#include <medusa/medusa_icons.hpp>
#include <iostream>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <filesystem>
#include <chrono>
#include <cctype>
#include <sstream>
#include <utility>

namespace medusa {

// ==============================================================================
// MedusaEditor (Screen Layout & Docking Splitter Assembly)
// ==============================================================================

MedusaEditor::MedusaEditor() {
    asset_drawer_ = std::make_unique<AssetLibraryDrawer>(state_);
    render_setup_dialog_ = std::make_unique<RenderSetupDialog>(state_);
    setup_regions();
}

MedusaEditor::~MedusaEditor() = default;

void MedusaEditor::setup_regions() {
    clear_areas();

    topbar_area_ = std::make_shared<Area>("Topbar");

    toolshelf_area_ = std::make_shared<Area>("Toolshelf");
    toolshelf_area_->add_region(std::make_shared<ToolshelfRegion>(state_));

    viewport_area_ = std::make_shared<Area>("Viewport");
    viewport_area_->add_region(std::make_shared<ViewportRegion>(state_));

    outliner_area_ = std::make_shared<Area>("Outliner");
    outliner_area_->add_region(std::make_shared<OutlinerRegion>(state_));
    add_area(outliner_area_);

    attribute_area_ = std::make_shared<Area>("Attributes");
    attribute_area_->add_region(std::make_shared<AttributeRegion>(state_));
    add_area(attribute_area_);

    timeline_area_ = std::make_shared<Area>("Timeline");
    timeline_area_->add_region(std::make_shared<TimelineRegion>(state_));
    add_area(timeline_area_);

    // Viewport popovers and delayed tooltips draw over panels without touching
    // their non-overlapping bounds.
    add_area(viewport_area_);

    // Draw the toolbar after neighboring panels so delayed tooltips stay legible.
    add_area(toolshelf_area_);

    // Draw the topbar last so its dropdowns render above the surrounding panels.
    topbar_area_->add_region(std::make_shared<TopbarRegion>(state_));
    add_area(topbar_area_);
}

void MedusaEditor::rebuild_layout(float width, float height) {
    set_size(width, height);
    if (asset_drawer_) asset_drawer_->layout(width, height);

    // Preserve three useful work zones at normal sizes while allowing the
    // viewport to shrink cleanly on narrow windows instead of overlapping docks.
    const float min_toolshelf_w = 32.0f;
    const float max_toolshelf_w = std::max(min_toolshelf_w, std::min(50.0f, width * 0.08f));
    toolshelf_w_ = std::clamp(toolshelf_w_, min_toolshelf_w, max_toolshelf_w);
    const float min_viewport_w = std::min(240.0f, std::max(160.0f, width * 0.26f));
    const float max_right_deck_w = std::max(0.0f, width - toolshelf_w_ - min_viewport_w);
    const float min_right_deck_w = std::min(150.0f, max_right_deck_w);
    right_deck_w_ = std::clamp(right_deck_w_, min_right_deck_w, max_right_deck_w);

    const float topbar_h = Theme::kTopbarHeight;
    const float max_timeline_h = std::max(80.0f, std::min(380.0f, height - topbar_h - 80.0f));
    timeline_h_ = std::clamp(timeline_h_, 80.0f, max_timeline_h);

    state_.ui_toolshelf_width = toolshelf_w_;
    state_.ui_right_deck_width = right_deck_w_;
    state_.ui_outliner_ratio = outliner_ratio_;
    state_.ui_timeline_height = timeline_h_;

    float center_y = topbar_h;
    float center_h = std::max(80.0f, height - topbar_h - timeline_h_);
    float dock_h = std::max(0.0f, height - center_y);

    float viewport_w = std::max(1.0f, width - toolshelf_w_ - right_deck_w_);

    topbar_area_->set_bounds(Rect(0, 0, width, topbar_h));
    toolshelf_area_->set_bounds(Rect(0, center_y, toolshelf_w_, dock_h));
    viewport_area_->set_bounds(Rect(toolshelf_w_, center_y, viewport_w, center_h));

    float outliner_h = dock_h * outliner_ratio_;
    float attribute_h = dock_h - outliner_h;
    outliner_area_->set_bounds(Rect(width - right_deck_w_, center_y, right_deck_w_, outliner_h));
    attribute_area_->set_bounds(Rect(width - right_deck_w_, center_y + outliner_h, right_deck_w_, attribute_h));

    timeline_area_->set_bounds(Rect(toolshelf_w_, height - timeline_h_, viewport_w, timeline_h_));
}

void MedusaEditor::set_layout_settings(float toolshelfWidth, float rightDeckWidth,
                                      float outlinerRatio, float timelineHeight) {
    toolshelf_w_ = std::clamp(toolshelfWidth, 32.0f, 56.0f);
    right_deck_w_ = std::clamp(rightDeckWidth, 150.0f, std::max(150.0f, width_ * 0.5f));
    outliner_ratio_ = std::clamp(outlinerRatio, 0.20f, 0.80f);
    timeline_h_ = std::clamp(timelineHeight, 80.0f, 380.0f);
    rebuild_layout(width_, height_);
}

void MedusaEditor::reset_layout() {
    toolshelf_w_ = 42.0f;
    right_deck_w_ = 310.0f;
    outliner_ratio_ = 0.48f;
    timeline_h_ = 160.0f;
    rebuild_layout(width_, height_);
}

std::string MedusaEditor::take_pending_command() {
    std::string command = std::move(state_.pending_command);
    state_.pending_command.clear();
    return command;
}

bool MedusaEditor::tick_asset_drawer_animation() {
    return asset_drawer_ && asset_drawer_->tick_animation();
}

bool MedusaEditor::asset_drawer_animating() const {
    return asset_drawer_ && asset_drawer_->animating();
}

void MedusaEditor::set_asset_library_root(const std::string& path) {
    if (asset_drawer_) asset_drawer_->set_root(path);
}

void MedusaEditor::advance_animation_frame() {
    if (!state_.is_playing) return;
    if (state_.playback_reverse)
        state_.current_frame = state_.current_frame <= state_.start_frame ? state_.end_frame : state_.current_frame - 1;
    else
        state_.current_frame = state_.current_frame >= state_.end_frame ? state_.start_frame : state_.current_frame + 1;
    state_.evaluate_keyframes();
}

void MedusaEditor::initialize_render_controls(const zegfx::ProductionScene& scene,
                                             const std::vector<EditorState::SceneNodeBounds>& nodeBounds) {
    state_.scene_nodes = scene.nodes;
    state_.rebuild_scene_hierarchy();
    state_.scene_node_bounds = nodeBounds;
    state_.scene_name = scene.sourceName;
    state_.active_camera_node = scene.activeCameraNode;
    state_.key_light_node = scene.keyLightNode;
    state_.active_camera = scene.activeCameraNode >= 0 &&
        static_cast<size_t>(scene.activeCameraNode) < scene.nodes.size()
            ? scene.nodes[static_cast<size_t>(scene.activeCameraNode)].name : std::string();
    state_.selected_scene_node = scene.activeCameraNode;
    state_.pending_scene_node = -1;
    ++state_.scene_generation;
    state_.pos_x = scene.cameraPosition[0];
    state_.pos_y = scene.cameraPosition[1];
    state_.pos_z = scene.cameraPosition[2];
    std::copy(scene.cameraPosition, scene.cameraPosition + 3, state_.camera_position);
    const float dx = scene.cameraTarget[0] - scene.cameraPosition[0];
    const float dy = scene.cameraTarget[1] - scene.cameraPosition[1];
    const float dz = scene.cameraTarget[2] - scene.cameraPosition[2];
    const float length = std::max(1.0e-6f, std::sqrt(dx * dx + dy * dy + dz * dz));
    state_.focus_distance = length;
    state_.rot_x = std::asin(std::clamp(dy / length, -1.0f, 1.0f)) * 57.2957795f;
    state_.rot_y = std::atan2(dx, -dz) * 57.2957795f;
    state_.rot_z = 0.0f;
    state_.camera_rotation[0] = state_.rot_x;
    state_.camera_rotation[1] = state_.rot_y;
    state_.camera_rotation[2] = state_.rot_z;
    state_.viewport_navigation_active = false;
    state_.viewport_distance = length;
    std::copy(scene.cameraPosition, scene.cameraPosition + 3, state_.viewport_position);
    for (int axis = 0; axis < 3; ++axis)
        state_.viewport_target[axis] = scene.cameraPosition[axis] + (axis == 0 ? dx : axis == 1 ? dy : dz);
    std::copy(state_.camera_rotation, state_.camera_rotation + 3, state_.viewport_rotation);
    state_.focal_length = 18.0f / std::tan(scene.verticalFovDegrees * 0.00872664626f);
    state_.camera_focal_length = state_.focal_length;
    state_.key_light_x = scene.keyLightPosition[0];
    state_.key_light_y = scene.keyLightPosition[1];
    state_.key_light_z = scene.keyLightPosition[2];
    state_.key_light_energy = scene.keyLightNode >= 0 ? 1.0f : 0.0f;
    state_.current_frame = state_.start_frame;
    state_.keyframes.clear();
    state_.node_keyframes.clear();
    state_.keyframes.push_back(state_.capture_keyframe());
}

void MedusaEditor::set_scene_node_state(uint32_t nodeIndex, bool visible, bool renderable) {
    state_.set_scene_node_state(nodeIndex, visible, renderable);
}

Rect MedusaEditor::get_render_viewport_bounds() const {
    if (!viewport_area_) return Rect();
    const Rect area = viewport_area_->get_bounds();
    return Rect(area.x, area.y + Theme::kAreaHeaderHeight,
                area.width, std::max(0.0f, area.height - Theme::kAreaHeaderHeight));
}

SplitterId MedusaEditor::hit_test_splitter(Point p) const {
    float topbar_h = Theme::kTopbarHeight;
    float center_y = topbar_h;
    float dock_h = std::max(0.0f, height_ - center_y);

    // 1. Right Deck Vertical Splitter (between Viewport & Right Deck)
    float split_right_x = width_ - right_deck_w_;
    if (std::abs(p.x - split_right_x) <= 4.0f && p.y >= center_y && p.y <= height_) {
        return SplitterId::RightDeckWidth;
    }

    // 2. Right Deck Horizontal Splitter (between Outliner & Attributes)
    float split_deck_y = center_y + dock_h * outliner_ratio_;
    if (std::abs(p.y - split_deck_y) <= 4.0f && p.x >= split_right_x && p.x <= width_) {
        return SplitterId::RightDeckHeight;
    }

    // 3. Timeline Horizontal Splitter (between Center Areas & Timeline)
    float split_timeline_y = height_ - timeline_h_;
    if (std::abs(p.y - split_timeline_y) <= 4.0f &&
        p.x >= toolshelf_w_ && p.x <= width_ - right_deck_w_) {
        return SplitterId::TimelineHeight;
    }

    return SplitterId::None;
}

CursorType MedusaEditor::get_cursor_type() const {
    if (asset_drawer_) {
        const CursorType drawer_cursor = asset_drawer_->cursor_type();
        if (drawer_cursor != CursorType::Arrow) return drawer_cursor;
    }
    SplitterId active = (active_splitter_ != SplitterId::None) ? active_splitter_ : hovered_splitter_;
    if (active == SplitterId::RightDeckWidth) {
        return CursorType::SizeWE;
    } else if (active == SplitterId::RightDeckHeight || active == SplitterId::TimelineHeight) {
        return CursorType::SizeNS;
    }
    return CursorType::Arrow;
}

void MedusaEditor::draw(Graphics &gfx) {
    Screen::draw(gfx);

    // Draw active or hovered splitter highlight lines
    SplitterId active = (active_splitter_ != SplitterId::None) ? active_splitter_ : hovered_splitter_;
    if (active == SplitterId::RightDeckWidth) {
        float x = width_ - right_deck_w_;
        gfx.draw_line(Point(x, Theme::kTopbarHeight), Point(x, height_), Theme::kSplitterHover, 2.0f);
    } else if (active == SplitterId::RightDeckHeight) {
        float dock_h = std::max(0.0f, height_ - Theme::kTopbarHeight);
        float y = Theme::kTopbarHeight + dock_h * outliner_ratio_;
        gfx.draw_line(Point(width_ - right_deck_w_, y), Point(width_, y), Theme::kSplitterHover, 2.0f);
    } else if (active == SplitterId::TimelineHeight) {
        float y = height_ - timeline_h_;
        gfx.draw_line(Point(toolshelf_w_, y), Point(width_ - right_deck_w_, y), Theme::kSplitterHover, 2.0f);
    }
    if (asset_drawer_) asset_drawer_->draw(gfx);
    if (state_.render_setup_open && render_setup_dialog_) {
        render_setup_dialog_->draw(gfx, Rect(0.0f, 0.0f, width_, height_));
        const Rect dialogPanel = render_setup_dialog_->panel_bounds(Rect(0.0f, 0.0f, width_, height_));
        gfx.set_realtime_viewport_overlay_bounds(&dialogPanel);
    }
}

bool MedusaEditor::handle_mouse_down(const MouseEvent &e) {
    if (state_.render_setup_open && render_setup_dialog_)
        return render_setup_dialog_->handle_mouse_down(e, Rect(0.0f, 0.0f, width_, height_));
    if (state_.timeline_range_edit_field != EditorState::TimelineRangeField::None &&
        !timeline_area_->get_bounds().contains(e.pos))
        state_.finish_timeline_range_edit(true);
    if (asset_drawer_ && asset_drawer_->handle_mouse_down(e)) return true;
    const std::string workspace_before = state_.active_workspace;
    const bool popup_was_open = state_.menu_popup_open || state_.viewport_popup_open;
    if (state_.menu_popup_open && topbar_area_->handle_mouse_down(e)) {
        captured_area_.reset();
        if (workspace_before != state_.active_workspace) {
            const Rect property_bounds = attribute_area_->get_bounds();
            attribute_area_->set_bounds(property_bounds);
        }
        return true;
    }
    if (state_.viewport_popup_open && viewport_area_->handle_mouse_down(e)) {
        captured_area_.reset();
        return true;
    }

    if (e.button == MouseButton::Left) {
        SplitterId split = hit_test_splitter(e.pos);
        if (split != SplitterId::None) {
            active_splitter_ = split;
            drag_start_pos_ = e.pos;
            if (split == SplitterId::RightDeckWidth) drag_start_val_ = right_deck_w_;
            else if (split == SplitterId::RightDeckHeight) drag_start_val_ = outliner_ratio_;
            else if (split == SplitterId::TimelineHeight) drag_start_val_ = timeline_h_;
            return true;
        }
    }

    const bool handled = Screen::handle_mouse_down(e);
    if (workspace_before != state_.active_workspace) {
        const Rect property_bounds = attribute_area_->get_bounds();
        attribute_area_->set_bounds(property_bounds);
    }
    const bool popup_dismissed = popup_was_open && !state_.menu_popup_open && !state_.viewport_popup_open;
    return handled || popup_dismissed;
}

bool MedusaEditor::handle_mouse_move(const MouseEvent &e) {
    bool changed = Screen::handle_mouse_move(e);
    if (state_.render_setup_open && render_setup_dialog_)
        return render_setup_dialog_->handle_mouse_move(e, Rect(0.0f, 0.0f, width_, height_)) || changed;
    if (asset_drawer_ && asset_drawer_->handle_mouse_move(e)) return true;
    if (active_splitter_ != SplitterId::None) {
        if (active_splitter_ == SplitterId::RightDeckWidth) {
            float dx = drag_start_pos_.x - e.pos.x;
            const float min_viewport_w = std::min(220.0f, std::max(140.0f, width_ * 0.25f));
            const float min_right_deck_w = std::min(220.0f, std::max(150.0f, width_ * 0.25f));
            const float max_right_deck_w = std::max(min_right_deck_w,
                std::min(width_ * 0.5f, width_ - min_viewport_w - toolshelf_w_));
            right_deck_w_ = std::clamp(drag_start_val_ + dx, min_right_deck_w, max_right_deck_w);
            rebuild_layout(width_, height_);
            return true;
        } else if (active_splitter_ == SplitterId::RightDeckHeight) {
            float dock_h = std::max(0.0f, height_ - Theme::kTopbarHeight);
            float dy = e.pos.y - drag_start_pos_.y;
            float new_h = (drag_start_val_ * dock_h) + dy;
            outliner_ratio_ = std::clamp(new_h / std::max(1.0f, dock_h), 0.20f, 0.80f);
            rebuild_layout(width_, height_);
            return true;
        } else if (active_splitter_ == SplitterId::TimelineHeight) {
            float dy = drag_start_pos_.y - e.pos.y;
            timeline_h_ = std::clamp(drag_start_val_ + dy, 80.0f, 380.0f);
            rebuild_layout(width_, height_);
            return true;
        }
    }

    SplitterId prev_hover = hovered_splitter_;
    hovered_splitter_ = hit_test_splitter(e.pos);
    changed = (prev_hover != hovered_splitter_) || changed;
    return changed;
}

bool MedusaEditor::handle_mouse_up(const MouseEvent &e) {
    if (state_.render_setup_open && render_setup_dialog_)
        return render_setup_dialog_->handle_mouse_up(e, Rect(0.0f, 0.0f, width_, height_));
    if (asset_drawer_ && asset_drawer_->handle_mouse_up(e)) return true;
    if (state_.menu_popup_open && topbar_area_->handle_mouse_up(e)) {
        captured_area_.reset();
        return true;
    }
    if (state_.viewport_popup_open && viewport_area_->handle_mouse_up(e)) {
        captured_area_.reset();
        return true;
    }

    if (active_splitter_ != SplitterId::None) {
        active_splitter_ = SplitterId::None;
        return true;
    }

    return Screen::handle_mouse_up(e);
}

bool MedusaEditor::handle_mouse_wheel(float delta, Point p) {
    if (state_.render_setup_open) return true;
    if (asset_drawer_ && asset_drawer_->handle_mouse_wheel(delta, p)) return true;
    return Screen::handle_mouse_wheel(delta, p);
}

bool MedusaEditor::handle_raw_mouse_delta(int deltaX, int deltaY) {
    if (state_.render_setup_open) return true;
    return Screen::handle_raw_mouse_delta(deltaX, deltaY);
}

bool MedusaEditor::handle_key_down(const KeyEvent& event) {
    auto& state = state_;
    if (state.render_setup_open && render_setup_dialog_)
        return render_setup_dialog_->handle_key_down(event);
    if (state.timeline_range_edit_field != EditorState::TimelineRangeField::None) {
        const uint32_t key = event.key_code;
        int digit = -1;
        if (key >= '0' && key <= '9') digit = static_cast<int>(key - '0');
        else if (key >= 0x60 && key <= 0x69) digit = static_cast<int>(key - 0x60);
        if (digit >= 0) {
            if (state.timeline_range_edit_replace) state.timeline_range_edit_buffer.clear();
            if (state.timeline_range_edit_buffer.size() < 7)
                state.timeline_range_edit_buffer.push_back(static_cast<char>('0' + digit));
            state.timeline_range_edit_replace = false;
            return true;
        }
        if (key == 8) {
            if (state.timeline_range_edit_replace) state.timeline_range_edit_buffer.clear();
            else if (!state.timeline_range_edit_buffer.empty()) state.timeline_range_edit_buffer.pop_back();
            state.timeline_range_edit_replace = false;
            return true;
        }
        if (event.ctrl && (key == 'A' || key == 'a')) {
            state.timeline_range_edit_buffer.clear();
            state.timeline_range_edit_replace = false;
            return true;
        }
        if (key == 0x26 || key == 0x28) {
            int value = 0;
            for (char character : state.timeline_range_edit_buffer)
                if (character >= '0' && character <= '9')
                    value = std::min(1000000, value * 10 + (character - '0'));
            if (state.timeline_range_edit_replace || state.timeline_range_edit_buffer.empty())
                value = state.timeline_range_edit_field == EditorState::TimelineRangeField::Start
                    ? state.start_frame : state.end_frame;
            value = std::clamp(value + (key == 0x26 ? 1 : -1), 1, 1000000);
            state.timeline_range_edit_buffer = std::to_string(value);
            state.timeline_range_edit_replace = false;
            return true;
        }
        if (key == 9) {
            state.timeline_range_edit_field = state.timeline_range_edit_field == EditorState::TimelineRangeField::Start
                ? EditorState::TimelineRangeField::End : EditorState::TimelineRangeField::Start;
            state.timeline_range_edit_buffer = std::to_string(
                state.timeline_range_edit_field == EditorState::TimelineRangeField::Start
                    ? state.start_frame : state.end_frame);
            state.timeline_range_edit_replace = true;
            return true;
        }
        if (key == 13) return state.finish_timeline_range_edit(true);
        if (key == 27) return state.finish_timeline_range_edit(false);
        state.finish_timeline_range_edit(true);
        return true;
    }
    if (event.ctrl && (event.key_code == 'Z' || event.key_code == 'z')) {
        state.pending_command = "undo";
        return true;
    }
    if (event.ctrl && (event.key_code == 'Y' || event.key_code == 'y')) {
        state.pending_command = "redo";
        return true;
    }
    if (event.shift && event.key_code == 'F') {
        state.camera_pilot_active = true;
        if (state.active_workspace != "Shot") state.enter_viewport_camera();
        return true;
    }
    if (state.camera_pilot_active && event.key_code == 27) {
        state.camera_pilot_active = false;
        return true;
    }
    switch (event.key_code) {
        case 'Q': state.active_tool = "tool_select"; break;
        case 'W': state.active_tool = "tool_move"; break;
        case 'E': state.active_tool = "tool_rotate"; break;
        case 'R': state.active_tool = "tool_scale"; break;
        case 'F': state.frame_selected(); break;
        case 'I': state.insert_keyframe(); break;
        case 32: state.is_playing = !state.is_playing; break;
        default: return false;
    }
    return true;
}

} // namespace medusa
