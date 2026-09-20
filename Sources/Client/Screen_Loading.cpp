// Screen_Loading.cpp: Loading Screen Implementation
//
// Handles progressive loading of all game resources including sprites,
// tiles, effects, and sounds. Loading is split across multiple frames
// to allow progress updates.
//
//////////////////////////////////////////////////////////////////////

#include "Screen_Loading.h"
#include "Game.h"
#include "GameModeManager.h"
#include "GameFonts.h"
#include "SpriteID.h"
#include "GlobalDef.h"
#include "AudioManager.h"
#include "WeatherManager.h"
#include "SpriteLoader.h"
#include "ItemSpriteMetadata.h"
#include "Log.h"
using namespace hb::client::sprite_id;

Screen_Loading::Screen_Loading(CGame* game)
    : IGameScreen(game)
{
}

void Screen_Loading::on_initialize()
{
    m_iLoadingStage = 0;

    // Pre-load the loading screen sprite so it can render immediately
    hb::shared::sprite::SpriteLoader::open_pak("interface/intermediate_screens", [&](hb::shared::sprite::SpriteLoader& loader) {
        m_game->m_sprite[InterfaceNdLoading] = loader.get_sprite(0, false);
    });
}
void Screen_Loading::on_update()
{
    // Process one loading stage per frame
    switch (m_iLoadingStage)
    {
    case 0:   LoadStage_Interface();    break;
    case 4:   LoadStage_Tiles1();       break;
    case 8:   LoadStage_Tiles2();       break;
    case 12:  LoadStage_Tiles3();       break;
    case 16:  LoadStage_Equipment1();   break;
    case 20:  LoadStage_Angels();       break;
    case 24:  LoadStage_Monsters1();    break;
    case 28:  LoadStage_Monsters2();    break;
    case 32:  LoadStage_Monsters3();    break;
    case 36:  LoadStage_Monsters4();    break;
    case 40:  LoadStage_Monsters5();    break;
    case 44:  LoadStage_Monsters6();    break;
    case 48:  LoadStage_CosmeticsMale();   break;
    case 52:  LoadStage_CosmeticsFemale(); break;
    case 56: case 60: case 64: case 68: case 72: case 76:
    case 80: case 84: case 88: case 92: case 96:
              LoadStage_EquipmentBatch();  break;
    case 100: LoadStage_Effects();      break;
    }
}

void Screen_Loading::on_render()
{
    // draw loading background
    draw_new_dialog_box(InterfaceNdLoading, 0, 0, 0, true);
    draw_version();

    // draw progress bar - scale loading stage (0-100) to actual sprite frame width
    int frame_width = m_game->m_sprite[InterfaceNdLoading]->GetFrameRect(1).width;
    int bar_width = (m_iLoadingStage * frame_width) / 100;
    m_game->m_sprite[InterfaceNdLoading]->DrawWidth(626, 552, 1, bar_width);
}

