#pragma once
#include <cstdint>

/*
 * EditorTool — the active editing mode in WorldBuilderState.
 *
 * PaintTile   : left-click sets the hovered tile to the selected terrain.
 * PlaceObject : left-click places an object of the selected type.
 * Erase       : left-click removes any object; resets tile to default Sand.
 * Select      : left-click prints tile/object info to console (inspect mode).
 */
enum class EditorTool : uint8_t {
    PaintTile   = 0,
    PlaceObject = 1,
    Erase       = 2,
    Select      = 3,
};

// Number of tool modes — used for cycling arithmetic.
static constexpr int EDITOR_TOOL_COUNT = 4;
