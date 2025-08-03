#ifndef __WBE_PARSER_JSON_HH__
#define __WBE_PARSER_JSON_HH__

#include "parser.hh"
#include <fstream>
#include <nlohmann/json.hpp>

namespace WhiteBirdEngine {

/**
 * @class JSONData
 * @brief Data class for json.
 *
 */
class JSONData : public ParserData<JSONData> {
    using json = nlohmann::json;
    friend class ParserJSON;
public:
    JSONData() {}
    ~JSONData() {}
    JSONData(const JSONData& p_other) : data(p_other.data) {}
    JSONData(JSONData&& p_other) : data(std::move(p_other.data)) {}
    JSONData& operator=(const JSONData& p_other) {
        data = p_other.data;
        return *this;
    }
    JSONData& operator=(JSONData&& p_other) {
        data = std::move(p_other.data);
        return *this;
    }

    JSONData(const json& p_data)
        : data(p_data) {}
    JSONData(json&& p_data)
        : data(std::move(p_data)) {}

    template <typename T> requires (!std::same_as<T, JSONData>)
    T get_value(const std::string& p_key) const {
        return data.at(p_key).get<T>();
    }

    template <std::same_as<JSONData> T>
    JSONData get_value(const std::string& p_key) const {
        return JSONData(data.at(p_key).get<json>());
    }

    std::vector<std::string> get_all_keys() const {
        std::vector<std::string> result;
        result.reserve(data.size());
        for (json::const_iterator it = data.begin(); it != data.end(); ++it) {
            result.push_back(it.key());
        }
        return result;
    }

    bool contains(const std::string& p_key) const {
        return data.contains(p_key);
    }

private:
    json data;
};

/**
 * @class ParserJSON
 * @brief Parser for JSON files.
 * @todo Test
 */
class ParserJSON : public Parser<ParserJSON> {
    using json = nlohmann::json;
public:
    using DataType = JSONData;

    ParserJSON() = default;
    virtual ~ParserJSON() {}
    ParserJSON(const ParserJSON& p_other)
        : data(p_other.data) {}
    ParserJSON(ParserJSON&& p_other)
        : data(std::move(p_other.data)) {}
    ParserJSON& operator=(const ParserJSON& p_other) {
        data = p_other.data;
        return *this;
    }
    ParserJSON& operator=(ParserJSON&& p_other) {
        data = std::move(p_other.data);
        return *this;
    }

    void parse(const Path& p_path) {
        std::ifstream f;
        f.open(static_cast<std::string>(p_path));
        if (!f.is_open()) {
            throw std::runtime_error("Failed to open file at path: " + static_cast<std::string>(p_path));
        }
        data.data = json::parse(f);
    }

    void parse_from_buffer(const std::string& p_buffer) {
        data.data = json::parse(p_buffer);
    }

    template <typename T> requires (!ParserDataConcept<T>)
    T get_value(const std::string& p_key) const {
        return data.data.at(p_key).get<T>();
    }

    template <ParserDataConcept T>
    JSONData get_value(const std::string& p_key) const {
        return JSONData(data.data.at(p_key).get<json>());
    }

    std::vector<std::string> get_all_keys() const {
        return data.get_all_keys();
    }

    const JSONData& get_data() const {
        return data;
    }

    JSONData& get_data() {
        return data;
    }

    bool contains(const std::string& p_key) const {
        return data.contains(p_key);
    }

private:
    JSONData data;
};

}

#endif
