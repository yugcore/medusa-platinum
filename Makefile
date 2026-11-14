CXX := g++
CC := gcc
CXXFLAGS := -std=c++17 -O2 -pthread -Wall -Wextra -Wno-unused-parameter -DWITH_DX12_BACKEND -DZEGFX_WITH_SHADER_TOOLCHAIN -DZEGFX_WITH_ASSIMP=0 -I"include" -I"." -I"third_party" -I"third_party/ufbx" -I"../ZeGFX/include" -I"../ZeGFX/src" -I"../ZeGFX/third_party/tinyexr" -I"../ZeGFX/third_party/tinyexr/deps/miniz" -I"../BluemanEngineEditor/include" -I"../BluemanEngineEditor" -finput-charset=UTF-8 -fexec-charset=UTF-8

BUILD_DIR := build
BIN_DIR := bin

SRCS := src/main.cpp \
        src/app/medusa_app.cpp \
        src/platform/medusa_window.cpp \
        src/graphics/medusa_graphics.cpp \
        src/ui/medusa_icons.cpp \
        src/core/medusa_screen.cpp \
        src/ui/medusa_widgets.cpp \
        src/ui/medusa_popup_menu.cpp \
        src/ui/dcc_transform_triplet.cpp \
        src/editor/medusa_editor.cpp \
        src/editor/gizmo/transform_gizmo.cpp \
        src/editor/gizmo/orientation_gizmo.cpp \
        src/editor/drawers/asset_library_drawer.cpp \
        src/editor/dialogs/render_setup_dialog.cpp \
        src/editor/regions/editor_topbar.cpp \
        src/editor/regions/editor_toolshelf.cpp \
        src/editor/regions/editor_outliner.cpp \
        src/editor/regions/editor_properties.cpp \
        src/editor/regions/editor_timeline.cpp \
        src/editor/regions/editor_viewport.cpp \
        src/project/medusa_project.cpp \
        src/project/medusa_production_scene.cpp \
        src/usd/usd_hydra_stage.cpp \
        src/scripting/medusa_python_engine.cpp \
        src/editor/drawers/medusa_python_console.cpp \
        $(ZEGFX_RENDERER_SRCS) \
        ../ZeGFX/src/render/pipelines/production/production_pipeline.cpp \
        ../ZeGFX/src/render/pipelines/production/progressive_accumulator.cpp \
        ../ZeGFX/src/render/pipelines/production/aov_manager.cpp \
        ../ZeGFX/src/render/pipelines/production/exr_writer.cpp \
        ../ZeGFX/src/render/pipelines/production/denoiser.cpp \
        ../ZeGFX/src/render/pipelines/production/embree_accel.cpp \
        ../ZeGFX/src/render/lighting/light_tree.cpp \
        ../ZeGFX/src/render/lighting/adaptive_sampler.cpp \
        ../ZeGFX/src/render/rhi/rhi_memory.cpp \
        ../ZeGFX/src/diagnostics/diagnostics.cpp \
        ../BluemanEngineEditor/src/assets/import/asset_importer.cpp \
        ../BluemanEngineEditor/src/assets/import/gltf_importer.cpp

