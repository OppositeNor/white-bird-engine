#ifndef __WBE_SERIALIZABLE_HH__
#define __WBE_SERIALIZABLE_HH__

#include "core/reflection/reflection_defs.hh"
#include "utils/utils.hh"
#include <string_view>

#define WBE_DECL_SERIALIZABLE(class_name)\
    virtual std::string_view get_type() const override {\
        return #class_name;\
    }\
    virtual TypeID get_type_id() const override {\
        return static_hash(#class_name);\
    }

namespace WhiteBirdEngine {

/**
 * @class Serializable
 * @brief Serializable struct.
 *
 */
WBE_CLASS(Serializable) {
public:
    /**
     * @brief Get the type of the serializable in string.
     * 
     * @return The type of the serializable in string.
     */
    virtual std::string_view get_type() const = 0;

    /**
     * @brief Get the type ID of the serializable.
     *
     * @return The type ID of the serializable.
     */
    virtual TypeID get_type_id() const = 0;

};


}

#endif
