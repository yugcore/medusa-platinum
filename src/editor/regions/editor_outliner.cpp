#include <medusa/editor/regions/editor_outliner.hpp>
#include <medusa/core/medusa_theme.hpp>
#include <medusa/ui/medusa_icons.hpp>
#include <algorithm>
#include <cmath>

namespace medusa {

OutlinerRegion::OutlinerRegion(EditorState &state) : state_(state) {}

void OutlinerRegion::layout(const Rect &bounds) {
    bounds_ = bounds;
    header_rect_ = Rect(bounds.x, bounds.y, bounds.width, Theme::kAreaHeaderHeight);
    summary_rect_ = Rect(bounds.x + 8.0f, bounds.y + 36.0f, bounds.width - 16.0f, Theme::kControlHeight);
    rebuild_rows();
    row_viewport_ = Rect(bounds.x, bounds.y + 66.0f, bounds.width, std::max(0.0f, bounds.bottom() - (bounds.y + 66.0f)));
    const float contentHeight = static_cast<float>(rows_.size()) * Theme::kOutlinerRowHeight;
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, std::max(0.0f, contentHeight - row_viewport_.height));
    position_rows();
}

void OutlinerRegion::draw(Graphics &gfx) {
    layout(bounds_);
    gfx.fill_rect(bounds_, Theme::kPanelBg);

    // Header (Blender Outliner style)
    gfx.fill_rect(header_rect_, Theme::kPanelHeaderBg);
    gfx.draw_line(Point(header_rect_.x, header_rect_.bottom()), Point(header_rect_.right(), header_rect_.bottom()), Theme::kBorderSubtle, 1.0f);

    // Outliner Icon
    Rect icon_r(header_rect_.x + 8.0f, header_rect_.y + 5.0f, 14.0f, 14.0f);
    IconRenderer::draw_icon(gfx, "outliner_collection", icon_r, Theme::kTextSecondary);

    // Title
    const std::string title = state_.scene_name.empty() ? "Scene Collection" : state_.scene_name;
    gfx.draw_text(title, Rect(header_rect_.x + 30.0f, header_rect_.y, header_rect_.width - 90.0f, header_rect_.height),
                  Theme::kTextPrimary, Theme::kPanelTitleFont, true, false, true, Theme::kUiFontFamily);

    // Restriction Column Icons on Header
    float col_x = header_rect_.right() - 56.0f;
    IconRenderer::draw_icon(gfx, "toggle_eye_on", Rect(col_x, header_rect_.y + 5.0f, 14.0f, 14.0f), Theme::kTextMuted);
    col_x += 18.0f;
    IconRenderer::draw_icon(gfx, "toggle_lock_on", Rect(col_x, header_rect_.y + 5.0f, 14.0f, 14.0f), Theme::kTextMuted);
    col_x += 18.0f;
    IconRenderer::draw_icon(gfx, "camera_actor", Rect(col_x, header_rect_.y + 5.0f, 14.0f, 14.0f), Theme::kTextMuted);

    gfx.fill_rect(summary_rect_, Theme::kControlBg);
    gfx.draw_rect(summary_rect_, Theme::kBorderSubtle, 1.0f);
    const std::string summary = std::to_string(state_.scene_nodes.size()) + " scene items";
    gfx.draw_text(summary, Rect(summary_rect_.x + 8.0f, summary_rect_.y,
                                summary_rect_.width - 16.0f, summary_rect_.height),
                  Theme::kTextSecondary, Theme::kSmallFont, false, false, true, Theme::kUiFontFamily);

    // Draw only rows intersecting the visible portion of the hierarchy.
    gfx.push_clip(row_viewport_);
    const size_t firstVisible = first_visible_row();
    const size_t lastVisible = last_visible_row();
    for (size_t i = firstVisible; i < lastVisible; ++i) rows_[i].draw(gfx);
    gfx.pop_clip();

    const float contentHeight = static_cast<float>(rows_.size()) * Theme::kOutlinerRowHeight;
    if (contentHeight > row_viewport_.height && row_viewport_.height > 0.0f) {
        const float trackX = bounds_.right() - 4.0f;
        const Rect track(trackX, row_viewport_.y, 3.0f, row_viewport_.height);
        gfx.fill_rect(track, Theme::kScrollbarTrack);
        const float thumbHeight = std::max(18.0f, track.height * row_viewport_.height / contentHeight);
        const float travel = std::max(0.0f, track.height - thumbHeight);
        const float range = std::max(1.0f, contentHeight - row_viewport_.height);
        gfx.fill_rect(Rect(trackX, track.y + travel * scroll_offset_ / range, track.width, thumbHeight),
                      Theme::kScrollbarThumb);
    }
    if (state_.scene_nodes.empty()) {
        const float center = bounds_.x + bounds_.width * 0.5f;
        const float top = summary_rect_.bottom() + 30.0f;
        IconRenderer::draw_icon(gfx, "file_import", Rect(center - 12.0f, top, 24.0f, 24.0f), Theme::kTextMuted);
        gfx.draw_text("Scene is empty", Rect(bounds_.x + 12.0f, top + 30.0f, bounds_.width - 24.0f, 20.0f),
                      Theme::kTextSecondary, 12.0f, true, true, true, Theme::kUiFontFamily);
        gfx.draw_text("Import a model to populate the collection.",
                      Rect(bounds_.x + 12.0f, top + 51.0f, bounds_.width - 24.0f, 34.0f),
                      Theme::kTextMuted, Theme::kSmallFont, false, true, true, Theme::kUiFontFamily);
    }
}

