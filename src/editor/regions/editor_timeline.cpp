#include <medusa/editor/regions/editor_timeline.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <medusa/ui/medusa_icons.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace medusa {

TimelineRegion::TimelineRegion(EditorState &state) : state_(state),
  sequencer_(state.current_frame, state.total_frames, [this](int f) {
      state_.current_frame = f;
      state_.evaluate_keyframes();
  }) {
    init_header();
}

void TimelineRegion::layout(const Rect &bounds) {
    bounds_ = bounds;
    header_rect_ = Rect(bounds.x, bounds.y, bounds.width, Theme::kTimelineHeaderHeight);

    // A. Left controls: (clock) ▾ + Menus (View, Marker, Playback ▾)
    float lx = bounds.x + 8.0f;
    time_icon_rect_ = Rect(lx, bounds.y + 6.0f, 16.0f, 16.0f);
    lx += 28.0f;

    for (auto &btn : menu_buttons_) {
        float mw = static_cast<float>(btn.get_label().length()) * 6.8f + 12.0f;
        btn.set_bounds(Rect(lx, bounds.y + 3.0f, mw, 22.0f));
        lx += mw + 6.0f;
    }

    // B. Center Transport Controls (Centered on width)
    float center_x = bounds.x + bounds.width * 0.5f;
    float btn_sz = 24.0f;
    float transport_pill_w = 6.0f * btn_sz;
    constexpr float group_gap = 8.0f;
    float total_center_w = 24.0f + group_gap + transport_pill_w + group_gap + 28.0f;
    float cx = center_x - total_center_w * 0.5f;

    autokey_rect_ = Rect(cx, bounds.y + 2.0f, 24.0f, 24.0f);
    cx += 24.0f + group_gap;

    transport_pill_rect_ = Rect(cx, bounds.y + 2.0f, transport_pill_w, btn_sz);
    float bx = cx;
    for (auto &btn : transport_buttons_) {
        btn.set_bounds(Rect(bx, bounds.y + 2.0f, btn_sz, btn_sz));
        bx += btn_sz;
    }
    cx += transport_pill_w + group_gap;

    keying_rect_ = Rect(cx, bounds.y + 2.0f, 28.0f, 24.0f);

    // C. Right Controls: reserve a fixed, fully in-bounds slot.
    float rx = bounds.right() - 314.0f;
    frame_box_rect_ = Rect(rx, bounds.y + 2.0f, 48.0f, 24.0f);
    rx += 52.0f;

    time_mode_rect_ = Rect(rx, bounds.y + 2.0f, 24.0f, 24.0f);
    rx += 28.0f;

    range_rect_ = Rect(rx, bounds.y + 2.0f, 170.0f, 24.0f);
    start_frame_rect_ = Rect(range_rect_.x + 38.0f, range_rect_.y, 40.0f, range_rect_.height);
    end_frame_rect_ = Rect(range_rect_.x + 122.0f, range_rect_.y, 44.0f, range_rect_.height);
    rx += 176.0f;

    fps_rect_ = Rect(rx, bounds.y + 2.0f, 48.0f, 24.0f);

    // Full-width Timeline Canvas below header seamlessly
    sequencer_.set_bounds(Rect(bounds.x, bounds.y + Theme::kTimelineHeaderHeight, bounds.width, bounds.height - Theme::kTimelineHeaderHeight));
}

