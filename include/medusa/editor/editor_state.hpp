#pragma once
#include <medusa/core/medusa_screen.hpp>
#include <medusa/ui/medusa_widgets.hpp>
#include <medusa/graphics/medusa_graphics.hpp>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <cstdint>

namespace medusa {
// Project-owned asset references are independent of transient renderer handles.
// Phase 2 can extend the manifest to multiple assets without changing the
// project identity model or invalidating existing single-asset projects.
struct ProjectAssetReference {
    std::string id;
    std::string path;
    std::string display_name;
    uint64_t source_size = 0;
    int64_t source_modified_ticks = 0;
};

struct ProjectMaterialReference {
    std::string id;
    std::string asset_id;
    uint32_t source_index = 0;
};

enum class SplitterId {
    None,
    RightDeckWidth,
    RightDeckHeight,
    TimelineHeight,
    ToolshelfWidth
};

struct EditorState {
    enum class TimelineRangeField { None, Start, End };
    enum class RenderFrameMode { SceneRange, CurrentFrame, CustomRange };

    struct SceneNodeBounds {
        float centerOffset[3]{};
        float radius = 0.0f;
        bool valid = false;
    };

    struct NodeKeyframe {
        int node_index = -1;
        std::string node_id;
        int frame = 1;
        float position[3]{};
        float rotation[3]{};
        float scale[3] = {1.0f, 1.0f, 1.0f};
    };

    // One saved project flows through three focused workspaces. Workspace is
    // project state, not a separate copy of the scene.
    std::string active_workspace = "Scene";
    std::string active_tool = "tool_move";
    bool camera_pilot_active = false;
    std::string active_camera;
    std::string pending_command;
    std::string asset_path;
    std::string asset_library_root;
    std::string asset_library_selected_path;
    std::string render_output_path;
    bool render_setup_open = false;
    RenderFrameMode render_frame_mode = RenderFrameMode::SceneRange;
    int render_custom_start = 1;
    int render_custom_end = 250;
    int render_output_width = 1280;
    int render_output_height = 720;
    bool render_output_exr = false;
    std::string project_path;
    std::string project_id;
    std::string scene_id;
    bool project_dirty = false;
    bool history_can_undo = false;
    bool history_can_redo = false;
    bool show_grid = true;
    bool show_safe_frame = true;
    bool show_camera_trail = true;
    ViewportShadingMode viewport_shading = ViewportShadingMode::Solid;
    bool menu_popup_open = false;
    bool viewport_popup_open = false;
    float ui_toolshelf_width = 42.0f;
    float ui_right_deck_width = 310.0f;
    float ui_outliner_ratio = 0.48f;
    float ui_timeline_height = 160.0f;

    int current_frame = 1;
    int total_frames = 250;
    int start_frame = 1;
    int end_frame = 250;
    TimelineRangeField timeline_range_edit_field = TimelineRangeField::None;
    std::string timeline_range_edit_buffer;
    bool timeline_range_edit_replace = true;
    bool is_playing = false;
    bool playback_reverse = false;
    bool is_autokey = false;
    bool snap_enabled = false;
    bool show_timeline_keyframes = true;
    int playback_fps = 24;

    // Free viewport navigation stays independent from the scene's active camera.
    bool viewport_navigation_active = true;
    bool free_camera_look_active = false;
    float viewport_target[3] = {0.0f, 0.0f, 0.0f};
    float viewport_position[3] = {5.0f, 3.0f, -6.0f};
    float viewport_rotation[3] = {-21.0f, -140.0f, 0.0f};
    float viewport_distance = 8.37f;

    // Camera transform drives the active ZeGFX production view.
    float pos_x = 0.000f;
    float pos_y = 1.250f;
    float pos_z = -3.420f;
    float rot_x = 0.0f;
    float rot_y = 45.0f;
    float rot_z = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float scale_z = 1.0f;

    // Parameters derived from the current production camera and scene.
    float focal_length = 50.0f;
    float focus_distance = 1.0f;
    float camera_position[3] = {0.0f, 1.25f, -3.42f};
    float camera_rotation[3] = {0.0f, 45.0f, 0.0f};
    float camera_focal_length = 50.0f;

