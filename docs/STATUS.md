# Medusa Platinum — DCC and Renderer Status

**Reviewed:** 2026-11-02  
**Current phase:** Early functional prototype (pre-alpha)  
**Overall production grade:** Not production-ready. The application has a working native shell, a shared project with Scene / Shot / Production workspaces, a GLB/glTF import path, interactive scene controls, a progressive renderer, and PNG/EXR output. It is still missing several core content-creation, rendering, animation, reliability, and delivery systems expected of production DCC software.

This is an implementation inventory based on the current Medusa Platinum and ZeGFX source. **[x]** means a code path exists; it does not claim full production validation. **[~]** means partial, limited, or not yet demonstrated broadly. **[ ]** means absent or not connected to the editor. This checklist is an engineering status, not a quality certification.

## Current production-readiness work

**Phase 1 — durable project and scene foundation: implemented in source; changed translation units compile.** Project files now write format version 4 with project/scene IDs, asset and material references, stable scene-node IDs, and node keyframes linked by node ID. The reader still accepts project versions 1–3 and assigns deterministic IDs to their existing primary asset hierarchy. Project writes use a temporary file followed by an atomic replacement. A bounded in-memory undo/redo history and periodic/on-close recovery snapshots are connected to the editor. This foundation does not yet provide multiple independently editable source assets, automatic source relinking, full preferences, multi-scene/shot management, a full application link/launch check, or crash-recovery qualification.

## Product readiness at a glance

| Area | Current grade | Production status |
|---|---|---|
| Application shell and workspace flow | Prototype foundation | Useful structure; command coverage and workflow depth are limited |
| Project and settings management | Prototype foundation | Versioned project state, stable identities, undo/redo, and recovery exist; preferences and general asset management are limited |
| Asset import | Experimental | GLB/glTF path only; narrow material/texture support and eager triangle expansion |
| Scene assembly | Early prototype | Imported hierarchy can be selected and transformed; creating a complete set inside Medusa is limited |
| Viewport and navigation | Prototype | Interactive navigation, picking, grid, camera framing, and transforms exist; viewport is coupled to preview rendering |
| Camera and lighting | Prototype | Basic camera controls and one simple key light; professional camera/light/environment systems are missing |
| Animation | Early prototype | Basic transform keyframes, timeline, and playback; no graph editor, rigging, or character adaptation |
| Rendering | Experimental | ZeGFX production pipeline is connected; current shading and lighting are far short of a complete production renderer |
| Output and delivery | Prototype | Single frame/sequence PNG or EXR export exists; output controls and delivery management are limited |
| Reliability, scale, and release | Not established | No production acceptance evidence for large scenes, long renders, crash recovery, or release support |

## 1. Application, platform, and editor shell

- [x] Native Windows desktop application and resizable editor window.
- [x] Direct2D / DirectWrite editor interface.
- [x] ZeGFX creates and owns the D3D12 production device and graphics queue; Medusa shares it with its interface composition path.
- [x] Main editor is divided into Scene, Shot, and Production workspaces over one shared project state.
- [x] Main menus, tool shelf, viewport header, scene collection, properties, and timeline are present.
- [~] The menus are only partly populated; Edit is currently mostly layout reset, and Create currently offers model import rather than a complete object-creation system.
- [ ] Cross-platform support, a documented minimum-hardware matrix, and a verified fallback policy.
- [ ] Workspace customization beyond the current fixed editor regions and saved panel sizes.

**Production gate:** Document and support target Windows/GPU configurations; complete command coverage; handle device loss and window/display changes reliably; establish a release and update process.

## 2. Project, scene, and preferences

- [x] Create/open/save/save-as project commands.
- [x] Version 4 `.medusa` projects store project/scene identities, asset and material manifests, stable scene-object identities, linked asset path, active workspace, selected tool, layout, display toggles, timeline range and rate, camera and light controls, render sample/clamp values, scene-node overrides, and keyframes.
- [x] Project readers retain compatibility with versions 1–3 and assign persistent migration IDs to older linked asset hierarchies.
- [x] Project saves are written through a temporary file and replace the destination after a successful write.
- [x] Relative asset paths are stored when the asset is under the project folder.
- [~] A project links one primary imported asset/scene. It does not package or manage a general collection of source assets.
- [~] Project loading depends on the linked external asset remaining available at its saved path.
- [ ] Multiple named scenes and shots, shot versions, asset relinking UI, project browser, project templates, and dependency collection.
- [~] Recovery snapshots are periodically written for dirty projects, offered at next launch when newer than the saved project, and removed after a clean save/close. This path still needs crash/restart qualification; editor preferences remain project-scoped.
- [ ] Collaborative editing, locking, version history, and production handoff metadata.