void TimelineRegion::draw(Graphics &gfx) {
    gfx.fill_rect(bounds_, Theme::kPanelBg);
    gfx.draw_line(Point(bounds_.x, bounds_.y), Point(bounds_.right(), bounds_.y), Theme::kDockDivider, 1.0f);

    // 1. Header Bar background & bottom dividing line
    gfx.fill_rect(header_rect_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(header_rect_.x, header_rect_.bottom()), Point(header_rect_.right(), header_rect_.bottom()), Theme::kDockDivider, 1.0f);

    // Editor Icon (Timeline clock face + small ▾)
    IconRenderer::draw_icon(gfx, "space_timeline", time_icon_rect_, Theme::kTextSecondary);
    gfx.draw_text("▾", Rect(time_icon_rect_.right() + 1.0f, header_rect_.y, 7.0f, header_rect_.height),
                  Theme::kTextMuted, 8.0f, false, false, true);

    // Menus
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        auto &btn = menu_buttons_[i];
        btn.set_active((btn.get_label() == "View" && state_.show_timeline_keyframes) ||
                       (i == 2 && popup_menu_.is_open()));
        btn.draw(gfx);
    }

    // 2. Center Group: Autokey + Grouped Transport Pill + Keying
    // Autokey button
    gfx.fill_rounded_rect(autokey_rect_, 3.0f, state_.is_autokey ? Theme::kSelectionBg : Theme::kControlBg);
    gfx.draw_rounded_rect(autokey_rect_, 3.0f, state_.is_autokey ? Theme::kAccentBorder : Theme::kBorderBright, 1.0f);
    Color autokey_col = state_.is_autokey ? Theme::kAccent : Theme::kTextMuted;
    IconRenderer::draw_icon(gfx, "anim_autokey", Rect(autokey_rect_.x + 4.0f, autokey_rect_.y + 4.0f, 12.0f, 12.0f), autokey_col);

    // Transport Pill Container (Rounded Capsule)
    gfx.fill_rounded_rect(transport_pill_rect_, 4.0f, Theme::kControlBg);
    gfx.draw_rounded_rect(transport_pill_rect_, 4.0f, Theme::kBorderBright, 1.0f);
    for (size_t i = 0; i < transport_buttons_.size(); ++i) {
        if (i == 2) transport_buttons_[i].set_active(state_.is_playing && state_.playback_reverse);
        else if (i == 3) transport_buttons_[i].set_active(state_.is_playing && !state_.playback_reverse);
        transport_buttons_[i].draw(gfx);
    }
    // Hairline separators between transport buttons
    for (size_t i = 1; i < transport_buttons_.size(); ++i) {
        float sep_x = transport_buttons_[i].get_bounds().x;
        gfx.draw_line(Point(sep_x, transport_pill_rect_.y + 3.0f),
                      Point(sep_x, transport_pill_rect_.bottom() - 3.0f),
                      Theme::kBorderBright, 0.75f);
    }

    // Keying dropdown pill
    gfx.fill_rounded_rect(keying_rect_, 3.0f, Theme::kControlBg);
    gfx.draw_rounded_rect(keying_rect_, 3.0f, Theme::kBorderBright, 1.0f);
    IconRenderer::draw_icon(gfx, "anim_keying", Rect(keying_rect_.x + 3.0f, keying_rect_.y + 4.0f, 11.0f, 11.0f), Theme::kTextSecondary);
    gfx.draw_text("▾", Rect(keying_rect_.right() - 8.0f, keying_rect_.y, 7.0f, keying_rect_.height),
                  Theme::kTextMuted, 7.5f, false, false, true);

    // 3. Right Group: [ 1 ]  (clock)  [ Start 1 | End 250 ]  (sync)
    // Current Frame Pill
    gfx.fill_rounded_rect(frame_box_rect_, 3.0f, Theme::kControlBg);
    gfx.draw_rounded_rect(frame_box_rect_, 3.0f, Theme::kAccentBorder, 1.0f);
    gfx.draw_text(std::to_string(state_.current_frame), frame_box_rect_, Theme::kTextPrimary, Theme::kUiFont, true, true, true, Theme::kNumericFontFamily);

    // Clock Mode Toggle Button
    gfx.draw_text("FR", time_mode_rect_, Theme::kTextMuted, Theme::kSmallFont,
                  true, true, true, Theme::kNumericFontFamily);

    // Grouped Start / End Capsule Pill
    gfx.fill_rounded_rect(range_rect_, 3.0f, Theme::kControlBg);
    gfx.draw_rounded_rect(range_rect_, 3.0f, Theme::kBorderBright, 1.0f);

    gfx.draw_text("Start", Rect(range_rect_.x + 7.0f, range_rect_.y, 30.0f, range_rect_.height), Theme::kTextSecondary, Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);
    const bool editingStart = state_.timeline_range_edit_field == EditorState::TimelineRangeField::Start;
    const bool editingEnd = state_.timeline_range_edit_field == EditorState::TimelineRangeField::End;
    if (editingStart) {
        gfx.fill_rounded_rect(start_frame_rect_, 2.0f, Theme::kSelectionBg);
        gfx.draw_rounded_rect(start_frame_rect_, 2.0f, Theme::kAccentBorder, 1.0f);
    }
    gfx.draw_text(editingStart ? state_.timeline_range_edit_buffer : std::to_string(state_.start_frame),
                  start_frame_rect_, Theme::kTextPrimary, Theme::kSmallFont, true, true, true,
                  Theme::kNumericFontFamily);
    float rx = range_rect_.x + 82.0f;

    // Subtle capsule divider
    gfx.draw_line(Point(rx, range_rect_.y + 4.0f), Point(rx, range_rect_.bottom() - 4.0f), Theme::kBorderBright, 1.0f);
    rx += 10.0f;

    gfx.draw_text("End", Rect(rx, range_rect_.y, 28.0f, range_rect_.height), Theme::kTextSecondary, Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);
    if (editingEnd) {
        gfx.fill_rounded_rect(end_frame_rect_, 2.0f, Theme::kSelectionBg);
        gfx.draw_rounded_rect(end_frame_rect_, 2.0f, Theme::kAccentBorder, 1.0f);
    }
    gfx.draw_text(editingEnd ? state_.timeline_range_edit_buffer : std::to_string(state_.end_frame),
                  end_frame_rect_, Theme::kTextPrimary, Theme::kSmallFont, true, true, true,
                  Theme::kNumericFontFamily);

    // Playback rate is a real control; click to cycle through common rates.
    gfx.fill_rounded_rect(fps_rect_, 3.0f, Theme::kControlBg);
    gfx.draw_rounded_rect(fps_rect_, 3.0f, Theme::kBorderBright, 1.0f);
    gfx.draw_text(std::to_string(state_.playback_fps) + " fps", fps_rect_,
                  Theme::kTextSecondary, Theme::kSmallFont, false, true, true,
                  Theme::kNumericFontFamily);

    // 4. Timeline Canvas
    sequencer_.set_frame_range(state_.start_frame, state_.end_frame, state_.total_frames);
    sequencer_.set_frame(state_.current_frame);
    refresh_channels();
    sequencer_.draw(gfx);
}