    float key_light_x = -3.0f;
    float key_light_y = 6.0f;
    float key_light_z = 4.0f;
    float key_light_energy = 1.0f;
    int max_samples_per_pixel = 64;
    float firefly_clamp = 10.0f;

    struct Keyframe {
        int frame = 1;
        float camera[7]{}; // position, Euler rotation, focal length
        float light[4]{};  // position and energy
    };
    std::vector<Keyframe> keyframes;
    std::vector<NodeKeyframe> node_keyframes;
    std::vector<zegfx::ProductionSceneNode> scene_nodes;
    std::vector<zegfx::ProductionTriangle> scene_triangles;
    std::vector<zegfx::ProductionMaterial> scene_materials;
    // Stable document identities are owned by Medusa and parallel to the
    // renderer's transient node indices/records.
    std::vector<std::string> scene_node_ids;
    std::vector<std::string> scene_node_asset_ids;
    std::vector<SceneNodeBounds> scene_node_bounds;
    std::vector<std::vector<uint32_t>> scene_node_children;
    int selected_scene_node = -1;
    int active_camera_node = -1;
    int key_light_node = -1;
    int pending_scene_node = -1;
    float pending_pick_origin[3]{};
    float pending_pick_direction[3]{};
    bool pending_previous_visible = true;
    bool pending_previous_renderable = true;
    float pending_previous_translation[3] = {};
    float pending_previous_rotation[3] = {};
    float pending_previous_scale[3] = {1.0f, 1.0f, 1.0f};
    float pending_previous_light_energy = 0.0f;
    uint64_t scene_generation = 0;
    std::string scene_name;
    std::vector<ProjectAssetReference> asset_references;
    std::vector<ProjectMaterialReference> material_references;

    zegfx::ProductionNodeType selected_node_type() const {
        if (selected_scene_node >= 0 && static_cast<size_t>(selected_scene_node) < scene_nodes.size())
            return scene_nodes[static_cast<size_t>(selected_scene_node)].type;
        return zegfx::ProductionNodeType::Group;
    }
    void rebuild_scene_hierarchy() {
        if (scene_node_ids.size() != scene_nodes.size()) scene_node_ids.resize(scene_nodes.size());
        if (scene_node_asset_ids.size() != scene_nodes.size()) scene_node_asset_ids.resize(scene_nodes.size());
        scene_node_children.assign(scene_nodes.size(), {});
        for (size_t i = 0; i < scene_nodes.size(); ++i) {
            const int parent = scene_nodes[i].parentIndex;
            if (parent >= 0 && static_cast<size_t>(parent) < scene_nodes.size())
                scene_node_children[static_cast<size_t>(parent)].push_back(static_cast<uint32_t>(i));
        }
    }
    bool camera_selected() const {
        return active_camera_node >= 0 ? selected_scene_node == active_camera_node
            : active_workspace == "Shot" && selected_scene_node < 0;
    }
    bool light_selected() const { return selected_node_type() == zegfx::ProductionNodeType::Light; }
    bool mesh_selected() const { return selected_node_type() == zegfx::ProductionNodeType::Mesh; }
    bool transformable_selected() const {
        const auto type = selected_node_type();
        return type == zegfx::ProductionNodeType::Mesh || type == zegfx::ProductionNodeType::Group;
    }
    void activate_workspace(const std::string& workspace) {
        if (workspace != "Scene" && workspace != "Shot" && workspace != "Production") return;
        if (active_workspace == workspace) return;
        active_workspace = workspace;
        if (workspace == "Shot") {
            select_active_camera();
            camera_pilot_active = false;
        } else if (workspace == "Scene") {
            enter_viewport_camera();
            viewport_navigation_active = true;
            camera_pilot_active = false;
        }
    }
    void select_active_camera() {
        if (active_camera_node >= 0 && static_cast<size_t>(active_camera_node) >= scene_nodes.size()) return;
        selected_scene_node = active_camera_node;
        active_camera = active_camera_node >= 0
            ? scene_nodes[static_cast<size_t>(active_camera_node)].name : "Render Camera";
        active_workspace = "Shot";
        pos_x = camera_position[0]; pos_y = camera_position[1]; pos_z = camera_position[2];
        rot_x = camera_rotation[0]; rot_y = camera_rotation[1]; rot_z = camera_rotation[2];
        focal_length = camera_focal_length;
        viewport_navigation_active = false;
        camera_pilot_active = false;
        update_camera_orbit_target();
    }
    void select_scene_node(uint32_t nodeIndex) {
        if (nodeIndex >= scene_nodes.size() || scene_nodes[nodeIndex].locked) return;
        if (active_workspace == "Shot" && static_cast<int>(nodeIndex) != active_camera_node) return;
        selected_scene_node = static_cast<int>(nodeIndex);
        const auto type = scene_nodes[nodeIndex].type;
        if (type == zegfx::ProductionNodeType::Camera) {
            active_camera_node = static_cast<int>(nodeIndex);
            select_active_camera();
            return;
        }
        active_workspace = "Scene";
        if (type == zegfx::ProductionNodeType::Mesh || type == zegfx::ProductionNodeType::Group ||
            type == zegfx::ProductionNodeType::Light) {
            pos_x = scene_nodes[nodeIndex].worldTransform[12];
            pos_y = scene_nodes[nodeIndex].worldTransform[13];
            pos_z = scene_nodes[nodeIndex].worldTransform[14];
            rot_x = scene_nodes[nodeIndex].rotationOffset[0];
            rot_y = scene_nodes[nodeIndex].rotationOffset[1];
            rot_z = scene_nodes[nodeIndex].rotationOffset[2];
            scale_x = scene_nodes[nodeIndex].scaleFactor[0];
            scale_y = scene_nodes[nodeIndex].scaleFactor[1];
            scale_z = scene_nodes[nodeIndex].scaleFactor[2];
            if (type == zegfx::ProductionNodeType::Light) {
                key_light_node = static_cast<int>(nodeIndex);
                key_light_x = scene_nodes[nodeIndex].worldTransform[12];
                key_light_y = scene_nodes[nodeIndex].worldTransform[13];
                key_light_z = scene_nodes[nodeIndex].worldTransform[14];
                key_light_energy = scene_nodes[nodeIndex].lightEnergy;
            }
        }
    }

