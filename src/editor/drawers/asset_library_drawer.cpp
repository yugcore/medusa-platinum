#include <medusa/editor/drawers/asset_library_drawer.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <medusa/ui/medusa_icons.hpp>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <functional>

namespace medusa {

AssetLibraryDrawer::AssetLibraryDrawer(EditorState& state) : state_(state) {
    if (!state_.asset_library_root.empty()) set_root(state_.asset_library_root);
}

void AssetLibraryDrawer::layout(float width, float height) {
    width_ = std::max(0.0f, width);
    height_ = std::max(0.0f, height);
    bar_rect_ = Rect(0.0f, std::max(0.0f, height_ - kBarHeight), width_, kBarHeight);
    const float max_height = std::max(0.0f, height_ - kBarHeight - 52.0f);
    target_height_ = std::min(max_height, std::clamp(height_ * 0.42f, 210.0f, 430.0f));
    visible_height_ = target_height_ * progress_;
    panel_rect_ = Rect(0.0f, bar_rect_.y - visible_height_, width_, visible_height_);
    header_rect_ = Rect(panel_rect_.x, panel_rect_.y, panel_rect_.width,
                        std::min(kHeaderHeight, panel_rect_.height));

    add_rect_ = Rect(std::max(0.0f, width_ - 122.0f), panel_rect_.y + 4.0f, 114.0f, 24.0f);
    refresh_rect_ = Rect(std::max(0.0f, add_rect_.x - 72.0f), panel_rect_.y + 4.0f, 68.0f, 24.0f);
    browse_rect_ = Rect(std::max(0.0f, refresh_rect_.x - 116.0f), panel_rect_.y + 4.0f, 112.0f, 24.0f);

    const float content_y = panel_rect_.y + kHeaderHeight;
    const float content_h = std::max(0.0f, panel_rect_.height - kHeaderHeight);
    tree_width_ = width_ * tree_ratio_;
    tree_rect_ = Rect(0.0f, content_y, tree_width_, content_h);
    grid_rect_ = Rect(tree_width_ + 1.0f, content_y,
                      std::max(0.0f, width_ - tree_width_ - 1.0f), content_h);
    tree_heading_rect_ = Rect(tree_rect_.x, tree_rect_.y, tree_rect_.width,
                              std::min(kPaneHeaderHeight, tree_rect_.height));
    grid_heading_rect_ = Rect(grid_rect_.x, grid_rect_.y, grid_rect_.width,
                              std::min(kPaneHeaderHeight, grid_rect_.height));
    tree_rows_rect_ = Rect(tree_rect_.x, tree_rect_.y + kPaneHeaderHeight, tree_rect_.width,
                           std::max(0.0f, tree_rect_.height - kPaneHeaderHeight - kFooterHeight));
    grid_content_rect_ = Rect(grid_rect_.x, grid_rect_.y + kPaneHeaderHeight, grid_rect_.width,
                              std::max(0.0f, grid_rect_.height - kPaneHeaderHeight - kFooterHeight));
    tree_divider_x_ = tree_width_;
    update_grid_metrics();
}

void AssetLibraryDrawer::draw(Graphics& gfx) {
    update_animation();
    layout(width_, height_);
    if (visible_height_ > 0.5f) {
        gfx.fill_rect(panel_rect_, Theme::kPanelBg);
        gfx.draw_rect(panel_rect_, Theme::kBorderSubtle, 1.0f);
        gfx.push_clip(panel_rect_);
        if (tab_ == Tab::Assets) {
            draw_header(gfx);
            draw_assets(gfx);
        } else if (tab_ == Tab::Output) {
            draw_header(gfx);
            draw_output(gfx);
        } else if (tab_ == Tab::Console) {
            console_widget_.layout(panel_rect_);
            console_widget_.draw(gfx);
        }
        gfx.pop_clip();
    }
    draw_bottom_bar(gfx);
}

bool AssetLibraryDrawer::handle_mouse_down(const MouseEvent& e) {
    if (e.button != MouseButton::Left) return false;
    if (bar_rect_.contains(e.pos)) {
        pressed_ = true;
        if (assets_tab_rect_.contains(e.pos)) select_tab(Tab::Assets);
        else if (output_tab_rect_.contains(e.pos)) select_tab(Tab::Output);
        else if (console_tab_rect_.contains(e.pos)) select_tab(Tab::Console);
        else if (tab_ == Tab::Assets) select_tab(Tab::Assets);
        else if (tab_ == Tab::Output) select_tab(Tab::Output);
        else select_tab(Tab::Console);
        return true;
    }
    if (!panel_rect_.contains(e.pos)) return false;
    pressed_ = true;
    if (tab_ == Tab::Console) {
        return console_widget_.handle_mouse_down(e);
    }
    if (tab_ == Tab::Assets) {
        if (std::abs(e.pos.x - tree_divider_x_) <= kSplitterHit && e.pos.y >= tree_rect_.y) {
            dragging_divider_ = true;
            return true;
        }
        if (browse_rect_.contains(e.pos)) {
            state_.pending_command = "choose_asset_library";
            return true;
        }
        if (refresh_rect_.contains(e.pos)) {
            refresh();
            return true;
        }
        if (add_rect_.contains(e.pos) && can_import_selected()) {
            state_.pending_command = "load_library_asset";
            return true;
        }
        if (tree_rows_rect_.contains(e.pos)) {
            const int row = static_cast<int>(std::floor((e.pos.y - tree_rows_rect_.y + tree_scroll_) / kTreeRowHeight));
            const auto& visible = visible_folders();
            if (row >= 0 && static_cast<size_t>(row) < visible.size()) {
                const auto [node_index, depth] = visible[static_cast<size_t>(row)];
                const float arrow_x = tree_rect_.x + 8.0f + static_cast<float>(depth) * kTreeIndent;
                if (e.pos.x < arrow_x + 22.0f) {
                    if (!folders_[node_index].children_loaded) load_children(node_index);
                    if (!folders_[node_index].children.empty()) {
                        folders_[node_index].expanded = !folders_[node_index].expanded;
                        flattened_folders_dirty_ = true;
                    }
                } else {
                    current_folder_ = node_index;
                    state_.asset_library_selected_path.clear();
                    load_files(folders_[node_index].path);
                }
                clamp_scrolls();
                return true;
            }
        }
        if (grid_content_rect_.contains(e.pos)) {
            const int index = file_at(e.pos);
            if (index >= 0) {
                state_.asset_library_selected_path = path_to_utf8(files_[static_cast<size_t>(index)].path);
                const auto now = std::chrono::steady_clock::now();
                if (index == last_clicked_file_ && now - last_file_click_ < std::chrono::milliseconds(480) &&
                    can_import_selected()) {
                    state_.pending_command = "load_library_asset";
                }
                last_clicked_file_ = index;
                last_file_click_ = now;
                return true;
            }
        }
    } else {
        if (render_current_rect_.contains(e.pos)) {
            state_.pending_command = "render_current";
            return true;
        }
        if (render_sequence_rect_.contains(e.pos)) {
            state_.render_setup_open = true;
            return true;
        }
    }
    return true;
}

bool AssetLibraryDrawer::handle_mouse_move(const MouseEvent& e) {
    if (tab_ == Tab::Console && is_open_) {
        if (console_widget_.handle_mouse_move(e)) return true;
    }
    if (dragging_divider_) {
        tree_ratio_ = std::clamp(e.pos.x / std::max(1.0f, width_), 0.20f, 0.42f);
        layout(width_, height_);
        return true;
    }
    int button = button_at(e.pos);
    bool changed = button != hovered_button_;
    hovered_button_ = button;
    int file = tab_ == Tab::Assets && grid_content_rect_.contains(e.pos) ? file_at(e.pos) : -1;
    if (file != hovered_file_) { hovered_file_ = file; changed = true; }
    const bool divider = panel_rect_.contains(e.pos) && std::abs(e.pos.x - tree_divider_x_) <= kSplitterHit &&
                         e.pos.y >= tree_rect_.y;
    if (divider != divider_hovered_) { divider_hovered_ = divider; changed = true; }
    return changed || pressed_ || panel_rect_.contains(e.pos) || bar_rect_.contains(e.pos);
}

bool AssetLibraryDrawer::handle_mouse_up(const MouseEvent& e) {
    if (e.button != MouseButton::Left || (!pressed_ && !dragging_divider_)) return false;
    pressed_ = false;
    dragging_divider_ = false;
    return true;
}

bool AssetLibraryDrawer::handle_mouse_wheel(float delta, Point p) {
    if (!panel_rect_.contains(p) || std::abs(delta) < 1.0e-4f) return false;
    if (tab_ == Tab::Console) {
        return console_widget_.handle_mouse_wheel(delta, p);
    }
    if (tab_ == Tab::Assets && tree_rows_rect_.contains(p)) {
        tree_scroll_ = std::max(0.0f, tree_scroll_ - delta * kTreeRowHeight * 3.0f);
        clamp_scrolls();
        return true;
    }
    if (tab_ == Tab::Assets && grid_content_rect_.contains(p)) {
        grid_scroll_ = std::max(0.0f, grid_scroll_ - delta * tile_stride_y_ * 2.5f);
        clamp_scrolls();
        return true;
    }
    return true;
}

bool AssetLibraryDrawer::handle_key_down(int key, bool ctrl, bool shift) {
    if (tab_ == Tab::Console && is_open_) {
        return console_widget_.handle_key_down(key, ctrl, shift);
    }
    return false;
}

bool AssetLibraryDrawer::handle_char(uint32_t codepoint) {
    if (tab_ == Tab::Console && is_open_) {
        return console_widget_.handle_char(codepoint);
    }
    return false;
}

void AssetLibraryDrawer::open_console() {
    tab_ = Tab::Console;
    is_open_ = true;
    begin_animation(1.0f);
}

void AssetLibraryDrawer::set_root(const std::string& utf8_path) {
    std::error_code ec;
    auto path = std::filesystem::absolute(std::filesystem::u8path(utf8_path), ec);
    if (ec) path = std::filesystem::u8path(utf8_path);
    ec.clear();
    if (!std::filesystem::is_directory(path, ec) || ec) return;
    root_path_ = path.lexically_normal();
    state_.asset_library_root = path_to_utf8(root_path_);
    folders_.clear();
    flattened_folders_dirty_ = true;
    FolderNode root;
    root.path = root_path_;
    root.name = root_path_.filename().u8string();
    if (root.name.empty()) root.name = path_to_utf8(root_path_);
    root.expanded = true;
    folders_.push_back(std::move(root));
    load_children(0);
    current_folder_ = 0;
    tree_scroll_ = grid_scroll_ = 0.0f;
    state_.asset_library_selected_path.clear();
    load_files(root_path_);
}

bool AssetLibraryDrawer::animating() const { return animating_; }

bool AssetLibraryDrawer::tick_animation() {
    update_animation();
    return animating_;
}

CursorType AssetLibraryDrawer::cursor_type() const {
    return dragging_divider_ || divider_hovered_ ? CursorType::SizeWE : CursorType::Arrow;
}

std::string AssetLibraryDrawer::path_to_utf8(const std::filesystem::path& path) {
    return path.u8string();
}

std::string AssetLibraryDrawer::lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool AssetLibraryDrawer::case_insensitive_less(const std::string& a, const std::string& b) {
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
        [](unsigned char left, unsigned char right) {
            return std::tolower(left) < std::tolower(right);
        });
}

