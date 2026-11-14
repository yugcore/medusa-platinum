#include <medusa/editor/regions/editor_topbar.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <medusa/ui/medusa_icons.hpp>
#include <algorithm>
#include <utility>
#include <cmath>

namespace medusa {

TopbarRegion::TopbarRegion(EditorState &state) : state_(state) {
    init_widgets();
}

bool TopbarRegion::accepts_input(Point p) const {
    return bounds_.contains(p) || (popup_menu_.is_open() && popup_menu_.contains(p));
}

Rect TopbarRegion::clip_bounds() const {
    // Topbar content stays within bounds; the popup is drawn unclipped in overlay pass
    return bounds_;
}

void TopbarRegion::layout(const Rect &bounds) {
    bounds_ = bounds;
    const float btn_h = bounds.height - 4.0f; // 24.0f on 28px topbar
    const float btn_y = bounds.y + 2.0f;

    // 1. Menu Buttons (File, Edit, Create, Camera, Render, Window, Options)
    float x = bounds.x + 8.0f;
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        const float label_w = static_cast<float>(menu_buttons_[i].get_label().length()) * 6.8f;
        const float btn_w = std::max(36.0f, label_w + 14.0f);
        menu_buttons_[i].set_bounds(Rect(x, btn_y, btn_w, btn_h));
        x += btn_w + 2.0f;
    }
    menus_end_x_ = x;

    // Subtle vertical hairline separator between menus and workspace tabs
    workspace_divider_x_ = x + 6.0f;

    // 2. Workspace Tabs (Scene, Shot, Production)
    float tab_x = workspace_divider_x_ + 10.0f;
    for (size_t i = 0; i < mode_buttons_.size(); ++i) {
        const float label_w = static_cast<float>(mode_buttons_[i].get_label().length()) * 6.8f;
        const float tab_w = std::max(48.0f, label_w + 20.0f);
        mode_buttons_[i].set_bounds(Rect(tab_x, btn_y, tab_w, btn_h));
        tab_x += tab_w + 3.0f;
    }

    // 3. Render Action Button on far right
    const float render_w = bounds.width < 540.0f ? 84.0f : 128.0f;
    const float render_x = bounds.right() - render_w - 8.0f;
    render_btn_.set_label(bounds.width < 540.0f ? "Render" : "Render Sequence");
    render_btn_.set_bounds(Rect(render_x, btn_y, render_w, btn_h));
}

void TopbarRegion::draw(Graphics &gfx) {
    // Sleek obsidian chassis background
    gfx.fill_rect(bounds_, Theme::kPanelHeaderBg);
    // Subtle bottom border separating topbar from viewport canvas and panels
    gfx.draw_line(Point(bounds_.x, bounds_.bottom()), Point(bounds_.right(), bounds_.bottom()),
                  Theme::kBorderSubtle, 1.0f);

    // 1. Menu Buttons
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        const Rect r = menu_buttons_[i].get_bounds();
        const bool is_active = (popup_menu_.is_open() && active_menu_ == static_cast<int>(i));
        const bool is_hovered = menu_buttons_[i].is_hovered();

        if (is_active) {
            // Blender-style active tab pill: dark graphite, resting flush onto the popup
            gfx.fill_rounded_rect(Rect(r.x, r.y, r.width, r.height + 2.0f), 3.0f, Color::from_hex(0x22242A));
            gfx.draw_rounded_rect(Rect(r.x, r.y, r.width, r.height + 2.0f), 3.0f, Color::from_hex(0x383B44), 1.0f);
            gfx.draw_text(menu_buttons_[i].get_label(), r, Theme::kTextPrimary, Theme::kUiFont, true, true, true, Theme::kUiFontFamily);
        } else if (is_hovered) {
            gfx.fill_rounded_rect(r, 3.0f, Color::from_hex(0x282A31));
            gfx.draw_text(menu_buttons_[i].get_label(), r, Color::from_hex(0xFFFFFF), Theme::kUiFont, false, true, true, Theme::kUiFontFamily);
        } else {
            gfx.draw_text(menu_buttons_[i].get_label(), r, Theme::kTextSecondary, Theme::kUiFont, false, true, true, Theme::kUiFontFamily);
        }
    }

    // 2. Vertical Divider Hairline between Menus and Workspace Tabs
    if (workspace_divider_x_ > bounds_.x && workspace_divider_x_ < bounds_.right()) {
        const float div_top = bounds_.y + 6.0f;
        const float div_bot = bounds_.bottom() - 6.0f;
        gfx.draw_line(Point(workspace_divider_x_, div_top), Point(workspace_divider_x_, div_bot),
                      Color::from_hex(0x2E3138), 1.0f);
    }

    // 3. Workspace Tabs (Scene, Shot, Production)
    for (size_t i = 0; i < mode_buttons_.size(); ++i) {
        const Rect r = mode_buttons_[i].get_bounds();
        const bool is_active = (mode_buttons_[i].get_label() == state_.active_workspace);
        const bool is_hovered = mode_buttons_[i].is_hovered();

        if (is_active) {
            gfx.fill_rounded_rect(r, 3.0f, Color::from_hex(0x272A32));
            gfx.draw_rounded_rect(r, 3.0f, Color::from_hex(0x3C404C), 1.0f);
            gfx.draw_text(mode_buttons_[i].get_label(), r, Theme::kTextPrimary, Theme::kUiFont, true, true, true, Theme::kUiFontFamily);
        } else if (is_hovered) {
            gfx.fill_rounded_rect(r, 3.0f, Color::from_hex(0x24262C));
            gfx.draw_text(mode_buttons_[i].get_label(), r, Color::from_hex(0xFFFFFF), Theme::kUiFont, false, true, true, Theme::kUiFontFamily);
        } else {
            gfx.draw_text(mode_buttons_[i].get_label(), r, Theme::kTextSecondary, Theme::kUiFont, false, true, true, Theme::kUiFontFamily);
        }
    }

    // 4. Render Action Button
    render_btn_.draw(gfx);

    // Tooltips
    for (auto &btn : menu_buttons_) btn.draw_tooltip(gfx);
    for (auto &tab : mode_buttons_) tab.draw_tooltip(gfx);
    render_btn_.draw_tooltip(gfx);
}

