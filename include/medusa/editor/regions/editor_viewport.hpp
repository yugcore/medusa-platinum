#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>
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

private:
    struct Vec3 {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
        Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
        Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    };

    static float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static float length(Vec3 v) { return std::sqrt(dot(v, v)); }
    static Vec3 normalized(Vec3 v) {
        const float l = length(v);
        return l > 1.0e-6f ? v * (1.0f / l) : Vec3{0.0f, 0.0f, -1.0f};
    }

    struct PopupItem {
        std::string label;
        std::string icon;
        bool enabled = true;
        bool separator_before = false;
        std::function<void()> action;
    };

    enum class NavigationDrag { None, Orbit, Pan, Dolly, FreeLook };

    struct ViewBasis {
        Vec3 position;
        Vec3 forward;
        Vec3 right;
        Vec3 up;
        float verticalFov = 0.785398f;
    };

    struct GizmoDrag {
        bool active = false;
        int handle = -1;
        int node = -1;
        Vec3 pivot;
        float position[3]{};
        float rotation[3]{};
        float scale[3] = {1.0f, 1.0f, 1.0f};
    };

    ViewBasis view_basis() const;
    bool project(Vec3 world, Point& screen, float* depth = nullptr) const;
    bool project_segment(Vec3 a, Vec3 b, Point& screenA, Point& screenB) const;
    void draw_camera_motion_trail(Graphics& gfx) const;
    void draw_world_grid(Graphics& gfx) const;
    void draw_starter_cube(Graphics& gfx) const;
    void draw_empty_scene_label(Graphics& gfx) const;
    Vec3 selected_pivot() const;
    bool gizmo_available() const;
    static float point_segment_distance(Point p, Point a, Point b);
    Vec3 world_axis(int axis) const;
    void draw_axis(Graphics& gfx, Vec3 pivot, int axis, float worldLength) const;
    void draw_transform_gizmo(Graphics& gfx) const;
    void draw_selection_outline(Graphics& gfx) const;
    static const char* light_icon(const zegfx::ProductionSceneNode& node);
    void draw_light_markers(Graphics& gfx) const;
    int hit_test_light_marker(Point point) const;
    void draw_orientation_gizmo(Graphics& gfx);
    int hit_test_gizmo(Point point) const;
    bool make_pick_ray(Point point, float origin[3], float direction[3]) const;
    void initialize_view_navigation();
    void update_view_position_from_orbit();
    void apply_free_look_delta(float deltaX, float deltaY);
    void update_view_navigation(Point point);
    bool begin_gizmo_drag(int handle, Point point);
    void update_gizmo_drag(Point point);
    void snap_view_to_axis(Point point);
    void init_menus();
    Rect popup_item_rect(size_t index) const;
    void populate_popup(int menu);
    void draw_popup(Graphics &gfx);

    EditorState &state_;
    Rect header_rect_;
    Rect canvas_rect_;
    Rect camera_rect_;
    Rect shading_rect_;
    Rect orientation_rect_;
    Rect popup_rect_;
    Point last_mouse_pos_;
    Point drag_start_;
    Point last_drag_;
    NavigationDrag navigation_drag_ = NavigationDrag::None;
    GizmoDrag gizmo_drag_;
    std::vector<std::pair<Point, Vec3>> orientation_endpoints_;
    int active_menu_ = -1;
    bool popup_open_ = false;
    std::vector<PopupItem> popup_items_;
    std::vector<ButtonWidget> menu_buttons_;
};

} // namespace medusa
