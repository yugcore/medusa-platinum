#include <medusa/editor/regions/editor_topbar.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <medusa/ui/medusa_icons.hpp>
#include <algorithm>
#include <utility>

namespace medusa {

TopbarRegion::TopbarRegion(EditorState &state) : state_(state) {
    init_widgets();
}

bool TopbarRegion::accepts_input(Point p) const {
    return bounds_.contains(p) || popup_open_;
}

Rect TopbarRegion::clip_bounds() const {
    const float bottom = popup_open_ ? std::max(bounds_.y + 250.0f, popup_rect_.bottom())
                                     : bounds_.y + 250.0f;
    return Rect(bounds_.x, bounds_.y, bounds_.width, bottom - bounds_.y);
}

void TopbarRegion::layout(const Rect &bounds) {
    bounds_ = bounds;
    constexpr float row_height = 26.0f;
    const float menu_y = bounds.y + 2.0f;
    const float workspace_y = bounds.y + 32.0f;
    constexpr float logo_size = 20.0f;
    logo_rect_ = Rect(bounds.x + 7.0f, menu_y + (row_height - logo_size) * 0.5f,
                      logo_size, logo_size);
    logo_separator_x_ = logo_rect_.right() + 7.0f;

    // Keep application menus on a dedicated compact row, beginning right
    // after the small, unboxed product mark.
    float x = logo_separator_x_ + 7.0f;
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        const float btn_w = std::max(38.0f, static_cast<float>(menu_buttons_[i].get_label().length()) * 6.4f + 16.0f);
        menu_buttons_[i].set_bounds(Rect(x, menu_y, btn_w, row_height));
        x += btn_w + 2.0f;
    }
    // Workspace tabs share a compact row with the render action.
    const float tab_width = bounds.width < 380.0f ? 54.0f : bounds.width < 620.0f ? 76.0f : 88.0f;
    const float tab_gap = 2.0f;
    const float render_w = bounds.width < 380.0f ? 104.0f : 136.0f;
    const float render_x = bounds.right() - 8.0f - render_w;
    render_btn_.set_label(bounds.width < 380.0f ? "Render" : "Render Sequence");
    const float tabs_w = mode_buttons_.empty() ? 0.0f :
        mode_buttons_.size() * tab_width + (mode_buttons_.size() - 1) * tab_gap;
    render_separator_x_ = render_x - 8.0f;
    const float tab_area_width = std::max(0.0f, render_separator_x_ - bounds.x - 12.0f);
    const float tabs_x = bounds.x + std::max(6.0f, (tab_area_width - tabs_w) * 0.5f);
    for (size_t i = 0; i < mode_buttons_.size(); ++i) {
        mode_buttons_[i].set_bounds(Rect(tabs_x + i * (tab_width + tab_gap), workspace_y,
                                         tab_width, row_height));
    }
    render_btn_.set_bounds(Rect(render_x, workspace_y, render_w, row_height));
}

void TopbarRegion::draw(Graphics &gfx) {
    gfx.fill_rect(bounds_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(bounds_.x, bounds_.bottom()), Point(bounds_.right(), bounds_.bottom()), Theme::kBorderSubtle, 1.0f);

    gfx.draw_app_logo(logo_rect_);

    // Fine dividers establish the two navigation rows without boxing the
    // logo or separating menus into a detached card.
    gfx.draw_line(Point(logo_separator_x_, bounds_.y + 5.0f), Point(logo_separator_x_, bounds_.y + 27.0f),
                  Theme::kBorderSubtle, 1.0f);
    gfx.draw_line(Point(bounds_.x, bounds_.y + 30.0f), Point(bounds_.right(), bounds_.y + 30.0f),
                  Theme::kBorderSubtle, 1.0f);
    gfx.draw_line(Point(render_separator_x_, bounds_.y + 35.0f),
                  Point(render_separator_x_, bounds_.bottom() - 4.0f),
                  Theme::kBorderSubtle, 1.0f);

    // Menus
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        menu_buttons_[i].set_active(popup_open_ && active_menu_ == static_cast<int>(i));
    }
    for (auto &btn : menu_buttons_) {
        btn.draw(gfx);
    }

    // Workspace Tabs
    for (auto &tab : mode_buttons_) {
        tab.set_active(tab.get_label() == state_.active_workspace);
        tab.draw(gfx);
    }

    draw_popup(gfx);

    // Render Sequence Button
    render_btn_.draw(gfx);

    for (auto &btn : menu_buttons_) btn.draw_tooltip(gfx);
    for (auto &tab : mode_buttons_) tab.draw_tooltip(gfx);
    render_btn_.draw_tooltip(gfx);
}

