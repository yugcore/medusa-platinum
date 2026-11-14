#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>
#include <medusa/ui/dcc_transform_triplet.hpp>

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
    bool show_material() const;
    void init_panels();

    EditorState &state_;
    Rect header_rect_;
    Rect status_rect_;

    CollapsibleSectionWidget transform_header_;
    CollapsibleSectionWidget light_header_;
    CollapsibleSectionWidget optics_header_;
    CollapsibleSectionWidget render_header_;
    CollapsibleSectionWidget material_header_;

    DCCTransformTriplet location_triplet_;
    DCCTransformTriplet rotation_triplet_;
    DCCTransformTriplet scale_triplet_;

    ScrubbableNumberWidget focal_len_;

    DCCTransformTriplet light_pos_triplet_;
    ScrubbableNumberWidget light_energy_;
    ScrubbableNumberWidget max_samples_;
    ScrubbableNumberWidget firefly_clamp_;

    // OpenPBR Layered Material Inspector widgets
    Rect mat_color_swatch_rect_;
    ScrubbableNumberWidget mat_col_r_, mat_col_g_, mat_col_b_;
    ScrubbableNumberWidget mat_roughness_;
    ScrubbableNumberWidget mat_metallic_;
    ScrubbableNumberWidget mat_transmission_;
    ScrubbableNumberWidget mat_ior_;
    ScrubbableNumberWidget mat_coat_weight_;
    ScrubbableNumberWidget mat_coat_roughness_;
    ScrubbableNumberWidget mat_sheen_weight_;
    ScrubbableNumberWidget mat_sheen_roughness_;
    ScrubbableNumberWidget mat_emission_;
};

} // namespace medusa
