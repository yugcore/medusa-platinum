#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>
#include <medusa/ui/medusa_popup_menu.hpp>
#include <vector>
#include <string>
#include <functional>

namespace medusa {

class TopbarRegion : public Region {
public:
    TopbarRegion(EditorState &state);

    bool accepts_input(Point p) const override;
    Rect clip_bounds() const override;
    void layout(const Rect &bounds) override;
    void draw(Graphics &gfx) override;
    bool handle_mouse_down(const MouseEvent &e) override;
    bool handle_mouse_move(const MouseEvent &e) override;
    bool handle_mouse_up(const MouseEvent &e) override;

    bool has_open_popup() const { return popup_menu_.is_open(); }
    bool is_animating() const { return popup_menu_.is_animating(); }
    const Rect& get_popup_rect() const { return popup_menu_.get_bounds(); }
    Rect get_cutout_bounds() const { return popup_menu_.get_cutout_bounds(); }
    void draw_popup(Graphics &gfx);

private:
    void init_widgets();
    void populate_popup(int menu, bool instant = false);
    bool popup_contains(Point p) const;

    EditorState &state_;
    float menus_end_x_ = 0.0f;
    float workspace_divider_x_ = 0.0f;
    Point last_mouse_pos_;
    int active_menu_ = -1;
    DCCPopupMenu popup_menu_;
    std::vector<ButtonWidget> menu_buttons_;
    std::vector<ButtonWidget> mode_buttons_;
    ButtonWidget render_btn_;
};

} // namespace medusa