void TimelineRegion::draw_popup(Graphics &gfx) {
    popup_menu_.draw(gfx);
}

bool TimelineRegion::handle_mouse_down(const MouseEvent &e) {
    if (popup_menu_.is_open()) {
        if (menu_buttons_.size() > 2 && menu_buttons_[2].get_bounds().contains(e.pos)) {
            popup_menu_.close();
            return true;
        }
        if (popup_menu_.contains(e.pos)) {
            return popup_menu_.handle_mouse_down(e);
        }
        popup_menu_.close();
        if (!bounds_.contains(e.pos)) return false;
    }

    if (state_.timeline_range_edit_field != EditorState::TimelineRangeField::None &&
        !(e.button == MouseButton::Left && range_rect_.contains(e.pos)))
        state_.finish_timeline_range_edit(true);
    for (auto &btn : menu_buttons_) {
        if (btn.handle_mouse_down(e)) return true;
    }
    for (auto &btn : transport_buttons_) {
        if (btn.handle_mouse_down(e)) return true;
    }
    if (autokey_rect_.contains(e.pos) && e.button == MouseButton::Left) {
        state_.is_autokey = !state_.is_autokey;
        return true;
    }
    if (keying_rect_.contains(e.pos) && e.button == MouseButton::Left) {
        state_.insert_keyframe();
        return true;
    }
    if (e.button == MouseButton::Left && range_rect_.contains(e.pos)) {
        state_.finish_timeline_range_edit(true);
        state_.timeline_range_edit_field = e.pos.x < range_rect_.x + range_rect_.width * 0.5f
            ? EditorState::TimelineRangeField::Start : EditorState::TimelineRangeField::End;
        state_.timeline_range_edit_buffer = std::to_string(
            state_.timeline_range_edit_field == EditorState::TimelineRangeField::Start
                ? state_.start_frame : state_.end_frame);
        state_.timeline_range_edit_replace = true;
        return true;
    }
    if (state_.timeline_range_edit_field != EditorState::TimelineRangeField::None)
        state_.finish_timeline_range_edit(true);
    if (sequencer_.handle_mouse_down(e)) return true;
    if (e.button == MouseButton::Left && fps_rect_.contains(e.pos)) {
        state_.playback_fps = state_.playback_fps == 24 ? 30 : state_.playback_fps == 30 ? 60 : 24;
        return true;
    }
    return false;
}

