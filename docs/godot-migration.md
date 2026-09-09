# Godot migration

## Decision and first milestone

Use Godot 4 for the fixed-view 2D presentation. The initial proof lives under
`godot/`, alongside the original C++ game. It loads the canonical map and provides
interactive exploration with the existing assets. This milestone establishes the
scene, asset staging, camera, UI, and map-coordinate boundary.

Godot owns rendering, scene composition, animation, UI, and input dispatch.
`UmmMapData` adapts the version-1 map format to the preview. Its temporary route
finder uses the existing six axial directions and the loader's road-cost rule
(`min(moveCost, 0.5)`), including maps that already store discounted road costs.

The map intentionally uses native Polygon2D, Sprite2D, Line2D, and shader resources
for layered scenery. It does not require terrain art to be confined to hex tiles.
A hex TileMapLayer may be useful later for authored transition tiles, but it is
not a prerequisite for retaining the logical hex grid.

## Next milestone: connect gameplay

1. Extract a separate C++ library target for the existing pure gameplay modules.
   Start with `HexGrid`, `WorldMap`, `CombatEngine`, `CombatAI`, `Hero`, resources,
   and turn management. Preserve the existing logic tests.
2. Wrap that library with a small GDExtension interface. Godot sends commands
   such as selecting a hero, requesting a route, moving, and ending a turn.
   C++ returns authoritative state and events; avoid a Godot node per C++ datum.
3. Extract gameplay embedded in screen/state classes as each screen is migrated.
   `AdventureState.cpp` currently combines input, rendering, and game transitions,
   so it cannot simply be loaded into Godot unchanged.
4. Replace the preview route finder and unrestricted movement with that interface.
   Define path conventions explicitly: the C++ path includes the start cell,
   while the current Godot animation queue contains only subsequent steps.
5. Connect one complete loop: exploration, combat, rewards, town recruitment, and
   save/load. C++ remains the sole authority for costs, combat outcomes, ownership,
   turn progression, and serialization.

GDScript should orchestrate views and animation. Do not independently reimplement
the combat or economy rules while the existing C++ systems can be retained.

## Visual acceptance before expanding

- Inspect the scene at 1× and overview zoom, with and without the hex overlay.
- Walk through the town and past a dungeon; verify sprite feet and occlusion.
- Check that road routing, blocked terrain, and hover information agree.
- Decide whether the layered 2D direction fits the desired desert atmosphere.
- Establish final terrain transitions and consistent palette/scale before replacing
  every legacy asset. The current missing-art markers are explicit placeholders.

Only after the playable loop works should the new project replace the existing
launcher. Packaging and deployment are separate milestones.