    void frame_selected() {
        if (selected_scene_node < 0 || static_cast<size_t>(selected_scene_node) >= scene_nodes.size()) return;
        if (active_workspace == "Shot" && camera_selected()) return;
        const size_t index = static_cast<size_t>(selected_scene_node);
        const auto& node = scene_nodes[index];
        std::copy(camera_rotation, camera_rotation + 3, viewport_rotation);
        float target[3] = {node.worldTransform[12], node.worldTransform[13], node.worldTransform[14]};
        float radius = 1.0f;
        if (index < scene_node_bounds.size() && scene_node_bounds[index].valid) {
            const auto& bounds = scene_node_bounds[index];
            for (int axis = 0; axis < 3; ++axis) target[axis] += bounds.centerOffset[axis];
            radius = std::max(0.1f, bounds.radius);
        }
        std::copy(target, target + 3, viewport_target);
        viewport_distance = std::clamp(radius * 2.8f, 0.25f, 100000.0f);
        const float pitch = viewport_rotation[0] * 0.017453292519943295f;
        const float yaw = viewport_rotation[1] * 0.017453292519943295f;
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float forward[3] = {std::sin(yaw) * cp, sp, -std::cos(yaw) * cp};
        for (int axis = 0; axis < 3; ++axis)
            viewport_position[axis] = viewport_target[axis] - forward[axis] * viewport_distance;
        viewport_navigation_active = true;
    }