//=============================================================================
// Stage 0: Interface sprites, dialog boxes, maps
//=============================================================================
void Screen_Loading::LoadStage_Interface()
{
    hb::shared::sprite::SpriteLoader::open_pak("interface/newmaps", [&](hb::shared::sprite::SpriteLoader& loader) {
        m_game->m_sprite[InterfaceNewMaps1] = loader.get_sprite(0, false);
        m_game->m_sprite[InterfaceNewMaps2] = loader.get_sprite(1, false);
        m_game->m_sprite[InterfaceNewMaps3] = loader.get_sprite(2, false);
        m_game->m_sprite[InterfaceNewMaps4] = loader.get_sprite(3, false);
        m_game->m_sprite[InterfaceNewMaps5] = loader.get_sprite(4, false);
    });

    hb::shared::sprite::SpriteLoader::open_pak("interface/intermediate_screens", [&](hb::shared::sprite::SpriteLoader& loader) {
        m_game->m_sprite[InterfaceNdMainMenu] = loader.get_sprite(1, false);
        m_game->m_sprite[InterfaceNdQuit] = loader.get_sprite(2, false);
        m_game->m_sprite[InterfaceNdNewAccount] = loader.get_sprite(2, false);
        m_game->m_sprite[InterfaceNdLogin] = loader.get_sprite(4, false);
        m_game->m_sprite[InterfaceNdSelectChar] = loader.get_sprite(5, false);
        m_game->m_sprite[InterfaceNdNewChar] = loader.get_sprite(6, false);
    });

    hb::shared::sprite::SpriteLoader::open_pak("interface/game_dialogs", [&](hb::shared::sprite::SpriteLoader& loader) {
        m_game->m_sprite[InterfaceNdGame1] = loader.get_sprite(0, false);
        m_game->m_sprite[InterfaceNdGame2] = loader.get_sprite(1, false);
        m_game->m_sprite[InterfaceNdGame3] = loader.get_sprite(2, false);
        m_game->m_sprite[InterfaceNdGame4] = loader.get_sprite(3, false);
        m_game->m_sprite[InterfaceNdCrusade] = loader.get_sprite(4, false);
        m_game->m_sprite[InterfaceNdIconPanel] = loader.get_sprite(5, false);
        m_game->m_sprite[InterfaceNdInventory] = loader.get_sprite(6, false);
        m_game->m_sprite[InterfaceNdNewExchange] = loader.get_sprite(7, false);
        m_game->m_sprite[MouseCursor] = loader.get_sprite(8, false);
        m_game->m_sprite[InterfaceNdText] = loader.get_sprite(9, false);
        m_game->m_sprite[InterfaceNdButton] = loader.get_sprite(10, false);
    });

    make_sprite("interface/telescope", InterfaceGuideMap, 32, false);
    make_sprite("interface/telescope2", InterfaceGuideMap + 35, 4, false);
    make_sprite("interface/monster", InterfaceMonster, 1, false);

    // load sprite_fonts sprites (font1, font2, sprfonts + number font, sprfonts2)
    hb::shared::sprite::SpriteLoader::open_pak("interface/sprite_fonts", [&](hb::shared::sprite::SpriteLoader& loader) {
        m_game->m_sprite[InterfaceFont1] = loader.get_sprite(0, false);
        m_game->m_sprite[InterfaceFont2] = loader.get_sprite(1, false);
        m_game->m_sprite[InterfaceSprFonts] = loader.get_sprite(2, false);
        m_game->m_sprite[InterfaceSprFonts3] = loader.get_sprite(3, false);
    });

    // Create and register bitmap fonts with TextLib
    // Font 1: Characters '!' (33) to 'z' (122)
    if (m_game->m_sprite[InterfaceFont1])
    {
        hb::shared::text::load_bitmap_font(GameFont::Bitmap1, m_game->m_sprite[InterfaceFont1].get(),
            '!', 'z', 0, GameFont::GetFontSpacing(GameFont::Bitmap1));
    }

    // Font 2: Characters ' ' (32) to '~' (126), uses dynamic spacing from sprite frames
    if (m_game->m_sprite[InterfaceFont2])
    {
        hb::shared::text::load_bitmap_font_dynamic(GameFont::Bitmap2, m_game->m_sprite[InterfaceFont2].get(), ' ', '~', 0);
    }

    // Number font: Digits '0' to '9', frame offset 30 in InterfaceSprFonts
    if (m_game->m_sprite[InterfaceSprFonts])
    {
        hb::shared::text::load_bitmap_font(GameFont::Numbers, m_game->m_sprite[InterfaceSprFonts].get(),
            '0', '9', 30, GameFont::GetFontSpacing(GameFont::Numbers));
    }

    // SPRFONTS3: Characters ' ' (32) to '~' (126), with 3 different sizes (types 0, 1, 2)
    // Each type has 95 frames offset
    if (m_game->m_sprite[InterfaceSprFonts3])
    {
        hb::shared::text::load_bitmap_font_dynamic(GameFont::SprFont3_0, m_game->m_sprite[InterfaceSprFonts3].get(), ' ', '~', 0);
        hb::shared::text::load_bitmap_font_dynamic(GameFont::SprFont3_1, m_game->m_sprite[InterfaceSprFonts3].get(), ' ', '~', 95);
        hb::shared::text::load_bitmap_font_dynamic(GameFont::SprFont3_2, m_game->m_sprite[InterfaceSprFonts3].get(), ' ', '~', 190);
    }

    m_iLoadingStage = 4;
}

