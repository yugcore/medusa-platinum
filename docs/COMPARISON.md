# Comparison of Editor

**Source snapshot:** the current Medusa Platinum folder after the replacement, compared with the local read-only Blender editor source snapshot.

**Purpose:** map each Medusa editor surface to the closest Blender editor code and show short source excerpts side by side. These are comparisons, not a diff and not a claim that every Blender feature exists in Medusa. No source files were changed to create this document.

**Important timeline distinction:** Medusa names its control SequencerWidget, but the current implementation draws a frame ruler, a playhead, and keyframe markers. Blender’s Sequencer draws clip strips. I compare Medusa’s current timeline to Blender’s Dope Sheet for keyframes, then show Blender’s actual Sequencer draw path separately.

Blender is a much larger multi-editor system. The Blender column points to the closest matching code in the supplied editor-source folder. Generic window-manager, data, rendering, and file-format layers often live outside that folder, so those are identified as architectural boundaries rather than forced into a false one-to-one match.

## Medusa editor source inventory

| Medusa source | What it owns | Closest Blender source |
|---|---|---|
| src/main.cpp, src/medusa_app.cpp | Win32 entry point, application startup, window creation, screenshot command path | Blender window-manager/application startup is outside the supplied editor-source subset |
| src/medusa_window.cpp | Native Win32 event bridge, file dialogs, command dispatch, project title/dirty state, undo history, recovery timer, animation/render timers | screen/area.cc, space_topbar/space_topbar.cc, editor keymaps/operators; Blender’s event manager itself is outside this subset |
| src/medusa_editor.cpp | Topbar, tool shelf, viewport, outliner, attributes/properties, timeline, fixed editor assembly, splitters, viewport navigation and gizmo hit testing | space_topbar, space_view3d, space_outliner, space_buttons, space_action, space_sequencer, transform |
| src/medusa_screen.cpp | Area/region bounds, clipping, draw traversal, mouse-event routing and capture | screen/area.cc and per-space region callbacks |
| src/medusa_widgets.cpp | Buttons, scrubbable numbers, collapsible headers, outliner rows, timeline ruler/playhead | interface/interface_widgets.cc, interface/interface_layout.cc, space_outliner, space_action |
| src/medusa_graphics.cpp | Direct2D/D3D drawing primitives, text, clipping, production preview bridge | space_view3d/view3d_draw.cc for editor drawing; renderer/GPU implementation is outside the supplied editor subset |
| src/medusa_icons.cpp, resources/icons/*.svg | Medusa’s custom SVG/path icon loading, tinting and cache | interface/interface_icons.cc; Blender icon IDs are comparison only, Medusa’s own icons stay in use |
| include/medusa/medusa_theme.hpp | Compile-time charcoal palette, type sizes, panel and control dimensions | Blender theme lookup and widget-state colors in interface/interface_widgets.cc and screen/area.cc |
| include/medusa/medusa_editor.hpp, medusa_types.hpp, medusa_screen.hpp, medusa_widgets.hpp, medusa_graphics.hpp, medusa_window.hpp | Editor state, geometry/event types, widget and region interfaces, rendering/window contracts | Blender uses editor-space structures, context, RNA properties, region types and operators |
| src/medusa_project.cpp, include/medusa/medusa_project.hpp | Project serialization, stable IDs, validation, save/recovery snapshots | Blender file read/write and undo data layers are outside the supplied editor subset |
| src/medusa_production_scene.cpp, include/medusa/medusa_production_scene.hpp | Imported scene conversion and renderer-facing scene data | Blender importers/data model and render engine are outside the editor UI subset |

## What the current source actually implements

| Medusa surface | Current implementation | Blender comparison |
|---|---|---|
| Editor shell | Six manually assembled areas: topbar, tool shelf, viewport, outliner, attributes and timeline. Splitter sizes are stored as editor state. | Blender registers editor spaces and region types with lifecycle, event, draw, keymap and gizmo callbacks. |
| Topbar | Fixed File/Edit/Create/Camera/Render/Window menus, three Medusa workflow tabs, one render action. | Blender’s topbar is built with registered menus/layout operators and dynamic workspace state. |
| Viewport | Direct2D overlays plus a ZeGFX production preview, hand-built camera HUD, grid, orientation widget and transform gizmo. | Blender uses the View3D draw pipeline and registered GPU gizmos/operators. |
| Outliner | Imported scene hierarchy with expand/select and viewport visibility, lock and renderability toggles. | Blender’s tree supports many Blender data types, RNA-backed restriction columns, sync and search. |
| Properties | Context-switched transform, lighting, camera optics and sampling panels. Numeric values are custom scrubbable widgets. | Blender panels are registered and polled; properties are generally bound through RNA and reusable layouts. |
| Timeline | Frame ruler, current frame, playhead, keyframe dots, range/FPS controls and transport buttons. | Closest match is Blender’s Action/Dope Sheet timeline. Blender’s Sequencer is a separate strip editor. |
| Icons/theme | Medusa-owned SVG/path icons and a compact charcoal token set. | Blender has a theme system and icon registry; the comparison does not copy Blender art assets. |

### 1. Area shell and region registration

Medusa directly creates six Area objects and chooses their drawing order. Blender registers a SpaceType and individual ARegionType callbacks, including lifecycle, draw, event, keymap, gizmo, and sidebar behavior. This is the closest architectural match for the editor shell.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L2270">src/medusa_editor.cpp</a> · lines 2270-2303</small><pre><code class="language-cpp">void MedusaEditor::setup_regions() {
    clear_areas();

    topbar_area_ = std::make_shared&lt;Area&gt;(&quot;Topbar&quot;);

    toolshelf_area_ = std::make_shared&lt;Area&gt;(&quot;Toolshelf&quot;);
    toolshelf_area_-&gt;add_region(std::make_shared&lt;ToolshelfRegion&gt;(state_));

    viewport_area_ = std::make_shared&lt;Area&gt;(&quot;Viewport&quot;);
    viewport_area_-&gt;add_region(std::make_shared&lt;ViewportRegion&gt;(state_));

    outliner_area_ = std::make_shared&lt;Area&gt;(&quot;Outliner&quot;);
    outliner_area_-&gt;add_region(std::make_shared&lt;OutlinerRegion&gt;(state_));
    add_area(outliner_area_);

    attribute_area_ = std::make_shared&lt;Area&gt;(&quot;Attributes&quot;);
    attribute_area_-&gt;add_region(std::make_shared&lt;AttributeRegion&gt;(state_));
    add_area(attribute_area_);

    timeline_area_ = std::make_shared&lt;Area&gt;(&quot;Timeline&quot;);
    timeline_area_-&gt;add_region(std::make_shared&lt;TimelineRegion&gt;(state_));
    add_area(timeline_area_);

    // Viewport popovers and delayed tooltips draw over panels without touching
    // their non-overlapping bounds.
    add_area(viewport_area_);

    // Draw the toolbar after neighboring panels so delayed tooltips stay legible.
    add_area(toolshelf_area_);

    // Draw the topbar last so its dropdowns render above the surrounding panels.
    topbar_area_-&gt;add_region(std::make_shared&lt;TopbarRegion&gt;(state_));
    add_area(topbar_area_);
}</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_view3d/space_view3d.cc#L1672">../Blender_Editor_READONLY/space_view3d/space_view3d.cc</a> · lines 1672-1716</small><pre><code class="language-cpp">void ED_spacetype_view3d()
{
  using namespace blender::ed;
  std::unique_ptr&lt;SpaceType&gt; st = std::make_unique&lt;SpaceType&gt;();
  ARegionType *art;

  st-&gt;spaceid = SPACE_VIEW3D;
  STRNCPY_UTF8(st-&gt;name, &quot;View3D&quot;);
  st-&gt;struct_size = sizeof(View3D);

  st-&gt;create = view3d_create;
  st-&gt;free = view3d_free;
  st-&gt;init = view3d_init;
  st-&gt;exit = view3d_exit;
  st-&gt;listener = space_view3d_listener;
  st-&gt;refresh = space_view3d_refresh;
  st-&gt;duplicate = view3d_duplicate;
  st-&gt;operatortypes = view3d_operatortypes;
  st-&gt;keymap = view3d_keymap;
  st-&gt;dropboxes = view3d_dropboxes;
  st-&gt;gizmos = view3d_widgets;
  st-&gt;context = view3d_context;
  st-&gt;id_remap = view3d_id_remap;
  st-&gt;foreach_id = view3d_foreach_id;
  st-&gt;blend_read_data = view3d_space_blend_read_data;
  st-&gt;blend_read_after_liblink = nullptr;
  st-&gt;blend_write = view3d_space_blend_write;

  /* regions: main window */
  art = MEM_new_zeroed&lt;ARegionType&gt;(&quot;spacetype view3d main region&quot;);
  art-&gt;regionid = RGN_TYPE_WINDOW;
  art-&gt;keymapflag = ED_KEYMAP_GIZMO | ED_KEYMAP_TOOL | ED_KEYMAP_GPENCIL;
  art-&gt;draw = view3d_main_region_draw;
  art-&gt;init = view3d_main_region_init;
  art-&gt;exit = view3d_main_region_exit;
  art-&gt;free = view3d_main_region_free;
  art-&gt;duplicate = view3d_main_region_duplicate;
#ifdef WITH_INPUT_IME
  art-&gt;cursor_ime = view3d_main_region_cursor_ime;
#endif
  art-&gt;listener = view3d_main_region_listener;
  art-&gt;message_subscribe = view3d_main_region_message_subscribe;
  art-&gt;cursor = view3d_main_region_cursor;
  art-&gt;lock = REGION_DRAW_LOCK_ALL;
  BLI_addhead(&amp;st-&gt;regiontypes, art);</code></pre></td>
</tr></tbody>
</table>

### 2. Topbar menus and workspace switching

Medusa stores a fixed menu/workspace list and builds local ButtonWidget instances. Blender menus create UI layout items that invoke registered operators; its workspace strip is backed by workspace data, rather than a hard-coded three-name list.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L197">src/medusa_editor.cpp</a> · lines 197-228</small><pre><code class="language-cpp">    void init_widgets() {
        // Clean menus without duplicates: File, Edit, Create, Camera, Render, Window
        std::vector&lt;std::string&gt; menus = { &quot;File&quot;, &quot;Edit&quot;, &quot;Create&quot;, &quot;Camera&quot;, &quot;Render&quot;, &quot;Window&quot; };
        for (const auto &amp;m : menus) {
            ButtonWidget button(m, &quot;&quot;, nullptr);
            button.set_flat(true);
            menu_buttons_.push_back(button);
        }

        // Medusa workflow: one shared project moves from set assembly to shots
        // and then to final output.
        std::vector&lt;std::string&gt; modes = { &quot;Scene&quot;, &quot;Shot&quot;, &quot;Production&quot; };
        const std::vector&lt;std::string&gt; mode_tips = {
            &quot;Build the set: add assets, arrange objects and set the lighting&quot;,
            &quot;Create shots, animate cameras and objects, and preview timing&quot;,
            &quot;Choose render quality and output the finished frames&quot;
        };
        for (size_t i = 0; i &lt; modes.size(); ++i) {
            const auto &amp;mode = modes[i];
            bool is_active = (mode == state_.active_workspace);
            ButtonWidget btn(mode, &quot;&quot;, nullptr);
            btn.set_active(is_active);
            btn.set_flat(true);
            btn.set_tooltip(mode_tips[i]);
            mode_buttons_.push_back(btn);
        }

        render_btn_ = ButtonWidget(&quot;Render Sequence&quot;, &quot;action_render&quot;, [this]() { state_.pending_command = &quot;render_sequence&quot;; });
        render_btn_.set_flat(true);
        render_btn_.set_primary(true);
        render_btn_.set_tooltip(&quot;Render the active frame range to image files&quot;);
    }</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_topbar/space_topbar.cc#L239">../Blender_Editor_READONLY/space_topbar/space_topbar.cc</a> · lines 239-260</small><pre><code class="language-cpp">static void recent_files_menu_draw(const bContext *C, Menu *menu)
{
  ui::Layout &amp;layout = *menu-&gt;layout;
  layout.operator_context_set(wm::OpCallContext::InvokeDefault);
  const bool is_menu_search = CTX_data_int_get(C, &quot;is_menu_search&quot;).value_or(false);
  if (is_menu_search) {
    template_recent_files(&amp;layout, U.recent_files);
  }
  else {
    const int limit = std::min&lt;int&gt;(U.recent_files, 20);
    if (recent_files_menu_list(C, layout, limit) != 0) {
      layout.separator();
      PointerRNA search_props = layout.op(
          &quot;WM_OT_search_single_menu&quot;, IFACE_(&quot;More...&quot;), ICON_VIEWZOOM);
      RNA_string_set(&amp;search_props, &quot;menu_idname&quot;, &quot;TOPBAR_MT_file_open_recent&quot;);
      layout.op(&quot;WM_OT_clear_recent_files&quot;, IFACE_(&quot;Clear Recent Files List...&quot;), ICON_TRASH);
    }
    else {
      layout.label(IFACE_(&quot;No Recent Files&quot;), ICON_NONE);
    }
  }
}</code></pre></td>
</tr></tbody>
</table>

