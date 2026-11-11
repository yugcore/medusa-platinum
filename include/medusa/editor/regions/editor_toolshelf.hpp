#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/editor/editor_state.hpp>
#include <medusa/ui/medusa_widgets.hpp>
#include <vector>
#include <string>

namespace medusa {

class ToolshelfRegion : public Region {
public:
    ToolshelfRegion(EditorState &state);

    Rect clip_bounds() const override;
    void layout(const Rect &bounds) override;
    void draw(Graphics &gfx) override;
    bool handle_mouse_down(const MouseEvent &e) override;
    bool handle_mouse_move(const MouseEvent &e) override;
    bool handle_mouse_up(const MouseEvent &e) override;

private:
    void init_tools();

    EditorState &state_;
    std::vector<ButtonWidget> tool_buttons_;
    std::vector<std::string> tool_ids_;
    std::vector<float> separator_ys_;
};

} // namespace medusa