    bool finish_timeline_range_edit(bool accept) {
        if (timeline_range_edit_field == TimelineRangeField::None) return false;
        if (accept && !timeline_range_edit_buffer.empty()) {
            int value = 0;
            for (char digit : timeline_range_edit_buffer) {
                if (digit < '0' || digit > '9') continue;
                value = std::min(1000000, value * 10 + (digit - '0'));
            }
            value = std::max(1, value);
            if (timeline_range_edit_field == TimelineRangeField::Start) {
                start_frame = value;
                if (end_frame < start_frame) end_frame = start_frame;
            } else {
                end_frame = std::max(start_frame, value);
            }
            total_frames = std::max(total_frames, end_frame);
            const int oldFrame = current_frame;
            current_frame = std::clamp(current_frame, start_frame, end_frame);
            if (current_frame != oldFrame) evaluate_keyframes();
        }
        timeline_range_edit_field = TimelineRangeField::None;
        timeline_range_edit_buffer.clear();
        timeline_range_edit_replace = true;
        return true;
    }

    void enter_viewport_camera() {
        // Keep the current free viewport when entering Camera Pilot. Only seed
        // it from the production camera when leaving camera-locked viewing.
        // Copying the default production camera here points away from the
        // origin, making the empty viewport appear completely blank.
        if (!viewport_navigation_active) {
            std::copy(camera_position, camera_position + 3, viewport_position);
            std::copy(camera_rotation, camera_rotation + 3, viewport_rotation);
            viewport_distance = std::max(0.1f, focus_distance);
            const float pitch = viewport_rotation[0] * 0.017453292519943295f;
            const float yaw = viewport_rotation[1] * 0.017453292519943295f;
            const float cp = std::cos(pitch), sp = std::sin(pitch);
            const float forward[3] = {std::sin(yaw) * cp, sp, -std::cos(yaw) * cp};
            for (int axis = 0; axis < 3; ++axis)
                viewport_target[axis] = viewport_position[axis] + forward[axis] * viewport_distance;
        }
        viewport_navigation_active = true;
    }

    void update_camera_orbit_target() {
        const float pitch = std::clamp(camera_rotation[0], -89.0f, 89.0f) * 0.017453292519943295f;
        const float yaw = camera_rotation[1] * 0.017453292519943295f;
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float forward[3] = {std::sin(yaw) * cp, sp, -std::cos(yaw) * cp};
        for (int axis = 0; axis < 3; ++axis)
            viewport_target[axis] = camera_position[axis] + forward[axis] * std::max(0.1f, focus_distance);
    }

    void update_camera_position_from_orbit() {
        const float pitch = std::clamp(camera_rotation[0], -89.0f, 89.0f) * 0.017453292519943295f;
        const float yaw = camera_rotation[1] * 0.017453292519943295f;
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float forward[3] = {std::sin(yaw) * cp, sp, -std::cos(yaw) * cp};
        focus_distance = std::clamp(focus_distance, 0.02f, 100000.0f);
        for (int axis = 0; axis < 3; ++axis)
            camera_position[axis] = viewport_target[axis] - forward[axis] * focus_distance;
        pos_x = camera_position[0]; pos_y = camera_position[1]; pos_z = camera_position[2];
        rot_x = camera_rotation[0]; rot_y = camera_rotation[1]; rot_z = camera_rotation[2];
        if (active_camera_node >= 0 && static_cast<size_t>(active_camera_node) < scene_nodes.size()) {
            auto& camera = scene_nodes[static_cast<size_t>(active_camera_node)];
            for (int axis = 0; axis < 3; ++axis) camera.worldTransform[12 + axis] = camera_position[axis];
        }
        record_keyframe_if_enabled();
    }

    void update_camera_pose_fields(bool recordKeyframe = true) {
        pos_x = camera_position[0]; pos_y = camera_position[1]; pos_z = camera_position[2];
        rot_x = camera_rotation[0]; rot_y = camera_rotation[1]; rot_z = camera_rotation[2];
        if (active_camera_node >= 0 && static_cast<size_t>(active_camera_node) < scene_nodes.size()) {
            auto& camera = scene_nodes[static_cast<size_t>(active_camera_node)];
            for (int axis = 0; axis < 3; ++axis) camera.worldTransform[12 + axis] = camera_position[axis];
        }
        update_camera_orbit_target();
        if (recordKeyframe) record_keyframe_if_enabled();
    }