//=============================================================================
// Stage 4: Map tiles, structures, trees
//=============================================================================
void Screen_Loading::LoadStage_Tiles1()
{
    make_tile_spr("tiles/maptiles1", 0, 32, true);
    hb::shared::sprite::SpriteLoader::open_pak("objects/structures1", [&](hb::shared::sprite::SpriteLoader& loader) {
        m_game->m_tile_spr[1 + 50] = loader.get_sprite(1, true);
        m_game->m_tile_spr[5 + 50] = loader.get_sprite(5, true);
    });
    make_tile_spr("tiles/sinside1", 70, 27, false);
    make_tile_spr("objects/trees1", 100, 46, true);
    make_tile_spr("objects/treeshadows", 150, 46, true);
    make_tile_spr("objects/objects1", 200, 10, true);
    make_tile_spr("objects/objects2", 211, 5, true);
    make_tile_spr("objects/objects3", 216, 4, true);
    make_tile_spr("objects/objects4", 220, 2, true);

    m_iLoadingStage = 8;
}

//=============================================================================
// Stage 8: More tiles and objects
//=============================================================================
void Screen_Loading::LoadStage_Tiles2()
{
    make_tile_spr("tiles/tile223-225", 223, 3, true);
    make_tile_spr("tiles/tile226-229", 226, 4, true);
    make_tile_spr("objects/objects5", 230, 9, true);
    make_tile_spr("objects/objects6", 238, 4, true);
    make_tile_spr("objects/objects7", 242, 7, true);
    make_tile_spr("tiles/maptiles2", 300, 15, true);
    make_tile_spr("tiles/maptiles4", 320, 10, true);
    make_tile_spr("tiles/maptiles5", 330, 19, true);
    make_tile_spr("tiles/maptiles6", 349, 4, true);
    make_tile_spr("tiles/maptiles353-361", 353, 9, true);
    make_tile_spr("tiles/tile363-366", 363, 4, true);
    make_tile_spr("tiles/tile367-367", 367, 1, true);
    make_tile_spr("tiles/tile370-381", 370, 12, true);
    make_tile_spr("tiles/tile382-387", 382, 6, true);
    make_tile_spr("tiles/tile388-402", 388, 15, true);

    m_iLoadingStage = 12;
}

//=============================================================================
// Stage 12: More tiles, item sprites
//=============================================================================
void Screen_Loading::LoadStage_Tiles3()
{
    make_tile_spr("tiles/tile403-405", 403, 3, true);
    make_tile_spr("tiles/tile406-421", 406, 16, true);
    make_tile_spr("tiles/htile406-421", HolidayTileOffset + 406, 16, true);  // Christmas variant
    make_tile_spr("tiles/tile422-429", 422, 8, true);
    make_tile_spr("tiles/tile430-443", 430, 14, true);
    make_tile_spr("tiles/tile444-444", 444, 1, true);
    make_tile_spr("tiles/tile445-461", 445, 17, true);
    make_tile_spr("tiles/tile462-473", 462, 12, true);
    make_tile_spr("tiles/tile474-478", 474, 5, true);
    make_tile_spr("tiles/tile479-488", 479, 10, true);
    make_tile_spr("tiles/tile489-522", 489, 34, true);
    make_tile_spr("tiles/tile523-530", 523, 8, true);
    make_tile_spr("tiles/tile531-540", 531, 10, true);
    make_tile_spr("tiles/tile541-545", 541, 5, true);

    make_sprite("objects/item-dynamic", ItemDynamicPivotPoint, 3, false);

    // Item atlas sprites (unified atlas for new display_id system)
    hb::shared::sprite::SpriteLoader::open_pak("items/item_atlas", [&](hb::shared::sprite::SpriteLoader& loader) {
        for (size_t i = 0; i < 3 && i < loader.get_sprite_count(); i++) {
            m_game->m_item_sprites[i] = loader.get_sprite(i, false);
        }
        hb::logger::log("Item atlas loaded: {} sprites", loader.get_sprite_count());
    });

    // Item sprite metadata (display_id → frame index mappings)
    item_sprite_manager::get().load("contents/ItemSpriteMetadata.json");

    m_iLoadingStage = 16;
}

