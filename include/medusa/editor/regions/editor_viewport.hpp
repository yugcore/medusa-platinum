#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>
#include <medusa/ui/medusa_popup_menu.hpp>
#include <medusa/editor/gizmo/gizmo_types.hpp>
#include <medusa/editor/gizmo/transform_gizmo.hpp>
#include <medusa/editor/gizmo/orientation_gizmo.hpp>
#include <vector>
#include <string>
#include <functional>
#include <cmath>

namespace medusa {

class ViewportRegion : public Region {
public:
    explicit ViewportRegion(EditorState &state);

    bool accepts_input(Point p) const override;
    Rect clip_bounds() const override;
    void layout(const Rect &bounds) override;
    void draw(Graphics &gfx) override;
    bool handle_mouse_down(const MouseEvent &e) override;
    bool handle_mouse_move(const MouseEvent &e) override;
    bool handle_mouse_up(const MouseEvent &e) override;
    bool handle_mouse_wheel(float delta, Point p) override;
    bool handle_raw_mouse_delta(int deltaX, int deltaY) override;

    bool has_open_popup() const { return popup_menu_.is_open(); }
    bool is_animating() const { return popup_menu_.is_animating(); }
    const Rect& get_popup_rect() const { return popup_menu_.get_bounds(); }
    Rect get_cutout_bounds() const { return popup_menu_.get_cutout_bounds(); }
    void draw_popup(Graphics &gfx);

private:
    using Vec3 = GizmoVec3;
    using ViewBasis = GizmoViewBasis;

    enum class NavigationDrag { None, Orbit, Pan, Dolly, FreeLook };

    ViewBasis view_basis() const;
    bool project(Vec3 world, Point& screen, float* depth = nullptr) const;
    bool project_segment(Vec3 a, Vec3 b, Point& screenA, Point& screenB) const;
    void draw_camera_motion_trail(Graphics& gfx) const;
    void draw_world_grid(Graphics& gfx) const;
    void draw_scene_geometry(Graphics& gfx) const;
    void draw_box_node(Graphics& gfx, const zegfx::ProductionSceneNode& node, bool selected) const;
    void draw_empty_scene_label(Graphics& gfx) const;
    static const char* light_icon(const zegfx::ProductionSceneNode& node);
    void draw_light_markers(Graphics& gfx) const;
    int hit_test_light_marker(Point point) const;
    bool make_pick_ray(Point point, float origin[3], float direction[3]) const;
    void initialize_view_navigation();
    void update_view_position_from_orbit();
    void apply_free_look_delta(float deltaX, float deltaY);
    void update_view_navigation(Point point);
    void init_menus();
    void populate_popup(int menu, bool instant = false);

    EditorState &state_;
    Rect header_rect_;
    Rect canvas_rect_;
    Rect camera_rect_;
    Rect denoise_rect_;
    Rect aov_rect_;
    Rect shading_rect_;
    Rect engine_badge_rect_;
    Point last_mouse_pos_;
    Point drag_start_;
    Point last_drag_;
    NavigationDrag navigation_drag_ = NavigationDrag::None;

    ViewportTransformGizmo transform_gizmo_;
    ViewportOrientationGizmo orientation_gizmo_;

    int active_menu_ = -1;
    DCCPopupMenu popup_menu_;
    std::vector<ButtonWidget> menu_buttons_;
};

} // namespace medusa