    void move_active_camera(float forwardInput, float rightInput, float verticalInput,
                            float deltaSeconds, float speedMultiplier = 1.0f) {
        if (active_workspace != "Shot" || deltaSeconds <= 0.0f) return;
        constexpr float radians = 0.017453292519943295f;
        const float pitch = std::clamp(camera_rotation[0], -89.0f, 89.0f) * radians;
        const float yaw = camera_rotation[1] * radians;
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float sy = std::sin(yaw), cy = std::cos(yaw);
        float dx = sy * cp * forwardInput + cy * rightInput;
        float dy = sp * forwardInput + verticalInput;
        float dz = -cy * cp * forwardInput + sy * rightInput;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (length <= 1.0e-5f) return;
        dx /= length; dy /= length; dz /= length;
        const float speed = std::max(3.0f, focus_distance * 0.75f) * speedMultiplier;
        const float movement[3] = {dx * speed * deltaSeconds,
                                   dy * speed * deltaSeconds,
                                   dz * speed * deltaSeconds};
        for (int axis = 0; axis < 3; ++axis) {
            camera_position[axis] += movement[axis];
            viewport_target[axis] += movement[axis];
        }
        update_camera_pose_fields();
    }

    void move_viewport(float forwardInput, float rightInput, float verticalInput,
                       float deltaSeconds, float speedMultiplier = 1.0f) {
        if (!viewport_navigation_active || deltaSeconds <= 0.0f) return;
        constexpr float radians = 0.017453292519943295f;
        const float pitch = std::clamp(viewport_rotation[0], -89.0f, 89.0f) * radians;
        const float yaw = viewport_rotation[1] * radians;
        const float cp = std::cos(pitch), sp = std::sin(pitch);
        const float sy = std::sin(yaw), cy = std::cos(yaw);
        float dx = std::sin(yaw) * cp * forwardInput + cy * rightInput;
        float dy = sp * forwardInput + verticalInput;
        float dz = -std::cos(yaw) * cp * forwardInput + sy * rightInput;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (length <= 1.0e-5f) return;
        dx /= length;
        dy /= length;
        dz /= length;
        const float speed = std::max(3.0f, viewport_distance * 0.75f) * speedMultiplier;
        const float distance = speed * deltaSeconds;
        const float movement[3] = {dx * distance, dy * distance, dz * distance};
        for (int axis = 0; axis < 3; ++axis) {
            viewport_position[axis] += movement[axis];
            viewport_target[axis] += movement[axis];
        }
    }

    Keyframe capture_keyframe() const {
        Keyframe key;
        key.frame = current_frame;
        key.camera[0] = camera_position[0]; key.camera[1] = camera_position[1]; key.camera[2] = camera_position[2];
        key.camera[3] = camera_rotation[0]; key.camera[4] = camera_rotation[1]; key.camera[5] = camera_rotation[2];
        key.camera[6] = camera_focal_length;
        key.light[0] = key_light_x; key.light[1] = key_light_y;
        key.light[2] = key_light_z; key.light[3] = key_light_energy;
        return key;
    }

    void record_keyframe_if_enabled() {
        if (!is_autokey) return;
        if (transformable_selected()) {
            record_selected_node_keyframe();
            return;
        }
        Keyframe key = capture_keyframe();
        auto it = std::lower_bound(keyframes.begin(), keyframes.end(), current_frame,
            [](const Keyframe& item, int frame) { return item.frame < frame; });
        if (it != keyframes.end() && it->frame == current_frame) *it = key;
        else keyframes.insert(it, key);
        record_selected_node_keyframe();
    }

