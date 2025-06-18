#ifndef __WBE_PARSER_YAML_H_
#define __WBE_PARSER_YAML_H_

#include "parser.h"
#include "yaml-cpp/node/parse.h"
#include <yaml-cpp/yaml.h>

namespace WhiteBirdEngine {

/**
 * @class YAMLData
 * @brief Data class for yaml.
 *
 */
class YAMLData : public ParserData<YAMLData> {
    friend class ParserYAML;
public:
    YAMLData() {}
    ~YAMLData() {}
    YAMLData(const YAMLData& p_other)
        : node(p_other.node) {}
    YAMLData(YAMLData&& p_other)
        : node(std::move(p_other.node)) {}
    YAMLData& operator=(const YAMLData& p_other) {
        node = p_other.node;
        return *this;
    }
    YAMLData& operator=(YAMLData&& p_other) {
        node = std::move(p_other.node);
        return *this;
    }
    YAMLData(const YAML::Node& p_node)
        : node(p_node) {}
    YAMLData(YAML::Node&& p_node)
        : node(std::move(p_node)) {}

    template <typename T> requires (!std::same_as<T, YAMLData>)
    T get_value(const std::string& p_key) const {
        return node[p_key].as<T>();
    }

    template <std::same_as<YAMLData> T>
    YAMLData get_value(const std::string& p_key) const {
        return YAMLData(node[p_key]);
    }

private:
    YAML::Node node;
};

/**
 * @class ParserYAML
 * @brief Parser for YAML files.
 * @todo Test
 */
class ParserYAML : public Parser<ParserYAML> {
public:

    using DataType = YAMLData;

    ParserYAML() = default;
    virtual ~ParserYAML() {}
    ParserYAML(const ParserYAML& p_other)
        : data(p_other.data) {}
    ParserYAML(ParserYAML&& p_other)
        : data(std::move(p_other.data) ){}
    ParserYAML& operator=(const ParserYAML& p_other) {
        data = p_other.data;
        return *this;
    }
    ParserYAML& operator=(ParserYAML&& p_other) {
        data = std::move(p_other.data);
        return *this;
    }

    void parse(const Path& p_path) {
        data.node = YAML::LoadFile(static_cast<std::string>(p_path));
    }

    void parse_from_buffer(const std::string& p_buffer) {
        data.node = YAML::Load(p_buffer);
    }

    template <typename T> requires (!ParserDataConcept<T>)
    T get_value(const std::string& p_key) const {
        return data.get_value<T>(p_key);
    }

    template <ParserDataConcept T>
    YAMLData get_value(const std::string& p_key) const {
        return YAMLData(data.get_value<YAML::Node>(p_key));
    }

private:
    YAMLData data;
};

}

#endif