**Production gate:** Make projects portable and recoverable; support multiple scenes/shots and their dependencies; add safe autosave, backup, and relinking flows.

## 3. Import and asset pipeline

- [x] The editor file picker accepts `.glb` and `.gltf` files.
- [x] The importer builds a scene hierarchy, converts mesh primitives into triangles, reads a subset of material properties, and decodes embedded textures.
- [x] Embedded textures are reduced to a maximum dimension of 2048 pixels during this import path.
- [~] Material import covers base color, emission, metallic, roughness, base-color texture, and alpha mode/cutoff. It is not a complete glTF material or extension implementation.
- [~] The current import action loads one scene asset and submits it as the production scene; it is not a general multi-file add/merge workflow.
- [ ] Native import of USD/USDZ, FBX, OBJ, Alembic, Ptex, and the Moana dataset's JSON/OBJ scene description.
- [ ] Native support for external texture references, broad glTF extensions, high-resolution/UDIM textures, texture color-space controls, and explicit missing-texture repair.
- [ ] Import progress, cancellation, background loading, memory estimates, import presets, validation report, and robust rollback on failed import.
- [ ] Asset browser/catalog, reusable asset instances, and non-destructive source reimport.
- [ ] A scalable instancing representation. The production scene currently stores expanded triangles rather than a compact instance graph.

**Production gate:** Add reliable multi-asset ingest and instancing, expand format/material support, stream assets safely, and make imports diagnosable and reversible.

## 4. Scene assembly and object editing

- [x] Scene collection displays the imported node hierarchy and supports selection, visibility, lock, and renderable state controls.
- [x] Viewport picking can select imported scene nodes.
- [x] Move, rotate, and scale tools and numeric transform controls exist for supported node types.
- [x] A frame-selected action and basic transform snapping toggle exist.
- [~] Transform edits operate on imported hierarchy nodes. This is not a full non-destructive modeling or procedural scene-authoring system.
- [~] Create and scene menus do not yet create a broad range of mesh primitives, collections, lights, cameras, terrain, or environment objects.
- [x] Bounded undo/redo snapshots for editor changes, with a maximum of 64 commands and 64 MiB retained history.
- [ ] Duplication, parenting/reparenting tools, constraints, collections management, editable pivots, and robust multi-selection.
- [ ] Mesh editing, sculpting, curves, procedural modeling, modifiers, geometry nodes, and scene instancing authoring.
- [ ] Scene templates, reusable groups/prefabs, and asset placement browser.

**Production gate:** Provide safe scene creation and edit history, efficient set dressing, instancing, and predictable hierarchy/transform behavior at scale.

## 5. Viewport, navigation, and interaction

- [x] Interactive 3D viewport with grid, safe frame, selection/picking, transform handles, and free viewport navigation.
- [x] Camera Pilot input supports mouse look and keyboard movement, with speed modifiers.
- [x] Viewport navigation state is separate from the active render camera and is saved in the project.
- [~] The viewport preview is driven by the production render pipeline and its progressively accumulated samples; it is not a separate fast raster/material preview mode.
- [~] Current viewport render size follows the available viewport area; an independent render-resolution/output-size workflow is not exposed as a complete user-facing control set.
- [ ] Display modes for wireframe, solid, material preview, and final render preview as distinct, optimized viewport pipelines.
- [ ] Viewport overlays for robust camera/light shapes, selectable helper objects, measurement tools, local view, clipping controls, and configurable grid units.
- [ ] A production interaction pass for high-DPI, multi-monitor, unusual aspect ratios, and large/complex scene responsiveness.

**Production gate:** Keep navigation responsive independently from final rendering; make display modes and overlays complete; verify interaction at large scene sizes and varied display setups.

## 6. Cameras, lenses, and shot layout

- [x] Basic active camera state, transform controls, focal length, focus distance state, safe frame, and camera keyframing are represented.
- [x] Camera Pilot is available for moving the viewport camera.
- [~] Focal length affects the production view; focus distance is stored but a complete depth-of-field / focus-pulling renderer workflow is not established.
- [~] Camera and scene node support is limited to the current imported hierarchy and editor-created control nodes.
- [ ] Create/manage multiple shots and cameras as a shot list, compare cameras, camera rigs, lens/sensor presets, aperture/shutter controls, depth of field, lens distortion, and robust camera bookmarks.
- [ ] Shot cuts, handles, edit decision list, audio sync, and shot-level render ranges.

