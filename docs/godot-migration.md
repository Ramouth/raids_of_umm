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

## Completed: dungeon combat vertical slice

`godot/native` is an isolated CMake build of the existing `CombatEngine`,
`CombatAI`, and `ResourceManager`. It exports a `UmmCombat` RefCounted GDExtension
class; no SDL/OpenGL renderer or state class is linked. The only change to the
shared engine is read-only initiative accessors. Bindings target the stable Godot
4.5 API, supported by the 4.7 runtime; the library manifest currently covers Linux
x86-64 only. Dependencies are pinned by revision/version and SHA-256.

The bridge accepts `begin_battle`, `act`, and `acknowledge`. JSON responses contain
an authoritative snapshot, ordered combat events, and an acknowledgement ticket.
The bridge validates occupancy, movement and attack range (including exhausted
ammo), ownership of the active turn, and the pending-animation lock. Godot renders
events in order before applying the next snapshot and acknowledging its ticket;
new player or AI actions cannot skip past unfinished effects. Movement animation
paths avoid occupied cells. Each stack has its own presentation node.

Exploration now enters battles from dungeon cells and resumes in the same scene.
The expedition stores survivor counts, collected artifacts, and cleared dungeon
cells in memory. Defeat does not regenerate the army, retreat gives no reward,
and a victory reward can only be applied once per dungeon. The original starting
army and dungeon guard composition are retained. This is a single guard encounter,
not a port of the original dungeon interior or special-character systems.

Check `godot/README.md` for launch, native regression tests, full Godot integration
tests, and rendered captures. No changes to the original game's renderer are
required. Dedicated unit attack/death sheets and battle balance remain follow-ups.

## Next milestone: connect the rest of gameplay

1. Extract a separate C++ library target for the existing pure gameplay modules.
   Extend the combat target to `HexGrid`, `WorldMap`, `Hero`, and turn management.
   Preserve the existing logic tests.
2. Wrap that library with a small GDExtension interface. Godot sends commands
   such as selecting a hero, requesting a route, moving, and ending a turn.
   C++ returns authoritative state and events; avoid a Godot node per C++ datum.
3. Extract gameplay embedded in screen/state classes as each screen is migrated.
   `AdventureState.cpp` currently combines input, rendering, and game transitions,
   so it cannot simply be loaded into Godot unchanged.
4. Replace the preview route finder and unrestricted movement with that interface.
   Define path conventions explicitly: the C++ path includes the start cell,
   while the current Godot animation queue contains only subsequent steps.
5. Extend exploration/combat/rewards with town recruitment and save/load. C++
   remains the sole authority for costs, combat outcomes, ownership,
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
