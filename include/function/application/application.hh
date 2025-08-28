#ifndef __WBE_APPLICATION_HH__
#define __WBE_APPLICATION_HH__

namespace WhiteBirdEngine {

/**
 * @class Application
 * @brief Application class.
 *
 */
class Application {
public:
    Application() = default;
    virtual ~Application() {}

    /**
     * @brief Function that ticks the rendering frames.
     *
     * @param p_delta The difference of time of the last two calls for this function.
     */
    virtual void render_tick(double p_delta) = 0;

    /**
     * @brief Function that ticks the logic frames.
     *
     * @param p_delta The difference of time of the last two calls for this function.
     */
    virtual void logic_tick(double p_delta) = 0;

};

}

#endif
