#pragma once
#include <medusa/core/medusa_types.hpp>
#include <medusa/graphics/medusa_graphics.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>
#include <string>
#include <utility>

namespace medusa {

class RenderSetupDialog {
public:
    explicit RenderSetupDialog(EditorState& state);

    Rect panel_bounds(const Rect& bounds);
    void draw(Graphics& gfx, const Rect& bounds);
    bool handle_mouse_down(const MouseEvent& event, const Rect& bounds);
    bool handle_mouse_move(const MouseEvent& event, const Rect& bounds);
    bool handle_mouse_up(const MouseEvent& event, const Rect& bounds);
    bool handle_key_down(const KeyEvent& event);

private:
    enum class Field { None, Start, End, Width, Height };

    void layout(const Rect& bounds);
    void draw_render_preview(Graphics& gfx, const Rect& previewRect);
    void draw_preview_geometry(Graphics& gfx, const Rect& frameRect) const;
    void draw_section(Graphics& gfx, const Rect& rect, const char* title);
    void draw_button(Graphics& gfx, const Rect& rect, const std::string& label, bool active, bool primary = false);
    void draw_number_field(Graphics& gfx, const Rect& rect, const char* label, const std::string& value, bool focused);
    std::pair<int, int> selected_range() const;
    bool is_preset_resolution() const;
    void set_resolution(int width, int height);
    int current_field_value() const;
    std::string displayed_number(Field field, int value) const;
    void begin_field(Field field);
    void apply_field_value(int value);
    void finish_field(bool accept);

    EditorState& state_;
    ScrubbableNumberWidget samples_, firefly_;
    Rect panel_, header_rect_, close_btn_rect_;
    Rect preview_panel_, settings_panel_;
    Rect title_rect_, subtitle_rect_, engine_card_, renderer_rect_;
    Rect renderer_menu_rect_, renderer_active_item_rect_;
    Rect output_card_, scene_range_rect_, current_frame_rect_, custom_range_rect_, custom_start_rect_, custom_end_rect_, range_summary_rect_;
    Rect resolution_label_rect_, hd_rect_, full_hd_rect_, custom_resolution_rect_, width_rect_, height_rect_, format_label_rect_, png_rect_, exr_rect_;
    Rect path_label_rect_, path_text_rect_, browse_rect_, quality_card_, samples_rect_, firefly_rect_, quality_note_rect_;
    Rect footer_rect_, footer_hint_rect_, cancel_rect_, render_rect_;
    Point last_mouse_;
    Field field_ = Field::None;
    std::string field_buffer_;
    bool replace_field_buffer_ = true;
    bool renderer_menu_open_ = false;
};

} // namespace medusa
