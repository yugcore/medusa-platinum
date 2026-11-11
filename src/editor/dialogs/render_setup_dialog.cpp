#include <medusa/editor/dialogs/render_setup_dialog.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <filesystem>
#include <algorithm>
#include <cmath>

namespace medusa {

RenderSetupDialog::RenderSetupDialog(EditorState& state)
    : state_(state),
      samples_("Samples", static_cast<float>(state.max_samples_per_pixel), "spp", Theme::kTextSecondary,
               1.0f, 4096.0f, 8.0f, [this](float value) {
                   state_.max_samples_per_pixel = std::clamp(static_cast<int>(std::round(value)), 1, 4096);
               }),
      firefly_("Clamp", state.firefly_clamp, "", Theme::kTextSecondary, 0.0f, 100.0f, 1.0f,
               [this](float value) { state_.firefly_clamp = value; }) {}

Rect RenderSetupDialog::panel_bounds(const Rect& bounds) {
    layout(bounds);
    return panel_;
}

void RenderSetupDialog::draw(Graphics& gfx, const Rect& bounds) {
    layout(bounds);
    gfx.fill_rect(bounds, Color(0.0f, 0.0f, 0.0f, 0.62f));
    gfx.fill_rounded_rect(panel_, 4.0f, Theme::kPanelBg);
    gfx.draw_rounded_rect(panel_, 4.0f, Theme::kBorderBright, 1.0f);
    gfx.draw_text("Render Setup", title_rect_, Theme::kTextPrimary, 16.0f, true, false, true, Theme::kUiFontFamily);
    gfx.draw_text("Review output and quality before starting the sequence.", subtitle_rect_,
                  Theme::kTextMuted, Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);

    draw_section(gfx, engine_card_, "RENDERER");
    draw_button(gfx, renderer_rect_, "ZeGFX  ▾", true);
    if (renderer_menu_open_) {
        gfx.fill_rect(renderer_menu_rect_, Theme::kPanelHeaderBg);
        gfx.draw_rect(renderer_menu_rect_, Theme::kBorderBright, 1.0f);
        gfx.fill_rect(renderer_active_item_rect_, Theme::kSelectionBg);
        gfx.draw_text("ZeGFX  ·  Active", renderer_active_item_rect_, Theme::kTextPrimary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
    }

    draw_section(gfx, output_card_, "OUTPUT");
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
    draw_button(gfx, hd_rect_, "HD  1280 × 720", state_.render_output_width == 1280 && state_.render_output_height == 720);
    draw_button(gfx, full_hd_rect_, "Full HD  1920 × 1080", state_.render_output_width == 1920 && state_.render_output_height == 1080);
    draw_button(gfx, custom_resolution_rect_, "Custom", !is_preset_resolution());
    draw_number_field(gfx, width_rect_, "W", displayed_number(Field::Width, state_.render_output_width), field_ == Field::Width);
    draw_number_field(gfx, height_rect_, "H", displayed_number(Field::Height, state_.render_output_height), field_ == Field::Height);
    gfx.draw_text("Format", format_label_rect_, Theme::kTextSecondary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
    draw_button(gfx, png_rect_, "PNG", !state_.render_output_exr);
    draw_button(gfx, exr_rect_, "OpenEXR", state_.render_output_exr);
    gfx.draw_text("Output file", path_label_rect_, Theme::kTextSecondary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
    std::string outputPathText = "Choose a filename when rendering";
    if (!state_.render_output_path.empty()) {
        std::filesystem::path outputPath = std::filesystem::u8path(state_.render_output_path);
        outputPath.replace_extension(state_.render_output_exr ? ".exr" : ".png");
        outputPathText = outputPath.u8string();
    }
    gfx.draw_text(outputPathText,
                  path_text_rect_, Theme::kTextMuted, Theme::kSmallFont, false, false, true, Theme::kNumericFontFamily);
    draw_button(gfx, browse_rect_, "Browse…", false);

    draw_section(gfx, quality_card_, "QUALITY");
    samples_.set_value(static_cast<float>(state_.max_samples_per_pixel));
    samples_.set_bounds(samples_rect_);
    samples_.draw(gfx);
    firefly_.set_value(state_.firefly_clamp);
    firefly_.set_bounds(firefly_rect_);
    firefly_.draw(gfx);
    gfx.draw_text("ZeGFX default color · Denoising and bounce controls are not available in this renderer yet.",
                  quality_note_rect_, Theme::kTextMuted, 9.5f, false, false, true, Theme::kUiFontFamily);
    if (renderer_menu_open_) {
        gfx.fill_rect(renderer_menu_rect_, Theme::kPanelHeaderBg);
        gfx.draw_rect(renderer_menu_rect_, Theme::kBorderBright, 1.0f);
        gfx.fill_rect(renderer_active_item_rect_, Theme::kSelectionBg);
        gfx.draw_text("ZeGFX  ·  Active", renderer_active_item_rect_, Theme::kTextPrimary,
                      Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
    }
    gfx.draw_line(Point(panel_.x + 16.0f, footer_rect_.y - 8.0f), Point(panel_.right() - 16.0f, footer_rect_.y - 8.0f), Theme::kBorderSubtle, 1.0f);
    gfx.draw_text("ZeGFX renders on the CPU. Full HD and higher sample counts take longer.", footer_hint_rect_,
                  Theme::kTextMuted, 9.5f, false, false, true, Theme::kUiFontFamily);
    draw_button(gfx, cancel_rect_, "Cancel", false);
    draw_button(gfx, render_rect_, "Render Sequence", true);
}

bool RenderSetupDialog::handle_mouse_down(const MouseEvent& event, const Rect& bounds) {
    layout(bounds);
    if (event.button != MouseButton::Left || !panel_.contains(event.pos)) return true;
    if (renderer_rect_.contains(event.pos)) { renderer_menu_open_ = !renderer_menu_open_; return true; }
    if (renderer_menu_open_) {
        if (renderer_menu_rect_.contains(event.pos)) {
            renderer_menu_open_ = false;
            return true;
        }
        renderer_menu_open_ = false;
    }
    if (scene_range_rect_.contains(event.pos)) { finish_field(true); state_.render_frame_mode = EditorState::RenderFrameMode::SceneRange; return true; }
    if (current_frame_rect_.contains(event.pos)) { finish_field(true); state_.render_frame_mode = EditorState::RenderFrameMode::CurrentFrame; return true; }
    if (custom_range_rect_.contains(event.pos)) {
        const bool enteringCustom = state_.render_frame_mode != EditorState::RenderFrameMode::CustomRange;
        finish_field(true); state_.render_frame_mode = EditorState::RenderFrameMode::CustomRange;
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
    if (browse_rect_.contains(event.pos)) { finish_field(true); renderer_menu_open_ = false; state_.render_setup_open = false; state_.pending_command = "render_output_browse"; return true; }
    if (samples_.handle_mouse_down(event) || firefly_.handle_mouse_down(event)) return true;
    if (cancel_rect_.contains(event.pos)) { finish_field(false); renderer_menu_open_ = false; state_.render_setup_open = false; return true; }
    if (render_rect_.contains(event.pos)) { finish_field(true); renderer_menu_open_ = false; state_.render_setup_open = false; state_.pending_command = "render_sequence"; return true; }
    finish_field(true);
    return true;
}

bool RenderSetupDialog::handle_mouse_move(const MouseEvent& event, const Rect& bounds) {
    layout(bounds);
    const bool changed = samples_.handle_mouse_move(event) | firefly_.handle_mouse_move(event);
    const bool moved = event.pos.x != last_mouse_.x || event.pos.y != last_mouse_.y;
    last_mouse_ = event.pos;
    return changed || moved;
}

bool RenderSetupDialog::handle_mouse_up(const MouseEvent& event, const Rect& bounds) {
    layout(bounds); samples_.handle_mouse_up(event); firefly_.handle_mouse_up(event); return true;
}

bool RenderSetupDialog::handle_key_down(const KeyEvent& event) {
    const uint32_t key = event.key_code;
    if (key == 27) { finish_field(false); renderer_menu_open_ = false; state_.render_setup_open = false; return true; }
    if (field_ != Field::None) {
        if (key >= '0' && key <= '9') {
            if (replace_field_buffer_) field_buffer_.clear();
            if (field_buffer_.size() < 7) field_buffer_.push_back(static_cast<char>(key));
            replace_field_buffer_ = false; return true;
        }
        if (key == 8) {
            if (replace_field_buffer_) field_buffer_.clear(); else if (!field_buffer_.empty()) field_buffer_.pop_back();
            replace_field_buffer_ = false; return true;
        }
        if (key == 9) {
            const Field next = field_ == Field::Start ? Field::End : field_ == Field::End ? Field::Start :
                field_ == Field::Width ? Field::Height : Field::Width;
            finish_field(true); begin_field(next); return true;
        }
        if (key == 13) { finish_field(true); return true; }
        if (key == 0x25 || key == 0x27) {
            apply_field_value(current_field_value() + (key == 0x27 ? 1 : -1));
            field_buffer_ = std::to_string(current_field_value()); replace_field_buffer_ = false; return true;
        }
        return true;
    }
    if (key == 13) { renderer_menu_open_ = false; state_.render_setup_open = false; state_.pending_command = "render_sequence"; }
    else if (key == 9) begin_field(state_.render_frame_mode == EditorState::RenderFrameMode::CustomRange ? Field::Start : Field::Width);
    return true;
}

void RenderSetupDialog::layout(const Rect& bounds) {
    const float width = std::min(640.0f, std::max(440.0f, bounds.width - 28.0f));
    const float height = std::min(570.0f, std::max(500.0f, bounds.height - 28.0f));
    panel_ = Rect(bounds.x + (bounds.width - width) * 0.5f, bounds.y + (bounds.height - height) * 0.5f, width, height);
    const float left = panel_.x + 18.0f, inner = panel_.width - 36.0f;
    title_rect_ = Rect(left, panel_.y + 13.0f, inner, 24.0f); subtitle_rect_ = Rect(left, panel_.y + 35.0f, inner, 18.0f);
    engine_card_ = Rect(left, panel_.y + 60.0f, inner, 52.0f); output_card_ = Rect(left, panel_.y + 120.0f, inner, 238.0f);
    quality_card_ = Rect(left, panel_.y + 366.0f, inner, 104.0f); footer_rect_ = Rect(left, panel_.bottom() - 62.0f, inner, 46.0f);
    renderer_rect_ = Rect(left + 12.0f, engine_card_.y + 23.0f, 130.0f, 23.0f);
    renderer_menu_rect_ = Rect(renderer_rect_.x, renderer_rect_.bottom() + 2.0f, 150.0f, 29.0f);
    renderer_active_item_rect_ = Rect(renderer_menu_rect_.x, renderer_menu_rect_.y, 150.0f, 25.0f);
    const float ol = output_card_.x + 12.0f, oright = output_card_.right() - 12.0f, ow = oright - ol, gap = 5.0f;
    const float optionW = (ow - gap * 2.0f) / 3.0f;
    scene_range_rect_ = Rect(ol, output_card_.y + 23.0f, optionW, 23.0f);
    current_frame_rect_ = Rect(scene_range_rect_.right() + gap, scene_range_rect_.y, optionW, 23.0f);
    custom_range_rect_ = Rect(current_frame_rect_.right() + gap, scene_range_rect_.y, optionW, 23.0f);
    custom_start_rect_ = Rect(ol, output_card_.y + 51.0f, ow * 0.49f, 22.0f);
    custom_end_rect_ = Rect(custom_start_rect_.right() + 7.0f, custom_start_rect_.y, ow - custom_start_rect_.width - 7.0f, 22.0f);
    range_summary_rect_ = Rect(ol, output_card_.y + 51.0f, ow, 22.0f);
    resolution_label_rect_ = Rect(ol, output_card_.y + 78.0f, 68.0f, 22.0f);
    const float rx = resolution_label_rect_.right() + 4.0f, rg = 5.0f, rw = (oright - rx - 90.0f - rg * 2.0f) / 3.0f;
    hd_rect_ = Rect(rx, resolution_label_rect_.y, rw, 22.0f); full_hd_rect_ = Rect(hd_rect_.right() + rg, hd_rect_.y, rw, 22.0f);
    custom_resolution_rect_ = Rect(full_hd_rect_.right() + rg, hd_rect_.y, 78.0f, 22.0f);
    width_rect_ = Rect(ol, output_card_.y + 105.0f, ow * 0.49f, 22.0f);
    height_rect_ = Rect(width_rect_.right() + 7.0f, width_rect_.y, ow - width_rect_.width - 7.0f, 22.0f);
    format_label_rect_ = Rect(ol, output_card_.y + 132.0f, 52.0f, 22.0f);
    png_rect_ = Rect(format_label_rect_.right() + 4.0f, format_label_rect_.y, 82.0f, 22.0f);
    exr_rect_ = Rect(png_rect_.right() + 5.0f, png_rect_.y, 100.0f, 22.0f);
    path_label_rect_ = Rect(ol, output_card_.y + 160.0f, 80.0f, 20.0f);
    browse_rect_ = Rect(oright - 92.0f, output_card_.y + 182.0f, 92.0f, 24.0f);
    path_text_rect_ = Rect(ol, browse_rect_.y, browse_rect_.x - ol - 10.0f, 24.0f);
    samples_rect_ = Rect(quality_card_.x + 12.0f, quality_card_.y + 23.0f, (inner - 31.0f) * 0.5f, 24.0f);
    firefly_rect_ = Rect(samples_rect_.right() + 7.0f, samples_rect_.y, samples_rect_.width, 24.0f);
    quality_note_rect_ = Rect(quality_card_.x + 12.0f, quality_card_.y + 53.0f, inner - 24.0f, 34.0f);
    footer_hint_rect_ = Rect(left + 2.0f, footer_rect_.y, inner - 220.0f, 30.0f);
    cancel_rect_ = Rect(panel_.right() - 200.0f, footer_rect_.y + 3.0f, 82.0f, 30.0f);
    render_rect_ = Rect(cancel_rect_.right() + 8.0f, cancel_rect_.y, 110.0f, 30.0f);
}

void RenderSetupDialog::draw_section(Graphics& gfx, const Rect& rect, const char* title) {
    gfx.fill_rounded_rect(rect, 3.0f, Theme::kPanelHeaderBg);
    gfx.draw_rounded_rect(rect, 3.0f, Theme::kBorderSubtle, 1.0f);
    gfx.draw_text(title, Rect(rect.x + 12.0f, rect.y + 4.0f, rect.width - 24.0f, 15.0f),
                  Theme::kTextMuted, 9.0f, true, false, true, Theme::kUiFontFamily);
}

void RenderSetupDialog::draw_button(Graphics& gfx, const Rect& rect, const std::string& label, bool active) {
    const bool hovered = rect.contains(last_mouse_);
    gfx.fill_rounded_rect(rect, 2.0f, active ? Theme::kSelectionBg : (hovered ? Theme::kControlHover : Theme::kControlBg));
    gfx.draw_rounded_rect(rect, 2.0f, active ? Theme::kAccentBorder : Theme::kBorderSubtle, 1.0f);
    gfx.draw_text(label, Rect(rect.x + 3.0f, rect.y, rect.width - 6.0f, rect.height),
                  active ? Theme::kAccentText : Theme::kTextSecondary,
                  Theme::kSmallFont, active, true, true, Theme::kUiFontFamily);
}

void RenderSetupDialog::draw_number_field(Graphics& gfx, const Rect& rect, const char* label, const std::string& value, bool focused) {
    gfx.fill_rect(rect, focused ? Theme::kSelectionBg : Theme::kControlBg);
    gfx.draw_rect(rect, focused ? Theme::kAccentBorder : Theme::kBorderSubtle, 1.0f);
    const float labelW = label[1] == '\0' ? 25.0f : 44.0f;
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
