#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>

namespace medusa {

class AttributeRegion : public Region {
public:
    AttributeRegion(EditorState &state);

    void layout(const Rect &bounds) override;
    void draw(Graphics &gfx) override;
    bool handle_mouse_down(const MouseEvent &e) override;
    bool handle_mouse_move(const MouseEvent &e) override;
    bool handle_mouse_up(const MouseEvent &e) override;

private:
    bool effectively_renderable(int index) const;
    bool show_transform() const;
    bool show_lighting() const;
    bool show_optics() const;
    bool show_render() const;
    void init_panels();

    EditorState &state_;
    Rect header_rect_;
    Rect status_rect_;

    CollapsibleSectionWidget transform_header_;
    CollapsibleSectionWidget light_header_;
    CollapsibleSectionWidget optics_header_;
    CollapsibleSectionWidget render_header_;

    ScrubbableNumberWidget pos_x_, pos_y_, pos_z_;
    ScrubbableNumberWidget rot_x_, rot_y_, rot_z_;
    ScrubbableNumberWidget scale_x_, scale_y_, scale_z_;

    ScrubbableNumberWidget focal_len_;

    ScrubbableNumberWidget light_x_, light_y_, light_z_;
    ScrubbableNumberWidget light_energy_;
    ScrubbableNumberWidget max_samples_;
    ScrubbableNumberWidget firefly_clamp_;
};

} // namespace medusa
