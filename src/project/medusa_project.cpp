#include <medusa/medusa_project.hpp>
#include <medusa/medusa_production_scene.hpp>

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace medusa {
namespace {

constexpr uint64_t kMaximumProjectItems = 1000000;

bool expect(std::istream& in, const char* expected) {
    std::string token;
    return static_cast<bool>(in >> token) && token == expected;
}

bool finite_values(std::initializer_list<float> values) {
    return std::all_of(values.begin(), values.end(), [](float value) {
        return std::isfinite(value);
    });
}

std::string migrated_asset_id(const std::string& path) {
    const std::string normalized = std::filesystem::u8path(path).lexically_normal().u8string();
    uint64_t first = 1469598103934665603ull;
    uint64_t second = 1099511628211ull ^ 0x9e3779b97f4a7c15ull;
    for (unsigned char value : normalized) {
        first = (first ^ value) * 1099511628211ull;
        second ^= static_cast<uint64_t>(value) + 0x9e3779b97f4a7c15ull + (second << 6) + (second >> 2);
    }
    std::ostringstream id;
    id << "legacy-asset-" << std::hex << first << second;
    return id.str();
}

std::string path_utf8(const std::filesystem::path& path) {
    return path.u8string();
}

bool write_atomic_file(const std::filesystem::path& target, const std::string& contents,
                       std::string& error, const char* description) {
    std::error_code ec;
    if (!target.parent_path().empty()) std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) { error = std::string("Could not create the ") + description + " folder: " + ec.message(); return false; }
    const std::filesystem::path temporary = target.wstring() + L".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file) { error = std::string("Could not write the ") + description + " file."; return false; }
        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        file.flush();
        if (!file) {
            error = std::string("The ") + description + " file could not be written completely.";
            file.close();
            std::filesystem::remove(temporary, ec);
            return false;
        }
    }
    if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = std::string("Could not finish writing the ") + description + " file (Windows error " +
            std::to_string(GetLastError()) + ").";
        std::filesystem::remove(temporary, ec);
        return false;
    }
    error.clear();
    return true;
}

