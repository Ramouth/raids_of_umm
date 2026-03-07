#pragma once
#include "core/StateMachine.h"
#include "hero/Hero.h"
#include "render/HUDRenderer.h"
#include <glad/glad.h>
#include <string>

/*
 * EquipmentState — G1–G3
 *
 * Equip / unequip WondrousItems onto SpecialCharacters in the hero's party.
 *
 * Directly mutates hero.items and hero.specials[i].equipped[].
 * No shared_ptr result needed — the Hero reference outlives this state.
 *
 * Slot layout (matches SpecialCharacter::equipped[] indices):
 *   [0] weapon   [1] armor   [2] amulet   [3] trinket
 *
 * G3 cursed rule: equipped[i] with item->cursed == true shows a red border
 * and cannot be unequipped.
 *
 * Controls:
 *   Click item in bag  → select it
 *   Click matching SC slot (empty) → equip selected item
 *   Click occupied slot (not cursed, no item selected) → unequip to bag
 *   ESC / EXIT button  → popState
 */
class EquipmentState final : public GameState {
public:
    explicit EquipmentState(Hero& hero);

    void onEnter()  override;
    void onExit()   override;
    void update(float dt) override;
    void render()   override;
    bool handleEvent(void* sdlEvent) override;

private:
    // Map slot name string → equipped[] index.
    static int        slotIndex(const std::string& slotName);
    // Short label shown on empty slot boxes.
    static const char* slotLabel(int idx);

    // Equip m_selectedItem onto SC[scIdx] in the correct slot.
    void equipItem(int scIdx);
    // Return equipped[slotIdx] of SC[scIdx] to the hero's bag.
    void unequipSlot(int scIdx, int slotIdx);

    Hero&               m_hero;
    const WondrousItem* m_selectedItem = nullptr;  // currently selected bag item
    HUDRenderer         m_hud;
};