void TopbarRegion::draw_popup(Graphics &gfx) {
    popup_menu_.draw(gfx);
}

bool TopbarRegion::popup_contains(Point p) const {
    return popup_menu_.contains(p);
}

bool TopbarRegion::handle_mouse_down(const MouseEvent &e) {
    if (popup_menu_.is_open()) {
        // 1. Check if clicked on a menu button (toggle or switch)
        for (size_t i = 0; i < menu_buttons_.size(); ++i) {
            if (menu_buttons_[i].get_bounds().contains(e.pos)) {
                if (active_menu_ == static_cast<int>(i)) {
                    popup_menu_.close();
                    state_.menu_popup_open = false;
                    state_.active_topbar_popup_rect = Rect{};
                    active_menu_ = -1;
                } else {
                    active_menu_ = static_cast<int>(i);
                    populate_popup(active_menu_);
                    state_.menu_popup_open = true;
                    state_.active_topbar_popup_rect = popup_menu_.get_bounds();
                }
                return true;
            }
        }

        // 2. Clicked on popup items
        if (popup_menu_.contains(e.pos)) {
            bool handled = popup_menu_.handle_mouse_down(e);
            state_.menu_popup_open = popup_menu_.is_open();
            state_.active_topbar_popup_rect = popup_menu_.get_bounds();
            if (!popup_menu_.is_open()) active_menu_ = -1;
            return handled;
        }

        // 3. Clicked outside popup: dismiss
        popup_menu_.close();
        state_.menu_popup_open = false;
        state_.active_topbar_popup_rect = Rect{};
        active_menu_ = -1;
        if (!bounds_.contains(e.pos)) return false;
    }

    // Normal menu button click when popup is closed
    for (size_t i = 0; i < menu_buttons_.size(); ++i) {
        if (menu_buttons_[i].get_bounds().contains(e.pos)) {
            active_menu_ = static_cast<int>(i);
            populate_popup(active_menu_);
            state_.menu_popup_open = true;
            state_.active_topbar_popup_rect = popup_menu_.get_bounds();
            return true;
        }
    }

    // Workspace tabs
    for (size_t i = 0; i < mode_buttons_.size(); ++i) {
        if (mode_buttons_[i].get_bounds().contains(e.pos)) {
            state_.activate_workspace(mode_buttons_[i].get_label());
            return true;
        }
    }

    // Render sequence button
    if (render_btn_.handle_mouse_down(e)) return true;

    return false;
}