### 3. Tool shelf

Medusa enumerates seven tool IDs and creates icon buttons in the region. Blender declares a tools region and delegates its drawing, snapping, and tool behavior to the tool-system region callbacks.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L416">src/medusa_editor.cpp</a> · lines 416-439</small><pre><code class="language-cpp">    void init_tools() {
        std::vector&lt;std::pair&lt;std::string, std::string&gt;&gt; tools = {
            { &quot;tool_select&quot;, &quot;Select imported scene items from the Scene Collection&quot; },
            { &quot;tool_move&quot;, &quot;Move the selected object with axis and plane handles&quot; },
            { &quot;tool_rotate&quot;, &quot;Rotate the selected object around an axis&quot; },
            { &quot;tool_scale&quot;, &quot;Scale the selected object along an axis&quot; },
            { &quot;tool_snapping&quot;, &quot;Toggle transform snapping&quot; },
            { &quot;tool_pivot&quot;, &quot;Frame the selected object in the viewport&quot; },
            { &quot;tool_camera_pilot&quot;, &quot;Camera Pilot: right-drag to look, WASD to move, Q/E for vertical movement. Shift speeds up; Ctrl slows down. Shift+F enters, Esc exits.&quot; }
        };

        for (size_t i = 0; i &lt; tools.size(); ++i) {
            std::function&lt;void()&gt; action;
            if (tools[i].first == &quot;tool_pivot&quot;)
                action = [this]() { state_.pending_command = &quot;frame_selected&quot;; };
            ButtonWidget btn(&quot;&quot;, tools[i].first, std::move(action));
            btn.set_flat(true);
            btn.set_flat_active_fill(false);
            btn.set_tooltip(tools[i].second);
            tool_ids_.push_back(tools[i].first);
            if (i == 0) btn.set_active(true);
            tool_buttons_.push_back(btn);
        }
    }</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_view3d/space_view3d.cc#L1734">../Blender_Editor_READONLY/space_view3d/space_view3d.cc</a> · lines 1734-1756</small><pre><code class="language-cpp">  /* regions: tool(bar) */
  art = MEM_new_zeroed&lt;ARegionType&gt;(&quot;spacetype view3d tools region&quot;);
  art-&gt;regionid = RGN_TYPE_TOOLS;
  art-&gt;prefsizex = int(UI_TOOLBAR_WIDTH);
  art-&gt;prefsizey = 50; /* XXX */
  art-&gt;keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art-&gt;listener = view3d_buttons_region_listener;
  art-&gt;message_subscribe = ED_region_generic_tools_region_message_subscribe;
  art-&gt;snap_size = ED_region_generic_tools_region_snap_size;
  art-&gt;init = view3d_tools_region_init;
  art-&gt;draw = view3d_tools_region_draw;
  BLI_addhead(&amp;st-&gt;regiontypes, art);

  /* regions: tool header */
  art = MEM_new_zeroed&lt;ARegionType&gt;(&quot;spacetype view3d tool header region&quot;);
  art-&gt;regionid = RGN_TYPE_TOOL_HEADER;
  art-&gt;prefsizey = HEADERY;
  art-&gt;keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
  art-&gt;listener = view3d_header_region_listener;
  art-&gt;message_subscribe = ED_area_do_mgs_subscribe_for_tool_header;
  art-&gt;init = view3d_header_region_init;
  art-&gt;draw = view3d_tools_header_region_draw;
  BLI_addhead(&amp;st-&gt;regiontypes, art);</code></pre></td>
</tr></tbody>
</table>

### 4. Viewport drawing

Medusa paints a canvas, asks the production renderer for a preview, and overlays sample progress. Blender's View3D region calls its scene draw pipeline, then cleans up caches and restores GPU state. The visual overlay code is only one part of Blender's much larger renderer.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L508">src/medusa_editor.cpp</a> · lines 508-522</small><pre><code class="language-cpp">        // 2. Viewport 3D Canvas
        gfx.fill_rect(canvas_rect_, Theme::kViewportBg);
        const bool hasProductionPreview = gfx.draw_production_preview(canvas_rect_);
        if (hasProductionPreview) {
            const auto progress = gfx.production_render_progress();
            const float progressWidth = 176.0f;
            const Rect progressRect(canvas_rect_.right() - progressWidth - 12.0f,
                                    canvas_rect_.bottom() - 28.0f, progressWidth, 20.0f);
            gfx.fill_rounded_rect(progressRect, 3.0f, Color(0.045f, 0.045f, 0.045f, 0.82f));
            gfx.draw_text(&quot;ZeGFX  &quot; + std::to_string(progress.currentSample) + &quot;/&quot; +
                              std::to_string(progress.targetSamples) + &quot; spp&quot;,
                          Rect(progressRect.x + 8.0f, progressRect.y, progressRect.width - 16.0f,
                               progressRect.height),
                          Theme::kTextSecondary, Theme::kSmallFont, false, false, true,
                          Theme::kNumericFontFamily);</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_view3d/view3d_draw.cc#L1707">../Blender_Editor_READONLY/space_view3d/view3d_draw.cc</a> · lines 1707-1725</small><pre><code class="language-cpp">void view3d_main_region_draw(const bContext *C, ARegion *region)
{
  using namespace blender::draw;
  Main *bmain = CTX_data_main(C);
  View3D *v3d = CTX_wm_view3d(C);

  view3d_update_viewer_path(C);
  view3d_draw_view(C, region);

  DRW_cache_free_old_subdiv();
  DRW_cache_free_old_batches(bmain);
  BKE_image_free_old_buffers(bmain);

  /* No depth test for drawing action zones afterwards. */
  GPU_depth_test(GPU_DEPTH_NONE);

  v3d-&gt;runtime.flag &amp;= ~V3D_RUNTIME_DEPTHBUF_OVERRIDDEN;
  /* TODO: Clear cache? */
}</code></pre></td>
</tr></tbody>
</table>

### 5. Viewport navigation gestures

Medusa hard-codes the mouse-button/modifier mapping in the viewport handler. Blender attaches keymaps and modal navigation operators to the View3D region; orbit, pan, zoom, fly and walk behavior is implemented in separate operator files.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L621">src/medusa_editor.cpp</a> · lines 621-645</small><pre><code class="language-cpp">        if (e.button == MouseButton::Right &amp;&amp; !e.alt &amp;&amp; state_.active_tool == &quot;tool_camera_pilot&quot;) {
            initialize_view_navigation();
            state_.viewport_navigation_active = true;
            navigation_drag_ = NavigationDrag::FreeLook;
            drag_start_ = e.pos;
            last_drag_ = e.pos;
            return true;
        }
        if (e.alt &amp;&amp; (e.button == MouseButton::Left || e.button == MouseButton::Middle || e.button == MouseButton::Right)) {
            initialize_view_navigation();
            state_.viewport_navigation_active = true;
            navigation_drag_ = e.button == MouseButton::Left ? NavigationDrag::Orbit :
                e.button == MouseButton::Middle ? (e.shift ? NavigationDrag::Dolly : NavigationDrag::Pan) : NavigationDrag::Dolly;
            drag_start_ = e.pos;
            last_drag_ = e.pos;
            return true;
        }
        if (e.button == MouseButton::Middle) {
            initialize_view_navigation();
            state_.viewport_navigation_active = true;
            navigation_drag_ = e.shift ? NavigationDrag::Pan : NavigationDrag::Orbit;
            drag_start_ = e.pos;
            last_drag_ = e.pos;
            return true;
        }</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_view3d/view3d_ops.cc#L270">../Blender_Editor_READONLY/space_view3d/view3d_ops.cc</a> · lines 270-285</small><pre><code class="language-cpp">void view3d_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, &quot;3D View Generic&quot;, SPACE_VIEW3D, RGN_TYPE_WINDOW);

  /* only for region 3D window */
  WM_keymap_ensure(keyconf, &quot;3D View&quot;, SPACE_VIEW3D, RGN_TYPE_WINDOW);

  fly_modal_keymap(keyconf);
  walk_modal_keymap(keyconf);
  viewrotate_modal_keymap(keyconf);
  viewmove_modal_keymap(keyconf);
  viewzoom_modal_keymap(keyconf);
  viewdolly_modal_keymap(keyconf);
  viewplace_modal_keymap(keyconf);
  vr_location_scouting_capture_review_modal_keymap(keyconf);
}</code></pre></td>
</tr></tbody>
</table>

### 6. Viewport object picking

Medusa computes a ray and hands it to its renderer-backed scene picker. Blender builds a ViewContext and uses a GPU selection buffer with object/edit-mode filters. The data model and selection semantics are broader in Blender.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L646">src/medusa_editor.cpp</a> · lines 646-655</small><pre><code class="language-cpp">        if (e.button == MouseButton::Left) {
            const int handle = hit_test_gizmo(e.pos);
            if (handle &gt;= 0 &amp;&amp; begin_gizmo_drag(handle, e.pos)) return true;
            float origin[3], direction[3];
            if (make_pick_ray(e.pos, origin, direction)) {
                std::copy(origin, origin + 3, state_.pending_pick_origin);
                std::copy(direction, direction + 3, state_.pending_pick_direction);
                state_.pending_command = &quot;viewport_pick&quot;;
            }
            return true;</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_view3d/view3d_select.cc#L2636">../Blender_Editor_READONLY/space_view3d/view3d_select.cc</a> · lines 2636-2659, 2674-2687</small><pre><code class="language-cpp">  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  /* Setup view context for argument to callbacks. */
  ViewContext vc = ED_view3d_viewcontext_init(C, depsgraph);

  const Main *bmain = CTX_data_main(C);
  Scene *scene = vc.scene;
  View3D *v3d = vc.v3d;

  /* Menu activation may find a base to make active (if it only finds a single item to select). */
  Base *basact_override = nullptr;

  const bool is_obedit = (vc.obedit != nullptr);
  if (object_only) {
    /* Signal for #view3d_gpu_select to skip edit-mode objects. */
    vc.obedit = nullptr;
  }

  /* Set for GPU depth buffer picking, leave null when selecting by center. */
  struct GPUData {
    GPUSelectBuffer buffer;
    int hits;
    bool do_nearest;
    bool has_bones;
    bool has_sub_object;

// ...

    /* If objects have pose-mode set, the bones are in the same selection buffer. */
    const eV3DSelectObjectFilter select_filter =
        ((object_only == false) ? ED_view3d_select_filter_from_mode(scene, v3d, vc.obact) :
                                  VIEW3D_SELECT_FILTER_NOP);
    gpu-&gt;hits = mixed_bones_object_selectbuffer_extended(
        &amp;vc, &amp;gpu-&gt;buffer, mval, select_filter, true, enumerate, &amp;gpu-&gt;do_nearest);
    gpu-&gt;has_bones = (object_only &amp;&amp; gpu-&gt;hits &gt; 0) ?
                         false :
                         selectbuffer_has_bones(gpu-&gt;buffer.storage.as_span().slice(0, gpu-&gt;hits));
    gpu-&gt;has_sub_object = (object_only &amp;&amp; gpu-&gt;hits &gt; 0) ?
                              false :
                              selectbuffer_has_sub_object(
                                  gpu-&gt;buffer.storage.as_span().slice(0, gpu-&gt;hits));
  }</code></pre></td>
