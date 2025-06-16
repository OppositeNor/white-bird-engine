#ifndef __WBE_PARSER_YAML_H_
#define __WBE_PARSER_YAML_H_

#include "parser.h"

namespace WhiteBirdEngine {

/**
 * @class ParserYAML
 * @brief Parser for YAML files.
 *
 */
class ParserYAML : public Parser<ParserYAML> {
public:
    ParserYAML() = default;
    virtual ~ParserYAML() {}
    ParserYAML(const ParserYAML&) {
        // TODO
    }
    ParserYAML(ParserYAML&&) {
        // TODO
    }
    ParserYAML& operator=(const ParserYAML& p_other) {
        // TODO
        return *this;
    }
    ParserYAML& operator=(ParserYAML&&) {
        // TODO
        return *this;
    }

    void parse(const Path& p_path) {
        // TODO
    }

    template <typename T>
    T get_value(const std::string& p_key) {
        return T(); // TODO
    }
private:

};

}

#endif
