#include "wyre.h"

#include <chrono> /* delta time */
using namespace std::chrono;
using timepoint = steady_clock::time_point;

/* Core module includes */
#include "core/graphics/device.h"
#include "core/graphics/renderer.h"
#include "core/assets/assets.h"
#include "core/ecs.h"
#include "core/system/window.h"
#include "core/system/input.h"
#include "core/system/files.h"
#include "core/system/log.h"

namespace wyre {

/* Create all the engine modules on the heap at initialization */
WyreEngine::WyreEngine(const LogLevel&& log_level)
    : device(*new Device()),
      assets(*new Assets()),
      ecs(*new ECS()),
      window(*new Window()),
      input(*new Input()),
      files(*new Files()),
      logger(*new Logger("log.txt", log_level)) {}

/* Free all the engine modules */
WyreEngine::~WyreEngine() {
    delete &files;
    delete &input;
    delete &window;
    delete &ecs;
    delete &assets;
    delete &device;

    delete &logger; /* Free the logger last */
}

/**
 * @brief Engine setup.
 */
bool WyreEngine::init() {
    window.init("Wyre Engine (Vulkan)");

    const Result<void> r_device = device.init(logger, window);
    if (r_device.is_err()) {
        logger.log(LogGroup::GRAPHICS_API, LogLevel::CRITICAL, "failed to init device: %s", r_device.unwrap_err().c_str());
        return false;
    }

    /* Register the Renderer system */
    renderer = &ecs.register_system<Renderer>(logger, window, device);

    logger.log(LogGroup::GRAPHICS_API, LogLevel::INFO, "initialized device & renderer.");

    return true;
}

/**
 * @brief Main engine loop.
 */
bool WyreEngine::run() {
    timepoint time = high_resolution_clock::now();
    while (window.open) {
        /* Find the delta time */
        const timepoint ctime = high_resolution_clock::now();
        const nanoseconds elapsed = ctime - time;
        time = ctime; /* Update time */
        const float dt = (float)((double)duration_cast<microseconds>(elapsed).count() / 1e6);

        window.poll_events(input); /* Input */
        ecs.systems_update(*this, dt);

        while (device.start_frame() == false); /* Start frame */
        ecs.systems_render(*this);
        device.end_frame(); /* End frame */
    }

    return true;
}

/**
 * @brief Engine resources cleanup.
 */
bool WyreEngine::destroy() {
    if (device.wait_idle() == false) return false;
    
    renderer->destroy(device);
    device.destroy();

    return true;
}

}  // namespace wyre
