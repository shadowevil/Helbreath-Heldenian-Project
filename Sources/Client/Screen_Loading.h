// Screen_Loading.h: Loading Screen Interface
//
// Resource loading screen that progressively loads game assets.
// Handles all sprite, tile, effect, and sound loading in stages.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "IGameScreen.h"

class Screen_Loading : public IGameScreen
{
public:
    SCREEN_TYPE(Screen_Loading)

    GameMode get_game_mode() const override { return GameMode::Loading; }

    explicit Screen_Loading(CGame* game);
    ~Screen_Loading() override = default;

    void on_initialize() override;
    void on_update() override;
    void on_render() override;

    // get loading progress (0-100)
    int get_progress() const { return m_iLoadingStage; }

private:
    // Loading stages - each stage loads a batch of resources
    void LoadStage_Interface();      // Stage 0: UI sprites, dialog boxes
    void LoadStage_Tiles1();         // Stage 4: Map tiles, structures, trees
    void LoadStage_Tiles2();         // Stage 8: More tiles, objects
    void LoadStage_Tiles3();         // Stage 12: More tiles, items
    void LoadStage_Equipment1();     // Stage 16: Male/Female equipment base
    void LoadStage_Angels();         // Stage 20: Tutelary angels, player bodies
    void LoadStage_Monsters1();      // Stage 24: Monsters (slime to William)
    void LoadStage_Monsters2();      // Stage 28: Monsters (Kennedy to EnergyBall)
    void LoadStage_Monsters3();      // Stage 32: Guard towers, structures
    void LoadStage_Monsters4();      // Stage 36: More monsters
    void LoadStage_Monsters5();      // Stage 40: More monsters
    void LoadStage_Monsters6();      // Stage 44: NPCs, new monsters
    void LoadStage_CosmeticsMale();   // Stage 48: gail, gate, male underwear, hair
    void LoadStage_CosmeticsFemale(); // Stage 52: female underwear, hair
    void LoadStage_EquipmentBatch();  // Stages 56-96: per-item equipment from metadata
    void LoadStage_Effects();        // Stage 100: Effects, sounds, finish

    // Sprite loading helpers
    void make_sprite(const char* FileName, short start, short count, bool alpha_effect);
    void make_tile_spr(const char* FileName, short start, short count, bool alpha_effect);
    void make_effect_spr(const char* FileName, short start, short count, bool alpha_effect);

    int m_iLoadingStage = 0;
    int m_equip_load_cursor = 0;
};