    void record_selected_node_keyframe() {
        if (selected_scene_node < 0 || static_cast<size_t>(selected_scene_node) >= scene_nodes.size()) return;
        const auto& node = scene_nodes[static_cast<size_t>(selected_scene_node)];
        if (node.type != zegfx::ProductionNodeType::Mesh && node.type != zegfx::ProductionNodeType::Group) return;
        NodeKeyframe key;
        key.node_index = selected_scene_node;
        key.node_id = static_cast<size_t>(selected_scene_node) < scene_node_ids.size()
            ? scene_node_ids[static_cast<size_t>(selected_scene_node)] : std::string();
        key.frame = current_frame;
        for (int axis = 0; axis < 3; ++axis) {
            key.position[axis] = node.worldTransform[12 + axis];
            key.rotation[axis] = node.rotationOffset[axis];
            key.scale[axis] = node.scaleFactor[axis];
        }
        auto it = std::lower_bound(node_keyframes.begin(), node_keyframes.end(), key,
            [](const NodeKeyframe& item, const NodeKeyframe& value) {
                return item.node_index < value.node_index ||
                    (item.node_index == value.node_index && item.frame < value.frame);
            });
        if (it != node_keyframes.end() && it->node_index == key.node_index && it->frame == key.frame) *it = key;
        else node_keyframes.insert(it, key);
    }

    void insert_keyframe() {
        Keyframe key = capture_keyframe();
        auto it = std::lower_bound(keyframes.begin(), keyframes.end(), current_frame,
            [](const Keyframe& item, int frame) { return item.frame < frame; });
        if (it != keyframes.end() && it->frame == current_frame) *it = key;
        else keyframes.insert(it, key);
        record_selected_node_keyframe();
    }

    void set_scene_node_state(uint32_t nodeIndex, bool visible, bool renderable) {
        if (nodeIndex >= scene_nodes.size()) return;
        pending_previous_visible = scene_nodes[nodeIndex].visible;
        pending_previous_renderable = scene_nodes[nodeIndex].renderable;
        scene_nodes[nodeIndex].visible = visible;
        scene_nodes[nodeIndex].renderable = renderable;
        pending_scene_node = static_cast<int>(nodeIndex);
        pending_command = "scene_node_state";
    }

    void set_scene_node_translation(uint32_t nodeIndex, const float position[3]) {
        if (nodeIndex >= scene_nodes.size() || !position) return;
        auto& node = scene_nodes[nodeIndex];
        float delta[3] = {};
        for (int axis = 0; axis < 3; ++axis) {
            pending_previous_translation[axis] = node.worldTransform[12 + axis];
            delta[axis] = position[axis] - pending_previous_translation[axis];
        }
        if (delta[0] == 0.0f && delta[1] == 0.0f && delta[2] == 0.0f) return;
        for (int axis = 0; axis < 3; ++axis) node.worldTransform[12 + axis] = position[axis];
        if (scene_node_children.size() != scene_nodes.size()) rebuild_scene_hierarchy();
        std::vector<uint32_t> pending{nodeIndex};
        while (!pending.empty()) {
            const uint32_t current = pending.back();
            pending.pop_back();
            if (current != nodeIndex)
                for (int axis = 0; axis < 3; ++axis) scene_nodes[current].worldTransform[12 + axis] += delta[axis];
            if (current < scene_node_children.size())
                pending.insert(pending.end(), scene_node_children[current].begin(), scene_node_children[current].end());
        }
        pending_scene_node = static_cast<int>(nodeIndex);
        pending_command = "scene_node_translation";
        if (is_autokey && selected_scene_node == static_cast<int>(nodeIndex)) record_selected_node_keyframe();
    }

    void set_scene_node_light_energy(uint32_t nodeIndex, float energy) {
        if (nodeIndex >= scene_nodes.size() || scene_nodes[nodeIndex].type != zegfx::ProductionNodeType::Light) return;
        auto& node = scene_nodes[nodeIndex];
        if (node.lightEnergy == energy) return;
        pending_previous_light_energy = node.lightEnergy;
        node.lightEnergy = std::clamp(energy, 0.0f, 100.0f);
        pending_scene_node = static_cast<int>(nodeIndex);
        pending_command = "scene_node_light_energy";
        if (is_autokey && selected_scene_node == static_cast<int>(nodeIndex)) record_selected_node_keyframe();
    }

    void revert_scene_node_light_energy(uint32_t nodeIndex) {
        if (nodeIndex >= scene_nodes.size()) return;
        scene_nodes[nodeIndex].lightEnergy = pending_previous_light_energy;
        if (selected_scene_node == static_cast<int>(nodeIndex)) key_light_energy = pending_previous_light_energy;
    }