# Medusa's interactive viewport links ZeGFX's real Direct3D 12 Renderer API.
# ProductionDCC remains available below for frame/sequence output.
ZEGFX_RENDERER_SRCS := \
        ../ZeGFX/src/api/zegfx.cpp \
        ../ZeGFX/src/scene/runtime_scene.cpp \
        ../ZeGFX/src/diagnostics/profiling.cpp \
        ../ZeGFX/src/render/backends/backend_factory.cpp \
        ../ZeGFX/src/render/backends/headless/headless_backend.cpp \
        ../ZeGFX/src/platform/windows/platform_win32.cpp \
        ../ZeGFX/src/render/backends/dx12/resources/dx12_upload_ring_buffer.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_backend_core.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_backend_2d.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_backend_features.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_rhi.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_backend_resources.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_backend_assets.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_backend_import.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_backend_models.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_backend_rays.cpp \
        ../ZeGFX/src/render/backends/dx12/dx12_backend_effects.cpp \
        ../ZeGFX/src/render/passes/frame_coordinator.cpp \
        ../ZeGFX/src/render/passes/water_pass.cpp \
        ../ZeGFX/src/render/passes/visual_pass.cpp \
        ../ZeGFX/src/render/passes/render_subsystems.cpp \
        ../ZeGFX/src/render/passes/world_pass.cpp \
        ../ZeGFX/src/render/passes/render_graph.cpp \
        ../ZeGFX/src/render/backends/dx12/resources/dx12_dxr.cpp \
        ../ZeGFX/src/render/shaders/shader_source.cpp \
        ../ZeGFX/src/render/shaders/shader_compiler.cpp \
        ../ZeGFX/src/render/shaders/shader_artifact.cpp \
        ../ZeGFX/src/render/shaders/shader_variant.cpp \
        ../ZeGFX/src/render/backends/dx12/resources/dx12_pipeline.cpp \
        ../ZeGFX/src/assets/registry/asset_registry.cpp \
        ../ZeGFX/src/assets/registry/pipeline/asset_registry.cpp \
        ../ZeGFX/src/assets/registry/asset_manager.cpp \
        ../ZeGFX/src/scene/scene.cpp \
        ../ZeGFX/src/render/frontend/render_world.cpp \
        ../ZeGFX/src/render/graph/render_graph.cpp \
        ../ZeGFX/src/render/frontend/renderer_spine.cpp \
        ../ZeGFX/src/render/backends/dx12/resources/transient_pool.cpp \
        ../ZeGFX/src/render/post/post_processing.cpp \
        ../ZeGFX/src/packaging/streaming_manager.cpp \
        ../ZeGFX/src/render/backends/dx12/features/view_manager.cpp \
        ../ZeGFX/src/render/backends/dx12/features/gpu_scene.cpp \
        ../ZeGFX/src/render/backends/dx12/features/light_grid_manager.cpp \
        ../ZeGFX/src/render/backends/dx12/features/gi_volume.cpp \
        ../ZeGFX/src/render/backends/dx12/features/reflection_pipeline.cpp \
        ../ZeGFX/src/render/backends/dx12/features/volumetrics.cpp \
        ../ZeGFX/third_party/bc7enc_rdo/bc7enc.cpp \
        ../ZeGFX/third_party/bc7enc_rdo/rgbcx.cpp \
        ../BluemanEngineEditor/src/assets/import/ufbx_importer.cpp \
        ../BluemanEngineEditor/src/assets/import/assimp_importer.cpp \
        ../BluemanEngineEditor/src/assets/import/vox_importer.cpp \
        ../BluemanEngineEditor/src/assets/pipeline/zegfx_graph.cpp \
        ../BluemanEngineEditor/src/assets/pipeline/zegfx_vox_translator.cpp \
        ../BluemanEngineEditor/src/assets/pipeline/zegfx_ufbx_translator.cpp \
        ../BluemanEngineEditor/src/assets/pipeline/zegfx_pipeline.cpp \
        ../BluemanEngineEditor/src/assets/pipeline/zegfx_factory.cpp \
        third_party/ufbx/ufbx.c

SRCS += $(ZEGFX_RENDERER_SRCS)

ZE_PHYSICS_DIR := ../out/build/windows-x64/release/zephysics-source
ZE_PHYSICS_LIBS := $(ZE_PHYSICS_DIR)/libZePhysics.a $(ZE_PHYSICS_DIR)/libJolt.a

LIBS := -ld2d1 -ldwrite -ldwmapi -ldxgi -ld3d11 -ld3d12 -ld3dcompiler -ldxguid -lwinmm -lole32 -luuid -lshell32 -lcomdlg32 -lgdi32 -luser32 -lwindowscodecs C:/msys64/ucrt64/lib/libembree4.dll.a

TARGET := Medusa_Platinum_ZeGFX.exe

.PHONY: all clean run production

all: production

production: $(ZE_PHYSICS_LIBS)
	$(CXX) $(CXXFLAGS) -mwindows -static -static-libgcc -static-libstdc++ $(SRCS) $(ZE_PHYSICS_LIBS) -o $(TARGET) $(LIBS)
	@powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(dir $(TARGET))resources/ZeGFX/shaders' | Out-Null; Copy-Item -Path '..\ZeGFX\shaders\*' -Destination '$(dir $(TARGET))resources/ZeGFX/shaders' -Recurse -Force | Out-Null"

clean:
	@if exist "$(BIN_DIR)\medusa_platinum.exe" del /Q /F "$(BIN_DIR)\medusa_platinum.exe"
	@if exist "$(BUILD_DIR)" rd /S /Q "$(BUILD_DIR)"

run: all
	@start $(TARGET)