void write_state(std::ostream& out, const EditorState& state, const std::string& assetPath) {
    out << "MEDUSA_PLATINUM 10\n";
    out << "asset " << std::quoted(assetPath) << '\n';
    out << "scene " << std::quoted(state.scene_name) << '\n';
    out << "document " << std::quoted(state.project_id) << ' ' << std::quoted(state.scene_id) << '\n';
    out << "assets " << state.asset_references.size() << '\n';
    for (size_t i = 0; i < state.asset_references.size(); ++i) {
        const auto& asset = state.asset_references[i];
        const std::string& storedPath = (i == 0 && !assetPath.empty()) ? assetPath : asset.path;
        out << "asset_ref " << std::quoted(asset.id) << ' ' << std::quoted(storedPath) << ' '
            << std::quoted(asset.display_name) << ' ' << asset.source_size << ' '
            << asset.source_modified_ticks << '\n';
    }
    out << "materials " << state.material_references.size() << '\n';
    for (const auto& material : state.material_references) {
        out << "material_ref " << std::quoted(material.id) << ' '
            << std::quoted(material.asset_id) << ' ' << material.source_index << '\n';
    }
    out << "workspace " << std::quoted(state.active_workspace) << '\n';
    out << "active_camera " << std::quoted(state.active_camera) << '\n';
    out << "tool " << std::quoted(state.active_tool) << '\n';
    out << "camera_pilot " << state.camera_pilot_active << '\n';
    out << "display " << state.show_grid << ' ' << state.show_safe_frame << ' '
        << state.show_camera_trail << '\n';
    out << "viewport_shading " << static_cast<unsigned int>(state.viewport_shading) << '\n';
    out << "tool_settings " << state.snap_enabled << '\n';
    out << "layout " << state.ui_toolshelf_width << ' ' << state.ui_right_deck_width << ' '
        << state.ui_outliner_ratio << ' ' << state.ui_timeline_height << '\n';
    out << "timeline " << state.current_frame << ' ' << state.start_frame << ' '
        << state.end_frame << ' ' << state.total_frames << ' ' << state.is_autokey << '\n';
    out << "playback_fps " << state.playback_fps << '\n';
    out << "view " << state.viewport_navigation_active;
    for (float value : state.viewport_position) out << ' ' << value;
    for (float value : state.viewport_target) out << ' ' << value;
    for (float value : state.viewport_rotation) out << ' ' << value;
    out << ' ' << state.viewport_distance << '\n';
    out << "camera " << state.pos_x << ' ' << state.pos_y << ' ' << state.pos_z << ' '
        << state.rot_x << ' ' << state.rot_y << ' ' << state.rot_z << ' '
        << state.scale_x << ' ' << state.scale_y << ' ' << state.scale_z << ' '
        << state.focus_distance << ' ' << state.focal_length << ' '
        << state.camera_position[0] << ' ' << state.camera_position[1] << ' ' << state.camera_position[2] << ' '
        << state.camera_rotation[0] << ' ' << state.camera_rotation[1] << ' ' << state.camera_rotation[2] << ' '
        << state.camera_focal_length << '\n';
    out << "light " << state.key_light_x << ' ' << state.key_light_y << ' '
        << state.key_light_z << ' ' << state.key_light_energy << '\n';
    out << "render " << state.max_samples_per_pixel << ' ' << state.firefly_clamp << '\n';
    out << "render_output " << state.render_output_width << ' ' << state.render_output_height << ' '
        << state.render_output_exr << ' ' << static_cast<int>(state.render_frame_mode) << ' '
        << state.render_custom_start << ' ' << state.render_custom_end << ' '
        << std::quoted(state.render_output_path) << '\n';
    out << "selection " << state.active_camera_node << ' ' << state.key_light_node << ' '
        << state.selected_scene_node << '\n';
    out << "nodes " << state.scene_nodes.size() << '\n';
    for (size_t nodeIndex = 0; nodeIndex < state.scene_nodes.size(); ++nodeIndex) {
        const auto& node = state.scene_nodes[nodeIndex];
        const std::string nodeId = nodeIndex < state.scene_node_ids.size()
            ? state.scene_node_ids[nodeIndex] : std::string();
        const std::string assetId = nodeIndex < state.scene_node_asset_ids.size()
            ? state.scene_node_asset_ids[nodeIndex] : std::string();
        out << "node " << std::quoted(nodeId) << ' ' << std::quoted(assetId) << ' '
            << std::quoted(node.name) << ' ' << node.parentIndex << ' '
            << static_cast<unsigned int>(node.type) << ' ' << node.visible << ' '
            << node.locked << ' ' << node.renderable << ' ' << node.expanded;
        for (float value : node.rotationOffset) out << ' ' << value;
        for (float value : node.scaleFactor) out << ' ' << value;
        for (float value : node.worldTransform) out << ' ' << value;
        out << ' ' << static_cast<unsigned int>(node.lightType);
        out << ' ' << node.areaLight;
        for (float value : node.lightColor) out << ' ' << value;
        out << ' ' << node.lightEnergy << ' ' << node.lightSpotAngleDegrees << ' ' << node.lightAreaSize;
        out << '\n';
    }
    out << "keyframes " << state.keyframes.size() << '\n';
    for (const auto& key : state.keyframes) {
        out << "keyframe " << key.frame;
        for (float value : key.camera) out << ' ' << value;
        for (float value : key.light) out << ' ' << value;
        out << '\n';
    }
    out << "node_keyframes " << state.node_keyframes.size() << '\n';
    for (const auto& key : state.node_keyframes) {
        out << "node_keyframe " << key.node_index << ' ' << key.frame;
        out << ' ' << std::quoted(key.node_id);
        for (float value : key.position) out << ' ' << value;
        for (float value : key.rotation) out << ' ' << value;
        for (float value : key.scale) out << ' ' << value;
        out << '\n';
    }
}

bool read_bool(std::istream& in, bool& value) {
    int encoded = -1;
    if (!(in >> encoded) || (encoded != 0 && encoded != 1)) return false;
    value = encoded != 0;
    return true;
}

