#ifndef __WBE_PARSER_YAML_H_
#define __WBE_PARSER_YAML_H_

#include "parser.h"

namespace WhiteBirdEngine {

/**
 * @class YAMLData
 * @brief Data class for yaml.
 *
 */
class YAMLData : public ParserData<YAMLData> {
    friend class ParserYAML;
public:
    YAMLData() {
        // TODO
    }

    ~YAMLData() {
        // TODO
    }

    YAMLData(const YAMLData& p_other) {
        // TODO
    }
    YAMLData(YAMLData&& p_other) {
        // TOOD
    }
    YAMLData& operator=(const YAMLData& p_other) {
        // TODO
        return *this;
    }
    YAMLData& operator=(YAMLData&& p_other) {
        // TODO
        return *this;
    }

    template <typename T> requires (!std::same_as<T, YAMLData>)
    T get_value(const std::string& p_key) const {
        return T(); // TODO
    }

    template <std::same_as<YAMLData> T>
    YAMLData get_value(const std::string& p_key) const {
        return YAMLData(); // TODO
    }

private:
    // TODO
};

template <>
struct ParserTrait<class ParserYAML> {
    WBE_TRAIT(ParserTrait<ParserYAML>);
    using ParserDataType = YAMLData;
    WBE_TRAIT_REQURIES(ParserTraitConcept);

};

/**
 * @class ParserYAML
 * @brief Parser for YAML files.
 * @todo Test
 */
class ParserYAML : public Parser<ParserYAML> {
public:
    ParserYAML() {
        // TODO
    }
    virtual ~ParserYAML() {
        // TODO
    }
    ParserYAML(const ParserYAML& p_other) {
        // TODO
    }
    ParserYAML(ParserYAML&& p_other) {
        // TODO
    }
    ParserYAML& operator=(const ParserYAML& p_other) {
        // TODO
        return *this;
    }
    ParserYAML& operator=(ParserYAML&& p_other) {
        // TODO
        return *this;
    }

    void parse(const Path& p_path) {
        // TODO
    }

    void parse_from_buffer(const std::string& p_buffer) {
        // TODO
    }

    template <typename T> requires (!ParserDataConcept<T>)
    T get_value(const std::string& p_key) const {
        return T(); // TODO
    }

    template <ParserDataConcept T>
    YAMLData get_value(const std::string& p_key) const {
        return YAMLData(); // TODO
    }

private:
    YAMLData data;
};

}

#endif
