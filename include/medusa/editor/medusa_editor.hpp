#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/ui/medusa_widgets.hpp>
#include <medusa/editor/editor_state.hpp>
#include <memory>
#include <string>
#include <vector>

namespace medusa {

class AssetLibraryDrawer;
class RenderSetupDialog;

class MedusaEditor : public Screen {
public:
    MedusaEditor();
    ~MedusaEditor();

    void rebuild_layout(float width, float height);
    void set_layout_settings(float toolshelfWidth, float rightDeckWidth,
                             float outlinerRatio, float timelineHeight);
    void reset_layout();
    std::string take_pending_command();
    void advance_animation_frame();
    bool tick_asset_drawer_animation();
    bool asset_drawer_animating() const;
    void set_asset_library_root(const std::string& path);
    void initialize_render_controls(const zegfx::ProductionScene& scene,
                                    const std::vector<EditorState::SceneNodeBounds>& nodeBounds = {});
    bool handle_key_down(const KeyEvent& event);
    Rect get_render_viewport_bounds() const;
    void set_scene_node_state(uint32_t nodeIndex, bool visible, bool renderable);

    void draw(Graphics &gfx) override;
    bool handle_mouse_down(const MouseEvent &e) override;
    bool handle_mouse_move(const MouseEvent &e) override;
    bool handle_mouse_up(const MouseEvent &e) override;
    bool handle_mouse_wheel(float delta, Point p) override;
    bool handle_raw_mouse_delta(int deltaX, int deltaY) override;

    CursorType get_cursor_type() const;

    EditorState& get_state() { return state_; }
    const EditorState& get_state() const { return state_; }

private:
    void setup_regions();
    SplitterId hit_test_splitter(Point p) const;

    EditorState state_;

    // Areas
    std::shared_ptr<Area> topbar_area_;
    std::shared_ptr<Area> toolshelf_area_;
    std::shared_ptr<Area> viewport_area_;
    std::shared_ptr<Area> outliner_area_;
    std::shared_ptr<Area> attribute_area_;
    std::shared_ptr<Area> timeline_area_;

    // Docking & Resizing Splitter State
    float toolshelf_w_ = 42.0f;
    float right_deck_w_ = 310.0f;
    float outliner_ratio_ = 0.48f;
    float timeline_h_ = 160.0f;

    SplitterId active_splitter_ = SplitterId::None;
    SplitterId hovered_splitter_ = SplitterId::None;
    Point drag_start_pos_;
    float drag_start_val_ = 0.0f;
    std::unique_ptr<AssetLibraryDrawer> asset_drawer_;
    std::unique_ptr<RenderSetupDialog> render_setup_dialog_;
};

} // namespace medusa
