#pragma once
#include <medusa/medusa_types.hpp>
#include <windows.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_3.h>
#include <dxgi1_4.h>
#include <d3d11_1.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <array>
#include <zegfx/zegfx.h>
#include <zegfx/profiles/production/production_pipeline.h>

namespace medusa {

using Microsoft::WRL::ComPtr;

class Graphics {
public:
    Graphics();
    ~Graphics();

    bool init(HWND hwnd, uint32_t width, uint32_t height);
    const std::string& last_init_error() const { return last_init_error_; }
    bool resize(uint32_t width, uint32_t height);
    bool resize_realtime_viewport(uint32_t width, uint32_t height);
    void set_realtime_interactive(bool active);
    void set_viewport_shading_mode(ViewportShadingMode mode);
    void set_selected_node(int nodeIndex, const std::string& activeTool = "tool_select") {
        if (selected_node_index_ != nodeIndex || active_tool_ != activeTool) {
            selected_node_index_ = nodeIndex;
            active_tool_ = activeTool;
            realtime_render_dirty_ = true;
        }
    }
    bool set_realtime_view(const zegfx::ProductionViewSettings& view);
    bool render_realtime_view();
    bool realtime_render_dirty() const { return realtime_render_dirty_; }
    bool realtime_view_available() const { return realtime_view_available_; }
    bool realtime_convergence_pending() const { return realtime_convergence_frames_remaining_ > 0; }
    void set_production_render_active(bool active) { production_render_active_ = active; }
    bool production_render_active() const { return production_render_active_; }
    bool resize_production_output(uint32_t width, uint32_t height);
    void destroy();

    void begin_draw();
    void end_draw();
    void present();

    void clear(Color color);
    void fill_rect(const Rect &r, Color color);
    void fill_polygon(const Point* points, size_t count, Color color);
    void draw_rect(const Rect &r, Color color, float stroke = 1.0f);
    void fill_rounded_rect(const Rect &r, float radius, Color color);
    void draw_rounded_rect(const Rect &r, float radius, Color color, float stroke = 1.0f);
    void draw_line(Point p1, Point p2, Color color, float stroke = 1.0f);
    void draw_text(const std::string &text, const Rect &r, Color color,
                   float font_size = 11.5f, bool bold = false,
                   bool center_h = false, bool center_v = true,
                   const std::string &font_family = "Segoe UI Variable Text");
    void draw_app_logo(const Rect& bounds);

    void push_clip(const Rect &r);
    void pop_clip();

    bool save_screenshot(const std::string &filepath);
    bool submit_production_scene(zegfx::ProductionScene scene, std::string& error);
    bool update_production_view(const zegfx::ProductionViewSettings& view, std::string& error);
    bool update_production_settings(const zegfx::ProductionRenderSettings& settings, std::string& error);
    bool set_production_node_state(uint32_t nodeIndex, bool visible, bool renderable, std::string& error);
    bool set_production_node_translation(uint32_t nodeIndex, const float position[3], std::string& error);
    bool set_production_node_light_energy(uint32_t nodeIndex, float energy, std::string& error);
    bool pick_production_node(const float rayOrigin[3], const float rayDirection[3],
                              uint32_t& nodeIndex, float& distance) const;
    bool set_production_node_rotation_scale(uint32_t nodeIndex, const float rotationOffset[3],
                                            const float scaleFactor[3],
                                            std::vector<zegfx::ProductionSceneNode>& updatedNodes,
                                            std::string& error);
    bool export_production_png(const std::string& path, std::string& error);
    bool export_production_exr(const std::string& path, std::string& error);
    void clear_production_scene();
    bool restart_production_render();
    void trace_production_frame();
    bool production_render_complete() const;
    zegfx::ProductionRenderProgress production_render_progress() const;
    bool has_production_scene() const { return production_scene_submitted_; }
    bool update_production_material(uint32_t materialIndex, const zegfx::ProductionMaterial& material);
    bool draw_realtime_viewport(const Rect& bounds, const Rect* overlayBounds = nullptr);
    bool draw_realtime_viewport(const Rect& bounds, const std::vector<Rect>& overlayBounds);
    bool draw_pathtracer_viewport(const Rect& bounds, bool denoiseActive,
                                  zegfx::ProductionAOV aov = zegfx::ProductionAOV::Beauty);
    void hide_realtime_viewport();
    void set_realtime_grid_enabled(bool enabled);
    void set_realtime_modal_overlay(bool enabled);
    void set_realtime_viewport_overlay_bounds(const Rect* overlayBounds);
    void set_realtime_viewport_overlay_bounds(const std::vector<Rect>& overlayBounds);

    ID2D1DeviceContext* get_context() const { return d2d_context_.Get(); }
    ID2D1Factory1* get_factory() const { return d2d_factory_.Get(); }
    IDWriteFactory* get_dwrite_factory() const { return dwrite_factory_.Get(); }

    zegfx::IProductionPipeline* get_production_pipeline() const { return production_pipeline_.get(); }

