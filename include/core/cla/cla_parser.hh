/* Copyright 2025 OppositeNor

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#ifndef WBE_FILE_CLA_PARSER_HH
#define WBE_FILE_CLA_PARSER_HH

#include "core/allocator/i_allocator.hh"
#include "core/cla/cla_ast.hh"
#include "core/cla/cla_utils.hh"
#include "core/memory/reference_strong.hh"
#include <string>
#include <vector>

namespace WhiteBirdEngine {

class EngineConfig;

/**
 * @class CLAParser
 * @brief Interface of parsers for command line arguments.
 * @todo Test
 */
class CLAParser {
public:
    CLAParser() = default;
    ~CLAParser() = default;
    CLAParser(const CLAParser&) = default;
    CLAParser(CLAParser&&) = default;
    CLAParser& operator=(const CLAParser&) = default;
    CLAParser& operator=(CLAParser&&) = default;

    /**
     * @brief Parse tokens into ast.
     *
     * @param p_tokens The tokens to parse.
     * @return
     */
    Ref<CLAASTNode> parse(const std::vector<CLAToken>& p_tokens);

private:
    enum class ParseState {
        START,
        GET_ROOT_OPERAND,
        GET_OPTION,
        GET_OPERAND
    } parse_state = ParseState::START;

    Ref<CLAASTNodeOperation> curr_option = MEM_NULL;

    void process_token(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations);
    void parse_start(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations);
    void parse_get_root_operand(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations);
    void parse_get_option(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations);
    void parse_get_operand(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations);
};

} // namespace WhiteBirdEngine

#endif
