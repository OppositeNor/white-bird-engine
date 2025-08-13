#ifndef __WBE_PARSER_YAML_HH__
#define __WBE_PARSER_YAML_HH__

#include "parser.hh"
#include <yaml-cpp/yaml.h>

namespace WhiteBirdEngine {

/**
 * @class YAMLData
 * @brief Data class for yaml.
 *
 */
class YAMLData : public ParserData<YAMLData> {
    using yaml = YAML::Node;
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
    YAMLData(const yaml& p_node)
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

    std::vector<std::string> get_all_keys() const {
        std::vector<std::string> result;
        result.reserve(node.size());
        for (yaml::const_iterator it = node.begin(); it != node.end(); ++it) {
            result.push_back(it->first.as<std::string>());
        }
        return result;
    }

    bool contains(const std::string& p_key) const {
        return node[p_key].IsDefined();
    }

private:
    yaml node;
};

/**
 * @class ParserYAML
 * @brief Parser for YAML files.
 * @todo Test
 */
class ParserYAML : public Parser<ParserYAML> {
    using yaml = YAML::Node;
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
        return YAMLData(data.get_value<yaml>(p_key));
    }

    std::vector<std::string> get_all_keys() const {
        return data.get_all_keys();
    }

    const YAMLData& get_data() const {
        return data;
    }

    YAMLData& get_data() {
        return data;
    }

    bool contains(const std::string& p_key) const {
        return data.contains(p_key);
    }

private:
    YAMLData data;
};

}

#endif
