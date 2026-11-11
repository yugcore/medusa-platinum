#pragma once
#include <medusa/medusa_types.hpp>
#include <medusa/medusa_graphics.hpp>
#include <medusa/medusa_editor.hpp>
#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <zegfx/profiles/production/production_pipeline.h>

namespace medusa {

class Window {
public:
    Window();
    ~Window();

    bool create(const std::wstring &title, int width = 1600, int height = 900);
    void show(int cmd_show = SW_SHOW);
    void request_redraw();
    void render_frame();
    void trace_production_frame();
    bool load_production_asset(const std::string& path, std::string& error,
                               const std::string& stableAssetId = {});
    void set_screenshot_target(const std::string &path) { screenshot_path_ = path; }

    HWND get_hwnd() const { return hwnd_; }
    Graphics& get_graphics() { return gfx_; }
    MedusaEditor* get_editor() { return editor_.get(); }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle_message(UINT msg, WPARAM wParam, LPARAM lParam);

    void on_paint();
    void on_resize(uint32_t width, uint32_t height);
    void apply_resize(uint32_t width, uint32_t height);
    void start_production_timer();
    void dispatch_editor_command();
    bool sync_production_controls();
    bool add_scene_object(const std::string& objectType, std::string& error);
    bool build_scene_with_procedural_objects(zegfx::ProductionScene& scene, const EditorState& state,
                                             size_t firstExtraNode, std::string& error) const;
    bool choose_render_output(std::wstring& path, const wchar_t* title, bool confirmOverwrite = true);
    void restore_interactive_render_size();
    bool choose_project_path(bool save, std::wstring& path, const wchar_t* title);
    bool save_project(bool saveAs);
    bool confirm_project_change();
    bool open_project(const std::string& path, std::string& error);
    bool apply_project_state(EditorState candidate, std::string& error, bool establishSavedBaseline = true);
    bool restore_project_snapshot(const std::string& snapshot, std::string& error);
    bool apply_project_node_overrides(const std::vector<zegfx::ProductionSceneNode>& imported,
                                      const std::vector<zegfx::ProductionSceneNode>& saved,
                                      const std::vector<std::string>& importedIds,
                                      const std::vector<std::string>& savedIds,
                                      std::string& error);
    void update_project_dirty();
    void begin_history_transaction();
    void commit_history_transaction();
    void clear_history();
    void sync_history_flags();
    bool apply_history_step(bool redo);
    std::string recovery_file_path() const;
    void maybe_restore_recovery();
    void save_recovery_snapshot();
    void set_project_title();
    void on_production_frame_complete();
    void start_animation_timer();
    void on_mouse_move(int x, int y, WPARAM wParam);
    void on_mouse_down(MouseButton btn, int x, int y, WPARAM wParam);
    void on_mouse_up(MouseButton btn, int x, int y, WPARAM wParam);
    void on_mouse_wheel(float delta, int x, int y, WPARAM wParam);
    void on_mouse_leave();

    HWND hwnd_ = nullptr;
    Graphics gfx_;
    std::unique_ptr<MedusaEditor> editor_;
    bool mouse_tracking_ = false;
    bool is_dragging_ = false;
    bool defer_project_dirty_ = false;
    std::chrono::steady_clock::time_point last_transform_submit_{};
    bool is_sizing_ = false;
    bool resize_pending_ = false;
    bool production_timer_active_ = false;
    bool production_timer_paused_ = false;
    bool sequence_render_active_ = false;
    bool export_current_on_complete_ = false;
    bool current_export_exr_ = false;
    bool sequence_export_exr_ = false;
    bool sequence_output_size_active_ = false;
    bool animation_timer_active_ = false;
    UINT animation_timer_interval_ = 0;
    bool raw_mouse_registered_ = false;
    bool free_look_active_ = false;
    uint32_t free_camera_keys_ = 0;
    int64_t free_camera_mouse_dx_ = 0;
    int64_t free_camera_mouse_dy_ = 0;
    std::chrono::steady_clock::time_point free_camera_last_tick_{};
    bool render_view_valid_ = false;
    bool render_settings_valid_ = false;
    zegfx::ProductionViewSettings last_render_view_{};
    zegfx::ProductionRenderSettings last_render_settings_{};
    std::wstring sequence_prefix_;
    std::wstring current_export_path_;
    int sequence_frame_ = 1;
    int sequence_end_frame_ = 1;
    uint32_t client_width_ = 0;
    uint32_t client_height_ = 0;
    std::string screenshot_path_;
    std::string saved_project_snapshot_;
    struct HistoryEntry { std::string before; std::string after; size_t bytes = 0; };
    std::deque<HistoryEntry> history_;
    size_t history_cursor_ = 0;
    size_t history_bytes_ = 0;
    std::string interaction_snapshot_;
    std::string interaction_project_id_;
    bool suppress_history_commit_ = false;
    bool recovery_timer_active_ = false;
    bool discard_changes_confirmed_ = false;
};

} // namespace medusa
