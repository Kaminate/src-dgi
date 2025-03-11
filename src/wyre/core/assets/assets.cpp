#include "assets.h"

namespace wyre {

void Assets::collect_garbage() {
    for (auto it = assets.begin(); it != assets.end();) {
        /* Remove asset if its reference count is 1 */
        if (it->second.use_count() <= 1)
            it = assets.erase(it);
        else
            ++it;
    }
}

}  // namespace wyre