    void revert_scene_node_translation(uint32_t nodeIndex) {
        if (nodeIndex >= scene_nodes.size()) return;
        float delta[3];
        for (int axis = 0; axis < 3; ++axis)
            delta[axis] = scene_nodes[nodeIndex].worldTransform[12 + axis] - pending_previous_translation[axis];
        if (scene_node_children.size() != scene_nodes.size()) rebuild_scene_hierarchy();
        std::vector<uint32_t> pending{nodeIndex};
        while (!pending.empty()) {
            const uint32_t current = pending.back();
            pending.pop_back();
            for (int axis = 0; axis < 3; ++axis) scene_nodes[current].worldTransform[12 + axis] -= delta[axis];
            if (current < scene_node_children.size())
                pending.insert(pending.end(), scene_node_children[current].begin(), scene_node_children[current].end());
        }
        if (selected_scene_node == static_cast<int>(nodeIndex)) {
            pos_x = pending_previous_translation[0];
            pos_y = pending_previous_translation[1];
            pos_z = pending_previous_translation[2];
        }
    }

    void set_scene_node_rotation_scale(uint32_t nodeIndex, const float rotation[3], const float scale[3]) {
        if (nodeIndex >= scene_nodes.size() || !rotation || !scale) return;
        auto& node = scene_nodes[nodeIndex];
        bool unchanged = true;
        for (int axis = 0; axis < 3; ++axis) {
            pending_previous_rotation[axis] = node.rotationOffset[axis];
            pending_previous_scale[axis] = node.scaleFactor[axis];
            unchanged = unchanged && rotation[axis] == node.rotationOffset[axis] && scale[axis] == node.scaleFactor[axis];
        }
        if (unchanged) return;
        std::copy(rotation, rotation + 3, node.rotationOffset);
        std::copy(scale, scale + 3, node.scaleFactor);
        pending_scene_node = static_cast<int>(nodeIndex);
        pending_command = "scene_node_rotation_scale";
        if (is_autokey && selected_scene_node == static_cast<int>(nodeIndex)) record_selected_node_keyframe();
    }

    void revert_scene_node_rotation_scale(uint32_t nodeIndex) {
        if (nodeIndex >= scene_nodes.size()) return;
        std::copy(pending_previous_rotation, pending_previous_rotation + 3,
                  scene_nodes[nodeIndex].rotationOffset);
        std::copy(pending_previous_scale, pending_previous_scale + 3,
                  scene_nodes[nodeIndex].scaleFactor);
        if (selected_scene_node == static_cast<int>(nodeIndex)) {
            rot_x = pending_previous_rotation[0]; rot_y = pending_previous_rotation[1]; rot_z = pending_previous_rotation[2];
            scale_x = pending_previous_scale[0]; scale_y = pending_previous_scale[1]; scale_z = pending_previous_scale[2];
        }
    }