bool read_project(std::istream& in, EditorState& state, std::string& assetPath, std::string& error) {
    int version = 0;
    if (!expect(in, "MEDUSA_PLATINUM") || !(in >> version) || (version < 1 || version > 10)) {
        error = "This is not a supported Medusa Platinum project file.";
        return false;
    }

    if (!expect(in, "asset") || !(in >> std::quoted(assetPath)) ||
        !expect(in, "scene") || !(in >> std::quoted(state.scene_name))) {
        error = "The project file is incomplete or malformed.";
        return false;
    }
    if (version >= 4) {
        uint64_t assetCount = 0, materialCount = 0;
        if (!expect(in, "document") || !(in >> std::quoted(state.project_id) >> std::quoted(state.scene_id)) ||
            !expect(in, "assets") || !(in >> assetCount) || assetCount > kMaximumProjectItems) {
            error = "The project document identity or asset manifest is malformed.";
            return false;
        }
        state.asset_references.clear();
        state.asset_references.reserve(static_cast<size_t>(assetCount));
        for (uint64_t i = 0; i < assetCount; ++i) {
            ProjectAssetReference asset;
            if (!expect(in, "asset_ref") || !(in >> std::quoted(asset.id) >> std::quoted(asset.path) >>
                    std::quoted(asset.display_name) >> asset.source_size >> asset.source_modified_ticks)) {
                error = "A project asset reference is malformed.";
                return false;
            }
            state.asset_references.push_back(std::move(asset));
        }
        if (!expect(in, "materials") || !(in >> materialCount) || materialCount > kMaximumProjectItems) {
            error = "The project material manifest is malformed or too large.";
            return false;
        }
        state.material_references.clear();
        state.material_references.reserve(static_cast<size_t>(materialCount));
        for (uint64_t i = 0; i < materialCount; ++i) {
            ProjectMaterialReference material;
            if (!expect(in, "material_ref") || !(in >> std::quoted(material.id) >>
                    std::quoted(material.asset_id) >> material.source_index)) {
                error = "A project material reference is malformed.";
                return false;
            }
            state.material_references.push_back(std::move(material));
        }
    }
    if (!expect(in, "workspace") || !(in >> std::quoted(state.active_workspace)) ||
        !expect(in, "active_camera") || !(in >> std::quoted(state.active_camera)) ||
        !expect(in, "tool") || !(in >> std::quoted(state.active_tool))) {
        error = "The project file is incomplete or malformed.";
        return false;
    }
    if (version >= 5) {
        if (!expect(in, "camera_pilot") || !read_bool(in, state.camera_pilot_active)) {
            error = "The project camera pilot setting is incomplete or malformed.";
            return false;
        }
    } else if (state.active_tool == "tool_camera_pilot") {
        state.active_tool = "tool_move";
        state.camera_pilot_active = true;
    }
    if (!expect(in, "display") || !read_bool(in, state.show_grid) || !read_bool(in, state.show_safe_frame) ||
        (version >= 6 && !read_bool(in, state.show_camera_trail))) {
        error = "The project file is incomplete or malformed.";
        return false;
    }
    if (version >= 10) {
        unsigned int shadingMode = 0;
        if (!expect(in, "viewport_shading") || !(in >> shadingMode) || shadingMode > 2) {
            error = "The project viewport shading mode is incomplete or invalid.";
            return false;
        }
        state.viewport_shading = static_cast<ViewportShadingMode>(shadingMode);
    }

    // Older projects use the previous workspace names. Keep them opening into
    // the closest equivalent stage while preserving the single shared scene.
    if (state.active_workspace == "Staging") state.active_workspace = "Scene";
    else if (state.active_workspace == "Lighting") state.active_workspace = "Production";
    else if (state.active_workspace == "Animation") state.active_workspace = "Shot";
    else if (state.active_workspace != "Scene" && state.active_workspace != "Shot" &&
             state.active_workspace != "Production") state.active_workspace = "Scene";

    if (version >= 2 && (!expect(in, "tool_settings") || !read_bool(in, state.snap_enabled))) {
        error = "The project tool settings are incomplete or malformed.";
        return false;
    }
    if (!expect(in, "layout") || !(in >> state.ui_toolshelf_width >> state.ui_right_deck_width
            >> state.ui_outliner_ratio >> state.ui_timeline_height)) {
        error = "The project layout settings are incomplete or malformed.";
        return false;
    }

    if (!expect(in, "timeline") || !(in >> state.current_frame >> state.start_frame >> state.end_frame
            >> state.total_frames) || !read_bool(in, state.is_autokey)) {
        error = "The project timeline settings are incomplete or malformed.";
        return false;
    }
    if (version >= 3 && (!expect(in, "playback_fps") || !(in >> state.playback_fps))) {
        error = "The project playback rate is incomplete or malformed.";
        return false;
    }
    if (version >= 2) {
        if (!expect(in, "view") || !read_bool(in, state.viewport_navigation_active)) {
            error = "The project viewport settings are incomplete or malformed.";
            return false;
        }
        for (float& value : state.viewport_position) if (!(in >> value)) { error = "The project viewport settings are incomplete."; return false; }
        for (float& value : state.viewport_target) if (!(in >> value)) { error = "The project viewport settings are incomplete."; return false; }
        for (float& value : state.viewport_rotation) if (!(in >> value)) { error = "The project viewport settings are incomplete."; return false; }
        if (!(in >> state.viewport_distance)) { error = "The project viewport settings are incomplete."; return false; }
    }

    if (!expect(in, "camera") || !(in >> state.pos_x >> state.pos_y >> state.pos_z
            >> state.rot_x >> state.rot_y >> state.rot_z >> state.scale_x >> state.scale_y >> state.scale_z
            >> state.focus_distance >> state.focal_length
            >> state.camera_position[0] >> state.camera_position[1] >> state.camera_position[2]
            >> state.camera_rotation[0] >> state.camera_rotation[1] >> state.camera_rotation[2]
            >> state.camera_focal_length) ||
        !expect(in, "light") || !(in >> state.key_light_x >> state.key_light_y >> state.key_light_z
            >> state.key_light_energy) ||
        !expect(in, "render") || !(in >> state.max_samples_per_pixel >> state.firefly_clamp)) {
        error = "The project settings are incomplete or malformed.";
        return false;
    }

    if (version >= 7) {
        int frameMode = 0;
        if (!expect(in, "render_output") || !(in >> state.render_output_width >> state.render_output_height) ||
            !read_bool(in, state.render_output_exr) || !(in >> frameMode >> state.render_custom_start >>
                state.render_custom_end >> std::quoted(state.render_output_path)) || frameMode < 0 || frameMode > 2) {
            error = "The project render output settings are incomplete or malformed.";
            return false;
        }
        state.render_frame_mode = static_cast<EditorState::RenderFrameMode>(frameMode);
    } else {
        state.render_custom_start = state.start_frame;
        state.render_custom_end = state.end_frame;
    }
    if (!expect(in, "selection") || !(in >> state.active_camera_node >> state.key_light_node
            >> state.selected_scene_node)) {
        error = "The project settings are incomplete or malformed.";
        return false;
    }

    if (state.start_frame < 1 || state.end_frame < state.start_frame || state.total_frames < state.end_frame ||
        state.total_frames > 1000000 || state.current_frame < 1 || state.current_frame > state.total_frames ||
        state.max_samples_per_pixel < 1 || state.max_samples_per_pixel > 4096 ||
        state.render_output_width < 320 || state.render_output_width > 1920 ||
        state.render_output_height < 180 || state.render_output_height > 1080 ||
        state.render_custom_start < 1 || state.render_custom_end < state.render_custom_start ||
        state.render_custom_end > 1000000 ||
        state.playback_fps < 1 || state.playback_fps > 240 ||
        state.ui_toolshelf_width < 32.0f || state.ui_toolshelf_width > 180.0f ||
        state.ui_right_deck_width < 220.0f || state.ui_right_deck_width > 4096.0f ||
        state.ui_outliner_ratio < 0.20f || state.ui_outliner_ratio > 0.80f ||
        state.ui_timeline_height < 80.0f || state.ui_timeline_height > 380.0f ||
        state.viewport_distance < 0.02f || state.viewport_distance > 100000.0f ||
        !finite_values({state.pos_x, state.pos_y, state.pos_z, state.rot_x, state.rot_y, state.rot_z,
            state.scale_x, state.scale_y, state.scale_z, state.focus_distance, state.focal_length,
            state.camera_position[0], state.camera_position[1], state.camera_position[2],
            state.camera_rotation[0], state.camera_rotation[1], state.camera_rotation[2],
            state.viewport_position[0], state.viewport_position[1], state.viewport_position[2],
            state.viewport_target[0], state.viewport_target[1], state.viewport_target[2],
            state.viewport_rotation[0], state.viewport_rotation[1], state.viewport_rotation[2], state.viewport_distance,
            state.camera_focal_length, state.ui_toolshelf_width, state.ui_right_deck_width,
            state.ui_outliner_ratio, state.ui_timeline_height, state.key_light_x, state.key_light_y, state.key_light_z,
            state.key_light_energy, state.firefly_clamp})) {
        error = "The project contains invalid camera, render, or timeline settings.";
        return false;
    }

    uint64_t nodeCount = 0;
    if (!expect(in, "nodes") || !(in >> nodeCount) || nodeCount > kMaximumProjectItems) {
        error = "The project scene hierarchy is invalid or too large.";
        return false;
    }
    state.scene_nodes.clear();
    state.scene_node_ids.clear();
    state.scene_node_asset_ids.clear();
    state.scene_nodes.reserve(static_cast<size_t>(nodeCount));
    state.scene_node_ids.reserve(static_cast<size_t>(nodeCount));
    state.scene_node_asset_ids.reserve(static_cast<size_t>(nodeCount));
    for (uint64_t i = 0; i < nodeCount; ++i) {
        zegfx::ProductionSceneNode node;
        std::string nodeId, nodeAssetId;
        unsigned int type = 0;
        if (!expect(in, "node") ||
            (version >= 4 && !(in >> std::quoted(nodeId) >> std::quoted(nodeAssetId))) ||
            !(in >> std::quoted(node.name) >> node.parentIndex >> type) ||
            type > static_cast<unsigned int>(zegfx::ProductionNodeType::Light) ||
            !read_bool(in, node.visible) || !read_bool(in, node.locked) ||
            !read_bool(in, node.renderable) || !read_bool(in, node.expanded)) {
            error = "A scene object in the project file is malformed.";
            return false;
        }
        node.type = static_cast<zegfx::ProductionNodeType>(type);
        for (float& value : node.rotationOffset) if (!(in >> value)) { error = "A scene object transform is incomplete."; return false; }
        for (float& value : node.scaleFactor) if (!(in >> value)) { error = "A scene object transform is incomplete."; return false; }
        for (float& value : node.worldTransform) if (!(in >> value)) { error = "A scene object transform is incomplete."; return false; }
        if (version >= 8) {
            unsigned int lightType = 0;
            if (!(in >> lightType)) {
                error = "A scene light type is invalid.";
                return false;
            }
            if (version <= 8) {
                // Versions 8 and earlier stored the Medusa-only enum:
                // Point=0, Sun=1, Spot=2, Area=3.
                if (lightType > 3) { error = "A scene light type is invalid."; return false; }
                node.lightType = lightType == 1 ? zegfx::LightType::Directional :
                                 lightType == 2 ? zegfx::LightType::Spot : zegfx::LightType::Point;
                node.areaLight = lightType == 3;
            } else {
                if (lightType > static_cast<unsigned int>(zegfx::LightType::Spot) ||
                    !read_bool(in, node.areaLight)) {
                    error = "A scene light type is invalid.";
                    return false;
                }
                node.lightType = static_cast<zegfx::LightType>(lightType);
            }
            for (float& value : node.lightColor) if (!(in >> value)) { error = "A scene light color is incomplete."; return false; }
            if (!(in >> node.lightEnergy >> node.lightSpotAngleDegrees >> node.lightAreaSize)) {
                error = "A scene light configuration is incomplete.";
                return false;
            }
        }
        if (!finite_values({node.rotationOffset[0], node.rotationOffset[1], node.rotationOffset[2],
            node.scaleFactor[0], node.scaleFactor[1], node.scaleFactor[2],
            node.worldTransform[0], node.worldTransform[1], node.worldTransform[2], node.worldTransform[3],
            node.worldTransform[4], node.worldTransform[5], node.worldTransform[6], node.worldTransform[7],
            node.worldTransform[8], node.worldTransform[9], node.worldTransform[10], node.worldTransform[11],
            node.worldTransform[12], node.worldTransform[13], node.worldTransform[14], node.worldTransform[15],
            node.lightColor[0], node.lightColor[1], node.lightColor[2], node.lightEnergy,
            node.lightSpotAngleDegrees, node.lightAreaSize}) || node.lightEnergy < 0.0f ||
            node.lightSpotAngleDegrees < 1.0f || node.lightSpotAngleDegrees > 179.0f || node.lightAreaSize <= 0.0f) {
            error = "A scene object contains a non-finite transform.";
            return false;
        }
        if (node.parentIndex < -1 || node.parentIndex >= static_cast<int32_t>(nodeCount) ||
            node.parentIndex == static_cast<int32_t>(i)) {
            error = "A scene object has an invalid parent reference.";
            return false;
        }
        state.scene_nodes.push_back(std::move(node));
        state.scene_node_ids.push_back(std::move(nodeId));
        state.scene_node_asset_ids.push_back(std::move(nodeAssetId));
    }

    // Version 1-3 projects had no durable IDs. Give their primary source asset
    // one ID, then derive node IDs from that asset and source node order so that
    // restoring the same project recreates the same object identities.
    state.asset_path = assetPath;
    if (version < 4 && state.asset_references.empty() && !assetPath.empty()) {
        ProjectAssetReference legacyAsset;
        legacyAsset.id = migrated_asset_id(assetPath);
        legacyAsset.path = assetPath;
        legacyAsset.display_name = std::filesystem::u8path(assetPath).stem().u8string();
        state.asset_references.push_back(std::move(legacyAsset));
    }
    ensure_medusa_document_identity(state);
    const std::string primaryAssetId = state.asset_references.empty()
        ? std::string() : state.asset_references.front().id;
    std::vector<std::string> nodeIds;
    nodeIds.reserve(state.scene_nodes.size());
    for (size_t i = 0; i < state.scene_nodes.size(); ++i) {
        auto& nodeId = state.scene_node_ids[i];
        auto& nodeAssetId = state.scene_node_asset_ids[i];
        if (nodeId.empty())
            nodeId = primaryAssetId.empty() ? create_medusa_stable_id()
                                            : primaryAssetId + "/node/" + std::to_string(i);
        if (nodeAssetId.empty() && nodeId.rfind("medusa-object-", 0) != 0) nodeAssetId = primaryAssetId;
        nodeIds.push_back(nodeId);
    }
    if (version >= 4) {
        auto unique_ids = [](const auto& values, auto getId) {
            std::unordered_set<std::string> seen;
            for (const auto& value : values) {
                const auto& id = getId(value);
                if (id.empty() || !seen.insert(id).second) return false;
            }
            return true;
        };
        if (state.project_id.empty() || state.scene_id.empty() ||
            state.scene_node_ids.size() != state.scene_nodes.size() ||
            !unique_ids(state.scene_node_ids, [](const auto& id) -> const std::string& { return id; }) ||
            !unique_ids(state.asset_references, [](const auto& asset) -> const std::string& { return asset.id; }) ||
            !unique_ids(state.material_references, [](const auto& material) -> const std::string& { return material.id; })) {
            error = "The project contains missing or duplicate document, asset, material, or object identities.";
            return false;
        }
        for (const auto& material : state.material_references) {
            const bool assetExists = std::any_of(state.asset_references.begin(), state.asset_references.end(),
                [&](const auto& asset) { return asset.id == material.asset_id; });
            if (!assetExists) {
                error = "A project material points to an asset that is not in the asset manifest.";
                return false;
            }
        }
    }

    uint64_t keyCount = 0;
    if (!expect(in, "keyframes") || !(in >> keyCount) || keyCount > kMaximumProjectItems) {
        error = "The project animation data is invalid or too large.";
        return false;
    }
    state.keyframes.clear();
    state.keyframes.reserve(static_cast<size_t>(keyCount));
    for (uint64_t i = 0; i < keyCount; ++i) {
        EditorState::Keyframe key;
        if (!expect(in, "keyframe") || !(in >> key.frame)) {
            error = "A keyframe in the project file is malformed.";
            return false;
        }
        for (float& value : key.camera) if (!(in >> value)) { error = "A keyframe is incomplete."; return false; }
        for (float& value : key.light) if (!(in >> value)) { error = "A keyframe is incomplete."; return false; }
        if (key.frame < state.start_frame || key.frame > state.end_frame ||
            !finite_values({key.camera[0], key.camera[1], key.camera[2], key.camera[3], key.camera[4],
                key.camera[5], key.camera[6], key.light[0], key.light[1], key.light[2], key.light[3]})) {
            error = "A keyframe contains an invalid frame or value.";
            return false;
        }
        state.keyframes.push_back(key);
    }
    std::sort(state.keyframes.begin(), state.keyframes.end(), [](const auto& a, const auto& b) {
        return a.frame < b.frame;
    });

    state.node_keyframes.clear();
    if (version >= 2) {
        uint64_t nodeKeyCount = 0;
        if (!expect(in, "node_keyframes") || !(in >> nodeKeyCount) || nodeKeyCount > kMaximumProjectItems) {
            error = "The scene animation data is invalid or too large.";
            return false;
        }
        state.node_keyframes.reserve(static_cast<size_t>(nodeKeyCount));
        for (uint64_t i = 0; i < nodeKeyCount; ++i) {
            EditorState::NodeKeyframe key;
            if (!expect(in, "node_keyframe") || !(in >> key.node_index >> key.frame) ||
                (version >= 4 && !(in >> std::quoted(key.node_id)))) {
                error = "A scene keyframe in the project file is malformed.";
                return false;
            }
            for (float& value : key.position) if (!(in >> value)) { error = "A scene keyframe is incomplete."; return false; }
            for (float& value : key.rotation) if (!(in >> value)) { error = "A scene keyframe is incomplete."; return false; }
            for (float& value : key.scale) if (!(in >> value)) { error = "A scene keyframe is incomplete."; return false; }
            if (version >= 4) {
                const auto node = std::find(state.scene_node_ids.begin(), state.scene_node_ids.end(), key.node_id);
                if (node == state.scene_node_ids.end()) {
                    error = "A scene keyframe points to an object that no longer exists.";
                    return false;
                }
                key.node_index = static_cast<int>(std::distance(state.scene_node_ids.begin(), node));
            } else if (key.node_index >= 0 && static_cast<size_t>(key.node_index) < nodeIds.size()) {
                key.node_id = nodeIds[static_cast<size_t>(key.node_index)];
            }
            if (key.node_index < 0 || static_cast<size_t>(key.node_index) >= state.scene_nodes.size() ||
                key.frame < state.start_frame || key.frame > state.end_frame ||
                !finite_values({key.position[0], key.position[1], key.position[2], key.rotation[0],
                    key.rotation[1], key.rotation[2], key.scale[0], key.scale[1], key.scale[2]}) ||
                key.scale[0] < 0.001f || key.scale[1] < 0.001f || key.scale[2] < 0.001f) {
                error = "A scene keyframe contains an invalid object, frame, or transform.";
                return false;
            }
            state.node_keyframes.push_back(key);
        }
        std::stable_sort(state.node_keyframes.begin(), state.node_keyframes.end(), [](const auto& a, const auto& b) {
            return a.node_index < b.node_index || (a.node_index == b.node_index && a.frame < b.frame);
        });
    }

    auto validNodeIndex = [&](int index) { return index == -1 || (index >= 0 && static_cast<size_t>(index) < state.scene_nodes.size()); };
    if (!validNodeIndex(state.active_camera_node) || !validNodeIndex(state.key_light_node) ||
        !validNodeIndex(state.selected_scene_node)) {
        error = "The project selection points to a missing scene object.";
        return false;
    }
    if (in >> std::ws, !in.eof()) {
        error = "The project file contains unexpected data after its scene.";
        return false;
    }
    state.pending_command.clear();
    state.project_dirty = false;
    if (state.scene_node_ids.size() != state.scene_nodes.size()) state.scene_node_ids.resize(state.scene_nodes.size());
    if (state.scene_node_asset_ids.size() != state.scene_nodes.size()) state.scene_node_asset_ids.resize(state.scene_nodes.size());
    if (state.active_workspace == "Shot") state.select_active_camera();
    return true;
}

} // namespace

