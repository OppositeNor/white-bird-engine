#ifndef __WBE_SERIALIZABLE_HH__
#define __WBE_SERIALIZABLE_HH__

namespace WhiteBirdEngine {

template <typename T>
class SerializableSD {
public:

    using ObjType = T;

    /**
     * @brief Deserialize.
     *
     * @param p_serializable The object to deserialize.
     */
    template <typename ParserData>
    static void deserialize(const ParserData& p_data, ObjType& p_serializable) {
         p_data.template get<ObjType>(p_serializable);
    }

};

}

#endif