bool TopbarRegion::handle_mouse_down(const MouseEvent &e) {
    if (popup_open_) {
        for (size_t i = 0; i < popup_items_.size(); ++i) {
            if (popup_item_rect(i).contains(e.pos)) {
                if (popup_items_[i].enabled && popup_items_[i].action) popup_items_[i].action();
                popup_open_ = false;
                state_.menu_popup_open = false;
                return true;
            }
        }
        popup_open_ = false;
        state_.menu_popup_open = false;
        if (!bounds_.contains(e.pos)) return false;
    }
    for (auto &btn : menu_buttons_) {
        if (btn.handle_mouse_down(e)) {
            active_menu_ = static_cast<int>(&btn - menu_buttons_.data());
            populate_popup(active_menu_);
            popup_open_ = true;
            state_.menu_popup_open = true;
            return true;
        }
    }
    for (size_t i = 0; i < mode_buttons_.size(); ++i) {
        if (mode_buttons_[i].handle_mouse_down(e)) {
            for (size_t j = 0; j < mode_buttons_.size(); ++j) {
                mode_buttons_[j].set_active(j == i);
            }
            state_.activate_workspace(mode_buttons_[i].get_label());
            return true;
        }
    }
    if (render_btn_.handle_mouse_down(e)) return true;
    return false;
}

bool TopbarRegion::handle_mouse_move(const MouseEvent &e) {
    bool changed = last_mouse_pos_.x != e.pos.x || last_mouse_pos_.y != e.pos.y;
    last_mouse_pos_ = e.pos;
    int hovered_menu = -1;
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        if (menu_buttons_[i].handle_mouse_move(e)) changed = true;
        if (menu_buttons_[i].get_bounds().contains(e.pos)) hovered_menu = static_cast<int>(i);
    }
    for (auto &tab : mode_buttons_) {
        if (tab.handle_mouse_move(e)) changed = true;
    }
    if (render_btn_.handle_mouse_move(e)) changed = true;
    if (popup_open_) {
        if (hovered_menu >= 0 && hovered_menu != active_menu_) {
            active_menu_ = hovered_menu;
            populate_popup(active_menu_);
            changed = true;
        } else if (hovered_menu < 0 && !popup_rect_.contains(e.pos)) {
            popup_open_ = false;
            state_.menu_popup_open = false;
            active_menu_ = -1;
            changed = true;
        }
    }
    return changed;
}

bool TopbarRegion::handle_mouse_up(const MouseEvent &e) {
    for (auto &btn : menu_buttons_) {
        if (btn.handle_mouse_up(e)) return true;
    }
    for (auto &tab : mode_buttons_) {
        if (tab.handle_mouse_up(e)) return true;
    }
    if (render_btn_.handle_mouse_up(e)) return true;
    return false;
}

void TopbarRegion::init_widgets() {
    // Clean menus without duplicates: File, Edit, Create, Camera, Render, Window
    std::vector<std::string> menus = { "File", "Edit", "Create", "Camera", "Render", "Window" };
    for (const auto &m : menus) {
        ButtonWidget button(m, "", nullptr);
        button.set_flat(true);
        menu_buttons_.push_back(button);
    }

    // Medusa workflow: one shared project moves from set assembly to shots
    // and then to final output.
    std::vector<std::string> modes = { "Scene", "Shot", "Production" };
    const std::vector<std::string> mode_tips = {
        "Build the set: add assets, arrange objects and set the lighting",
        "Create shots, animate cameras and objects, and preview timing",
        "Choose render quality and output the finished frames"
    };

    for (size_t i = 0; i < modes.size(); ++i) {
        const auto &mode = modes[i];
        bool is_active = (mode == state_.active_workspace);
        ButtonWidget btn(mode, "", nullptr);
        btn.set_active(is_active);
        btn.set_flat(true);
        btn.set_tooltip(mode_tips[i]);
        mode_buttons_.push_back(btn);
    }

    render_btn_ = ButtonWidget("Render Sequence", "action_render", [this]() { state_.render_setup_open = true; });
    render_btn_.set_flat(true);
    render_btn_.set_primary(true);
    render_btn_.set_tooltip("Render the active frame range to image files");
}

Rect TopbarRegion::popup_item_rect(size_t index) const {
    constexpr float item_height = 30.0f;
    return Rect(popup_rect_.x, popup_rect_.y + 4.0f + index * item_height,
                popup_rect_.width, item_height);
}