std::string create_medusa_stable_id() {
    GUID guid{};
    if (SUCCEEDED(CoCreateGuid(&guid))) {
        wchar_t buffer[40]{};
        const int length = StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
        if (length > 1) {
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
            if (bytes > 1) {
                std::string result(static_cast<size_t>(bytes), '\0');
                WideCharToMultiByte(CP_UTF8, 0, buffer, -1, result.data(), bytes, nullptr, nullptr);
                result.resize(static_cast<size_t>(bytes - 1));
                return result;
            }
        }
    }
    // CoCreateGuid is expected to be available on Windows; this fallback keeps
    // documents editable even if the system GUID provider fails unexpectedly.
    static uint64_t counter = 0;
    return "medusa-id-" + std::to_string(GetTickCount64()) + "-" + std::to_string(++counter);
}

void ensure_medusa_document_identity(EditorState& state) {
    if (state.project_id.empty()) state.project_id = create_medusa_stable_id();
    if (state.scene_id.empty()) state.scene_id = create_medusa_stable_id();
    if (state.scene_name.empty() && !state.project_path.empty())
        state.scene_name = std::filesystem::u8path(state.project_path).stem().u8string();
    if (state.asset_references.empty() && !state.asset_path.empty()) {
        ProjectAssetReference asset;
        asset.id = create_medusa_stable_id();
        asset.path = state.asset_path;
        asset.display_name = is_builtin_production_scene(state.asset_path)
            ? "Starter Cube" : std::filesystem::u8path(state.asset_path).stem().u8string();
        if (!is_builtin_production_scene(state.asset_path)) {
            std::error_code ec;
            asset.source_size = std::filesystem::file_size(std::filesystem::u8path(state.asset_path), ec);
            if (ec) asset.source_size = 0;
            ec.clear();
            const auto modified = std::filesystem::last_write_time(std::filesystem::u8path(state.asset_path), ec);
            if (!ec) asset.source_modified_ticks = modified.time_since_epoch().count();
        }
        state.asset_references.push_back(std::move(asset));
    }
    for (size_t i = 0; i < state.asset_references.size(); ++i) {
        auto& asset = state.asset_references[i];
        if (asset.id.empty()) asset.id = create_medusa_stable_id();
        if (asset.path.empty() && i == 0) asset.path = state.asset_path;
        if (asset.display_name.empty() && !asset.path.empty())
            asset.display_name = is_builtin_production_scene(asset.path)
                ? "Starter Cube" : std::filesystem::u8path(asset.path).stem().u8string();
    }
    const std::string primaryAssetId = state.asset_references.empty()
        ? std::string() : state.asset_references.front().id;
    state.rebuild_scene_hierarchy();
    for (size_t i = 0; i < state.scene_nodes.size(); ++i) {
        auto& nodeId = state.scene_node_ids[i];
        auto& nodeAssetId = state.scene_node_asset_ids[i];
        if (nodeId.empty())
            nodeId = primaryAssetId.empty() ? create_medusa_stable_id()
                                            : primaryAssetId + "/node/" + std::to_string(i);
        if (nodeAssetId.empty() && nodeId.rfind("medusa-object-", 0) != 0) nodeAssetId = primaryAssetId;
    }
    for (auto& key : state.node_keyframes) {
        if (key.node_id.empty() && key.node_index >= 0 &&
            static_cast<size_t>(key.node_index) < state.scene_nodes.size())
            key.node_id = state.scene_node_ids[static_cast<size_t>(key.node_index)];
    }
}