bool OutlinerRegion::handle_mouse_down(const MouseEvent &e) {
    rebuild_rows();
    position_rows();
    if (!row_viewport_.contains(e.pos)) return false;
    const float contentHeight = static_cast<float>(rows_.size()) * Theme::kOutlinerRowHeight;
    if (e.button == MouseButton::Left && contentHeight > row_viewport_.height &&
        e.pos.x >= bounds_.right() - 7.0f) {
        is_drag_scrolling_ = true;
        scroll_drag_y_ = e.pos.y;
        scroll_drag_start_ = scroll_offset_;
        return true;
    }
    const size_t firstVisible = first_visible_row();
    const size_t lastVisible = last_visible_row();
    for (size_t i = firstVisible; i < lastVisible; ++i) {
        if (!rows_[i].get_bounds().contains(e.pos)) continue;
        if (state_.active_workspace == "Shot" && i < row_node_indices_.size() &&
            static_cast<int>(row_node_indices_[i]) != state_.active_camera_node) return true;
        if (rows_[i].handle_mouse_down(e)) return true;
    }
    return false;
}

bool OutlinerRegion::handle_mouse_move(const MouseEvent &e) {
    rebuild_rows();
    if (is_drag_scrolling_) {
        const float contentHeight = static_cast<float>(rows_.size()) * Theme::kOutlinerRowHeight;
        const float maxScroll = std::max(0.0f, contentHeight - row_viewport_.height);
        const float thumbHeight = std::max(18.0f, row_viewport_.height * row_viewport_.height /
                                                   std::max(1.0f, contentHeight));
        const float thumbTravel = std::max(1.0f, row_viewport_.height - thumbHeight);
        scroll_offset_ = std::clamp(scroll_drag_start_ + (e.pos.y - scroll_drag_y_) * maxScroll / thumbTravel,
                                    0.0f, maxScroll);
        position_rows();
        return true;
    }
    position_rows();
    bool changed = false;
    const size_t firstVisible = first_visible_row();
    const size_t lastVisible = last_visible_row();
    for (size_t i = firstVisible; i < lastVisible; ++i)
        if (rows_[i].handle_mouse_move(e)) changed = true;
    return changed;
}

bool OutlinerRegion::handle_mouse_up(const MouseEvent &e) {
    rebuild_rows();
    if (is_drag_scrolling_) {
        is_drag_scrolling_ = false;
        return true;
    }
    position_rows();
    const size_t firstVisible = first_visible_row();
    const size_t lastVisible = last_visible_row();
    for (size_t i = firstVisible; i < lastVisible; ++i)
        if (rows_[i].handle_mouse_up(e)) return true;
    return false;
}

bool OutlinerRegion::handle_mouse_wheel(float delta, Point p) {
    if (!row_viewport_.contains(p) || rows_.empty() || std::abs(delta) < 1.0e-4f) return false;
    const float contentHeight = static_cast<float>(rows_.size()) * Theme::kOutlinerRowHeight;
    const float maxScroll = std::max(0.0f, contentHeight - row_viewport_.height);
    if (maxScroll <= 0.0f) return false;
    scroll_offset_ = std::clamp(scroll_offset_ - (delta > 0.0f ? 3.0f : -3.0f) * Theme::kOutlinerRowHeight,
                                0.0f, maxScroll);
    position_rows();
    return true;
}

size_t OutlinerRegion::first_visible_row() const {
    return std::min(rows_.size(), static_cast<size_t>(scroll_offset_ / Theme::kOutlinerRowHeight));
}