//=============================================================================
// Stage 16: Male/Female equipment base, player bodies
//=============================================================================
void Screen_Loading::LoadStage_Equipment1()
{
    // Equipment models (body, hair, underwear) — male 0-2, female 3-5
    hb::shared::sprite::SpriteLoader::open_pak("interface/equip_models", [&](hb::shared::sprite::SpriteLoader& loader) {
        m_game->m_sprite[ItemEquipPivotPoint + 0] = loader.get_sprite(0, false);
        m_game->m_sprite[ItemEquipPivotPoint + 18] = loader.get_sprite(1, false);
        m_game->m_sprite[ItemEquipPivotPoint + 19] = loader.get_sprite(2, false);
        m_game->m_sprite[ItemEquipPivotPoint + 40] = loader.get_sprite(3, false);
        m_game->m_sprite[ItemEquipPivotPoint + 58] = loader.get_sprite(4, false);
        m_game->m_sprite[ItemEquipPivotPoint + 59] = loader.get_sprite(5, false);
    });

    // Player body sprites
    make_sprite("players/black-man", 500 + 15 * 8 * 0, 96, true);
    make_sprite("players/white-man", 500 + 15 * 8 * 1, 96, true);
    make_sprite("players/yellow-man", 500 + 15 * 8 * 2, 96, true);

    m_iLoadingStage = 20;
}

//=============================================================================
// Stage 20: Tutelary angels, female player bodies
//=============================================================================
void Screen_Loading::LoadStage_Angels()
{
    make_sprite("pets/tutelary-angel1", TutelaryAngelsPivotPoint + 50 * 0, 48, false);
    make_sprite("pets/tutelary-angel2", TutelaryAngelsPivotPoint + 50 * 1, 48, false);
    make_sprite("pets/tutelary-angel3", TutelaryAngelsPivotPoint + 50 * 2, 48, false);
    make_sprite("pets/tutelary-angel4", TutelaryAngelsPivotPoint + 50 * 3, 48, false);

    make_sprite("players/black-woman", 500 + 15 * 8 * 3, 96, true);
    make_sprite("players/white-woman", 500 + 15 * 8 * 4, 96, true);
    make_sprite("players/yellow-woman", 500 + 15 * 8 * 5, 96, true);

    m_iLoadingStage = 24;
}

//=============================================================================
// Stage 24: Monsters (Slime to William)
//=============================================================================
void Screen_Loading::LoadStage_Monsters1()
{
    make_sprite("npcs/slime", Mob + 7 * 8 * 0, 40, true);
    make_sprite("npcs/skeleton", Mob + 7 * 8 * 1, 40, true);
    make_sprite("npcs/stone-golem", Mob + 7 * 8 * 2, 40, true);
    make_sprite("npcs/cyclops", Mob + 7 * 8 * 3, 40, true);
    make_sprite("npcs/orc-mage", Mob + 7 * 8 * 4, 40, true);
    make_sprite("npcs/shopkeeper", Mob + 7 * 8 * 5, 8, true);
    make_sprite("npcs/giant-ant", Mob + 7 * 8 * 6, 40, true);
    make_sprite("npcs/scorpion", Mob + 7 * 8 * 7, 40, true);
    make_sprite("npcs/zombie", Mob + 7 * 8 * 8, 40, true);
    make_sprite("npcs/gandalf", Mob + 7 * 8 * 9, 8, true);
    make_sprite("npcs/howard", Mob + 7 * 8 * 10, 8, true);
    make_sprite("npcs/guard", Mob + 7 * 8 * 11, 40, true);
    make_sprite("npcs/amphis", Mob + 7 * 8 * 12, 40, true);
    make_sprite("npcs/clay-golem", Mob + 7 * 8 * 13, 40, true);
    make_sprite("npcs/tom", Mob + 7 * 8 * 14, 8, true);
    make_sprite("npcs/william", Mob + 7 * 8 * 15, 8, true);

    m_iLoadingStage = 28;
}