std::string medusa_project_snapshot(const EditorState& state) {
    EditorState normalized = state;
    ensure_medusa_document_identity(normalized);
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    write_state(out, normalized, normalized.asset_path);
    return out.str();
}

bool restore_medusa_project_snapshot(const std::string& snapshot, EditorState& state, std::string& error) {
    std::istringstream input(snapshot);
    EditorState loaded;
    std::string assetPath;
    if (!read_project(input, loaded, assetPath, error)) return false;
    loaded.asset_path = std::move(assetPath);
    ensure_medusa_document_identity(loaded);
    loaded.project_path = state.project_path;
    loaded.project_dirty = true;
    state = std::move(loaded);
    error.clear();
    return true;
}

bool save_medusa_recovery(const EditorState& state, const std::string& recoveryPath,
                          const std::string& sourceProjectPath, std::string& error) {
    if (recoveryPath.empty()) { error = "The recovery location is unavailable."; return false; }
    EditorState normalized = state;
    ensure_medusa_document_identity(normalized);
    const std::string snapshot = medusa_project_snapshot(normalized);
    std::ostringstream out;
    out << "MEDUSA_RECOVERY 1\n" << "source " << std::quoted(sourceProjectPath) << '\n'
        << "snapshot_bytes " << snapshot.size() << '\n';
    out.write(snapshot.data(), static_cast<std::streamsize>(snapshot.size()));
    return write_atomic_file(std::filesystem::u8path(recoveryPath), out.str(), error, "recovery");
}