bool TopbarRegion::handle_mouse_move(const MouseEvent &e) {
    bool changed = false;
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

    if (popup_menu_.is_open()) {
        // Seamless menu hopping: hovering over another menu button switches active dropdown (like Blender)
        if (hovered_menu >= 0 && hovered_menu != active_menu_) {
            active_menu_ = hovered_menu;
            populate_popup(active_menu_, /*instant=*/true);
            state_.active_topbar_popup_rect = popup_menu_.get_bounds();
            changed = true;
        } else {
            if (popup_menu_.handle_mouse_move(e)) changed = true;
        }
    }

    return changed;
}

bool TopbarRegion::handle_mouse_up(const MouseEvent &e) {
    for (auto &btn : menu_buttons_) btn.handle_mouse_up(e);
    for (auto &tab : mode_buttons_) tab.handle_mouse_up(e);
    render_btn_.handle_mouse_up(e);
    return false;
}

void TopbarRegion::init_widgets() {
    // 7 Standard DCC Menus: File, Edit, Create, Camera, Render, Window, Options
    std::vector<std::string> menus = { "File", "Edit", "Create", "Camera", "Render", "Window", "Options" };
    for (const auto &m : menus) {
        ButtonWidget button(m, "", nullptr);
        button.set_flat(true);
        menu_buttons_.push_back(button);
    }

    // 3 Production Workspace Modes
    std::vector<std::string> modes = { "Scene", "Shot", "Production" };
    const std::vector<std::string> mode_tips = {
        "Build the set: add assets, arrange objects and set the lighting",
        "Create shots, animate cameras and objects, and preview timing",
        "Choose render quality and output the finished frames"
    };

    for (size_t i = 0; i < modes.size(); ++i) {
        ButtonWidget btn(modes[i], "", nullptr);
        btn.set_flat(true);
        btn.set_tooltip(mode_tips[i]);
        mode_buttons_.push_back(btn);
    }

    render_btn_ = ButtonWidget("Render Sequence", "action_render", [this]() {
        state_.render_setup_open = true;
    });
    render_btn_.set_flat(true);
    render_btn_.set_primary(true);
    render_btn_.set_tooltip("Render the active frame range to image files");
}