    void evaluate_keyframes() {
        if (keyframes.empty()) {
            evaluate_node_keyframes();
            return;
        }
        auto upper = std::lower_bound(keyframes.begin(), keyframes.end(), current_frame,
            [](const Keyframe& key, int frame) { return key.frame < frame; });
        const Keyframe* a = nullptr;
        const Keyframe* b = nullptr;
        if (upper == keyframes.begin()) a = b = &*upper;
        else if (upper == keyframes.end()) a = b = &keyframes.back();
        else if (upper->frame == current_frame) a = b = &*upper;
        else { a = &*(upper - 1); b = &*upper; }
        const float t = (a == b || b->frame == a->frame) ? 0.0f :
            std::clamp(static_cast<float>(current_frame - a->frame) /
                       static_cast<float>(b->frame - a->frame), 0.0f, 1.0f);
        float values[11]{};
        for (int i = 0; i < 7; ++i) values[i] = a->camera[i] + (b->camera[i] - a->camera[i]) * t;
        for (int i = 0; i < 4; ++i) values[7 + i] = a->light[i] + (b->light[i] - a->light[i]) * t;
        camera_position[0] = values[0]; camera_position[1] = values[1]; camera_position[2] = values[2];
        camera_rotation[0] = values[3]; camera_rotation[1] = values[4]; camera_rotation[2] = values[5];
        camera_focal_length = values[6];
        if (camera_selected()) {
            pos_x = values[0]; pos_y = values[1]; pos_z = values[2];
            rot_x = values[3]; rot_y = values[4]; rot_z = values[5]; focal_length = values[6];
        }
        key_light_x = values[7]; key_light_y = values[8]; key_light_z = values[9];
        key_light_energy = values[10];
        if (active_camera_node >= 0 && static_cast<size_t>(active_camera_node) < scene_nodes.size()) {
            scene_nodes[static_cast<size_t>(active_camera_node)].worldTransform[12] = camera_position[0];
            scene_nodes[static_cast<size_t>(active_camera_node)].worldTransform[13] = camera_position[1];
            scene_nodes[static_cast<size_t>(active_camera_node)].worldTransform[14] = camera_position[2];
        }
        if (active_workspace == "Shot") update_camera_orbit_target();
        if (key_light_node >= 0 && static_cast<size_t>(key_light_node) < scene_nodes.size()) {
            scene_nodes[static_cast<size_t>(key_light_node)].worldTransform[12] = key_light_x;
            scene_nodes[static_cast<size_t>(key_light_node)].worldTransform[13] = key_light_y;
            scene_nodes[static_cast<size_t>(key_light_node)].worldTransform[14] = key_light_z;
        }
        evaluate_node_keyframes();
    }

    void evaluate_node_keyframes() {
        bool changed = false;
        size_t keyCursor = 0;
        for (size_t nodeIndex = 0; nodeIndex < scene_nodes.size(); ++nodeIndex) {
            while (keyCursor < node_keyframes.size() && node_keyframes[keyCursor].node_index < static_cast<int>(nodeIndex)) ++keyCursor;
            const size_t firstIndex = keyCursor;
            while (keyCursor < node_keyframes.size() && node_keyframes[keyCursor].node_index == static_cast<int>(nodeIndex)) ++keyCursor;
            if (firstIndex == keyCursor) continue;
            const size_t lastIndex = keyCursor - 1;
            const NodeKeyframe* first = node_keyframes.data() + firstIndex;
            const NodeKeyframe* end = node_keyframes.data() + keyCursor;
            const auto upper = std::lower_bound(first, end, current_frame,
                [](const NodeKeyframe& key, int frame) { return key.frame < frame; });
            const NodeKeyframe* before = upper == first ? first : upper - 1;
            const NodeKeyframe* after = upper == end ? &node_keyframes[lastIndex] : upper;
            const float t = before == after || after->frame == before->frame ? 0.0f :
                std::clamp(static_cast<float>(current_frame - before->frame) /
                           static_cast<float>(after->frame - before->frame), 0.0f, 1.0f);
            auto& node = scene_nodes[nodeIndex];
            for (int axis = 0; axis < 3; ++axis) {
                const float position = before->position[axis] + (after->position[axis] - before->position[axis]) * t;
                const float rotationDelta = std::remainder(after->rotation[axis] - before->rotation[axis], 360.0f);
                const float rotation = before->rotation[axis] + rotationDelta * t;
                const float scale = before->scale[axis] + (after->scale[axis] - before->scale[axis]) * t;
                changed = changed || node.worldTransform[12 + axis] != position ||
                    node.rotationOffset[axis] != rotation || node.scaleFactor[axis] != scale;
                node.worldTransform[12 + axis] = position;
                node.rotationOffset[axis] = rotation;
                node.scaleFactor[axis] = scale;
            }
            if (selected_scene_node == static_cast<int>(nodeIndex)) {
                pos_x = node.worldTransform[12]; pos_y = node.worldTransform[13]; pos_z = node.worldTransform[14];
                rot_x = node.rotationOffset[0]; rot_y = node.rotationOffset[1]; rot_z = node.rotationOffset[2];
                scale_x = node.scaleFactor[0]; scale_y = node.scaleFactor[1]; scale_z = node.scaleFactor[2];
            }
        }
        if (changed) pending_command = "evaluate_scene_animation";
    }
};
} // namespace medusa