bool TimelineRegion::handle_mouse_move(const MouseEvent &e) {
    bool changed = false;
    for (auto &btn : menu_buttons_) {
        if (btn.handle_mouse_move(e)) changed = true;
    }
    for (auto &btn : transport_buttons_) {
        if (btn.handle_mouse_move(e)) changed = true;
    }
    if (sequencer_.handle_mouse_move(e)) changed = true;
    if (popup_menu_.is_open()) {
        if (popup_menu_.handle_mouse_move(e)) changed = true;
    }
    return changed;
}

bool TimelineRegion::handle_mouse_up(const MouseEvent &e) {
    for (auto &btn : menu_buttons_) {
        if (btn.handle_mouse_up(e)) return true;
    }
    for (auto &btn : transport_buttons_) {
        if (btn.handle_mouse_up(e)) return true;
    }
    if (sequencer_.handle_mouse_up(e)) return true;
    return false;
}

bool TimelineRegion::handle_mouse_wheel(float delta, Point p) {
    if (sequencer_.handle_mouse_wheel(delta, p)) return true;
    if (!frame_box_rect_.contains(p) || std::abs(delta) < 1.0e-4f) return false;
    const int step = delta > 0.0f ? 1 : -1;
    state_.current_frame = std::clamp(state_.current_frame + step, state_.start_frame, state_.end_frame);
    state_.evaluate_keyframes();
    return true;
}

void TimelineRegion::refresh_channels() {
    if (cached_scene_generation_ == state_.scene_generation &&
        cached_camera_key_count_ == state_.keyframes.size() &&
        cached_node_key_count_ == state_.node_keyframes.size() &&
        cached_show_keys_ == state_.show_timeline_keyframes &&
        cached_workspace_ == state_.active_workspace) return;

    std::vector<DopeSheetChannel> channels;
    DopeSheetChannel shot;
    shot.name = "Shot";
    shot.icon = "channel_summary";
    shot.indent = 0;
    channels.push_back(std::move(shot));

    DopeSheetChannel camera_light;
    camera_light.name = state_.active_workspace == "Shot" ? "Camera" : "Camera + Light";
    camera_light.icon = "camera_actor";
    camera_light.indent = 1;
    if (state_.show_timeline_keyframes) {
        camera_light.keyframes.reserve(state_.keyframes.size());
        for (const auto& key : state_.keyframes) camera_light.keyframes.push_back(key.frame);
    }
    std::sort(camera_light.keyframes.begin(), camera_light.keyframes.end());
    camera_light.keyframes.erase(std::unique(camera_light.keyframes.begin(), camera_light.keyframes.end()),
                                 camera_light.keyframes.end());
    channels.push_back(std::move(camera_light));

    std::vector<std::pair<int, int>> keyedFrames;
    if (state_.show_timeline_keyframes && state_.active_workspace != "Shot") {
        keyedFrames.reserve(state_.node_keyframes.size());
        for (const auto& key : state_.node_keyframes) {
            if (key.node_index >= 0 &&
                static_cast<size_t>(key.node_index) < state_.scene_nodes.size())
                keyedFrames.emplace_back(key.node_index, key.frame);
        }
    }
    std::sort(keyedFrames.begin(), keyedFrames.end());
    for (size_t cursor = 0; cursor < keyedFrames.size();) {
        const int nodeIndex = keyedFrames[cursor].first;
        const auto& node = state_.scene_nodes[nodeIndex];
        DopeSheetChannel channel;
        channel.name = node.name;
        switch (node.type) {
            case zegfx::ProductionNodeType::Mesh: channel.icon = "outliner_mesh"; break;
            case zegfx::ProductionNodeType::Camera: channel.icon = "camera_actor"; break;
            case zegfx::ProductionNodeType::Light: channel.icon = "light_directional"; break;
            case zegfx::ProductionNodeType::Group: default: channel.icon = "outliner_collection"; break;
        }
        channel.indent = 1;
        int lastFrame = std::numeric_limits<int>::min();
        while (cursor < keyedFrames.size() && keyedFrames[cursor].first == nodeIndex) {
            const int frame = keyedFrames[cursor].second;
            if (frame != lastFrame) channel.keyframes.push_back(frame);
            lastFrame = frame;
            ++cursor;
        }
        channels.push_back(std::move(channel));
    }
    sequencer_.set_channels(std::move(channels));
    cached_scene_generation_ = state_.scene_generation;
    cached_camera_key_count_ = state_.keyframes.size();
    cached_node_key_count_ = state_.node_keyframes.size();
    cached_show_keys_ = state_.show_timeline_keyframes;
    cached_workspace_ = state_.active_workspace;
}

