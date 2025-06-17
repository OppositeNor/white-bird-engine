#ifndef __WBE_PARSER_JSON_H__
#define __WBE_PARSER_JSON_H__

#include "parser.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace WhiteBirdEngine {

/**
 * @class ParserJSON
 * @brief Parser for JSON files.
 * @todo Test
 */
class ParserJSON : public Parser<ParserJSON> {
    using json = nlohmann::json;
public:
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
        data = json::parse(static_cast<std::string>(p_path));
    }

    void parse_from_buffer(const std::string& p_buffer) {
        data = json::parse(p_buffer);
    }

    template <typename T>
    T get_value(const std::string& p_key) {
        return data.at(p_key).get<T>();
    }

private:
    json data;
};

}

#endif
