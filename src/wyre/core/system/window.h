#pragma once

/**
 * @brief OS Windowing.
 * 
 * --- [definition] ---
 * class Window;
 * 
 * --- [functions] ---
 * Window(const char* title);
 * void poll_events();
 * TODO: Write out all Window functions.
 */

#if defined(_WIN32) || defined(_WIN64)
#include "wyre/platform/windows/window.h"
#else
#error "platform is not supported."
#endif