std::string AssetLibraryDrawer::file_icon(const std::string& extension) {
    const std::string ext = lowercase(extension);
    if (ext == ".glb" || ext == ".gltf" || ext == ".obj" || ext == ".fbx" ||
        ext == ".usd" || ext == ".usda" || ext == ".usdc" || ext == ".blend") return "outliner_mesh";
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tif" ||
        ext == ".tiff" || ext == ".exr" || ext == ".hdr") return "view_rendered";
    if (ext == ".mp4" || ext == ".mov" || ext == ".avi" || ext == ".mkv") return "action_render";
    return "file_import";
}

bool AssetLibraryDrawer::can_import_selected() const {
    if (state_.asset_library_selected_path.empty()) return false;
    const std::string ext = lowercase(std::filesystem::u8path(state_.asset_library_selected_path).extension().u8string());
    return ext == ".glb" || ext == ".gltf" || ext == ".usd" || ext == ".usda" || ext == ".usdc" || ext == ".usdz" || ext == ".obj" || ext == ".fbx";
}

void AssetLibraryDrawer::select_tab(Tab tab) {
    if (is_open_ && tab_ == tab) {
        is_open_ = false;
        begin_animation(0.0f);
    } else {
        tab_ = tab;
        is_open_ = true;
        begin_animation(1.0f);
    }
}

