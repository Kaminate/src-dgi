#include "result.h"

namespace wyre {
namespace hidden {
thread_local std::string error = "";
}
}  // namespace wyre

wyre::types::err wyre::Err(const std::string fmt, ...) {
    int size = ((int)fmt.size()) * 2 + 50;
    std::string str;
    va_list ap;
    for (;;) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char*)str.data(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {
            str.resize(n);
            break;
        }
        if (n > -1)
            size = n + 1;
        else
            size *= 2;
    }
    wyre::hidden::error = str;
    return types::err();
}

wyre::types::err wyre::Err(const char* msg) {
    wyre::hidden::error = msg;
    return types::err();
}
