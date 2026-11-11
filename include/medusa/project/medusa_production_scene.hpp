#pragma once

#include "zegfx/profiles/production/production_pipeline.h"
#include <string>

namespace medusa {

inline constexpr char kBuiltinCubeScenePath[] = "builtin://starter-cube";

bool is_builtin_production_scene(const std::string& path);
bool create_builtin_cube_scene(zegfx::ProductionScene& scene, std::string& error);
bool append_builtin_primitive_node(zegfx::ProductionScene& scene, const std::string& shape,
                                   const std::string& name, uint32_t& nodeIndex, std::string& error);
bool append_builtin_primitive_geometry(zegfx::ProductionScene& scene, const std::string& shape,
                                       uint32_t nodeIndex, std::string& error);

bool load_production_scene(const std::string& path, zegfx::ProductionScene& scene,
                           std::string& error);

} // namespace medusa