void TopbarRegion::populate_popup(int menu, bool instant) {
    popup_menu_.clear();

    auto add_item = [this](const char* label, const char* icon, const char* shortcut,
                           bool enabled, std::function<void()> action = {}) {
        popup_menu_.add_item(label, icon ? icon : "", shortcut ? shortcut : "", enabled, std::move(action));
    };
    auto add_check = [this](const char* label, bool checked, std::function<void()> action) {
        popup_menu_.add_check(label, checked, std::move(action));
    };
    auto add_sep = [this]() {
        popup_menu_.add_separator();
    };

    switch (menu) {
        case 0: // File
            add_item("New Project...", "file_import", "Ctrl+N", true, [this]() { state_.pending_command = "new_project"; });
            add_item("Open Project...", "file_open", "Ctrl+O", true, [this]() { state_.pending_command = "open_project"; });
            add_sep();
            add_item("Save Project", "file_save", "Ctrl+S", true, [this]() { state_.pending_command = "save_project"; });
            add_item("Save Project As...", "file_save", "Ctrl+Shift+S", true, [this]() { state_.pending_command = "save_project_as"; });
            add_sep();
            add_item("Import Model...", "file_open", "Ctrl+I", state_.active_workspace != "Shot", [this]() { state_.pending_command = "import_model"; });
            add_item("Import USD Stage...", "view_grid", "Ctrl+Shift+I", state_.active_workspace != "Shot", [this]() { state_.pending_command = "import_usd"; });
            add_item("Export USD Scene...", "file_save", "Ctrl+E", true, [this]() { state_.pending_command = "export_usd"; });
            add_sep();
            add_item("Exit", "", "Alt+F4", true, []() { PostQuitMessage(0); });
            break;

        case 1: // Edit
            add_item("Undo", "edit_undo", "Ctrl+Z", true, [this]() { state_.pending_command = "undo"; });
            add_item("Redo", "edit_redo", "Ctrl+Y", true, [this]() { state_.pending_command = "redo"; });
            add_sep();
            add_item("Duplicate", "mesh_cube", "Shift+D", state_.active_workspace != "Shot", [this]() { state_.pending_command = "duplicate_object"; });
            add_item("Delete", "", "Del", state_.active_workspace != "Shot", [this]() { state_.pending_command = "delete_object"; });
            add_sep();
            add_item("Select All", "", "Ctrl+A", true, [this]() { state_.pending_command = "select_all"; });
            add_item("Deselect All", "", "Alt+A", true, [this]() { state_.pending_command = "deselect_all"; });
            break;

        case 2: // Create
            add_item("Cube", "mesh_cube", "Shift+A", state_.active_workspace != "Shot", [this]() { state_.pending_command = "add_cube"; });
            add_item("Sphere", "mesh_sphere", "", state_.active_workspace != "Shot", [this]() { state_.pending_command = "add_sphere"; });
            add_item("Plane", "view_grid", "", state_.active_workspace != "Shot", [this]() { state_.pending_command = "add_plane"; });
            add_sep();
            add_item("Directional Light", "light_directional", "", state_.active_workspace != "Shot", [this]() { state_.pending_command = "add_light_directional"; });
            add_item("Point Light", "light_point", "", state_.active_workspace != "Shot", [this]() { state_.pending_command = "add_light_point"; });
            add_item("Spot Light", "light_spot", "", state_.active_workspace != "Shot", [this]() { state_.pending_command = "add_light_spot"; });
            add_sep();
            add_item("Camera", "camera_actor", "", state_.active_workspace != "Shot", [this]() { state_.pending_command = "add_camera"; });
            break;

        case 3: // Camera
            add_check("Show Safe Frame", state_.show_safe_frame, [this]() { state_.show_safe_frame = !state_.show_safe_frame; });
            add_check("Show Camera Trail", state_.show_camera_trail, [this]() { state_.show_camera_trail = !state_.show_camera_trail; });
            add_sep();
            add_item("View Active Camera", "camera_actor", "Numpad 0", true, [this]() { state_.select_active_camera(); });
            add_item("Frame Active Object", "", "Numpad .", true, [this]() { state_.pending_command = "frame_selected"; });
            add_sep();
            add_item("Camera Properties", "camera_aperture", "", true, [this]() { state_.select_active_camera(); });
            break;

        case 4: // Render
            add_item("Render Current Frame...", "action_render", "F12", true, [this]() { state_.pending_command = "render_current"; });
            add_item("Render Sequence...", "action_render", "Ctrl+F12", true, [this]() { state_.render_setup_open = true; });
            add_sep();
            add_item("Render Settings...", "render_settings", "", true, [this]() { state_.render_setup_open = true; });
            add_check("Wavelet AI Denoising", state_.denoiser_enabled, [this]() { state_.denoiser_enabled = !state_.denoiser_enabled; });
            break;

        case 5: // Window
            add_item("Toggle Fullscreen", "", "F11", true, [this]() { state_.pending_command = "toggle_fullscreen"; });
            add_item("Python Console", "tool_select", "Ctrl+~", true, [this]() { state_.pending_command = "toggle_python_console"; });
            add_sep();
            add_item("Reset Layout", "", "", true, [this]() { state_.pending_command = "reset_layout"; });
            add_check("Show Ground Grid", state_.show_grid, [this]() { state_.show_grid = !state_.show_grid; });
            add_check("Show Timeline", state_.ui_timeline_height > 20.0f, [this]() {
                state_.ui_timeline_height = (state_.ui_timeline_height > 20.0f) ? 0.0f : 160.0f;
            });
            break;

        case 6: // Options
            add_item("Preferences...", "render_settings", "Ctrl+,", true, [this]() { state_.render_setup_open = true; });
            add_check("Obsidian Studio Theme", true, []() {});
            add_sep();
            add_item("ZeGFX Engine Diagnostics...", "view_grid", "", true, [this]() { state_.pending_command = "diagnostics"; });
            add_item("About Medusa Platinum...", "", "", true, [this]() { state_.pending_command = "about"; });
            break;
    }

    const Rect anchor = menu_buttons_[static_cast<size_t>(menu)].get_bounds();
    Rect screen_bounds(0.0f, 0.0f, 4000.0f, 3000.0f);
    popup_menu_.open(anchor, screen_bounds, 210.0f, 340.0f, instant);
    state_.active_topbar_popup_rect = popup_menu_.get_bounds();
}

} // namespace medusa
