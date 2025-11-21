#ifndef __WBE_OBJECT_HH__
#define __WBE_OBJECT_HH__

#include <string_view>
#include "core/reflection/serializable.hh"
#include "core/reflection/reflection_defs.hh"
namespace WhiteBirdEngine {

/**
 * @class Object
 * @brief Object class.
 *
 */
WBE_CLASS(Object, WBE_SERIALIZABLE) : public Serializable {
public:
    Object() = default;
    virtual ~Object() {}

};

}

#endif