bool load_medusa_recovery(const std::string& recoveryPath, EditorState& state,
                          std::string& sourceProjectPath, std::string& error) {
    std::ifstream file(std::filesystem::u8path(recoveryPath), std::ios::binary);
    if (!file) { error = "Could not open the recovery file."; return false; }
    uint64_t snapshotBytes = 0;
    int version = 0;
    if (!expect(file, "MEDUSA_RECOVERY") || !(file >> version) || version != 1 ||
        !expect(file, "source") || !(file >> std::quoted(sourceProjectPath)) ||
        !expect(file, "snapshot_bytes") || !(file >> snapshotBytes) ||
        snapshotBytes == 0 || snapshotBytes > 512ull * 1024ull * 1024ull) {
        error = "The recovery file header is invalid or too large.";
        return false;
    }
    file.get();
    std::string snapshot(static_cast<size_t>(snapshotBytes), '\0');
    file.read(snapshot.data(), static_cast<std::streamsize>(snapshot.size()));
    if (file.gcount() != static_cast<std::streamsize>(snapshot.size())) {
        error = "The recovery file is incomplete.";
        return false;
    }
    if (!restore_medusa_project_snapshot(snapshot, state, error)) return false;
    state.project_path = sourceProjectPath;
    state.project_dirty = true;
    error.clear();
    return true;
}