//=============================================================================
// Stage 28: Monsters (Kennedy to Energy Ball)
//=============================================================================
void Screen_Loading::LoadStage_Monsters2()
{
    make_sprite("npcs/kennedy", Mob + 7 * 8 * 16, 8, true);
    make_sprite("npcs/hellhound", Mob + 7 * 8 * 17, 40, true);
    make_sprite("npcs/troll", Mob + 7 * 8 * 18, 40, true);
    make_sprite("npcs/ogre", Mob + 7 * 8 * 19, 40, true);
    make_sprite("npcs/liche", Mob + 7 * 8 * 20, 40, true);
    make_sprite("npcs/demon", Mob + 7 * 8 * 21, 40, true);
    make_sprite("npcs/unicorn", Mob + 7 * 8 * 22, 40, true);
    make_sprite("npcs/werewolf", Mob + 7 * 8 * 23, 40, true);
    make_sprite("npcs/dummy", Mob + 7 * 8 * 24, 40, true);

    // Energy Ball - all 40 slots use the same sprite
    hb::shared::sprite::SpriteLoader::open_pak("effects/effect5", [&](hb::shared::sprite::SpriteLoader& loader) {
        for (int i = 0; i < 40; i++)
            m_game->m_sprite[Mob + i + 7 * 8 * 25] = loader.get_sprite(0, true);
    });

    m_iLoadingStage = 32;
}

//=============================================================================
// Stage 32: Guard towers, structures
//=============================================================================
void Screen_Loading::LoadStage_Monsters3()
{
    make_sprite("npcs/arrow-guard-tower", Mob + 7 * 8 * 26, 40, true);
    make_sprite("npcs/cannon-guard-tower", Mob + 7 * 8 * 27, 40, true);
    make_sprite("npcs/mana-collector", Mob + 7 * 8 * 28, 40, true);
    make_sprite("npcs/detector", Mob + 7 * 8 * 29, 40, true);
    make_sprite("npcs/energy-shield", Mob + 7 * 8 * 30, 40, true);
    make_sprite("npcs/grand-magic-generator", Mob + 7 * 8 * 31, 40, true);
    make_sprite("npcs/mana-stone", Mob + 7 * 8 * 32, 40, true);
    make_sprite("npcs/light-war-beetle", Mob + 7 * 8 * 33, 40, true);
    make_sprite("npcs/gods-hand-knight", Mob + 7 * 8 * 34, 40, true);
    make_sprite("npcs/gods-hand-knight-ck", Mob + 7 * 8 * 35, 40, true);
    make_sprite("npcs/temple-knight", Mob + 7 * 8 * 36, 40, true);
    make_sprite("npcs/battle-golem", Mob + 7 * 8 * 37, 40, true);

    m_iLoadingStage = 36;
}

//=============================================================================
// Stage 36: More monsters
//=============================================================================
void Screen_Loading::LoadStage_Monsters4()
{
    make_sprite("npcs/stalker", Mob + 7 * 8 * 38, 40, true);
    make_sprite("npcs/hell-claw", Mob + 7 * 8 * 39, 40, true);
    make_sprite("npcs/tiger-worm", Mob + 7 * 8 * 40, 40, true);
    make_sprite("npcs/catapult", Mob + 7 * 8 * 41, 40, true);
    make_sprite("npcs/gargoyle", Mob + 7 * 8 * 42, 40, true);
    make_sprite("npcs/beholder", Mob + 7 * 8 * 43, 40, true);
    make_sprite("npcs/dark-elf", Mob + 7 * 8 * 44, 40, true);
    make_sprite("npcs/bunny", Mob + 7 * 8 * 45, 40, true);
    make_sprite("npcs/cat", Mob + 7 * 8 * 46, 40, true);
    make_sprite("npcs/giant-frog", Mob + 7 * 8 * 47, 40, true);
    make_sprite("npcs/mountain-giant", Mob + 7 * 8 * 48, 40, true);

    m_iLoadingStage = 40;
}

