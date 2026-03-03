#pragma once
#include "hex/HexCoord.h"
#include <string>

/*
 * Hero — a named character on the adventure map.
 *
 * Currently holds only the data needed for adventure-map movement.
 * This struct will grow to include RPG stats, army slots, spellbook,
 * equipment, and skill trees — all as separate modules that attach here.
 *
 * Keep concerns separated: this file must not include anything from
 * render/, core/, or adventure/. It may include world/ and hex/ only
 * for spatial data types.
 */
struct Hero {
    std::string name      = "Hero";
    HexCoord    pos       { 0, 0 };
    int         movesMax  { 8 };
    int         movesLeft { 8 };

    void resetMoves() { movesLeft = movesMax; }
    bool hasMoves()   const { return movesLeft > 0; }
    bool canReach(int steps) const { return steps <= movesLeft; }
};