void AssetLibraryDrawer::begin_animation(float target) {
    update_animation();
    animation_start_progress_ = progress_;
    animation_target_ = target;
    animation_start_time_ = std::chrono::steady_clock::now();
    animating_ = std::abs(animation_target_ - progress_) > 0.001f;
    if (!animating_) progress_ = animation_target_;
}

void AssetLibraryDrawer::update_animation() {
    if (!animating_) return;
    const float t = std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now() - animation_start_time_).count() / 0.19f,
                               0.0f, 1.0f);
    const float eased = t * t * (3.0f - 2.0f * t);
    progress_ = animation_start_progress_ + (animation_target_ - animation_start_progress_) * eased;
    if (t >= 1.0f) {
        progress_ = animation_target_;
        animating_ = false;
    }
}

void AssetLibraryDrawer::refresh() {
    if (root_path_.empty()) return;
    set_root(path_to_utf8(root_path_));
}

void AssetLibraryDrawer::load_children(size_t index) {
    if (index >= folders_.size() || folders_[index].children_loaded) return;
    const auto directory = folders_[index].path;
    struct ChildFolder { std::filesystem::path path; std::string name; };
    std::vector<ChildFolder> child_paths;
    std::error_code ec;
    std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        std::error_code item_ec;
        if (it->is_directory(item_ec) && !item_ec) {
            ChildFolder child;
            child.path = it->path();
            child.name = child.path.filename().u8string();
            child_paths.push_back(std::move(child));
        }
    }
    std::sort(child_paths.begin(), child_paths.end(), [](const auto& a, const auto& b) {
        return case_insensitive_less(a.name, b.name);
    });
    auto& parent = folders_[index];
    parent.children_loaded = true;
    flattened_folders_dirty_ = true;
    parent.children.reserve(child_paths.size());
    for (auto& entry : child_paths) {
        FolderNode child;
        child.path = std::move(entry.path);
        child.name = std::move(entry.name);
        child.parent = index;
        const size_t child_index = folders_.size();
        folders_.push_back(std::move(child));
        folders_[index].children.push_back(child_index);
    }
}