//=============================================================================
// Stage 40: More monsters
//=============================================================================
void Screen_Loading::LoadStage_Monsters5()
{
    make_sprite("npcs/ettin", Mob + 7 * 8 * 49, 40, true);
    make_sprite("npcs/cannibal-plant", Mob + 7 * 8 * 50, 40, true);
    make_sprite("npcs/rudolph", Mob + 7 * 8 * 51, 40, true);
    make_sprite("npcs/dire-boar", Mob + 7 * 8 * 52, 40, true);
    make_sprite("npcs/frost", Mob + 7 * 8 * 53, 40, true);
    make_sprite("npcs/crops", Mob + 7 * 8 * 54, 40, true);
    make_sprite("npcs/ice-golem", Mob + 7 * 8 * 55, 40, true);
    make_sprite("npcs/wyvern", Mob + 7 * 8 * 56, 24, true);
    make_sprite("npcs/mcgaffin", Mob + 7 * 8 * 57, 16, true);
    make_sprite("npcs/perry", Mob + 7 * 8 * 58, 16, true);
    make_sprite("npcs/devlin", Mob + 7 * 8 * 59, 16, true);
    make_sprite("npcs/barlog", Mob + 7 * 8 * 60, 40, true);
    make_sprite("npcs/centaur", Mob + 7 * 8 * 61, 40, true);
    make_sprite("npcs/claw-turtle", Mob + 7 * 8 * 62, 40, true);
    make_sprite("npcs/fire-wyvern", Mob + 7 * 8 * 63, 24, true);
    make_sprite("npcs/giant-crayfish", Mob + 7 * 8 * 64, 40, true);
    make_sprite("npcs/giant-lizard", Mob + 7 * 8 * 65, 40, true);

    m_iLoadingStage = 44;
}

//=============================================================================
// Stage 44: New NPCs and monsters
//=============================================================================
void Screen_Loading::LoadStage_Monsters6()
{
    make_sprite("npcs/giant-plant", Mob + 7 * 8 * 66, 40, true);
    make_sprite("npcs/master-mage-orc", Mob + 7 * 8 * 67, 40, true);
    make_sprite("npcs/minotaur", Mob + 7 * 8 * 68, 40, true);
    make_sprite("npcs/nizie", Mob + 7 * 8 * 69, 40, true);
    make_sprite("npcs/tentacle", Mob + 7 * 8 * 70, 40, true);
    make_sprite("npcs/abaddon", Mob + 7 * 8 * 71, 32, true);
    make_sprite("npcs/sorceress", Mob + 7 * 8 * 72, 40, true);
    make_sprite("npcs/temple-knight-atk", Mob + 7 * 8 * 73, 40, true);
    make_sprite("npcs/master-elf", Mob + 7 * 8 * 74, 40, true);
    make_sprite("npcs/dark-knight", Mob + 7 * 8 * 75, 40, true);
    make_sprite("npcs/helbreath-tank", Mob + 7 * 8 * 76, 32, true);
    make_sprite("npcs/crusade-barricade-turret", Mob + 7 * 8 * 77, 32, true);
    make_sprite("npcs/barbarian", Mob + 7 * 8 * 78, 40, true);
    make_sprite("npcs/apocalypse-cannon", Mob + 7 * 8 * 79, 32, true);

    m_iLoadingStage = 48;
}

//=============================================================================
// Stage 48: Gail, Gate, Male cosmetics (underwear, hair)
//=============================================================================
void Screen_Loading::LoadStage_CosmeticsMale()
{
    make_sprite("npcs/gail", Mob + 7 * 8 * 80, 8, true);
    make_sprite("npcs/gate", Mob + 7 * 8 * 81, 24, true);

    // Male underwear
    hb::shared::sprite::SpriteLoader::open_pak("players/male-underwear", [&](hb::shared::sprite::SpriteLoader& loader) {
        for (int g = 0; g < 8; g++) {
            for (int i = 0; i < 12; i++) {
                m_game->m_sprite[UndiesM + i + 15 * g] = loader.get_sprite(i + 12 * g, true);
            }
        }
    });

    // Male hair
    hb::shared::sprite::SpriteLoader::open_pak("players/male-hair", [&](hb::shared::sprite::SpriteLoader& loader) {
        for (int g = 0; g < 8; g++) {
            for (int i = 0; i < 12; i++) {
                m_game->m_sprite[HairM + i + 15 * g] = loader.get_sprite(i + 12 * g, true);
            }
        }
    });

    m_iLoadingStage = 52;
}

