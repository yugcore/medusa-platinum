#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace medusa {

class OutlinerRegion : public Region {
public:
    explicit OutlinerRegion(EditorState &state);

    void layout(const Rect &bounds) override;
    void draw(Graphics &gfx) override;
    bool handle_mouse_down(const MouseEvent &e) override;
    bool handle_mouse_move(const MouseEvent &e) override;
    bool handle_mouse_up(const MouseEvent &e) override;
    bool handle_mouse_wheel(float delta, Point p) override;

private:
    size_t first_visible_row() const;
    size_t last_visible_row() const;
    static const char* icon_for(const zegfx::ProductionSceneNode& node);
    bool has_children(uint32_t index) const;
    void append_node(uint32_t index, int depth, std::vector<uint8_t>& visited);
    void rebuild_rows();
    void position_rows();

    EditorState &state_;
    Rect header_rect_;
    Rect summary_rect_;
    Rect row_viewport_;
    float scroll_offset_ = 0.0f;
    float scroll_drag_y_ = 0.0f;
    float scroll_drag_start_ = 0.0f;
    bool is_drag_scrolling_ = false;
    std::vector<OutlinerRowWidget> rows_;
    std::vector<uint32_t> row_node_indices_;
    uint64_t rows_generation_ = ~uint64_t(0);
};

} // namespace medusa
