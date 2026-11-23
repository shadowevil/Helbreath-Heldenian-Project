#include "resolution_config.h"

namespace hb::shared::render {

bool ResolutionConfig::s_initialized = false;

void ResolutionConfig::initialize(int windowWidth, int windowHeight)
{
	get().m_width = BASE_RESOLUTION_WIDTH;
	get().m_height = BASE_RESOLUTION_HEIGHT;

	// Store window size and calculate screen offset
	get().m_windowWidth = windowWidth;
	get().m_windowHeight = windowHeight;
	get().recalculate_screen_offset();

	s_initialized = true;
}

ResolutionConfig& ResolutionConfig::get()
{
	static ResolutionConfig instance;
	return instance;
}

void ResolutionConfig::set_window_size(int windowWidth, int windowHeight)
{
	m_windowWidth = windowWidth;
	m_windowHeight = windowHeight;
	recalculate_screen_offset();
}


void ResolutionConfig::recalculate_screen_offset()
{
	// Center the logical resolution within the window
	// This is used when window size differs from logical size
	m_screenX = (m_windowWidth - m_width) / 2;
	m_screenY = (m_windowHeight - m_height) / 2;

	// Ensure non-negative offsets
	if (m_screenX < 0) m_screenX = 0;
	if (m_screenY < 0) m_screenY = 0;
}

} // namespace hb::shared::render