//=============================================================================
// Stage 52: Female cosmetics (underwear, hair)
//=============================================================================
void Screen_Loading::LoadStage_CosmeticsFemale()
{
    // Female underwear
    hb::shared::sprite::SpriteLoader::open_pak("players/female-underwear", [&](hb::shared::sprite::SpriteLoader& loader) {
        for (int g = 0; g < 8; g++) {
            for (int i = 0; i < 12; i++) {
                m_game->m_sprite[UndiesW + i + 15 * g] = loader.get_sprite(i + 12 * g, true);
            }
        }
    });

    // Female hair
    hb::shared::sprite::SpriteLoader::open_pak("players/female-hair", [&](hb::shared::sprite::SpriteLoader& loader) {
        for (int g = 0; g < 8; g++) {
            for (int i = 0; i < 12; i++) {
                m_game->m_sprite[HairW + i + 15 * g] = loader.get_sprite(i + 12 * g, true);
            }
        }
    });

    m_iLoadingStage = 56;
}

//=============================================================================
// Stages 56-96: Per-item equipment sprites from metadata (batched)
//=============================================================================
void Screen_Loading::LoadStage_EquipmentBatch()
{
    constexpr int equip_stages = 11; // stages 56, 60, 64, 68, 72, 76, 80, 84, 88, 92, 96

    // Count total loadable entries
    int total = 0;
    item_sprite_manager::get().for_each_equippable([&](const item_sprite_entry&) {
        total++;
    });

    int per_batch = (total + equip_stages - 1) / equip_stages;
    int batch_end = m_equip_load_cursor + per_batch;

    int current = 0;
    item_sprite_manager::get().for_each_equippable([&](const item_sprite_entry& entry) {
        if (current < m_equip_load_cursor || current >= batch_end) { current++; return; }
        current++;

        if (entry.pak_file.empty()) return;

        // Strip .pak extension and prepend items/ subdirectory
        std::string pak_name = entry.pak_file;
        if (pak_name.size() > 4 && pak_name.compare(pak_name.size() - 4, 4, ".pak") == 0)
            pak_name.resize(pak_name.size() - 4);
        std::string pak_path = "items/" + pak_name;

        try {
            hb::shared::sprite::SpriteLoader::open_pak(pak_path.c_str(), [&](hb::shared::sprite::SpriteLoader& loader) {
                size_t sprite_count = loader.get_sprite_count();
                // Male sprites: pak indices 0-11
                for (int pose = 0; pose < equip_sprite::sprites_per_item; pose++) {
                    int slot = equip_sprite::index(false, entry.id, pose);
                    if (slot >= 0 && static_cast<size_t>(pose) < sprite_count)
                        m_game->m_equip_sprites[slot] = loader.get_sprite(pose, true);
                }
                // Female sprites: pak indices 12-23
                for (int pose = 0; pose < equip_sprite::sprites_per_item; pose++) {
                    int pak_idx = equip_sprite::sprites_per_item + pose;
                    int slot = equip_sprite::index(true, entry.id, pose);
                    if (slot >= 0 && static_cast<size_t>(pak_idx) < sprite_count)
                        m_game->m_equip_sprites[slot] = loader.get_sprite(pak_idx, true);
                }
            });
        } catch (const std::exception& ex) {
            hb::logger::warn("Failed to load equipment pak '{}': {}", pak_path, ex.what());
        }
    });

    m_equip_load_cursor = batch_end;

    if (m_iLoadingStage == 96) {
        hb::logger::log("Equipment sprites: loaded {} equippable items into m_equip_sprites", total);
    }

    m_iLoadingStage += 4;
}