**Production gate:** Build a usable shot system with camera rigs and film controls, plus independently managed shots and frame ranges.

## 7. Lighting, environment, and materials

- [x] A basic key-light position and energy control is wired to the production view.
- [x] Imported material base color, emission, metallic, roughness, and basic alpha information can feed the scene renderer.
- [~] The current production view exposes a single simple light control; it is not a general light authoring and management system.
- [~] The current renderer uses a simplified direct-light/shadow and ambient contribution model. It does not provide production global illumination or complete physically based light transport.
- [ ] Multiple light types with photometric units, area-light shaping, light linking, light groups, light sampling controls, and per-light render controls.
- [ ] HDRI/environment lighting and rotation/intensity controls connected through the production renderer.
- [ ] Complete material editing, node-based shading, texture maps for normal/roughness/metalness/emission, displacement, subsurface scattering, transmission/refraction, and material libraries.
- [ ] Volume authoring/rendering, hair/curves shading, Ptex, UDIM, and displacement workflows.
- [ ] Color management controls such as view transforms, exposure, display/look selection, and scene-linear output configuration.

**Production gate:** Expand the shading and lighting model, add environment and material authoring, and verify color-managed output against reference scenes.

## 8. Animation, timeline, and character support

- [x] Timeline frame range, current frame, playback rate, forward/reverse playback, stepping, and playback controls exist.
- [x] Manual keyframe insertion and auto-keying exist for camera/light state and supported scene-node transforms.
- [x] Basic interpolation between stored keyframes is implemented.
- [~] Animation is limited to transform values represented by the editor state. Playback is not a complete animation authoring environment.
- [~] Timeline markers show keyed frames, but the editor does not have a full Dope Sheet or Graph Editor.
- [ ] Editable animation curves, handles, interpolation modes, tangent editing, motion paths, retiming, and robust key selection/editing.
- [ ] Character rigs, skeleton/skin editing, retargeting, constraints, IK/FK, animation layers, clip browser, and motion capture workflows.
- [ ] Facial animation, lip sync, audio tracks, and non-linear animation sequencing.
- [ ] Simulation systems for cloth, hair, particles, fluids, and dynamics.

**Production gate:** Make animation easy to author and revise, then add rig/character workflows, graph editing, non-linear shot assembly, and simulation as supported product scope.

## 9. ZeGFX renderer and render integration

- [x] Medusa creates a ZeGFX-owned D3D12 production device and connects ZeGFX's ProductionDCC pipeline.
- [x] Production scenes, camera/light view settings, sample count, and firefly clamp are sent to ZeGFX.
- [x] Progressive sample accumulation and render progress are available to the editor.
- [x] The production pipeline includes a D3D12 renderer path and a CPU fallback path.
- [x] The pipeline has Beauty, Albedo, World Normal, Linear Depth, Diffuse Direct, Diffuse Indirect, and Specular Direct AOV data paths.
- [~] Medusa currently surfaces only a small subset of renderer controls: sample count and firefly clamp.
- [~] The current production shading implementation is a limited renderer: primary camera rays, a direct-light/shadow check, a simple ambient/sky contribution, emission, and a simplified specular term. It is not yet a full multi-bounce path tracer or a production GI renderer.
- [~] Denoising is disabled in the current pipeline configuration.
- [ ] Render AOV selection and viewing, denoising, reflection/transmission transport, multi-bounce indirect lighting, motion blur, depth of field, volumetrics, hair/curve primitives, and production texture systems.
- [ ] Renderer feature qualification and image-quality comparisons against trusted reference implementations.
- [ ] Adaptive sampling, resumable renders, memory-aware scene paging, and robust cancellation/recovery for long jobs.

**Production gate:** Define and implement the intended renderer feature set; expose supported settings honestly; qualify image quality, performance, memory use, and repeatability on reference scenes.

## 10. Production settings, frame rendering, and output

- [x] Render Current Frame and Render Sequence commands exist.
- [x] Sequence output advances through the selected frame range and writes numbered image files.
- [x] PNG and OpenEXR output paths are connected.
- [~] Render dimensions derive from the current viewport dimensions; a full output resolution, aspect-ratio, crop, and pixel-sample preset workflow is not present.
- [~] OpenEXR export exists, but a complete multi-layer EXR/AOV configuration and color pipeline is not exposed through the editor.
- [ ] Render queue with multiple jobs/shots, pause/resume, retry, priority, background rendering, and distributed rendering.
- [ ] Output naming templates, metadata, color-space/bit-depth controls, per-shot destinations, and delivery presets.
- [ ] Reviewable render logs, render history, restartable jobs, and clear progress/estimated-time reporting for production sequences.
- [ ] Movie encoding, audio, compositing, final conform, and deliverable packaging.

