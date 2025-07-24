#ifndef __WBE_REFLECTION_H__
#define __WBE_REFLECTION_H__

#include <string>
namespace WhiteBirdEngine {

/**
 * @class Reflection
 * @brief Used for reflection.
 *
 */
class Reflection {
public:
    Reflection() {}
    ~Reflection() {}
    Reflection(const Reflection&) = delete;
    Reflection(Reflection&&) = delete;
    Reflection& operator=(const Reflection&) = delete;
    Reflection& operator=(Reflection&&) = delete;

    /**
     * @brief Serialize to a string.
     *
     * @param p_serializer The serializer.
     */
    template <SerializerConcept SerializerT>
    std::string serailize(SerializerConcept& p_serializer) {
        return std::string();// TODO
    }

    /**
     * @brief Serialaize and output to a file.
     *
     * @param p_serializer The serializer.
     * @param p_path The path to the file to save to.
     */
    template <SerializerConcept SerializerT>
    void serailize(SerializerConcept& p_serializer, const Path& p_path) {
        // TODO
    }

private:

};

};

#endif