//=============================================================================
// Stage 100: Effects, sounds, finish loading
//=============================================================================
void Screen_Loading::LoadStage_Effects()
{
    make_effect_spr("effects/effect", 0, 10, false);
    make_effect_spr("effects/effect2", 10, 3, false);
    make_effect_spr("effects/effect3", 13, 6, false);
    make_effect_spr("effects/effect4", 19, 5, false);

    // Effect5 batch load
    hb::shared::sprite::SpriteLoader::open_pak("effects/effect5", [&](hb::shared::sprite::SpriteLoader& loader) {
        for (int i = 0; i <= 6; i++) {
            m_game->m_effect_sprites[i + 24] = loader.get_sprite(i + 1, false);
        }
    });

    make_effect_spr("effects/crusade-effect", 31, 9, false);
    make_effect_spr("effects/effect6", 40, 5, false);
    make_effect_spr("effects/effect7", 45, 12, false);
    make_effect_spr("effects/effect8", 57, 9, false);
    make_effect_spr("effects/effect9", 66, 21, false);
    make_effect_spr("effects/effect10", 87, 2, false);
    make_effect_spr("effects/effect11", 89, 14, false);
    make_effect_spr("effects/effect11-shadow", 104, 1, false);
    make_effect_spr("effects/abaddon-effect2", 140, 8, false);
    make_effect_spr("effects/effect12", 148, 4, false);
    make_effect_spr("effects/abaddon-effect3", 152, 16, false);
    make_effect_spr("effects/abaddon-effect4", 133, 7, false);

    // initialize effect_manager with loaded sprites
    m_game->m_effect_manager->set_effect_sprites(m_game->m_effect_sprites);
    weather_manager::get().set_dependencies(*m_game->m_Renderer, m_game->m_effect_sprites, m_game->m_Camera);
    weather_manager::get().set_map_data(m_game->m_map_data.get());

    // load all sound effects
    audio_manager::get().load_sounds();

    // Loading complete - transition to main menu
    m_game->change_game_mode(GameMode::MainMenu);
}

//=============================================================================
// Sprite loading helpers
//=============================================================================
void Screen_Loading::make_sprite(const char* FileName, short start, short count, bool alpha_effect)
{
    try {
        hb::shared::sprite::SpriteLoader::open_pak(FileName, [&](hb::shared::sprite::SpriteLoader& loader) {
            size_t totalInPak = loader.get_sprite_count();
            size_t toLoad = static_cast<size_t>(count);
            if (toLoad > totalInPak) toLoad = totalInPak;

            for (size_t i = 0; i < toLoad; i++) {
                m_game->m_sprite[i + start] = loader.get_sprite(i, alpha_effect);
            }
        });
    } catch (const std::exception& e) {
        printf("[make_sprite] FAILED %s: %s\n", FileName, e.what());
    } catch (...) {
        printf("[make_sprite] FAILED %s: unknown exception\n", FileName);
    }
}

void Screen_Loading::make_tile_spr(const char* FileName, short start, short count, bool alpha_effect)
{
    try {
        hb::shared::sprite::SpriteLoader::open_pak(FileName, [&](hb::shared::sprite::SpriteLoader& loader) {
            size_t totalInPak = loader.get_sprite_count();
            size_t toLoad = static_cast<size_t>(count);
            if (toLoad > totalInPak) toLoad = totalInPak;

            for (size_t i = 0; i < toLoad; i++) {
                m_game->m_tile_spr[i + start] = loader.get_sprite(i, alpha_effect);
            }
        });
    } catch (const std::exception& e) {
        printf("[make_tile_spr] FAILED %s: %s\n", FileName, e.what());
    }
}

void Screen_Loading::make_effect_spr(const char* FileName, short start, short count, bool alpha_effect)
{
    try {
        hb::shared::sprite::SpriteLoader::open_pak(FileName, [&](hb::shared::sprite::SpriteLoader& loader) {
            size_t totalInPak = loader.get_sprite_count();
            size_t toLoad = static_cast<size_t>(count);
            if (toLoad > totalInPak) toLoad = totalInPak;

            for (size_t i = 0; i < toLoad; i++) {
                m_game->m_effect_sprites[i + start] = loader.get_sprite(i, alpha_effect);
            }
        });
    } catch (const std::exception& e) {
        printf("[make_effect_spr] FAILED %s: %s\n", FileName, e.what());
    }
}