</tr></tbody>
</table>

### 7. Transform gizmo

Medusa draws axis arrows and rotation rings itself, then hit-tests its own geometry. Blender creates typed arrow, dial and primitive gizmos and binds them to translate, rotate and resize operators. This is a major architectural difference even where the visible affordance is similar.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L972">src/medusa_editor.cpp</a> · lines 972-1011</small><pre><code class="language-cpp">    void draw_transform_gizmo(Graphics&amp; gfx) const {
        if (!gizmo_available() || state_.active_tool == &quot;tool_select&quot;) return;
        const Vec3 pivot = selected_pivot();
        const ViewBasis view = view_basis();
        const float depth = std::max(0.1f, dot(pivot - view.position, view.forward));
        const float worldLength = std::max(0.15f, depth * 0.13f);
        Point center;
        if (!project(pivot, center)) return;
        if (state_.active_tool == &quot;tool_rotate&quot;) {
            const Color colors[] = {Theme::kAxisX, Theme::kAxisY, Theme::kAxisZ};
            for (int axis = 0; axis &lt; 3; ++axis) {
                const Vec3 u = world_axis((axis + 1) % 3), v = world_axis((axis + 2) % 3);
                Point previous;
                bool hasPrevious = false;
                for (int step = 0; step &lt;= 36; ++step) {
                    const float angle = 6.28318530718f * static_cast&lt;float&gt;(step) / 36.0f;
                    Point current;
                    const Vec3 position = pivot + (u * std::cos(angle) + v * std::sin(angle)) * worldLength;
                    const bool projected = project(position, current);
                    if (projected &amp;&amp; hasPrevious)
                        gfx.draw_line(previous, current, colors[axis], 1.7f);
                    previous = current;
                    hasPrevious = projected;
                }
            }
        } else {
            for (int axis = 0; axis &lt; 3; ++axis) draw_axis(gfx, pivot, axis, worldLength);
            if (state_.active_tool == &quot;tool_move&quot;) {
                const int planes[3][2] = {{0, 1}, {0, 2}, {1, 2}};
                const Color planeColors[] = {Theme::kAxisY, Theme::kAxisZ, Theme::kAxisX};
                for (int i = 0; i &lt; 3; ++i) {
                    Point a, b;
                    if (!project(pivot + world_axis(planes[i][0]) * worldLength * 0.26f, a) ||
                        !project(pivot + world_axis(planes[i][1]) * worldLength * 0.26f, b)) continue;
                    const Point c((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
                    gfx.fill_rounded_rect(Rect(c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f), 2.0f, Color(planeColors[i].r, planeColors[i].g, planeColors[i].b, 0.55f));
                }
            }
        }
        gfx.fill_rounded_rect(Rect(center.x - 4.0f, center.y - 4.0f, 8.0f, 8.0f), 4.0f, Theme::kTextPrimary);</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/transform/transform_gizmo_3d.cc#L1651">../Blender_Editor_READONLY/transform/transform_gizmo_3d.cc</a> · lines 1651-1678, 1822-1853</small><pre><code class="language-cpp">  const wmGizmoType *gzt_arrow = WM_gizmotype_find(&quot;GIZMO_GT_arrow_3d&quot;, true);
  const wmGizmoType *gzt_dial = WM_gizmotype_find(&quot;GIZMO_GT_dial_3d&quot;, true);
  const wmGizmoType *gzt_prim = WM_gizmotype_find(&quot;GIZMO_GT_primitive_3d&quot;, true);

  wmGizmoPropertyFnParams params{};
  params.value_get_fn = rotation_get_fn;
  params.value_set_fn = rotation_set_fn;
  params.user_data = ggd;

#define GIZMO_NEW_ARROW(v) \
  { \
    ggd-&gt;gizmos[v] = WM_gizmo_new_ptr(gzt_arrow, gzgroup, nullptr); \
  } \
  ((void)0)
#define GIZMO_NEW_DIAL(v) \
  { \
    ggd-&gt;gizmos[v] = WM_gizmo_new_ptr(gzt_dial, gzgroup, nullptr); \
    WM_gizmo_target_property_def_func(ggd-&gt;gizmos[v], &quot;offset&quot;, &amp;params); \
  } \
  ((void)0)
#define GIZMO_NEW_PRIM(v) \
  { \
    ggd-&gt;gizmos[v] = WM_gizmo_new_ptr(gzt_prim, gzgroup, nullptr); \
  } \
  ((void)0)

  /* Add/init widgets - order matters! */
  GIZMO_NEW_DIAL(MAN_AXIS_ROT_T);

// ...

    switch (axis_type) {
      case MAN_AXES_TRANSLATE:
        if (ot_store.translate == nullptr) {
          ot_store.translate = WM_operatortype_find(&quot;TRANSFORM_OT_translate&quot;, true);
        }
        ptr = WM_gizmo_operator_set(axis, 0, ot_store.translate, nullptr);
        break;
      case MAN_AXES_ROTATE: {
        wmOperatorType *ot_rotate;
        if (axis_idx == MAN_AXIS_ROT_T) {
          if (ot_store.trackball == nullptr) {
            ot_store.trackball = WM_operatortype_find(&quot;TRANSFORM_OT_trackball&quot;, true);
          }
          ot_rotate = ot_store.trackball;
        }
        else {
          if (ot_store.rotate == nullptr) {
            ot_store.rotate = WM_operatortype_find(&quot;TRANSFORM_OT_rotate&quot;, true);
          }
          ot_rotate = ot_store.rotate;
        }
        ptr = WM_gizmo_operator_set(axis, 0, ot_rotate, nullptr);
        break;
      }
      case MAN_AXES_SCALE: {
        if (ot_store.resize == nullptr) {
          ot_store.resize = WM_operatortype_find(&quot;TRANSFORM_OT_resize&quot;, true);
        }
        ptr = WM_gizmo_operator_set(axis, 0, ot_store.resize, nullptr);
        break;
      }
    }</code></pre></td>
</tr></tbody>
</table>

### 8. Outliner tree construction

Medusa recursively follows parent indices and creates rows for scene nodes. Blender rebuilds a typed tree when requested, synchronizes selection with the view layer, and preserves scroll-to-active behavior. Blender's tree model handles many more ID and hierarchy kinds.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L1538">src/medusa_editor.cpp</a> · lines 1538-1568</small><pre><code class="language-cpp">    void append_node(uint32_t index, int depth, std::vector&lt;uint8_t&gt;&amp; visited) {
        if (index &gt;= state_.scene_nodes.size() || visited[index]) return;
        visited[index] = 1;
        auto&amp; node = state_.scene_nodes[index];
        const bool children = has_children(index);
        rows_.emplace_back(node.name, icon_for(node.type), depth, children, node.expanded,
            state_.selected_scene_node == static_cast&lt;int&gt;(index), node.visible, node.locked, node.renderable,
            [this, index]() {
                state_.select_scene_node(index);
            },
            [this, index](bool expanded) {
                if (index &lt; state_.scene_nodes.size()) state_.scene_nodes[index].expanded = expanded;
                rows_generation_ = ~uint64_t(0);
            },
            [this, index](bool visible) {
                if (index &lt; state_.scene_nodes.size())
                    state_.set_scene_node_state(index, visible, state_.scene_nodes[index].renderable);
            },
            [this, index](bool locked) {
                if (index &lt; state_.scene_nodes.size()) state_.scene_nodes[index].locked = locked;
            },
            [this, index](bool renderable) {
                if (index &lt; state_.scene_nodes.size())
                    state_.set_scene_node_state(index, state_.scene_nodes[index].visible, renderable);
            });
        row_node_indices_.push_back(index);
        if (node.expanded) {
            for (uint32_t child = 0; child &lt; state_.scene_nodes.size(); ++child)
                if (state_.scene_nodes[child].parentIndex == static_cast&lt;int32_t&gt;(index))
                    append_node(child, depth + 1, visited);
        }</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_outliner/outliner_draw.cc#L3874">../Blender_Editor_READONLY/space_outliner/outliner_draw.cc</a> · lines 3874-3898</small><pre><code class="language-cpp">  if (do_rebuild) {
    outliner_build_tree(
        mainvar, workspace, tvc.scene, tvc.view_layer, space_outliner, region); /* Always. */

    /* If global sync select is dirty, flag other outliners. */
    if (ED_outliner_select_sync_is_dirty(C)) {
      ED_outliner_select_sync_flag_outliners(C);
    }

    /* Sync selection state from view layer. */
    if (space_outliner-&gt;flag &amp; SO_SYNC_SELECT) {
      if (!ELEM(space_outliner-&gt;outlinevis,
                SO_LIBRARIES,
                SO_OVERRIDES_LIBRARY,
                SO_DATA_API,
                SO_ID_ORPHANS))
      {
        short idcode = 0;
        if (outliner_sync_selection(C, tvc, space_outliner, idcode) &amp;&amp;
            (space_outliner-&gt;flag &amp; SO_SCROLL_TO_ACTIVE))
        {
          outliner_scroll_to_active(space_outliner, region, idcode);
        }
      }
    }</code></pre></td>
</tr></tbody>
</table>

### 9. Outliner row restrictions

Medusa divides each row into fixed click zones for visible, locked and renderable. Blender resolves restriction controls from RNA properties for objects, collections, modifiers, constraints, bones and other data. Medusa's three toggles are a focused subset.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_widgets.cpp#L291">src/medusa_widgets.cpp</a> · lines 291-323</small><pre><code class="language-cpp">bool OutlinerRowWidget::handle_mouse_down(const MouseEvent &amp;e) {
    if (bounds_.contains(e.pos) &amp;&amp; e.button == MouseButton::Left) {
        // Check restriction column clicks
        float eye_x = bounds_.right() - 56.0f;
        if (e.pos.x &gt;= eye_x &amp;&amp; e.pos.x &lt; eye_x + 16.0f) {
            is_visible_ = !is_visible_;
            if (on_toggle_vis_) on_toggle_vis_(is_visible_);
            return true;
        }
        float lock_x = eye_x + 18.0f;
        if (e.pos.x &gt;= lock_x &amp;&amp; e.pos.x &lt; lock_x + 16.0f) {
            is_locked_ = !is_locked_;
            if (on_toggle_lock_) on_toggle_lock_(is_locked_);
            return true;
        }
        float render_x = lock_x + 18.0f;
        if (e.pos.x &gt;= render_x &amp;&amp; e.pos.x &lt; render_x + 16.0f) {
            is_renderable_ = !is_renderable_;
            if (on_toggle_render_) on_toggle_render_(is_renderable_);
            return true;
        }

        // Chevron click toggles expand
        float indent_x = bounds_.x + 8.0f + static_cast&lt;float&gt;(indent_level_ * 16);
        if (has_children_ &amp;&amp; e.pos.x &gt;= indent_x &amp;&amp; e.pos.x &lt;= indent_x + 14.0f) {
            is_expanded_ = !is_expanded_;
            if (on_toggle_expand_) on_toggle_expand_(is_expanded_);
            return true;
        }

        // Row selection
        if (on_select_) on_select_();
        return true;</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_outliner/outliner_draw.cc#L1174">../Blender_Editor_READONLY/space_outliner/outliner_draw.cc</a> · lines 1174-1215</small><pre><code class="language-cpp">static void outliner_draw_restrictbuts(ui::Block *block,
                                       const Main &amp;bmain,
                                       Scene *scene,
                                       ViewLayer *view_layer,
                                       ARegion *region,
                                       SpaceOutliner *space_outliner,
                                       ListBaseT&lt;TreeElement&gt; *lb,
                                       RestrictPropertiesActive props_active_parent)
{
  /* Get RNA properties (once for speed). */
  static RestrictProperties props = {false};
  if (!props.initialized) {
    props.object_hide_viewport = RNA_struct_type_find_property(RNA_Object, &quot;hide_viewport&quot;);
    props.object_hide_select = RNA_struct_type_find_property(RNA_Object, &quot;hide_select&quot;);
    props.object_hide_render = RNA_struct_type_find_property(RNA_Object, &quot;hide_render&quot;);
    props.object_holdout = RNA_struct_type_find_property(RNA_Object, &quot;is_holdout&quot;);
    props.object_indirect_only = RNA_struct_type_find_property(RNA_Object, &quot;visible_camera&quot;);
    props.base_hide_viewport = RNA_struct_type_find_property(RNA_ObjectBase, &quot;hide_viewport&quot;);
    props.collection_hide_viewport = RNA_struct_type_find_property(RNA_Collection,
                                                                   &quot;hide_viewport&quot;);
    props.collection_hide_select = RNA_struct_type_find_property(RNA_Collection, &quot;hide_select&quot;);
    props.collection_hide_render = RNA_struct_type_find_property(RNA_Collection, &quot;hide_render&quot;);
    props.layer_collection_exclude = RNA_struct_type_find_property(RNA_LayerCollection, &quot;exclude&quot;);
    props.layer_collection_holdout = RNA_struct_type_find_property(RNA_LayerCollection, &quot;holdout&quot;);
    props.layer_collection_indirect_only = RNA_struct_type_find_property(RNA_LayerCollection,
                                                                         &quot;indirect_only&quot;);
    props.layer_collection_hide_viewport = RNA_struct_type_find_property(RNA_LayerCollection,
                                                                         &quot;hide_viewport&quot;);
    props.modifier_show_viewport = RNA_struct_type_find_property(RNA_Modifier, &quot;show_viewport&quot;);
    props.modifier_show_render = RNA_struct_type_find_property(RNA_Modifier, &quot;show_render&quot;);

    props.constraint_enable = RNA_struct_type_find_property(RNA_Constraint, &quot;enabled&quot;);

    props.bone_hide_viewport = RNA_struct_type_find_property(RNA_PoseBone, &quot;hide&quot;);

    props.initialized = true;
  }

  struct {
    int enable;
    int select;
    int hide;</code></pre></td>
</tr></tbody>
</table>

### 10. Properties panel context and layout

Medusa manually computes each control rectangle and chooses which panels appear from workspace/object state. Blender polls whether a panel applies to the active context, then builds its content in reusable Layout columns and RNA-backed controls.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L1623">src/medusa_editor.cpp</a> · lines 1623-1652, 1912-1929</small><pre><code class="language-cpp">    void layout(const Rect &amp;bounds) override {
        bounds_ = bounds;
        header_rect_ = Rect(bounds.x, bounds.y, bounds.width, Theme::kAreaHeaderHeight);

        float y = bounds.y + 34.0f;
        float w = (bounds.width - 24.0f) / 3.0f;

        if (show_transform()) {
            transform_header_.set_bounds(Rect(bounds.x, y, bounds.width, 24.0f));
            y += 28.0f;

            if (transform_header_.is_expanded()) {
                pos_x_.set_bounds(Rect(bounds.x + 8.0f, y, w, 24.0f));
                pos_y_.set_bounds(Rect(bounds.x + 8.0f + w + 6.0f, y, w, 24.0f));
                pos_z_.set_bounds(Rect(bounds.x + 8.0f + (w + 6.0f) * 2.0f, y, w, 24.0f));
                y += 30.0f;

                if (state_.camera_selected() || state_.transformable_selected()) {
                    rot_x_.set_bounds(Rect(bounds.x + 8.0f, y, w, 24.0f));
                    rot_y_.set_bounds(Rect(bounds.x + 8.0f + w + 6.0f, y, w, 24.0f));
                    rot_z_.set_bounds(Rect(bounds.x + 8.0f + (w + 6.0f) * 2.0f, y, w, 24.0f));
                    y += 32.0f;
                }
                if (state_.transformable_selected()) {
                    scale_x_.set_bounds(Rect(bounds.x + 8.0f, y, w, 24.0f));
                    scale_y_.set_bounds(Rect(bounds.x + 8.0f + w + 6.0f, y, w, 24.0f));
                    scale_z_.set_bounds(Rect(bounds.x + 8.0f + (w + 6.0f) * 2.0f, y, w, 24.0f));
                    y += 32.0f;
                }
            }

// ...

    bool show_transform() const {
        return (state_.active_workspace == &quot;Scene&quot; || state_.active_workspace == &quot;Shot&quot;) &amp;&amp;
            (state_.camera_selected() || state_.transformable_selected()) &amp;&amp;
             state_.selected_scene_node &gt;= 0 &amp;&amp;
             static_cast&lt;size_t&gt;(state_.selected_scene_node) &lt; state_.scene_nodes.size() &amp;&amp;
             !state_.scene_nodes[static_cast&lt;size_t&gt;(state_.selected_scene_node)].locked;
    }
    bool show_lighting() const { return state_.active_workspace == &quot;Scene&quot; &amp;&amp; state_.light_selected(); }
    bool show_optics() const { return state_.active_workspace == &quot;Shot&quot; &amp;&amp; state_.camera_selected(); }
    bool show_render() const { return state_.active_workspace == &quot;Production&quot;; }

    void init_panels() {
        Color neutral(0.0f, 0.0f, 0.0f, 0.0f);
        // Collapsible section headers
        transform_header_ = CollapsibleSectionWidget(&quot;Transform&quot;, true);
        optics_header_ = CollapsibleSectionWidget(&quot;Camera Lens&quot;, true);
        light_header_ = CollapsibleSectionWidget(&quot;Key Light&quot;, true);
        render_header_ = CollapsibleSectionWidget(&quot;Production Sampling&quot;, true);</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_view3d/view3d_buttons.cc#L2700">../Blender_Editor_READONLY/space_view3d/view3d_buttons.cc</a> · lines 2700-2749</small><pre><code class="language-cpp">static bool view3d_panel_transform_poll(const bContext *C, PanelType * /*pt*/)
{
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  return (BKE_view_layer_active_base_get(view_layer) != nullptr);
}

static void view3d_panel_transform(const bContext *C, Panel *panel)
{
  ui::Block *block;
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  Object *ob = BKE_view_layer_active_object_get(view_layer);
  Object *obedit = OBEDIT_FROM_OBACT(ob);

  block = panel-&gt;layout-&gt;block();
  block_func_handle_set(block, do_view3d_region_buttons, nullptr);

  ui::Layout &amp;col = panel-&gt;layout-&gt;column(false);

  if (ob == obedit) {
    if (ob-&gt;type == OB_ARMATURE) {
      v3d_editarmature_buts(col, ob);
    }
    else if (ob-&gt;type == OB_MBALL) {
      v3d_editmetaball_buts(col, ob);
    }
    else {
      View3D *v3d = CTX_wm_view3d(C);
      v3d_editvertex_buts(C, &amp;col, v3d, ob, FLT_MAX);
    }
  }
  else if (ob-&gt;mode &amp; OB_MODE_POSE) {
    v3d_posearmature_buts(C, col, ob);
  }
  else {
    PointerRNA obptr = RNA_id_pointer_create(&amp;ob-&gt;id);
    v3d_transform_butsR(C, col, &amp;obptr);

    /* Dimensions and editmode are mostly the same check. */
    if (OB_TYPE_SUPPORT_EDITMODE(ob-&gt;type) || ELEM(ob-&gt;type, OB_VOLUME, OB_LIGHTPROBE)) {
      View3D *v3d = CTX_wm_view3d(C);
      v3d_object_dimension_buts(nullptr, &amp;col, v3d, ob);
    }
  }
}</code></pre></td>
</tr></tbody>
</table>

### 11. Scrubbable numeric controls

Both support drag-based numeric adjustment. Medusa's widget owns drag speed, modifiers, bounds and formatting in a small class. Blender's number widget is integrated with common button state, text entry, hover zones, theme colors and the wider event system.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_widgets.cpp#L105">src/medusa_widgets.cpp</a> · lines 105-167</small><pre><code class="language-cpp">void ScrubbableNumberWidget::draw(Graphics &amp;gfx) {
    Color bg = is_dragging_ ? Theme::kControlActive : (is_hovered_ ? Theme::kControlHover : Theme::kControlBg);
    gfx.fill_rounded_rect(bounds_, 3.0f, bg);
    gfx.draw_rounded_rect(bounds_, 4.0f, is_dragging_ ? Theme::kAccentBorder : Theme::kBorderSubtle, 1.0f);

    // Left Axis Indicator (Pure neutral tag)
    float tag_w = std::max(20.0f, static_cast&lt;float&gt;(axis_label_.length()) * 7.5f + 8.0f);
    Rect tag_rect(bounds_.x, bounds_.y, tag_w, bounds_.height);
    gfx.fill_rounded_rect(tag_rect, 3.0f, Theme::kPanelHeaderBg);

    gfx.draw_text(axis_label_, tag_rect, is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary,
                  Theme::kSmallFont, true, true, true, Theme::kUiFontFamily);

    // Formatted Value Display (Neutral white/gray numbers)
    char value_text[64];
    std::snprintf(value_text, sizeof(value_text), &quot;%.3f%s%s&quot;, value_,
                  unit_.empty() ? &quot;&quot; : &quot; &quot;, unit_.c_str());
    Rect val_rect(bounds_.x + tag_w + 6.0f, bounds_.y, bounds_.width - tag_w - 8.0f, bounds_.height);
    gfx.draw_text(value_text, val_rect, Theme::kTextPrimary, Theme::kUiFont, false, false, true, Theme::kNumericFontFamily);
}

bool ScrubbableNumberWidget::handle_mouse_down(const MouseEvent &amp;e) {
    if (bounds_.contains(e.pos) &amp;&amp; e.button == MouseButton::Left) {
        is_dragging_ = true;
        drag_start_x_ = e.pos.x;
        drag_start_val_ = value_;
        return true;
    }
    return false;
}

bool ScrubbableNumberWidget::handle_mouse_move(const MouseEvent &amp;e) {
    bool was_hovered = is_hovered_;
    is_hovered_ = bounds_.contains(e.pos);

    if (is_dragging_) {
        float dx = e.pos.x - drag_start_x_;
        float speed = step_;
        if (e.shift) speed *= 0.1f; // Shift: micro-step
        if (e.ctrl) speed *= 5.0f;  // Ctrl: coarse step

        float new_val = drag_start_val_ + dx * speed * 0.1f;
        if (e.ctrl) {
            new_val = std::round(new_val / step_) * step_; // Ctrl: snap
        }
        if (new_val &lt; min_val_) new_val = min_val_;
        if (new_val &gt; max_val_) new_val = max_val_;

        if (new_val != value_) {
            value_ = new_val;
            if (on_change_) on_change_(value_);
        }
        return true;
    }
    return was_hovered != is_hovered_;
}

bool ScrubbableNumberWidget::handle_mouse_up(const MouseEvent &amp;e) {
    if (is_dragging_) {
        is_dragging_ = false;
        return true;
    }
    return false;</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/interface/interface_widgets.cc#L4263">../Blender_Editor_READONLY/interface/interface_widgets.cc</a> · lines 4263-4316</small><pre><code class="language-cpp">static void widget_numbut_draw(const Button *but,
                               uiWidgetColors *wcol,
                               rcti *rect,
                               const float zoom,
                               const WidgetStateInfo *state,
                               int roundboxalign,
                               bool emboss)
{
  const float rad = widget_radius_from_zoom(zoom, wcol);
  const int handle_width = min_ii(BLI_rcti_size_x(rect) / 3, BLI_rcti_size_y(rect) * 0.7f);

  if (state-&gt;but_flag &amp; UI_SELECT) {
    std::swap(wcol-&gt;shadetop, wcol-&gt;shadedown);
  }

  WidgetBase wtb;
  widget_init(&amp;wtb);

  if (!emboss) {
    round_box_edges(&amp;wtb, roundboxalign, rect, rad);
  }
  else {
    wtb.draw_inner = false;
    wtb.draw_outline = false;
  }

  /* decoration */
  if (((state-&gt;but_flag &amp; UI_HOVER) || (U.uiflag2 &amp; USER_ALWAYS_SHOW_NUMBER_ARROWS)) &amp;&amp;
      !state-&gt;is_text_input)
  {
    uiWidgetColors wcol_zone;
    WidgetBase wtb_zone;
    rcti rect_zone;
    int roundboxalign_zone;

    /* left arrow zone */
    widget_init(&amp;wtb_zone);
    wtb_zone.draw_outline = false;
    wtb_zone.draw_emboss = false;

    wcol_zone = *wcol;
    copy_v3_v3_uchar(wcol_zone.item, wcol-&gt;text);
    if (!(state-&gt;but_flag &amp; UI_HOVER)) {
      wcol_zone.item[3] = 180;
    }
    if (state-&gt;but_drawflag &amp; BUT_HOVER_LEFT) {
      widget_active_color(&amp;wcol_zone);
    }

    rect_zone = *rect;
    rect_zone.xmax = rect-&gt;xmin + handle_width + U.pixelsize;
    roundboxalign_zone = roundboxalign &amp; ~(CNR_TOP_RIGHT | CNR_BOTTOM_RIGHT);
    round_box_edges(&amp;wtb_zone, roundboxalign_zone, &amp;rect_zone, rad);
</code></pre></td>
</tr></tbody>
</table>

### 12. Timeline keys and Dope Sheet comparison

This is the closest keyframe comparison. Medusa lays out frame ticks at fixed intervals and draws its keyframe vector as dots on one lane. Blender filters animation channels, culls channels outside the visible View2D range, and draws multiple channel types.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_widgets.cpp#L359">src/medusa_widgets.cpp</a> · lines 359-426</small><pre><code class="language-cpp">void SequencerWidget::draw(Graphics &amp;gfx) {
    float margin_left = 42.0f;
    float margin_right = 50.0f;
    float span_w = std::max(50.0f, bounds_.width - (margin_left + margin_right));
    float total_span_frames = static_cast&lt;float&gt;(std::max(1, total_frames_ - start_frame_));
    float frame_w = span_w / total_span_frames;

    auto frame_to_x = [&amp;](int f) -&gt; float {
        return bounds_.x + margin_left + static_cast&lt;float&gt;(f - start_frame_) * frame_w;
    };

    float start_bound_x = frame_to_x(start_frame_);
    float end_bound_x = frame_to_x(end_frame_);

    // --------------------------------------------------------------------------
    // A. Canvas Background &amp; Out-of-Range Demarcation
    // --------------------------------------------------------------------------
    // Base active playback canvas (smooth neutral dark studio charcoal)
    gfx.fill_rect(bounds_, Theme::kPanelBg);

    // Inactive out-of-range zones (&lt; start_frame and &gt; end_frame)
    Color out_of_bounds_bg = Theme::kChassisBg;
    if (start_bound_x &gt; bounds_.x) {
        gfx.fill_rect(Rect(bounds_.x, bounds_.y, start_bound_x - bounds_.x, bounds_.height), out_of_bounds_bg);
    }
    if (end_bound_x &lt; bounds_.right()) {
        gfx.fill_rect(Rect(end_bound_x, bounds_.y, bounds_.right() - end_bound_x, bounds_.height), out_of_bounds_bg);
    }

    // Boundary demarcation hairline vertical lines
    Color bound_line_col = Theme::kBorderBright;
    gfx.draw_line(Point(start_bound_x, bounds_.y), Point(start_bound_x, bounds_.bottom()), bound_line_col, 1.0f);
    gfx.draw_line(Point(end_bound_x, bounds_.y), Point(end_bound_x, bounds_.bottom()), bound_line_col, 1.0f);

    // --------------------------------------------------------------------------
    // B. Vertical Rhythmic Grid Lines &amp; Frame Numbers (Multiples of 12 at 24 fps)
    // --------------------------------------------------------------------------
    // Minor tick lines (every 4 frames) at ruler top
    for (int f = start_frame_; f &lt;= end_frame_; f += 4) {
        if (f % 12 == 0) continue; // Major tick handles this
        float x = frame_to_x(f);
        if (x &lt; bounds_.x || x &gt; bounds_.right()) continue;
        gfx.draw_line(Point(x, bounds_.y + 12.0f), Point(x, bounds_.y + 16.0f),
                      Theme::kBorderSubtle, 0.75f);
    }

    // Major frame numbers and continuous vertical grid lines
    Color grid_hairline_col = Theme::kBorderSubtle;
    Color ruler_text_col = Theme::kTextSecondary;

    for (int f = 12; f &lt;= end_frame_; f += 12) {
        float x = frame_to_x(f);
        if (x &lt; bounds_.x || x &gt; bounds_.right()) continue;

        // Number centered above ruler
        gfx.draw_text(std::to_string(f), Rect(x - 14.0f, bounds_.y + 1.0f, 28.0f, 13.0f),
                      ruler_text_col, Theme::kSmallFont, false, true, false, Theme::kNumericFontFamily);

        // Vertical hairline grid line spanning the entire canvas from ruler down
        gfx.draw_line(Point(x, bounds_.y + 16.0f), Point(x, bounds_.bottom()),
                      grid_hairline_col, 0.75f);
    }

    for (int frame : keyframes_) {
        if (frame &lt; start_frame_ || frame &gt; end_frame_) continue;
        const float x = frame_to_x(frame);
        if (x &lt; bounds_.x || x &gt; bounds_.right()) continue;
        gfx.fill_rect(Rect(x - 2.0f, bounds_.y + 20.0f, 4.0f, 4.0f), Theme::kTextPrimary);</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_action/action_draw.cc#L311">../Blender_Editor_READONLY/space_action/action_draw.cc</a> · lines 311-355</small><pre><code class="language-cpp">static void draw_keyframes(bAnimContext *ac,
                           View2D *v2d,
                           SpaceAction *saction,
                           const ListBaseT&lt;bAnimListElem&gt; &amp;anim_data)
{
  /* Draw keyframes
   * 1) Only channels that are visible in the Action Editor get drawn/evaluated.
   *    This is to try to optimize this for heavier data sets
   * 2) Keyframes which are out of view horizontally are disregarded
   */
  int action_flag = saction-&gt;flag;
  bDopeSheet *ads = &amp;saction-&gt;ads;

  if (saction-&gt;mode == SACTCONT_TIMELINE) {
    action_flag &amp;= ~SACTION_SHOW_INTERPOLATION;
  }

  const float channel_step = ANIM_UI_get_channel_step();
  float ymax = ANIM_UI_get_first_channel_top(v2d);

  ChannelDrawList *draw_list = ED_channel_draw_list_create();

  const float scale_factor = ANIM_UI_get_keyframe_scale_factor();

  bAnimListElem *ale;
  for (ale = anim_data.first(); ale; ale = ale-&gt;next, ymax -= channel_step) {
    const float ymin = ymax - ANIM_UI_get_channel_height();
    float ycenter = (ymin + ymax) / 2.0f;

    /* check if visible */
    if (!(IN_RANGE(ymin, v2d-&gt;cur.ymin, v2d-&gt;cur.ymax) ||
          IN_RANGE(ymax, v2d-&gt;cur.ymin, v2d-&gt;cur.ymax)))
    {
      continue;
    }

    /* check if anything to show for this channel */
    if (ale-&gt;datatype == ALE_NONE) {
      continue;
    }

    /* Add channels to list to draw later. */
    switch (ale-&gt;datatype) {
      case ALE_ALL:
        ED_add_summary_channel(</code></pre></td>
</tr></tbody>
</table>

### 13. Medusa timeline versus Blender Sequencer

The Medusa excerpt shows keyframe markers and its playhead. Blender's actual Sequencer draws channel lanes, frame grid, frame range, strips, markers, overlays and gizmos. There is no Medusa clip-strip, audio-waveform, strip-effect or retiming model in the current widget code.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_widgets.cpp#L422">src/medusa_widgets.cpp</a> · lines 422-426, 439-469</small><pre><code class="language-cpp">    for (int frame : keyframes_) {
        if (frame &lt; start_frame_ || frame &gt; end_frame_) continue;
        const float x = frame_to_x(frame);
        if (x &lt; bounds_.x || x &gt; bounds_.right()) continue;
        gfx.fill_rect(Rect(x - 2.0f, bounds_.y + 20.0f, 4.0f, 4.0f), Theme::kTextPrimary);

// ...

    float cti_x = frame_to_x(current_frame_);

    // Scrubber Badge in Ruler (Pentagonal pill with downward triangular point)
    float bw = 20.0f;
    float bh = 15.0f;
    float tip_h = 4.0f;
    Rect badge_box(cti_x - bw * 0.5f, bounds_.y, bw, bh);

    gfx.fill_rounded_rect(badge_box, 3.0f, Theme::kAccent);

    // Downward arrowhead pointer
    Point p_tip(cti_x, bounds_.y + bh + tip_h);
    Point p_left(cti_x - bw * 0.5f, bounds_.y + bh);
    Point p_right(cti_x + bw * 0.5f, bounds_.y + bh);

    // Fill arrow with small rect/lines to tip
    for (float dy = 0.0f; dy &lt;= tip_h; dy += 0.5f) {
        float frac = 1.0f - (dy / tip_h);
        float hw = (bw * 0.5f) * frac;
        gfx.draw_line(Point(cti_x - hw, bounds_.y + bh + dy),
                      Point(cti_x + hw, bounds_.y + bh + dy),
                      Theme::kAccent, 1.0f);
    }

    // Number inside scrubber badge
    gfx.draw_text(std::to_string(current_frame_), badge_box, Theme::kAccentText,
                  Theme::kSmallFont, true, true, true, Theme::kNumericFontFamily);

    // Vertical razor needle across full canvas from arrowhead tip to bottom
    gfx.draw_line(Point(cti_x, bounds_.y + bh + tip_h), Point(cti_x, bounds_.bottom()),
                  Theme::kAccent, 1.0f);</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_sequencer/sequencer_timeline_draw.cc#L2035">../Blender_Editor_READONLY/space_sequencer/sequencer_timeline_draw.cc</a> · lines 2035-2068</small><pre><code class="language-cpp">void draw_timeline_seq(const bContext *C, const ARegion *region)
{
  PRF_scope_with_name(&quot;SeqTimelineDraw&quot;, ProfileCategory::Draw);

  SeqQuadsBatch quads_batch;
  TimelineDrawContext ctx = timeline_draw_context_get(C, &amp;quads_batch);
  StripsDrawBatch strips_batch(ctx.v2d);

  draw_timeline_pre_view_callbacks(ctx);
  ui::theme::frame_buffer_clear(TH_BACK);

  ui::view2d_view_ortho(ctx.v2d);
  draw_seq_timeline_channels(ctx);
  draw_timeline_grid(ctx);
  draw_timeline_sfra_efra(ctx);
  draw_seq_strips(ctx, strips_batch);
  draw_timeline_markers(ctx);
  if (ctx.scene) {
    ANIM_draw_previewrange(ctx.scene, ctx.v2d, 1);
  }
  draw_timeline_post_view_callbacks(ctx);
  ui::view2d_view_restore(C);

  draw_timeline_gizmos(ctx);

  ui::view2d_view_ortho(ctx.v2d);
  if (ctx.scene) {
    const int fps = round_db_to_int(ctx.scene-&gt;frames_per_second());
    ED_time_scrub_draw(region, ctx.scene, !(ctx.sseq-&gt;flag &amp; SEQ_DRAWFRAMES), true, fps);
  }

  if (ctx.scene) {
    seq_prefetch_wm_notify(C, ctx.scene);
  }</code></pre></td>
</tr></tbody>
</table>

### 14. Timeline editor regions

Medusa places transport, frame range, FPS and timeline canvas into one custom region. Blender's Sequencer registers separate timeline, preview, properties, toolbar and channel regions with separate draw, layout, event and context callbacks.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L1988">src/medusa_editor.cpp</a> · lines 1988-2050</small><pre><code class="language-cpp">// 6. Timeline Region (Blender Timeline 1-on-1 Layout)
// ==============================================================================
class TimelineRegion : public Region {
public:
    TimelineRegion(EditorState &amp;state) : state_(state),
      sequencer_(state.current_frame, state.total_frames, [this](int f) {
          state_.current_frame = f;
          state_.evaluate_keyframes();
      }) {
        init_header();
    }

    void layout(const Rect &amp;bounds) override {
        bounds_ = bounds;
        header_rect_ = Rect(bounds.x, bounds.y, bounds.width, Theme::kTimelineHeaderHeight);

        // A. Left controls: (clock) ▾ + Menus (View, Marker, Playback ▾)
        float lx = bounds.x + 8.0f;
        time_icon_rect_ = Rect(lx, bounds.y + 6.0f, 16.0f, 16.0f);
        lx += 28.0f;

        for (auto &amp;btn : menu_buttons_) {
            float mw = static_cast&lt;float&gt;(btn.get_label().length()) * 6.8f + 12.0f;
            btn.set_bounds(Rect(lx, bounds.y + 3.0f, mw, 22.0f));
            lx += mw + 6.0f;
        }

        // B. Center Transport Controls (Centered on width)
        float center_x = bounds.x + bounds.width * 0.5f;
        float btn_sz = 24.0f;
        float transport_pill_w = 6.0f * btn_sz;
        constexpr float group_gap = 8.0f;
        float total_center_w = 24.0f + group_gap + transport_pill_w + group_gap + 28.0f;
        float cx = center_x - total_center_w * 0.5f;

        autokey_rect_ = Rect(cx, bounds.y + 2.0f, 24.0f, 24.0f);
        cx += 24.0f + group_gap;

        transport_pill_rect_ = Rect(cx, bounds.y + 2.0f, transport_pill_w, btn_sz);
        float bx = cx;
        for (auto &amp;btn : transport_buttons_) {
            btn.set_bounds(Rect(bx, bounds.y + 2.0f, btn_sz, btn_sz));
            bx += btn_sz;
        }
        cx += transport_pill_w + group_gap;

        keying_rect_ = Rect(cx, bounds.y + 2.0f, 28.0f, 24.0f);

        // C. Right Controls: reserve a fixed, fully in-bounds slot.
        float rx = bounds.right() - 294.0f;
        frame_box_rect_ = Rect(rx, bounds.y + 2.0f, 48.0f, 24.0f);
        rx += 52.0f;

        time_mode_rect_ = Rect(rx, bounds.y + 2.0f, 24.0f, 24.0f);
        rx += 28.0f;

        range_rect_ = Rect(rx, bounds.y + 2.0f, 150.0f, 24.0f);
        rx += 156.0f;

        fps_rect_ = Rect(rx, bounds.y + 2.0f, 48.0f, 24.0f);

        // Full-width Timeline Canvas below header seamlessly
        sequencer_.set_bounds(Rect(bounds.x, bounds.y + Theme::kTimelineHeaderHeight, bounds.width, bounds.height - Theme::kTimelineHeaderHeight));</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_sequencer/space_sequencer.cc#L1294">../Blender_Editor_READONLY/space_sequencer/space_sequencer.cc</a> · lines 1294-1353</small><pre><code class="language-cpp">  /* Create regions: */
  /* Main window. */
  art = MEM_new_zeroed&lt;ARegionType&gt;(&quot;spacetype sequencer region&quot;);
  art-&gt;regionid = RGN_TYPE_WINDOW;
  art-&gt;poll = sequencer_main_region_poll;
  art-&gt;init = sequencer_main_region_init;
  art-&gt;draw = sequencer_main_region_draw;
  art-&gt;draw_overlay = sequencer_main_region_draw_overlay;
  art-&gt;layout = sequencer_main_region_layout;
  art-&gt;on_view2d_changed = sequencer_main_region_view2d_changed;
  art-&gt;listener = sequencer_main_region_listener;
  art-&gt;message_subscribe = sequencer_main_region_message_subscribe;
  art-&gt;keymapflag = ED_KEYMAP_TOOL | ED_KEYMAP_GIZMO | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES |
                    ED_KEYMAP_ANIMATION;
  art-&gt;cursor = sequencer_main_cursor;
  art-&gt;event_cursor = true;
  art-&gt;clip_gizmo_events_by_ui = true;
  BLI_addhead(&amp;st-&gt;regiontypes, art);

  /* Preview. */
  art = MEM_new_zeroed&lt;ARegionType&gt;(&quot;spacetype sequencer region&quot;);
  art-&gt;regionid = RGN_TYPE_PREVIEW;
  art-&gt;poll = sequencer_preview_region_poll;
  art-&gt;init = sequencer_preview_region_init;
  art-&gt;layout = sequencer_preview_region_layout;
  art-&gt;on_view2d_changed = sequencer_preview_region_view2d_changed;
  art-&gt;draw = sequencer_preview_region_draw;
#ifdef WITH_INPUT_IME
  art-&gt;cursor_ime = sequencer_preview_region_cursor_ime;
#endif
  art-&gt;listener = sequencer_preview_region_listener;
  art-&gt;keymapflag = ED_KEYMAP_TOOL | ED_KEYMAP_GIZMO | ED_KEYMAP_GPENCIL;
  BLI_addhead(&amp;st-&gt;regiontypes, art);

  /* List-view/buttons. */
  art = MEM_new_zeroed&lt;ARegionType&gt;(&quot;spacetype sequencer region&quot;);
  art-&gt;regionid = RGN_TYPE_UI;
  art-&gt;flag = ARegionTypeFlag::UsePanelCategoriesSearch;
  art-&gt;prefsizex = UI_SIDEBAR_PANEL_WIDTH * 1.3f;
  art-&gt;keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art-&gt;message_subscribe = ED_area_do_mgs_subscribe_for_tool_ui;
  art-&gt;listener = sequencer_buttons_region_listener;
  art-&gt;init = sequencer_buttons_region_init;
  art-&gt;snap_size = ED_region_generic_panel_region_snap_size;
  art-&gt;draw = sequencer_buttons_region_draw;
  BLI_addhead(&amp;st-&gt;regiontypes, art);

  sequencer_buttons_register(art);
  /* Toolbar. */
  art = MEM_new_zeroed&lt;ARegionType&gt;(&quot;spacetype sequencer tools region&quot;);
  art-&gt;regionid = RGN_TYPE_TOOLS;
  art-&gt;prefsizex = int(UI_TOOLBAR_WIDTH);
  art-&gt;prefsizey = 50; /* XXX */
  art-&gt;keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art-&gt;message_subscribe = ED_region_generic_tools_region_message_subscribe;
  art-&gt;snap_size = ED_region_generic_tools_region_snap_size;
  art-&gt;init = sequencer_tools_region_init;
  art-&gt;draw = sequencer_tools_region_draw;
  art-&gt;listener = sequencer_main_region_listener;
  BLI_addhead(&amp;st-&gt;regiontypes, art);</code></pre></td>
</tr></tbody>
</table>

### 15. Shared button appearance and state

Medusa selects colors and draws the button directly. Blender resolves theme colors for selected, hovered, disabled, warning and multi-edit states before the common widget renderer paints the control.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_widgets.cpp#L15">src/medusa_widgets.cpp</a> · lines 15-56</small><pre><code class="language-cpp">void ButtonWidget::draw(Graphics &amp;gfx) {
    Color bg = is_primary_ ? Theme::kAccentBg : Theme::kControlBg;
    if (is_primary_ &amp;&amp; is_hovered_) bg = Theme::kAccentHover;
    else if (is_active_ || is_pressed_) bg = Theme::kSelectionBg;
    else if (!is_primary_ &amp;&amp; is_hovered_) bg = Theme::kControlHover;

    if (!is_flat_) {
        gfx.fill_rounded_rect(bounds_, 3.0f, bg);
        if (is_primary_) {
            gfx.draw_rounded_rect(bounds_, 4.0f, Theme::kAccentBorder, 1.0f);
        } else if (is_active_) {
            gfx.draw_rounded_rect(bounds_, 4.0f, Theme::kBorderBright, 1.0f);
        } else {
            gfx.draw_rounded_rect(bounds_, 4.0f, Theme::kBorderSubtle, 1.0f);
        }
    } else {
        if (is_hovered_ || is_pressed_ || (is_active_ &amp;&amp; flat_active_fill_)) {
            gfx.fill_rounded_rect(bounds_, 3.0f, bg);
        }
    }

    if (!icon_.empty() &amp;&amp; !label_.empty()) {
        float sz = std::min(bounds_.height - 8.0f, 16.0f);
        Rect icon_rect(bounds_.x + 9.0f, bounds_.y + (bounds_.height - sz) * 0.5f, sz, sz);
        Color icon_col = is_primary_ ? Theme::kAccentText : (is_active_ || is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary);
        IconRenderer::draw_icon(gfx, icon_, icon_rect, icon_col);
        Rect label_rect(bounds_.x + 31.0f, bounds_.y, bounds_.width - 36.0f, bounds_.height);
        Color txt_col = is_primary_ ? Theme::kAccentText : (is_active_ || is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary);
        gfx.draw_text(label_, label_rect, txt_col, Theme::kUiFont, is_primary_ || is_active_, true, true, Theme::kUiFontFamily);
    } else if (!icon_.empty()) {
        float sz = std::min(bounds_.width, bounds_.height) * 0.58f;
        Rect icon_rect(bounds_.x + (bounds_.width - sz) * 0.5f,
                       bounds_.y + (bounds_.height - sz) * 0.5f,
                       sz, sz);
        Color icon_col = is_primary_ ? Theme::kAccentText : (is_active_ || is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary);
        IconRenderer::draw_icon(gfx, icon_, icon_rect, icon_col);
    } else if (!label_.empty()) {
        Color txt_col = is_primary_ ? Theme::kAccentText : (is_active_ || is_hovered_ ? Theme::kTextPrimary : Theme::kTextSecondary);
        gfx.draw_text(label_, bounds_, txt_col, Theme::kUiFont, is_primary_ || is_active_, true, true, Theme::kUiFontFamily);
    }

}</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/interface/interface_widgets.cc#L3322">../Blender_Editor_READONLY/interface/interface_widgets.cc</a> · lines 3322-3374</small><pre><code class="language-cpp">static void widget_state(WidgetType *wt, const WidgetStateInfo *state, EmbossType emboss)
{
  const uiWidgetStateColors *wcol_state = wt-&gt;wcol_state;

  if (state-&gt;but_flag &amp; BUT_LIST_ITEM) {
    /* Override default widget&#39;s colors. */
    const bTheme *btheme = theme::theme_get();
    wt-&gt;wcol_theme = &amp;btheme-&gt;tui.wcol_list_item;

    if (state-&gt;but_flag &amp; (BUT_DISABLED | BUT_INACTIVE | UI_SEARCH_FILTER_NO_MATCH)) {
      widget_type_color_disabled(wt, state);
    }
  }

  wt-&gt;wcol = *(wt-&gt;wcol_theme);
  if (state-&gt;draw_as_link) {
    theme::get_color_4ubv(TH_LINK, wt-&gt;wcol.text);
    theme::get_color_4ubv(TH_LINK, wt-&gt;wcol.text_sel);
    if (state-&gt;but_flag &amp; (BUT_DISABLED | BUT_INACTIVE | UI_SEARCH_FILTER_NO_MATCH)) {
      widget_color_disabled(wt-&gt;wcol, state);
    }
  }
  const uchar *color_blend = widget_color_blend_from_flags(wcol_state, state, emboss);

  if (state-&gt;but_flag &amp; UI_SELECT) {
    copy_v4_v4_uchar(wt-&gt;wcol.inner, wt-&gt;wcol.inner_sel);
    copy_v4_v4_uchar(wt-&gt;wcol.outline, wt-&gt;wcol.outline_sel);
    if (color_blend != nullptr) {
      color_blend_v3_v3(wt-&gt;wcol.inner, color_blend, wcol_state-&gt;blend);
    }

    copy_v3_v3_uchar(wt-&gt;wcol.text, wt-&gt;wcol.text_sel);

    std::swap(wt-&gt;wcol.shadetop, wt-&gt;wcol.shadedown);
  }
  else {
    if (state-&gt;but_flag &amp; BUT_ACTIVE_DEFAULT) {
      copy_v4_v4_uchar(wt-&gt;wcol.inner, wt-&gt;wcol.inner_sel);
      copy_v4_v4_uchar(wt-&gt;wcol.outline, wt-&gt;wcol.outline_sel);
      copy_v4_v4_uchar(wt-&gt;wcol.text, wt-&gt;wcol.text_sel);
    }
    if (color_blend != nullptr) {
      color_blend_v3_v3(wt-&gt;wcol.inner, color_blend, wcol_state-&gt;blend);
    }

    /* Add &quot;hover&quot; highlight. Ideally this could apply in all cases,
     * even if UI_SELECT. But currently this causes some flickering
     * as buttons can be created and updated without respect to mouse
     * position and so can draw without UI_HOVER set.  See D6503. */
    if (state-&gt;but_flag &amp; UI_HOVER) {
      widget_active_color(&amp;wt-&gt;wcol);
    }
  }</code></pre></td>
</tr></tbody>
</table>

### 16. Theme tokens and icon drawing

Medusa's charcoal values and sizing are compile-time constants. Blender resolves colors through a user theme at draw time. Medusa's custom SVG and vector icons are kept in its own resource set; Blender icon IDs are not copied.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="include/medusa/medusa_theme.hpp#L6">include/medusa/medusa_theme.hpp</a> · lines 6-42</small><pre><code class="language-cpp">// Shared DCC theme tokens. Editor chrome stays within a neutral grayscale.
struct Theme {
    static constexpr Color kChassisBg       = Color(0.086f, 0.086f, 0.086f, 1.0f); // #161616
    static constexpr Color kPanelBg         = Color(0.114f, 0.114f, 0.114f, 1.0f); // #1D1D1D
    static constexpr Color kPanelHeaderBg   = Color(0.145f, 0.145f, 0.145f, 1.0f); // #252525
    static constexpr Color kControlBg       = Color(0.196f, 0.196f, 0.196f, 1.0f); // #323232
    static constexpr Color kControlHover    = Color(0.231f, 0.231f, 0.231f, 1.0f); // #3B3B3B
    static constexpr Color kControlActive   = Color(0.255f, 0.255f, 0.255f, 1.0f); // #414141
    static constexpr Color kSelectionBg     = Color(0.216f, 0.216f, 0.216f, 1.0f); // #373737

    static constexpr Color kBorderSubtle   = Color(0.184f, 0.184f, 0.184f, 1.0f); // #2F2F2F
    static constexpr Color kBorderBright   = Color(0.290f, 0.290f, 0.290f, 1.0f); // #4A4A4A
    static constexpr Color kSplitterHover  = Color(0.510f, 0.510f, 0.510f, 1.0f); // #828282

    static constexpr Color kTextPrimary   = Color(0.910f, 0.910f, 0.910f, 1.0f); // #E8E8E8
    static constexpr Color kTextSecondary = Color(0.710f, 0.710f, 0.710f, 1.0f); // #B5B5B5
    static constexpr Color kTextMuted     = Color(0.545f, 0.545f, 0.545f, 1.0f); // #8B8B8B

    static constexpr Color kAccent        = Color(0.800f, 0.800f, 0.800f, 1.0f); // #CCCCCC
    static constexpr Color kAccentBg      = Color(0.286f, 0.286f, 0.286f, 1.0f); // #494949
    static constexpr Color kAccentHover   = Color(0.333f, 0.333f, 0.333f, 1.0f); // #555555
    static constexpr Color kAccentBorder  = Color(0.420f, 0.420f, 0.420f, 1.0f); // #6B6B6B
    static constexpr Color kAccentText    = Color(0.941f, 0.941f, 0.941f, 1.0f); // #F0F0F0

    static constexpr Color kViewportBg       = Color(0.106f, 0.106f, 0.106f, 1.0f); // #1B1B1B
    static constexpr Color kViewportGrid     = Color(0.184f, 0.184f, 0.184f, 1.0f); // #2F2F2F
    static constexpr Color kViewportGridMajor = Color(0.255f, 0.255f, 0.255f, 1.0f); // #414141
    static constexpr Color kKeyframeGold   = kAccent;
    static constexpr Color kPlayheadBlue   = kAccent;
    static constexpr Color kChannelBg      = Color(0.137f, 0.137f, 0.137f, 1.0f);
    static constexpr Color kTrackLaneEven  = Color(0.157f, 0.157f, 0.157f, 1.0f);
    static constexpr Color kTrackLaneOdd   = Color(0.180f, 0.180f, 0.180f, 1.0f);

    // Axis colors remain semantic; they are not used for selection or focus.
    static constexpr Color kAxisX          = Color(0.780f, 0.350f, 0.350f, 1.0f);
    static constexpr Color kAxisY          = Color(0.350f, 0.700f, 0.400f, 1.0f);
    static constexpr Color kAxisZ          = Color(0.350f, 0.550f, 0.800f, 1.0f);</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/interface/interface_widgets.cc#L3322">../Blender_Editor_READONLY/interface/interface_widgets.cc</a> · lines 3322-3355</small><pre><code class="language-cpp">static void widget_state(WidgetType *wt, const WidgetStateInfo *state, EmbossType emboss)
{
  const uiWidgetStateColors *wcol_state = wt-&gt;wcol_state;

  if (state-&gt;but_flag &amp; BUT_LIST_ITEM) {
    /* Override default widget&#39;s colors. */
    const bTheme *btheme = theme::theme_get();
    wt-&gt;wcol_theme = &amp;btheme-&gt;tui.wcol_list_item;

    if (state-&gt;but_flag &amp; (BUT_DISABLED | BUT_INACTIVE | UI_SEARCH_FILTER_NO_MATCH)) {
      widget_type_color_disabled(wt, state);
    }
  }

  wt-&gt;wcol = *(wt-&gt;wcol_theme);
  if (state-&gt;draw_as_link) {
    theme::get_color_4ubv(TH_LINK, wt-&gt;wcol.text);
    theme::get_color_4ubv(TH_LINK, wt-&gt;wcol.text_sel);
    if (state-&gt;but_flag &amp; (BUT_DISABLED | BUT_INACTIVE | UI_SEARCH_FILTER_NO_MATCH)) {
      widget_color_disabled(wt-&gt;wcol, state);
    }
  }
  const uchar *color_blend = widget_color_blend_from_flags(wcol_state, state, emboss);

  if (state-&gt;but_flag &amp; UI_SELECT) {
    copy_v4_v4_uchar(wt-&gt;wcol.inner, wt-&gt;wcol.inner_sel);
    copy_v4_v4_uchar(wt-&gt;wcol.outline, wt-&gt;wcol.outline_sel);
    if (color_blend != nullptr) {
      color_blend_v3_v3(wt-&gt;wcol.inner, color_blend, wcol_state-&gt;blend);
    }

    copy_v3_v3_uchar(wt-&gt;wcol.text, wt-&gt;wcol.text_sel);

    std::swap(wt-&gt;wcol.shadetop, wt-&gt;wcol.shadedown);</code></pre></td>
</tr></tbody>
</table>

### 17. Icon registry and rendering

Both centralize icon rendering and sizing. Medusa resolves string names to custom SVG/path geometries and tints them with the current color. Blender resolves registered integer icon IDs through the shared interface icon system.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_icons.cpp#L933">src/medusa_icons.cpp</a> · lines 933-958</small><pre><code class="language-cpp">void IconRenderer::draw_icon(Graphics &amp;gfx, const std::string &amp;name, const Rect &amp;dest, Color color) {
    if (draw_svg_icon(gfx, name, dest, color)) return;
    ID2D1DeviceContext *d2d = gfx.get_context();
    ID2D1Factory1 *factory = gfx.get_factory();
    if (!d2d || !factory) return;

    ID2D1PathGeometry *geom = get_geometry(factory, name);
    if (!geom) return;

    auto* brush = gfx.get_brush(color);
    if (!brush) return;

    float scale = std::min(dest.width, dest.height) / 24.0f;
    float offset_x = dest.x + (dest.width - 24.0f * scale) * 0.5f;
    float offset_y = dest.y + (dest.height - 24.0f * scale) * 0.5f;

    D2D1_MATRIX_3X2_F old_transform;
    d2d-&gt;GetTransform(&amp;old_transform);

    D2D1_MATRIX_3X2_F m = D2D1::Matrix3x2F::Scale(scale, scale) *
                          D2D1::Matrix3x2F::Translation(offset_x, offset_y) *
                          old_transform;
    d2d-&gt;SetTransform(m);
    d2d-&gt;FillGeometry(geom, brush);
    d2d-&gt;SetTransform(old_transform);
}</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/interface/interface_icons.cc#L2288">../Blender_Editor_READONLY/interface/interface_icons.cc</a> · lines 2288-2313</small><pre><code class="language-cpp">void icon_draw_ex(float x,
                  float y,
                  int icon_id,
                  float aspect,
                  float alpha,
                  float desaturate,
                  const uchar mono_color[4],
                  const bool mono_border,
                  const IconTextOverlay *text_overlay,
                  const bool inverted,
                  const float scale)
{
  const int draw_size = get_draw_size(ICON_SIZE_ICON) * scale;
  icon_draw_size(x,
                 y,
                 icon_id,
                 aspect,
                 alpha,
                 ICON_SIZE_ICON,
                 draw_size,
                 desaturate,
                 mono_color,
                 mono_border,
                 text_overlay,
                 inverted);
}</code></pre></td>
</tr></tbody>
</table>

### 18. Native input and Blender keymap dispatch

Medusa's Win32 window procedure receives WM_PAINT, WM_CLOSE, keyboard and mouse messages, then calls editor methods directly. Blender associates keymaps and modal operators with a space/region; the general event-dispatch system is outside this read-only editor subset.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_window.cpp#L1311">src/medusa_window.cpp</a> · lines 1311-1337</small><pre><code class="language-cpp">LRESULT Window::handle_message(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT &amp;&amp; editor_) {
                CursorType cur = editor_-&gt;get_cursor_type();
                if (cur == CursorType::SizeWE) {
                    SetCursor(LoadCursor(nullptr, IDC_SIZEWE));
                    return TRUE;
                } else if (cur == CursorType::SizeNS) {
                    SetCursor(LoadCursor(nullptr, IDC_SIZENS));
                    return TRUE;
                }
            }
            break;

        case WM_ERASEBKGND:
            // Suppress standard GDI flicker
            return 1;

        case WM_PAINT:
            on_paint();
            return 0;

        case WM_CLOSE:
            if (!confirm_project_change()) return 0;
            DestroyWindow(hwnd_);
            return 0;</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_view3d/view3d_ops.cc#L270">../Blender_Editor_READONLY/space_view3d/view3d_ops.cc</a> · lines 270-285</small><pre><code class="language-cpp">void view3d_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, &quot;3D View Generic&quot;, SPACE_VIEW3D, RGN_TYPE_WINDOW);

  /* only for region 3D window */
  WM_keymap_ensure(keyconf, &quot;3D View&quot;, SPACE_VIEW3D, RGN_TYPE_WINDOW);

  fly_modal_keymap(keyconf);
  walk_modal_keymap(keyconf);
  viewrotate_modal_keymap(keyconf);
  viewmove_modal_keymap(keyconf);
  viewzoom_modal_keymap(keyconf);
  viewdolly_modal_keymap(keyconf);
  viewplace_modal_keymap(keyconf);
  vr_location_scouting_capture_review_modal_keymap(keyconf);
}</code></pre></td>
</tr></tbody>
</table>

### 19. Commands, save/load and undo history

Medusa widgets set string command IDs, and Window executes project save/open plus a serialized snapshot undo/redo history. Blender's topbar undo-history menu enumerates the global undo stack and invokes the undo-history operator; Blender file persistence itself is outside this editor-source subset.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_window.cpp#L217">src/medusa_window.cpp</a> · lines 217-229, 288-300</small><pre><code class="language-cpp">void Window::dispatch_editor_command() {
    if (!editor_) return;
    const std::string command = editor_-&gt;take_pending_command();
    if (command.empty()) {
        if (editor_-&gt;get_state().is_playing) start_animation_timer();
        else if (animation_timer_active_) {
            KillTimer(hwnd_, 3);
            animation_timer_active_ = false;
            animation_timer_interval_ = 0;
        }
        if (!defer_project_dirty_) update_project_dirty();
        return;
    }

// ...

    } else if (command == &quot;undo&quot;) {
        apply_history_step(false);
    } else if (command == &quot;redo&quot;) {
        apply_history_step(true);
    } else if (command == &quot;save_project&quot;) {
        save_project(false);
    } else if (command == &quot;save_project_as&quot;) {
        save_project(true);
    } else if (command == &quot;reset_layout&quot;) {
        editor_-&gt;reset_layout();
        request_redraw();
    } else if (command == &quot;frame_selected&quot;) {
        editor_-&gt;get_state().frame_selected();</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_topbar/space_topbar.cc#L274">../Blender_Editor_READONLY/space_topbar/space_topbar.cc</a> · lines 274-315</small><pre><code class="language-cpp">static void undo_history_draw_menu(const bContext *C, Menu *menu)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm-&gt;runtime-&gt;undo_stack == nullptr) {
    return;
  }

  int undo_step_count = 0;
  int undo_step_count_all = 0;
  for (UndoStep &amp;us : wm-&gt;runtime-&gt;undo_stack-&gt;steps.items_reversed()) {
    undo_step_count_all += 1;
    if (us.skip) {
      continue;
    }
    undo_step_count += 1;
  }

  ui::Layout &amp;split = menu-&gt;layout-&gt;split(0.0f, false);
  ui::Layout *column = nullptr;

  const int col_size = 20 + (undo_step_count / 12);

  undo_step_count = 0;

  /* Reverse the order so the most recent state is first in the menu. */
  int i = undo_step_count_all - 1;
  for (UndoStep *us = wm-&gt;runtime-&gt;undo_stack-&gt;steps.last(); us; us = us-&gt;prev, i--) {
    if (us-&gt;skip) {
      continue;
    }
    if (!(undo_step_count % col_size)) {
      column = &amp;split.column(false);
    }
    const bool is_active = (us == wm-&gt;runtime-&gt;undo_stack-&gt;step_active);
    ui::Layout &amp;row = column-&gt;row(false);
    row.enabled_set(!is_active);
    PointerRNA op_ptr = row.op(&quot;ED_OT_undo_history&quot;,
                               CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, us-&gt;name),
                               is_active ? ICON_LAYER_ACTIVE : ICON_NONE);
    RNA_int_set(&amp;op_ptr, &quot;item&quot;, i);
    undo_step_count += 1;
  }</code></pre></td>
</tr></tbody>
</table>

### 20. Redraw scope and UI performance

Medusa's Screen::draw traverses every area on a draw, while Area clips and draws its regions. Blender can tag an individual ARegion for redraw and clear that region's draw state. This is the clearest source-level difference in redraw granularity.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_screen.cpp#L20">src/medusa_screen.cpp</a> · lines 20-37, 89-97</small><pre><code class="language-cpp">void Area::draw(Graphics &amp;gfx) {
    // Keep dock areas visually connected; separators define the work regions
    // without outlining every panel like a floating card.
    gfx.fill_rect(bounds_, Theme::kPanelBg);

    for (auto &amp;reg : regions_) {
        if (reg) {
            gfx.push_clip(reg-&gt;clip_bounds());
            reg-&gt;draw(gfx);
            gfx.pop_clip();
        }
    }

    gfx.draw_line(Point(bounds_.right(), bounds_.y), Point(bounds_.right(), bounds_.bottom()),
                  Theme::kBorderSubtle, 1.0f);
    gfx.draw_line(Point(bounds_.x, bounds_.bottom()), Point(bounds_.right(), bounds_.bottom()),
                  Theme::kBorderSubtle, 1.0f);
}

// ...

void Screen::draw(Graphics &amp;gfx) {
    gfx.clear(Theme::kChassisBg);

    for (auto &amp;area : areas_) {
        if (area) {
            area-&gt;draw(gfx);
        }
    }
}</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/screen/area.cc#L681">../Blender_Editor_READONLY/screen/area.cc</a> · lines 681-694</small><pre><code class="language-cpp">void ED_region_tag_redraw(ARegion *region)
{
  /* don&#39;t tag redraw while drawing, it shouldn&#39;t happen normally
   * but python scripts can cause this to happen indirectly */
  if (region &amp;&amp; !(region-&gt;runtime-&gt;do_draw &amp; RGN_DRAWING)) {
    /* zero region means full region redraw */
    region-&gt;runtime-&gt;do_draw &amp;= ~(RGN_DRAW_PARTIAL | RGN_DRAW_NO_REBUILD |
                                  RGN_DRAW_EDITOR_OVERLAYS);
    region-&gt;runtime-&gt;do_draw |= RGN_DRAW;
    /* Also refresh the IME cursor position on the next draw. */
    region-&gt;runtime-&gt;do_ime = true;
    region-&gt;runtime-&gt;drawrct = rcti{};
  }
}</code></pre></td>
</tr></tbody>
</table>

### 21. Outliner refresh and cached rows

Medusa reuses its row vector while scene_generation is unchanged, updating selection and flags in place. Blender rebuilds its data-backed tree only on a requested rebuild, then performs selection synchronization and scroll-to-active handling.

<table>
<thead><tr><th>Medusa Platinum</th><th>Blender editor source</th></tr></thead>
<tbody><tr>
<td valign="top"><small><a href="src/medusa_editor.cpp#L1571">src/medusa_editor.cpp</a> · lines 1571-1595</small><pre><code class="language-cpp">    void rebuild_rows() {
        if (rows_generation_ == state_.scene_generation) {
            for (size_t i = 0; i &lt; rows_.size() &amp;&amp; i &lt; row_node_indices_.size(); ++i) {
                const uint32_t index = row_node_indices_[i];
                if (index &gt;= state_.scene_nodes.size()) continue;
                const auto&amp; node = state_.scene_nodes[index];
                rows_[i].set_selected(state_.selected_scene_node == static_cast&lt;int&gt;(index));
                rows_[i].set_visible(node.visible);
                rows_[i].set_locked(node.locked);
                rows_[i].set_renderable(node.renderable);
                rows_[i].set_expanded(node.expanded);
            }
            return;
        }
        rows_.clear();
        row_node_indices_.clear();
        std::vector&lt;uint8_t&gt; visited(state_.scene_nodes.size(), 0);
        for (uint32_t i = 0; i &lt; state_.scene_nodes.size(); ++i) {
            const int parent = state_.scene_nodes[i].parentIndex;
            if (parent &lt; 0 || static_cast&lt;size_t&gt;(parent) &gt;= state_.scene_nodes.size())
                append_node(i, 0, visited);
        }
        for (uint32_t i = 0; i &lt; state_.scene_nodes.size(); ++i)
            if (!visited[i]) append_node(i, 0, visited);
        rows_generation_ = state_.scene_generation;</code></pre></td>
<td valign="top"><small><a href="../Blender_Editor_READONLY/space_outliner/outliner_draw.cc#L3874">../Blender_Editor_READONLY/space_outliner/outliner_draw.cc</a> · lines 3874-3898</small><pre><code class="language-cpp">  if (do_rebuild) {
    outliner_build_tree(
        mainvar, workspace, tvc.scene, tvc.view_layer, space_outliner, region); /* Always. */

    /* If global sync select is dirty, flag other outliners. */
    if (ED_outliner_select_sync_is_dirty(C)) {
      ED_outliner_select_sync_flag_outliners(C);
    }

    /* Sync selection state from view layer. */
    if (space_outliner-&gt;flag &amp; SO_SYNC_SELECT) {
      if (!ELEM(space_outliner-&gt;outlinevis,
                SO_LIBRARIES,
                SO_OVERRIDES_LIBRARY,
                SO_DATA_API,
                SO_ID_ORPHANS))
      {
        short idcode = 0;
        if (outliner_sync_selection(C, tvc, space_outliner, idcode) &amp;&amp;
            (space_outliner-&gt;flag &amp; SO_SCROLL_TO_ACTIVE))
        {
          outliner_scroll_to_active(space_outliner, region, idcode);
        }
      }
    }</code></pre></td>
</tr></tbody>
</table>

## Source-grounded conclusions

- The visible Blender-like vocabulary is present in the current Medusa source: compact headers, tool buttons, a dark neutral palette, a scene hierarchy, context properties and a frame timeline.
- The implementation is Medusa’s own small Direct2D/Win32 editor layer. It does not use Blender’s editor framework, RNA property system, workspace/tool system, or GPU gizmo/operator architecture.
- The strongest feature gap is animation: Medusa currently has keyframe markers and playback controls, while Blender separates its Dope Sheet and full strip-based Sequencer.
- The strongest optimization gap visible in these excerpts is redraw granularity: Medusa traverses all editor areas on draw; Blender tracks redraw state per region. Blender also culls animation channels to the visible view and builds its Outliner tree only when a rebuild is requested.
- This comparison describes the source that is in the folders now. It is not a before/after patch report and does not imply that an earlier UI pass remains in the replaced copy.

## Reference map

The inventory covers every Medusa editor-related source file. The paired excerpts cover the main visual regions and shared UI mechanisms. Project serialization, scene import and renderer integration are listed in the inventory because their closest Blender counterparts live outside the supplied editor-source folder. Each excerpt links to its full source file.

Blender source paths are relative to the supplied read-only editor-source folder. The Blender comparison is limited to editor code where possible; its window manager, data model, file I/O and render engine live in other Blender subsystems.