void AssetLibraryDrawer::load_files(const std::filesystem::path& directory) {
    files_.clear();
    if (directory.empty()) return;
    std::error_code ec;
    std::filesystem::directory_iterator it(directory, std::filesystem::directory_options::skip_permission_denied, ec), end;
    for (; !ec && it != end; it.increment(ec)) {
        std::error_code item_ec;
        if (!it->is_regular_file(item_ec) || item_ec) continue;
        FileEntry file;
        file.path = it->path();
        file.name = file.path.filename().u8string();
        file.extension = file.path.extension().u8string();
        file.size = it->file_size(item_ec);
        if (item_ec) file.size = 0;
        files_.push_back(std::move(file));
    }
    std::sort(files_.begin(), files_.end(), [](const FileEntry& a, const FileEntry& b) {
        return case_insensitive_less(a.name, b.name);
    });
    grid_scroll_ = 0.0f;
    clamp_scrolls();
}

const std::vector<std::pair<size_t, int>>& AssetLibraryDrawer::visible_folders() const {
    if (!flattened_folders_dirty_) return flattened_folders_;
    flattened_folders_.clear();
    if (folders_.empty()) {
        flattened_folders_dirty_ = false;
        return flattened_folders_;
    }
    std::function<void(size_t, int)> append = [&](size_t index, int depth) {
        flattened_folders_.emplace_back(index, depth);
        const auto& node = folders_[index];
        if (!node.expanded) return;
        for (size_t child : node.children) append(child, depth + 1);
    };
    append(0, 0);
    flattened_folders_dirty_ = false;
    return flattened_folders_;
}

void AssetLibraryDrawer::update_grid_metrics() {
    tile_stride_y_ = kTileHeight + kTileGapY;
    const float usable = std::max(1.0f, grid_content_rect_.width - 20.0f);
    grid_columns_ = std::max(1, static_cast<int>((usable + kTileGapX) / (kTileWidth + kTileGapX)));
}

void AssetLibraryDrawer::clamp_scrolls() {
    const auto& visible = visible_folders();
    const float tree_max = std::max(0.0f, visible.size() * kTreeRowHeight - tree_rows_rect_.height);
    tree_scroll_ = std::clamp(tree_scroll_, 0.0f, tree_max);
    const int rows = (static_cast<int>(files_.size()) + grid_columns_ - 1) / grid_columns_;
    const float grid_max = std::max(0.0f, rows * tile_stride_y_ + 10.0f - grid_content_rect_.height);
    grid_scroll_ = std::clamp(grid_scroll_, 0.0f, grid_max);
}

int AssetLibraryDrawer::file_at(Point p) const {
    const float left = grid_content_rect_.x + 10.0f;
    const float top = grid_content_rect_.y + 8.0f - grid_scroll_;
    const int column = static_cast<int>((p.x - left) / (kTileWidth + kTileGapX));
    const int row = static_cast<int>((p.y - top) / tile_stride_y_);
    if (column < 0 || column >= grid_columns_ || row < 0) return -1;
    const Rect tile(left + column * (kTileWidth + kTileGapX), top + row * tile_stride_y_, kTileWidth, kTileHeight);
    if (!tile.contains(p)) return -1;
    const int index = row * grid_columns_ + column;
    return index >= 0 && static_cast<size_t>(index) < files_.size() ? index : -1;
}

