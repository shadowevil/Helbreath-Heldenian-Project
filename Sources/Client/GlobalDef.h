#pragma once

/****************************************************************
*		 This client was writen by Diuuude & Snoopy81.			*
*					Based on Cleroth work.						*
*																*
*			V3.51 compatibility by Cleroth						*
*			V3.51 dialogs by Diuuude							*
*			Effects, mobs, Apocalypse, Heldenian				*
*			& finalizing by Snoopy81							*
*			V3.82 Crafting & Angels by Snoopy81					*
*****************************************************************/

/****************************************************************
*	Find here all compilation options							*
*	I removed useless ones and tryed to add some explanations	*
*	( Snoopy81 06/2005 )										*
*****************************************************************/

/*** Put here global data for your server ***/
#include "version_info.h"

constexpr const char* NAME_WORLDNAME1 = "WS1";

#ifndef _DEBUG
	#define DEF_TEST_SERVER // Comment this out to use local IP instead of public test server
#endif

#ifdef DEF_TEST_SERVER
constexpr const char* DEF_SERVER_IP = "199.187.160.239";
#else
constexpr const char* DEF_SERVER_IP = "127.0.0.1";
#endif
constexpr const int DEF_SERVER_PORT = 2500;
constexpr const int DEF_GSERVER_PORT = 9907;

// Resolution-dependent values are now provided by hb::shared::render::ResolutionConfig singleton
// Include ResolutionConfig.h and use hb::shared::render::ResolutionConfig::get().MethodName()
//
// For backward compatibility, these inline functions provide the same interface
// as the old macros but now return dynamic values based on settings.json

// Define guard to prevent RenderConstants.h from redefining these functions
#define GLOBALDEF_H_RESOLUTION_FUNCTIONS

#include "ResolutionConfig.h"

inline int LOGICAL_WIDTH()      { return hb::shared::render::ResolutionConfig::get().logical_width(); }
inline int LOGICAL_HEIGHT()     { return hb::shared::render::ResolutionConfig::get().logical_height(); }
inline int BASE_SCREEN_WIDTH()  { return hb::shared::render::ResolutionConfig::get().logical_width(); }
inline int BASE_SCREEN_HEIGHT() { return hb::shared::render::ResolutionConfig::get().logical_height(); }
inline int LOGICAL_MAX_X()      { return hb::shared::render::ResolutionConfig::get().logical_max_x(); }
inline int LOGICAL_MAX_Y()      { return hb::shared::render::ResolutionConfig::get().logical_max_y(); }
inline int VIEW_TILE_WIDTH()    { return hb::shared::render::ResolutionConfig::get().view_tile_width(); }
inline int VIEW_TILE_HEIGHT()   { return hb::shared::render::ResolutionConfig::get().view_tile_height(); }
inline int VIEW_CENTER_TILE_X() { return hb::shared::render::ResolutionConfig::get().view_center_tile_x(); }
inline int VIEW_CENTER_TILE_Y() { return hb::shared::render::ResolutionConfig::get().view_center_tile_y(); }
inline int ICON_PANEL_WIDTH()   { return hb::shared::render::ResolutionConfig::get().icon_panel_width(); }
inline int ICON_PANEL_HEIGHT()  { return hb::shared::render::ResolutionConfig::get().icon_panel_height(); }
inline int ICON_PANEL_OFFSET_X(){ return hb::shared::render::ResolutionConfig::get().icon_panel_offset_x(); }

inline int CHAT_INPUT_X()       { return hb::shared::render::ResolutionConfig::get().chat_input_x(); }
inline int CHAT_INPUT_Y()       { return hb::shared::render::ResolutionConfig::get().chat_input_y(); }
inline int EVENTLIST2_BASE_Y()  { return hb::shared::render::ResolutionConfig::get().event_list2_base_y(); }
inline int LEVELUP_TEXT_X()     { return hb::shared::render::ResolutionConfig::get().level_up_text_x(); }
inline int LEVELUP_TEXT_Y()     { return hb::shared::render::ResolutionConfig::get().level_up_text_y(); }


/*** Some more compilation options ***/
#define DEF_COMMA_GOLD			// Allows to show comma nbe as original HB (ie: 1,200,000)

#define DEF_WINDOWED_MODE		// Shows HB in a windowed mode (for debug purpose only, sprite will bug....)
