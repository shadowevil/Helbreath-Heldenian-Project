// render_constants.h: Shared rendering constants between client and renderer
//
// This header contains constants needed by the renderer that were originally
// defined in global_def.h. Moving them here allows the renderer library to
// be independent of client-specific definitions.
//
// Resolution-dependent values are now provided by hb::shared::render::ResolutionConfig singleton.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "resolution_config.h"

// Logical rendering resolution - now dynamic based on settings
// Use these inline functions instead of the old macros
inline int RENDER_LOGICAL_WIDTH()  { return hb::shared::render::ResolutionConfig::get().logical_width(); }
inline int RENDER_LOGICAL_HEIGHT() { return hb::shared::render::ResolutionConfig::get().logical_height(); }

// These are also defined in global_def.h for client code
#ifndef GLOBALDEF_H_RESOLUTION_FUNCTIONS
inline int LOGICAL_WIDTH()  { return hb::shared::render::ResolutionConfig::get().logical_width(); }
inline int LOGICAL_HEIGHT() { return hb::shared::render::ResolutionConfig::get().logical_height(); }
inline int LOGICAL_MAX_X()  { return hb::shared::render::ResolutionConfig::get().logical_max_x(); }
inline int LOGICAL_MAX_Y()  { return hb::shared::render::ResolutionConfig::get().logical_max_y(); }
#endif


