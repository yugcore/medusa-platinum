#pragma once
#include <medusa/core/medusa_types.hpp>
#include <medusa/graphics/medusa_graphics.hpp>
#include <medusa/editor/editor_state.hpp>
#include <filesystem>
#include <chrono>
#include <vector>
#include <string>

namespace medusa {

class AssetLibraryDrawer {
public:
    explicit AssetLibraryDrawer(EditorState& state);

    void layout(float width, float height);
    void draw(Graphics& gfx);
    bool handle_mouse_down(const MouseEvent& e);
    bool handle_mouse_move(const MouseEvent& e);
    bool handle_mouse_up(const MouseEvent& e);
    bool handle_mouse_wheel(float delta, Point p);

    void set_root(const std::string& utf8_path);
    bool animating() const;
    bool tick_animation();
    CursorType cursor_type() const;

private:
    enum class Tab { Assets, Output };
    struct FolderNode {
        std::filesystem::path path;
        std::string name;
        size_t parent = static_cast<size_t>(-1);
        std::vector<size_t> children;
        bool expanded = false;
        bool children_loaded = false;
    };
    struct FileEntry {
        std::filesystem::path path;
        std::string name;
        std::string extension;
        uintmax_t size = 0;
    };

    static constexpr float kBarHeight = 30.0f;
    static constexpr float kHeaderHeight = 34.0f;
    static constexpr float kPaneHeaderHeight = 25.0f;
    static constexpr float kFooterHeight = 22.0f;
    static constexpr float kTreeRowHeight = 23.0f;
    static constexpr float kTreeIndent = 15.0f;
    static constexpr float kSplitterHit = 5.0f;
    static constexpr float kTileWidth = 116.0f;
    static constexpr float kTileHeight = 112.0f;
    static constexpr float kTileGapX = 9.0f;
    static constexpr float kTileGapY = 8.0f;

    static std::string path_to_utf8(const std::filesystem::path& path);
    static std::string lowercase(std::string value);
    static bool case_insensitive_less(const std::string& a, const std::string& b);
    static std::string file_icon(const std::string& extension);
    bool can_import_selected() const;
    void select_tab(Tab tab);
    void begin_animation(float target);
    void update_animation();
    void refresh();
    void load_children(size_t index);
    void load_files(const std::filesystem::path& directory);
    const std::vector<std::pair<size_t, int>>& visible_folders() const;
    void update_grid_metrics();
    void clamp_scrolls();
    int file_at(Point p) const;
    int button_at(Point p) const;
    void draw_header(Graphics& gfx);
    void draw_assets(Graphics& gfx);
    void draw_folder_tree(Graphics& gfx);
    void draw_file_grid(Graphics& gfx);
    void draw_output(Graphics& gfx);
    void draw_output_card(Graphics& gfx, Rect rect, const std::string& title,
                          const std::string& value, const std::string& detail);
    void draw_scrollbar(Graphics& gfx, Rect viewport, float content_extent, float scroll);
    void draw_button(Graphics& gfx, Rect rect, const std::string& label, const std::string& icon,
                     int button_id, bool enabled);
    void draw_bottom_bar(Graphics& gfx);
    void draw_tab(Graphics& gfx, Rect rect, const std::string& label, const std::string& icon, bool active);

    EditorState& state_;
    Tab tab_ = Tab::Assets;
    bool is_open_ = false;
    bool animating_ = false;
    float progress_ = 0.0f;
    float animation_start_progress_ = 0.0f;
    float animation_target_ = 0.0f;
    std::chrono::steady_clock::time_point animation_start_time_{};
    float width_ = 0.0f;
    float height_ = 0.0f;
    float target_height_ = 0.0f;
    float visible_height_ = 0.0f;
    float tree_ratio_ = 0.30f;
    float tree_width_ = 0.0f;
    float tree_divider_x_ = 0.0f;
    float tree_scroll_ = 0.0f;
    float grid_scroll_ = 0.0f;
    float tile_stride_y_ = kTileHeight + kTileGapY;
    int grid_columns_ = 1;
    int hovered_button_ = -1;
    int hovered_file_ = -1;
    int last_clicked_file_ = -1;
    std::chrono::steady_clock::time_point last_file_click_{};
    bool pressed_ = false;
    bool dragging_divider_ = false;
    bool divider_hovered_ = false;
    std::filesystem::path root_path_;
    std::vector<FolderNode> folders_;
    std::vector<FileEntry> files_;
    mutable bool flattened_folders_dirty_ = true;
    mutable std::vector<std::pair<size_t, int>> flattened_folders_;
    size_t current_folder_ = 0;
    Rect bar_rect_, panel_rect_, header_rect_;
    Rect assets_tab_rect_, output_tab_rect_;
    Rect browse_rect_, refresh_rect_, add_rect_;
    Rect render_current_rect_, render_sequence_rect_;
    Rect tree_rect_, grid_rect_, tree_heading_rect_, grid_heading_rect_;
    Rect tree_rows_rect_, grid_content_rect_;
};

} // namespace medusa
