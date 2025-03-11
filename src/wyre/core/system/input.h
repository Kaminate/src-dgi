#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include "wyre/platform/windows/input.h"
#else
#error "platform is not supported."
#endif