int AssetLibraryDrawer::button_at(Point p) const {
    if (bar_rect_.contains(p)) {
        return assets_tab_rect_.contains(p) ? 0 : output_tab_rect_.contains(p) ? 1 :
               console_tab_rect_.contains(p) ? 7 : -1;
    }
    if (browse_rect_.contains(p)) return 2;
    if (refresh_rect_.contains(p)) return 3;
    if (add_rect_.contains(p)) return 4;
    if (render_current_rect_.contains(p)) return 5;
    if (render_sequence_rect_.contains(p)) return 6;
    return -1;
}

void AssetLibraryDrawer::draw_header(Graphics& gfx) {
    if (header_rect_.height <= 1.0f) return;
    gfx.fill_rect(header_rect_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(header_rect_.x, header_rect_.bottom()), Point(header_rect_.right(), header_rect_.bottom()),
                  Theme::kBorderSubtle, 1.0f);
    if (tab_ == Tab::Assets) {
        IconRenderer::draw_icon(gfx, "file_import", Rect(10.0f, panel_rect_.y + 8.0f, 17.0f, 17.0f), Theme::kTextSecondary);
        gfx.draw_text("ASSET LIBRARY", Rect(34.0f, panel_rect_.y + 2.0f, 130.0f, 30.0f),
                      Theme::kTextPrimary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
        const std::string location = root_path_.empty() ? "Choose a folder to browse its assets" : path_to_utf8(root_path_);
        gfx.push_clip(Rect(170.0f, panel_rect_.y + 3.0f, std::max(0.0f, browse_rect_.x - 180.0f), 28.0f));
        gfx.draw_text(location, Rect(174.0f, panel_rect_.y + 3.0f, std::max(0.0f, browse_rect_.x - 184.0f), 28.0f),
                      Theme::kTextMuted, Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);
        gfx.pop_clip();
        draw_button(gfx, browse_rect_, "Choose Folder", "file_open", 2, true);
        draw_button(gfx, refresh_rect_, "Refresh", "", 3, true);
        draw_button(gfx, add_rect_, "Add to Scene", "file_import", 4, can_import_selected());
    } else {
        IconRenderer::draw_icon(gfx, "action_render", Rect(10.0f, panel_rect_.y + 8.0f, 17.0f, 17.0f), Theme::kTextSecondary);
        gfx.draw_text("OUTPUT", Rect(34.0f, panel_rect_.y + 2.0f, 120.0f, 30.0f),
                      Theme::kTextPrimary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
        gfx.draw_text("Render and delivery", Rect(114.0f, panel_rect_.y + 2.0f, 180.0f, 30.0f),
                      Theme::kTextMuted, Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);
        render_sequence_rect_ = Rect(std::max(0.0f, width_ - 154.0f), panel_rect_.y + 4.0f, 146.0f, 24.0f);
        render_current_rect_ = Rect(std::max(0.0f, render_sequence_rect_.x - 148.0f), panel_rect_.y + 4.0f, 140.0f, 24.0f);
        draw_button(gfx, render_current_rect_, "Render Current", "action_render", 5, true);
        draw_button(gfx, render_sequence_rect_, "Render Sequence", "action_render", 6, true);
    }
}

void AssetLibraryDrawer::draw_assets(Graphics& gfx) {
    if (tree_rect_.height <= 0.0f) return;
    gfx.fill_rect(tree_rect_, Theme::kPanelBg);
    gfx.fill_rect(grid_rect_, Theme::kPanelBg);
    gfx.fill_rect(tree_heading_rect_, Color::from_hex(0x262729));
    gfx.fill_rect(grid_heading_rect_, Color::from_hex(0x262729));
    gfx.draw_text("FOLDERS", Rect(tree_heading_rect_.x + 11.0f, tree_heading_rect_.y, tree_heading_rect_.width - 18.0f, tree_heading_rect_.height),
                  Theme::kTextMuted, 10.0f, true, false, true, Theme::kUiFontFamily);
    const std::string folder_name = current_folder_ < folders_.size() ? folders_[current_folder_].name : "Files";
    gfx.draw_text(folder_name, Rect(grid_heading_rect_.x + 12.0f, grid_heading_rect_.y,
                                   std::max(0.0f, grid_heading_rect_.width - 24.0f), grid_heading_rect_.height),
                  Theme::kTextSecondary, Theme::kSmallFont, true, false, true, Theme::kUiFontFamily);
    gfx.draw_line(Point(tree_divider_x_, tree_rect_.y), Point(tree_divider_x_, tree_rect_.bottom()),
                  divider_hovered_ || dragging_divider_ ? Theme::kAccent : Theme::kBorderSubtle, 1.0f);
    draw_folder_tree(gfx);
    draw_file_grid(gfx);
}

void AssetLibraryDrawer::draw_folder_tree(Graphics& gfx) {
    gfx.push_clip(tree_rows_rect_);
    const auto& visible = visible_folders();
    const size_t first_row = static_cast<size_t>(std::max(0.0f, std::floor(tree_scroll_ / kTreeRowHeight)));
    const size_t last_row = std::min(visible.size(), static_cast<size_t>(std::ceil((tree_scroll_ + tree_rows_rect_.height) / kTreeRowHeight)) + 1);
    for (size_t row = first_row; row < last_row; ++row) {
        const size_t index = visible[row].first;
        const int depth = visible[row].second;
        const float y = tree_rows_rect_.y + row * kTreeRowHeight - tree_scroll_;
        if (y + kTreeRowHeight < tree_rows_rect_.y || y > tree_rows_rect_.bottom()) continue;
        const Rect row_rect(tree_rows_rect_.x, y, tree_rows_rect_.width, kTreeRowHeight);
        const bool selected = index == current_folder_;
        if (selected) gfx.fill_rect(Rect(row_rect.x + 3.0f, row_rect.y + 1.0f, row_rect.width - 6.0f, row_rect.height - 2.0f), Theme::kSelectionBg);
        const auto& node = folders_[index];
        if (!node.children.empty() || !node.children_loaded) {
            IconRenderer::draw_icon(gfx, node.expanded ? "ui_chevron_down" : "ui_chevron_right",
                Rect(7.0f + depth * kTreeIndent, y + 5.0f, 13.0f, 13.0f), Theme::kTextMuted);
        }
        const float icon_x = 25.0f + depth * kTreeIndent;
        IconRenderer::draw_icon(gfx, "outliner_collection", Rect(icon_x, y + 4.0f, 15.0f, 15.0f),
                                selected ? Theme::kTextPrimary : Theme::kTextSecondary);
        gfx.draw_text(node.name, Rect(icon_x + 22.0f, y + 1.0f, std::max(0.0f, row_rect.right() - (icon_x + 24.0f)), kTreeRowHeight - 2.0f),
                      selected ? Theme::kTextPrimary : Theme::kTextSecondary, Theme::kUiFont, selected, false, true, Theme::kUiFontFamily);
    }
    draw_scrollbar(gfx, tree_rows_rect_, visible.size() * kTreeRowHeight, tree_scroll_);
    gfx.pop_clip();
}

void AssetLibraryDrawer::draw_file_grid(Graphics& gfx) {
    gfx.push_clip(grid_content_rect_);
    const float left = grid_content_rect_.x + 10.0f;
    const float top = grid_content_rect_.y + 8.0f - grid_scroll_;
    for (size_t i = 0; i < files_.size(); ++i) {
        const int col = static_cast<int>(i) % grid_columns_;
        const int row = static_cast<int>(i) / grid_columns_;
        const float x = left + col * (kTileWidth + kTileGapX);
        const float y = top + row * tile_stride_y_;
        if (y + kTileHeight < grid_content_rect_.y || y > grid_content_rect_.bottom()) continue;
        const Rect tile(x, y, kTileWidth, kTileHeight);
        const bool selected = state_.asset_library_selected_path == path_to_utf8(files_[i].path);
        const bool hovered = hovered_file_ == static_cast<int>(i);
        gfx.fill_rounded_rect(tile, 3.0f, selected ? Theme::kSelectionBg : hovered ? Theme::kControlHover : Theme::kControlBg);
        gfx.draw_rounded_rect(tile, 3.0f, selected ? Theme::kBorderBright : Theme::kBorderSubtle, 1.0f);
        const Rect preview(tile.x + 6.0f, tile.y + 6.0f, tile.width - 12.0f, tile.height - 38.0f);
        gfx.fill_rounded_rect(preview, 2.0f, Theme::kPanelHeaderBg);
        IconRenderer::draw_icon(gfx, file_icon(files_[i].extension),
                                Rect(preview.x + (preview.width - 24.0f) * 0.5f, preview.y + (preview.height - 24.0f) * 0.5f, 24.0f, 24.0f),
                                selected ? Theme::kAccentText : Theme::kTextSecondary);
        gfx.draw_text(files_[i].name, Rect(tile.x + 6.0f, tile.bottom() - 30.0f, tile.width - 12.0f, 16.0f),
                      selected ? Theme::kTextPrimary : Theme::kTextSecondary, 10.5f, selected, false, true, Theme::kUiFontFamily);
        const std::string ext = files_[i].extension.empty() ? "" : files_[i].extension.substr(1);
        const std::string size_str = files_[i].size > 1048576 ? std::to_string(files_[i].size / 1048576) + " MB" :
                                     files_[i].size > 1024 ? std::to_string(files_[i].size / 1024) + " KB" :
                                     std::to_string(files_[i].size) + " B";
        gfx.draw_text(ext + (ext.empty() ? "" : " · ") + size_str, Rect(tile.x + 6.0f, tile.bottom() - 15.0f, tile.width - 12.0f, 13.0f),
                      Theme::kTextMuted, 9.0f, false, false, true, Theme::kUiFontFamily);
    }
    const int total_rows = (static_cast<int>(files_.size()) + grid_columns_ - 1) / grid_columns_;
    draw_scrollbar(gfx, grid_content_rect_, total_rows * tile_stride_y_ + 10.0f, grid_scroll_);
    gfx.pop_clip();
}

void AssetLibraryDrawer::draw_output(Graphics& gfx) {
    if (grid_rect_.height <= 0.0f) return;
    gfx.fill_rect(panel_rect_, Theme::kPanelBg);
    const float pad = 16.0f;
    const float card_w = std::min(260.0f, (width_ - pad * 3.0f) * 0.5f);
    const float card_h = 78.0f;
    const float y0 = panel_rect_.y + kHeaderHeight + pad;
    draw_output_card(gfx, Rect(pad, y0, card_w, card_h), "ACTIVE CAMERA",
                     state_.active_camera.empty() ? "None" : state_.active_camera,
                     "Focal Length: " + std::to_string(static_cast<int>(std::lround(state_.focal_length))) + "mm");
    draw_output_card(gfx, Rect(pad * 2.0f + card_w, y0, card_w, card_h), "OUTPUT FORMAT",
                     state_.render_output_exr ? "OpenEXR (16-bit Float)" : "PNG (8-bit sRGB)",
                     std::to_string(state_.render_output_width) + " x " + std::to_string(state_.render_output_height));
    draw_output_card(gfx, Rect(pad, y0 + card_h + pad, card_w, card_h), "FRAME RANGE",
                     state_.render_frame_mode == EditorState::RenderFrameMode::SceneRange ? "Scene Range: 1 - 250" :
                     "Custom: " + std::to_string(state_.render_custom_start) + " - " + std::to_string(state_.render_custom_end),
                     "Current: Frame " + std::to_string(state_.current_frame));
    draw_output_card(gfx, Rect(pad * 2.0f + card_w, y0 + card_h + pad, card_w, card_h), "RENDER DESTINATION",
                     state_.render_output_path.empty() ? "Default output directory" : state_.render_output_path,
                     "Production render pipeline active");
}

void AssetLibraryDrawer::draw_output_card(Graphics& gfx, Rect rect, const std::string& title,
                                          const std::string& value, const std::string& detail) {
    gfx.fill_rounded_rect(rect, 2.0f, Theme::kControlBg);
    gfx.draw_rounded_rect(rect, 2.0f, Theme::kBorderSubtle, 1.0f);
    gfx.draw_text(title, Rect(rect.x + 12.0f, rect.y + 7.0f, rect.width - 24.0f, 16.0f),
                  Theme::kTextMuted, 9.5f, true, false, true, Theme::kUiFontFamily);
    gfx.draw_text(value, Rect(rect.x + 12.0f, rect.y + 25.0f, rect.width - 24.0f, 25.0f),
                  Theme::kTextPrimary, Theme::kUiFont, true, false, true, Theme::kUiFontFamily);
    gfx.draw_text(detail, Rect(rect.x + 12.0f, rect.y + 51.0f, rect.width - 24.0f, 20.0f),
                  Theme::kTextMuted, 10.0f, false, false, true, Theme::kUiFontFamily);
}

void AssetLibraryDrawer::draw_scrollbar(Graphics& gfx, Rect viewport, float content_extent, float scroll) {
    if (content_extent <= viewport.height + 1.0f || viewport.height <= 0.0f) return;
    const float track_h = viewport.height;
    const float thumb_h = std::max(22.0f, track_h * track_h / content_extent);
    const float max_scroll = content_extent - track_h;
    const float thumb_y = viewport.y + (track_h - thumb_h) * (scroll / std::max(1.0f, max_scroll));
    const Rect track(viewport.right() - 5.0f, viewport.y + 2.0f, 3.0f, track_h - 4.0f);
    gfx.fill_rect(track, Theme::kScrollbarTrack);
    gfx.fill_rounded_rect(Rect(track.x, thumb_y, track.width, thumb_h), 2.0f, Theme::kScrollbarThumb);
}

void AssetLibraryDrawer::draw_button(Graphics& gfx, Rect rect, const std::string& label, const std::string& icon,
                                     int button_id, bool enabled) {
    const bool hovered = hovered_button_ == button_id && enabled;
    const Color fill = !enabled ? Theme::kPanelHeaderBg : hovered ? Theme::kControlHover : Theme::kControlBg;
    const Color text = enabled ? Theme::kTextSecondary : Theme::kTextMuted;
    gfx.fill_rounded_rect(rect, 2.0f, fill);
    gfx.draw_rounded_rect(rect, 2.0f, hovered ? Theme::kBorderBright : Theme::kBorderSubtle, 1.0f);
    if (!icon.empty()) {
        const float icon_size = label.empty() ? 14.0f : 13.0f;
        const float icon_x = label.empty() ? rect.x + (rect.width - icon_size) * 0.5f : rect.x + 7.0f;
        IconRenderer::draw_icon(gfx, icon, Rect(icon_x, rect.y + (rect.height - icon_size) * 0.5f, icon_size, icon_size), text);
    }
    if (!label.empty()) {
        const float left = icon.empty() ? 5.0f : 23.0f;
        gfx.draw_text(label, Rect(rect.x + left, rect.y, rect.width - left - 5.0f, rect.height),
                      enabled ? Theme::kTextPrimary : Theme::kTextMuted, 10.5f, true, false, true, Theme::kUiFontFamily);
    }
}

void AssetLibraryDrawer::draw_bottom_bar(Graphics& gfx) {
    gfx.fill_rect(bar_rect_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(bar_rect_.x, bar_rect_.y), Point(bar_rect_.right(), bar_rect_.y), Theme::kBorderSubtle, 1.0f);
    assets_tab_rect_ = Rect(7.0f, bar_rect_.y + 3.0f, 92.0f, 24.0f);
    output_tab_rect_ = Rect(101.0f, bar_rect_.y + 3.0f, 92.0f, 24.0f);
    console_tab_rect_ = Rect(195.0f, bar_rect_.y + 3.0f, 130.0f, 24.0f);
    draw_tab(gfx, assets_tab_rect_, "Assets", "file_import", tab_ == Tab::Assets && is_open_);
    draw_tab(gfx, output_tab_rect_, "Output", "action_render", tab_ == Tab::Output && is_open_);
    draw_tab(gfx, console_tab_rect_, "Python Console", "tool_select", tab_ == Tab::Console && is_open_);
    const float grip_x = width_ * 0.5f;
    for (int i = 0; i < 3; ++i) {
        gfx.fill_rounded_rect(Rect(grip_x - 14.0f + i * 10.0f, bar_rect_.y + 13.0f, 5.0f, 3.0f), 1.0f, Theme::kTextMuted);
    }
    gfx.draw_text(is_open_ ? "Drawer" : "Open asset, output & python drawer",
                  Rect(std::max(335.0f, width_ - 240.0f), bar_rect_.y, 230.0f, bar_rect_.height),
                  Theme::kTextMuted, 9.5f, false, false, true, Theme::kUiFontFamily);
}

void AssetLibraryDrawer::draw_tab(Graphics& gfx, Rect rect, const std::string& label, const std::string& icon, bool active) {
    const bool hovered = (label == "Assets" && hovered_button_ == 0) ||
                         (label == "Output" && hovered_button_ == 1) ||
                         (label == "Python Console" && hovered_button_ == 7);
    if (active || hovered) gfx.fill_rounded_rect(rect, 2.0f, active ? Theme::kSelectionBg : Theme::kControlHover);
    IconRenderer::draw_icon(gfx, icon, Rect(rect.x + 8.0f, rect.y + 5.0f, 14.0f, 14.0f),
                            active ? Theme::kAccentText : Theme::kTextSecondary);
    gfx.draw_text(label, Rect(rect.x + 28.0f, rect.y, rect.width - 34.0f, rect.height),
                  active ? Theme::kTextPrimary : Theme::kTextSecondary, 10.5f, active, false, true, Theme::kUiFontFamily);
}

} // namespace medusa
