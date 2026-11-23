// shared_item.h: Master include file for the unified Item system
//
// Include this single header to get access to all Item-related types:
//   - item_enums.h       - Type-safe enums (EquipPos, ItemType, etc.)
//   - item_attributes.h  - Attribute parsing and formatting
//   - item.h            - Unified CItem class
//
// Note: Display names are now stored directly in the database.
// The server sends display names in m_name, no client-side lookup needed.
//
//////////////////////////////////////////////////////////////////////

#pragma once

// Item enums and constants
#include "item_enums.h"

// Attribute parsing helpers
#include "item_attributes.h"

// Unified CItem class
#include "item.h"