bool save_medusa_project(const EditorState& state, const std::string& path, std::string& error) {
    if (path.empty()) { error = "Choose a location for the project file."; return false; }
    EditorState normalized = state;
    ensure_medusa_document_identity(normalized);
    std::filesystem::path target = std::filesystem::u8path(path);
    std::string storedAsset = normalized.asset_path;
    if (!storedAsset.empty() && !is_builtin_production_scene(storedAsset)) {
        const auto projectDir = target.parent_path();
        const auto asset = std::filesystem::u8path(storedAsset);
        const auto relative = asset.lexically_relative(projectDir);
        if (!relative.empty()) storedAsset = path_utf8(relative);
    }
    for (size_t i = 0; i < normalized.asset_references.size(); ++i) {
        auto& reference = normalized.asset_references[i];
        if (i == 0 && !storedAsset.empty()) {
            reference.path = storedAsset;
            continue;
        }
        if (reference.path.empty() || is_builtin_production_scene(reference.path)) continue;
        const auto referencedPath = std::filesystem::u8path(reference.path);
        const auto relative = referencedPath.lexically_relative(target.parent_path());
        if (!relative.empty()) reference.path = path_utf8(relative);
    }

    std::ostringstream data;
    data << std::setprecision(std::numeric_limits<float>::max_digits10);
    write_state(data, normalized, storedAsset);
    return write_atomic_file(target, data.str(), error, "project");
}