**Production gate:** Separate output settings from viewport size, create managed render jobs and reliable sequence recovery, and define color/metadata/delivery behavior.

## 11. Performance, memory, and stability

- [x] Window resizing updates the swap chain and production viewport buffers.
- [x] The ZeGFX production pipeline builds a triangle acceleration structure and uploads scene data for the D3D12 path.
- [~] The current scene representation expands mesh triangles and stores them in memory. This is not a scalable substitute for compact instancing on very large scenes.
- [~] Import and scene submission are synchronous through the editor command path; progress/cancellation and incremental streaming are not established.
- [ ] Documented scene/texture/VRAM limits, predictable out-of-memory handling across all renderer stages, and resource-budget reporting.
- [ ] Large-scene stress qualification, long-sequence soak runs, GPU/device-loss recovery, crash-safe autosave, and diagnostics bundle.
- [ ] Performance budgets for viewport interaction, import, shader/device startup, first sample, samples per second, and output writing.

**Production gate:** Measure on representative hardware and scenes, remove unbounded/eager memory paths, and pass stability and recovery runs over long sessions.

## 12. Interchange, extensibility, and production pipeline

- [x] ZeGFX exposes a standalone production pipeline API consumed directly by Medusa.
- [x] The editor submits renderer-neutral production scene data rather than using the game-engine application path.
- [~] The current Medusa boundary carries triangles, simple materials/textures, nodes, one camera, and a basic key light.
- [ ] USD pipeline, layered scene composition, asset references, versioned interchange, and round-trip validation.
- [ ] Plug-in/script interface for tools, importers, render devices, procedural content, and studio pipeline automation.
- [ ] Render-farm/client integration, job manifests, command-line/batch rendering, and headless project processing.
- [ ] Pipeline documentation, stable public API, compatibility guarantees, and project migration tools.

**Production gate:** Define durable scene interchange and automation interfaces; test round trips and farm/headless workflows with versioned projects.

## 13. Verification, release, and support

- [x] ZeGFX contains unit and integration coverage for parts of its production pipeline and asset importer.
- [~] Renderer tests do not establish production quality across Medusa's full user workflow, supported formats, devices, and large scenes.
- [ ] Medusa end-to-end acceptance suite covering create/open/save/reopen, import, edit, animate, render, and export.
- [ ] Golden-image regression suite with tolerances and reference hardware/configuration.
- [ ] Installer, signed release, crash reporting, update/rollback, user-facing diagnostics, and documented support policy.
- [ ] Production documentation for import compatibility, renderer behavior, color management, animation limits, and output guarantees.

**Production gate:** Establish repeatable release qualification and support processes before calling the application production-ready.

## How far from production?

Medusa is at the **early functional prototype** stage. It can open a compatible GLB/glTF scene, provide basic scene and camera interaction, save a project description linked to that asset, animate a limited set of transforms, progressively render a frame, and write PNG/EXR images. That is enough to run focused renderer integration tests on appropriately sized assets.

It is **not yet a complete rendering DCC**. The largest gaps are scalable scene/import representation, multi-asset set assembly, material and lighting depth, production-grade light transport, animation and character tools, independent output configuration, job management, recovery, and end-to-end qualification. There is no reliable percentage-to-production because production readiness depends on the chosen feature set and acceptance targets; the checklist above defines the remaining gates more usefully.

## Primary source map

- `src/medusa_window.cpp` — application commands, asset loading, project operations, render/sequence export, and renderer updates.
- `src/medusa_production_scene.cpp` — GLB/glTF import-to-production-scene conversion and texture decoding.
- `src/medusa_project.cpp` — `.medusa` project serialization, validation, and save/load behavior.
- `src/medusa_editor.cpp` and `include/medusa/medusa_editor.hpp` — workspaces, UI controls, timeline, interaction, and editor state/keyframes.
- `src/medusa_graphics.cpp` — Windows graphics setup and ZeGFX production pipeline connection.
- `../ZeGFX/include/zegfx/profiles/production/production_pipeline.h` — production renderer scene, settings, AOV, progress, and export API.
- `../ZeGFX/src/render/pipelines/production/production_pipeline.cpp` — current production renderer implementation and CPU path.
- `../ZeGFX/src/render/pipelines/production/production_gpu_d3d12.cpp` — D3D12 production render path.
- `../ZeGFX/include/zegfx/render/production_device.h` — ZeGFX-owned production device contract.

