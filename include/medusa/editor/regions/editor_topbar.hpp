#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>
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

private:
    struct PopupItem {
        std::string label;
        std::string icon;
        bool enabled = true;
        bool separator_before = false;
        std::function<void()> action;
    };

    void init_widgets();
    Rect popup_item_rect(size_t index) const;
    void populate_popup(int menu);
    void draw_popup(Graphics& gfx);
    bool popup_contains(Point p) const;

    EditorState &state_;
    Rect logo_rect_;
    float logo_separator_x_ = 0.0f;
    float render_separator_x_ = 0.0f;
    Rect popup_rect_;
    Point last_mouse_pos_;
    int active_menu_ = -1;
    bool popup_open_ = false;
    std::vector<PopupItem> popup_items_;
    std::vector<ButtonWidget> menu_buttons_;
    std::vector<ButtonWidget> mode_buttons_;
    ButtonWidget render_btn_;
};

} // namespace medusa
