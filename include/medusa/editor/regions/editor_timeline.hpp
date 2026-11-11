#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>
#include <vector>
#include <string>

namespace medusa {

class TimelineRegion : public Region {
public:
    TimelineRegion(EditorState &state);

    void layout(const Rect &bounds) override;
    void draw(Graphics &gfx) override;
    bool handle_mouse_down(const MouseEvent &e) override;
    bool handle_mouse_move(const MouseEvent &e) override;
    bool handle_mouse_up(const MouseEvent &e) override;
    bool handle_mouse_wheel(float delta, Point p) override;

private:
    void init_header();
    void refresh_channels();

    EditorState &state_;
    Rect header_rect_;
    Rect time_icon_rect_;
    Rect autokey_rect_;
    Rect transport_pill_rect_;
    Rect keying_rect_;
    Rect frame_box_rect_;
    Rect time_mode_rect_;
    Rect range_rect_;
    Rect start_frame_rect_;
    Rect end_frame_rect_;
    Rect fps_rect_;

    std::vector<ButtonWidget> menu_buttons_;
    std::vector<ButtonWidget> transport_buttons_;
    SequencerWidget sequencer_;
    uint64_t cached_scene_generation_ = ~uint64_t(0);
    size_t cached_camera_key_count_ = static_cast<size_t>(-1);
    size_t cached_node_key_count_ = static_cast<size_t>(-1);
    bool cached_show_keys_ = false;
    std::string cached_workspace_;
};

} // namespace medusa