size_t OutlinerRegion::last_visible_row() const {
    const size_t visible = static_cast<size_t>(std::ceil(row_viewport_.height / Theme::kOutlinerRowHeight)) + 1;
    return std::min(rows_.size(), first_visible_row() + visible);
}

const char* OutlinerRegion::icon_for(const zegfx::ProductionSceneNode& node) {
    switch (node.type) {
        case zegfx::ProductionNodeType::Mesh: return "outliner_mesh";
        case zegfx::ProductionNodeType::Camera: return "camera_actor";
        case zegfx::ProductionNodeType::Light:
            if (node.areaLight) return "light_area";
            if (node.lightType == zegfx::LightType::Directional) return "light_directional";
            if (node.lightType == zegfx::LightType::Spot) return "light_spot";
            return "light_point";
        case zegfx::ProductionNodeType::Group: default: return "outliner_collection";
    }
}

bool OutlinerRegion::has_children(uint32_t index) const {
    return index < state_.scene_node_children.size() && !state_.scene_node_children[index].empty();
}

void OutlinerRegion::append_node(uint32_t index, int depth, std::vector<uint8_t>& visited) {
    if (index >= state_.scene_nodes.size() || visited[index]) return;
    visited[index] = 1;
    auto& node = state_.scene_nodes[index];
    const bool children = has_children(index);
    rows_.emplace_back(node.name, icon_for(node), depth, children, node.expanded,
        state_.selected_scene_node == static_cast<int>(index), node.visible,
        node.locked || (state_.active_workspace == "Shot" && static_cast<int>(index) != state_.active_camera_node),
        node.renderable,
        [this, index]() {
            state_.select_scene_node(index);
        },
        [this, index](bool expanded) {
            if (index < state_.scene_nodes.size()) state_.scene_nodes[index].expanded = expanded;
            rows_generation_ = ~uint64_t(0);
        },
        [this, index](bool visible) {
            if (index < state_.scene_nodes.size())
                state_.set_scene_node_state(index, visible, state_.scene_nodes[index].renderable);
        },
        [this, index](bool locked) {
            if (index < state_.scene_nodes.size()) state_.scene_nodes[index].locked = locked;
        },
        [this, index](bool renderable) {
            if (index < state_.scene_nodes.size())
                state_.set_scene_node_state(index, state_.scene_nodes[index].visible, renderable);
        });
    row_node_indices_.push_back(index);
    if (node.expanded) {
        if (index < state_.scene_node_children.size())
            for (uint32_t child : state_.scene_node_children[index])
                append_node(child, depth + 1, visited);
    }
}

void OutlinerRegion::rebuild_rows() {
    if (state_.scene_node_children.size() != state_.scene_nodes.size()) state_.rebuild_scene_hierarchy();
    if (rows_generation_ == state_.scene_generation) {
        for (size_t i = 0; i < rows_.size() && i < row_node_indices_.size(); ++i) {
            const uint32_t index = row_node_indices_[i];
            if (index >= state_.scene_nodes.size()) continue;
            const auto& node = state_.scene_nodes[index];
            rows_[i].set_selected(state_.selected_scene_node == static_cast<int>(index));
            rows_[i].set_visible(node.visible);
            rows_[i].set_locked(node.locked ||
                (state_.active_workspace == "Shot" && static_cast<int>(index) != state_.active_camera_node));
            rows_[i].set_renderable(node.renderable);
            rows_[i].set_expanded(node.expanded);
        }
        return;
    }
    rows_.clear();
    row_node_indices_.clear();
    std::vector<uint8_t> visited(state_.scene_nodes.size(), 0);
    for (uint32_t i = 0; i < state_.scene_nodes.size(); ++i) {
        const int parent = state_.scene_nodes[i].parentIndex;
        if (parent < 0 || static_cast<size_t>(parent) >= state_.scene_nodes.size())
            append_node(i, 0, visited);
    }
    for (uint32_t i = 0; i < state_.scene_nodes.size(); ++i)
        if (!visited[i]) append_node(i, 0, visited);
    rows_generation_ = state_.scene_generation;
}

void OutlinerRegion::position_rows() {
    const size_t first = first_visible_row();
    const size_t last = last_visible_row();
    for (size_t i = first; i < last; ++i) {
        const float y = row_viewport_.y + static_cast<float>(i) * Theme::kOutlinerRowHeight - scroll_offset_;
        rows_[i].set_bounds(Rect(bounds_.x, y, bounds_.width, Theme::kOutlinerRowHeight));
    }
}

} // namespace medusa