void TopbarRegion::populate_popup(int menu) {
    popup_items_.clear();
    auto add = [this](const char* label, const char* icon, bool enabled,
                      std::function<void()> action = {}, bool separator = false) {
        popup_items_.push_back({label, icon, enabled, separator, std::move(action)});
    };
    switch (menu) {
        case 0: // File
            add("New Project...", "file_import", true, [this]() { state_.pending_command = "new_project"; });
            add("Open Project...", "file_open", true, [this]() { state_.pending_command = "open_project"; });
            add("Save Project", "file_save", true, [this]() { state_.pending_command = "save_project"; }, true);
            add("Save Project As...", "file_save", true, [this]() { state_.pending_command = "save_project_as"; });
            add("Add Model...", "file_open", state_.active_workspace != "Shot",
                [this]() { state_.pending_command = "open_asset"; });
            add("Exit", "", true, [this]() { state_.pending_command = "exit"; }, true);
            break;
        case 1: // Edit
            add("Undo", "edit_undo", state_.history_can_undo,
                [this]() { state_.pending_command = "undo"; });
            add("Redo", "edit_redo", state_.history_can_redo,
                [this]() { state_.pending_command = "redo"; });
            add("", "", true, {}, true);
            add("Reset Layout", "", true, [this]() { state_.pending_command = "reset_layout"; });
            break;
        case 2: // Create
            add("Add Model...", "file_import", state_.active_workspace != "Shot",
                [this]() { state_.pending_command = "open_asset"; });
            break;
        case 3: // Camera
            add("Show Safe Frame", "camera_safe_frame", true, [this]() { state_.show_safe_frame = !state_.show_safe_frame; });
            add("Show Camera Motion Path", "", true,
                [this]() { state_.show_camera_trail = !state_.show_camera_trail; });
            add("Camera Properties", "camera_aperture", true, [this]() {
                state_.select_active_camera();
            });
            break;
        case 4: // Render
            add("Render Current Frame...", "action_render", true, [this]() { state_.pending_command = "render_current"; });
            add("Render Sequence...", "action_render", true, [this]() { state_.render_setup_open = true; });
            add("Render Settings", "render_settings", true, [this]() { state_.render_setup_open = true; });
            break;
        case 5: // Window
            add("Reset Layout", "", true, [this]() { state_.pending_command = "reset_layout"; });
            add("Show Ground Grid", "view_grid", true, [this]() { state_.show_grid = !state_.show_grid; });
            break;
    }
    const Rect anchor = menu_buttons_[static_cast<size_t>(menu)].get_bounds();
    float width = 224.0f;
    for (const auto& item : popup_items_)
        width = std::max(width, static_cast<float>(item.label.length()) * 7.0f + 52.0f);
    const float popup_x = std::clamp(anchor.x, bounds_.x + 4.0f, std::max(bounds_.x + 4.0f, bounds_.right() - width - 4.0f));
    popup_rect_ = Rect(popup_x, anchor.bottom() + 2.0f, width,
                       popup_items_.size() * 30.0f + 8.0f);
}

void TopbarRegion::draw_popup(Graphics& gfx) {
    if (!popup_open_) return;
    gfx.fill_rect(popup_rect_, Theme::kPanelBg);
    gfx.draw_rect(popup_rect_, Theme::kBorderBright, 1.0f);
    for (size_t i = 0; i < popup_items_.size(); ++i) {
        const Rect row = popup_item_rect(i);
        if (popup_items_[i].separator_before)
            gfx.draw_line(Point(row.x + 8.0f, row.y + 2.0f), Point(row.right() - 8.0f, row.y + 2.0f), Theme::kBorderSubtle, 1.0f);
        if (row.contains(last_mouse_pos_)) gfx.fill_rect(row, Theme::kControlHover);
        if (!popup_items_[i].icon.empty())
            IconRenderer::draw_icon(gfx, popup_items_[i].icon, Rect(row.x + 9.0f, row.y + 7.0f, 16.0f, 16.0f),
                                    popup_items_[i].enabled ? Theme::kTextSecondary : Theme::kTextMuted);
        gfx.draw_text(popup_items_[i].label, Rect(row.x + 34.0f, row.y, row.width - 44.0f, row.height),
                      popup_items_[i].enabled ? Theme::kTextPrimary : Theme::kTextMuted,
                      Theme::kUiFont, false, false, true, Theme::kUiFontFamily);
    }
}

bool TopbarRegion::popup_contains(Point p) const {
    return popup_open_ && popup_rect_.contains(p);
}

} // namespace medusa
