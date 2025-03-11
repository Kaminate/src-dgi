#include "transform.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace wyre {

glm::mat4 Transform::get_model() const {
    glm::mat4 model = glm::mat4(1.0f);
    /* Translate */
    model = glm::translate(model, position);
    /* Rotate */
    model = model * glm::toMat4(rotation);
    /* Scale */
    model = glm::scale(model, scale);
    return model;
}

}  // namespace wyre
