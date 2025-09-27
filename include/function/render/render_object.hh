#ifndef __WBE_RENDER_OBJECT_HH__
#define __WBE_RENDER_OBJECT_HH__

#include <string>
namespace WhiteBirdEngine {

/**
 * @class RenderObject
 * @brief Class for all the render objects, such as pipelines, shaders, etc.
 *
 */
class RenderObject {
public:

    RenderObject() = default;
    virtual ~RenderObject() {}

    /**
     * @brief Get the type of the render object.
     */
    virtual std::string get_ro_type() const = 0;

};

}

#endif