void TimelineRegion::open_playback_menu() {
    popup_menu_.clear();
    popup_menu_.add_check(state_.is_playing ? "Pause" : "Play", "anim_play", state_.is_playing, [this]() {
        state_.is_playing = !state_.is_playing;
    }, "Space");
    popup_menu_.add_check("Play Reverse", "anim_play_reverse", state_.is_playing && state_.playback_reverse, [this]() {
        if (state_.is_playing && state_.playback_reverse) state_.is_playing = false;
        else { state_.playback_reverse = true; state_.is_playing = true; }
    });
    popup_menu_.add_separator();
    popup_menu_.add_item("Jump to Start Frame", "anim_jump_start", "Shift+Left", [this]() {
        state_.current_frame = state_.start_frame;
        state_.evaluate_keyframes();
    });
    popup_menu_.add_item("Jump to End Frame", "anim_jump_end", "Shift+Right", [this]() {
        state_.current_frame = state_.end_frame;
        state_.evaluate_keyframes();
    });
    popup_menu_.add_separator("PLAYBACK RATE");
    popup_menu_.add_check("24 FPS (Cinema Standard)", "", state_.playback_fps == 24, [this]() {
        state_.playback_fps = 24;
    });
    popup_menu_.add_check("30 FPS (Video Standard)", "", state_.playback_fps == 30, [this]() {
        state_.playback_fps = 30;
    });
    popup_menu_.add_check("60 FPS (High Frame Rate)", "", state_.playback_fps == 60, [this]() {
        state_.playback_fps = 60;
    });
    popup_menu_.add_separator("KEYFRAMING");
    popup_menu_.add_check("Auto-Keyframing", "anim_autokey", state_.is_autokey, [this]() {
        state_.is_autokey = !state_.is_autokey;
    });

    if (menu_buttons_.size() > 2) {
        Rect anchor = menu_buttons_[2].get_bounds();
        popup_menu_.open(anchor, Rect(0.0f, 0.0f, 4000.0f, 3000.0f), 220.0f, 320.0f);
    }
}

void TimelineRegion::init_header() {
    menu_buttons_.emplace_back("View", "", [this]() {
        state_.show_timeline_keyframes = !state_.show_timeline_keyframes;
    });
    menu_buttons_.emplace_back("Marker", "", [this]() { state_.insert_keyframe(); });
    menu_buttons_.emplace_back("Playback ▾", "", [this]() {
        if (popup_menu_.is_open()) popup_menu_.close();
        else open_playback_menu();
    });
    for (auto &btn : menu_buttons_) {
        btn.set_flat(true);
    }
    menu_buttons_[0].set_tooltip("Show or hide keyframe markers");
    menu_buttons_[1].set_tooltip("Insert a keyframe at the current frame");
    menu_buttons_[2].set_tooltip("Playback options and transport controls");

    transport_buttons_.emplace_back("", "anim_jump_start", [this]() { state_.current_frame = state_.start_frame; state_.evaluate_keyframes(); });
    transport_buttons_.emplace_back("", "anim_step_back", [this]() { state_.current_frame = std::max(state_.start_frame, state_.current_frame - 1); state_.evaluate_keyframes(); });
    transport_buttons_.emplace_back("", "anim_play_reverse", [this]() {
        if (state_.is_playing && state_.playback_reverse) state_.is_playing = false;
        else { state_.playback_reverse = true; state_.is_playing = true; }
    });
    transport_buttons_.emplace_back("", "anim_play", [this]() {
        if (state_.is_playing && !state_.playback_reverse) state_.is_playing = false;
        else { state_.playback_reverse = false; state_.is_playing = true; }
    });
    transport_buttons_.emplace_back("", "anim_step_forward", [this]() { state_.current_frame = std::min(state_.end_frame, state_.current_frame + 1); state_.evaluate_keyframes(); });
    transport_buttons_.emplace_back("", "anim_jump_end", [this]() { state_.current_frame = state_.end_frame; state_.evaluate_keyframes(); });
    for (auto &btn : transport_buttons_) {
        btn.set_flat(true);
    }
}

} // namespace medusa