bool load_medusa_project(const std::string& path, EditorState& state, std::string& error) {
    const auto projectFile = std::filesystem::u8path(path);
    std::ifstream file(projectFile, std::ios::binary);
    if (!file) { error = "Could not open the Medusa project file."; return false; }

    EditorState loaded;
    std::string assetPath;
    if (!read_project(file, loaded, assetPath, error)) return false;
    if (!assetPath.empty() && !is_builtin_production_scene(assetPath)) {
        auto asset = std::filesystem::u8path(assetPath);
        if (asset.is_relative()) asset = projectFile.parent_path() / asset;
        assetPath = path_utf8(asset.lexically_normal());
    }
    loaded.asset_path = std::move(assetPath);
    loaded.project_path = path_utf8(projectFile);
    for (auto& reference : loaded.asset_references) {
        if (reference.path.empty() || is_builtin_production_scene(reference.path)) continue;
        auto asset = std::filesystem::u8path(reference.path);
        if (asset.is_relative()) asset = projectFile.parent_path() / asset;
        reference.path = path_utf8(asset.lexically_normal());
    }
    if (!loaded.asset_references.empty() && !loaded.asset_path.empty())
        loaded.asset_references.front().path = loaded.asset_path;
    ensure_medusa_document_identity(loaded);
    loaded.project_dirty = false;
    state = std::move(loaded);
    error.clear();
    return true;
}

} // namespace medusa