    float get_dpi_scale() const { return dpi_scale_; }
    // Borrowed from the graphics-owned cache; valid until destroy().
    ID2D1SolidColorBrush* get_brush(Color color);

private:
    IDWriteTextFormat* get_text_format(const std::string &font_family, float font_size, bool bold);
    bool initialize_realtime_backend(HWND hwnd, uint32_t width, uint32_t height);
    bool create_realtime_target(uint32_t width, uint32_t height);
    void destroy_realtime_postprocess();
    void mark_realtime_history_dirty();
    bool rebuild_backbuffer_targets();
    bool recreate_d2d_device_targets();
    void destroy_realtime_scene();
    bool build_realtime_scene(const zegfx::ProductionScene& scene, std::string& error);

    HWND hwnd_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t realtime_viewport_width_ = 0;
    uint32_t realtime_viewport_height_ = 0;
    uint32_t realtime_render_width_ = 0;
    uint32_t realtime_render_height_ = 0;
    bool production_render_active_ = false;
    bool realtime_interactive_ = false;
    float dpi_scale_ = 1.0f;

    ComPtr<ID2D1Factory1> d2d_factory_;
    ComPtr<IDWriteFactory> dwrite_factory_;
    ComPtr<ID3D11Device> d3d_device_;
    ComPtr<ID3D11DeviceContext> d3d_context_;
    ComPtr<ID3D11On12Device> d3d11on12_device_;
    ComPtr<IDXGISwapChain1> swapchain_;
    ComPtr<IDXGISwapChain3> swapchain3_;
    ComPtr<ID2D1Bitmap1> realtime_preview_bitmap_;
    ComPtr<ID2D1Bitmap1> pathtracer_bitmap_;
    std::vector<uint8_t> pathtracer_rgba_buffer_;
    uint32_t pathtracer_width_ = 0;
    uint32_t pathtracer_height_ = 0;
    uint32_t last_denoised_sample_ = 0;
    ComPtr<ID3D12Device> d3d12_device_;
    ComPtr<ID3D12CommandQueue> d3d12_queue_;
    ComPtr<ID3D12Resource> realtime_target_;
    ComPtr<ID3D12DescriptorHeap> realtime_rtv_heap_;
    ComPtr<ID3D11Resource> realtime_wrapped_target_;
    std::array<ComPtr<ID3D11Resource>, 2> wrapped_backbuffers_{};
    std::array<ComPtr<ID2D1Bitmap1>, 2> backbuffer_bitmaps_{};
    UINT current_backbuffer_index_ = 0;
    bool realtime_target_acquired_ = false;
    bool backbuffer_acquired_ = false;
    bool d2d_frame_active_ = false;
    HWND realtime_host_window_ = nullptr;
    RECT realtime_child_bounds_{};
    bool realtime_child_bounds_valid_ = false;
    RECT realtime_child_overlay_bounds_{};
    bool realtime_child_overlay_valid_ = false;
    std::vector<RECT> realtime_child_overlay_rects_;
    Rect realtime_viewport_logical_bounds_{};
    bool realtime_grid_enabled_ = true;
    bool realtime_modal_overlay_active_ = false;
    ViewportShadingMode viewport_shading_mode_ = ViewportShadingMode::Solid;
    ComPtr<ID2D1Device> d2d_device_;
    ComPtr<ID2D1DeviceContext> d2d_context_;
    ComPtr<ID2D1Bitmap1> target_bitmap_;
    ComPtr<ID2D1Bitmap1> app_logo_;
    bool app_logo_loaded_ = false;
    bool com_initialized_ = false;

    std::unique_ptr<zegfx::IProductionPipeline> production_pipeline_;
    std::unique_ptr<zegfx::Renderer> realtime_renderer_;
    void* realtime_hdr_pipeline_ = nullptr;
    void* realtime_depth_normal_ = nullptr;
    void* realtime_ao_ = nullptr;
    void* realtime_tone_map_ = nullptr;
    void* realtime_bloom_ = nullptr;
    void* realtime_color_grade_ = nullptr;
    uint32_t realtime_convergence_frames_remaining_ = 0;
    zegfx::Camera3DHandle realtime_camera_{};
    std::vector<zegfx::ProductionSceneNode> realtime_nodes_;
    struct RealtimeWireEdge {
        std::array<float, 3> start{};
        std::array<float, 3> end{};
    };
    struct RealtimeDrawItem {
        uint32_t node_index = 0;
        zegfx::RenderMeshHandle mesh{};
        zegfx::RenderMaterialHandle material{};
        std::vector<RealtimeWireEdge> wire_edges;
    };
    std::vector<RealtimeDrawItem> realtime_draw_items_;
    std::vector<zegfx::RenderMaterialHandle> realtime_materials_;
    std::vector<zegfx::TextureHandle> realtime_textures_;
    std::vector<zegfx::LightRenderCommand> realtime_local_lights_;
    zegfx::ProductionViewSettings realtime_view_{};
    uint64_t realtime_scene_generation_ = 0;
    bool realtime_view_available_ = false;
    bool realtime_render_dirty_ = true;
    bool production_scene_submitted_ = false;
    std::string last_init_error_;

    std::unordered_map<uint32_t, ComPtr<ID2D1SolidColorBrush>> brush_cache_;
    std::unordered_map<std::string, ComPtr<IDWriteTextFormat>> font_cache_;

    int selected_node_index_ = -1;
    std::string active_tool_ = "tool_select";
    void draw_realtime_selection_and_gizmo(zegfx::GraphicsBackend* backend,
                                           const zegfx::ProductionSceneNode& node,
                                           const std::string& tool);
};

} // namespace medusa
