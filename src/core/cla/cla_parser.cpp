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
#include "core/cla/cla_parser.hh"
#include "core/allocator/allocator.hh"
#include "core/cla/cla_ast.hh"
#include "core/engine_core.hh"
#include "core/memory/reference_strong.hh"
#include <stdexcept>
#include <string>

namespace WhiteBirdEngine {

Ref<CLAASTNode> CLAParser::parse(const std::vector<CLAToken>& p_tokens) {
    std::string utility_name;
    std::vector<Ref<CLAASTNode>> operations;
    parse_state = ParseState::START;
    for (auto& token : p_tokens) {
        process_token(token, utility_name, operations);
    }
    Ref<CLAASTNodeRoot> root_node = make_ref<CLAASTNodeRoot>(global_allocator(), utility_name, std::move(operations));
    return root_node;
}

void CLAParser::process_token(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations) {
    switch (parse_state) {
        case ParseState::START:
            parse_start(p_token, p_utility_name, p_operations);
            break;
        case ParseState::GET_ROOT_OPERAND:
            parse_get_root_operand(p_token, p_utility_name, p_operations);
            break;
        case ParseState::GET_OPTION:
            parse_get_option(p_token, p_utility_name, p_operations);
            break;
        case ParseState::GET_OPERAND:
            parse_get_operand(p_token, p_utility_name, p_operations);
            break;
    }
}

void CLAParser::parse_start(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations) {
    if (p_token.type != CLAToken::Type::UTILITY_NAME) {
        throw std::runtime_error("Failed to parse CLA: the first token of the input tokens must be the utility name.");
    }
    p_utility_name = p_token.value;
    parse_state = ParseState::GET_ROOT_OPERAND;
}

void CLAParser::parse_get_root_operand(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations) {
    switch (p_token.type) {
        case CLAToken::Type::OPTION_SHORT:
        case CLAToken::Type::OPTION_LONG:
            parse_state = ParseState::GET_OPTION;
            process_token(p_token, p_utility_name, p_operations);
            break;
        case CLAToken::Type::OPERAND: {
            Ref<CLAASTNodeRootOperand> operand = make_ref<CLAASTNodeRootOperand>(global_allocator(), p_token.value);
            p_operations.push_back(operand);
            break;
        }
        default:
            throw std::runtime_error("Failed to parse CLA: invalid token: " + p_token.value + ".");
    }
}

void CLAParser::parse_get_option(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations) {
    switch (p_token.type) {
        case CLAToken::Type::OPTION_SHORT: {
            WBE_DEBUG_ASSERT(p_token.value.size() >= 1);
            WBE_DEBUG_ASSERT(p_token.value.rfind("-", 0) == 0);
            std::string options = p_token.value.substr(1);
            if (options.size() > 1) {
                for (auto operation : options) {
                    curr_option = make_ref<CLAASTNodeOperation>(global_allocator(), std::string(1, operation), true);
                    p_operations.push_back(curr_option);
                }
                parse_state = ParseState::GET_ROOT_OPERAND;
            }
            else {
                curr_option = make_ref<CLAASTNodeOperation>(global_allocator(), p_token.value, true);
                p_operations.push_back(curr_option);
                parse_state = ParseState::GET_OPERAND;
            }
            break;
        }
        case CLAToken::Type::OPTION_LONG:
            WBE_DEBUG_ASSERT(p_token.value.size() >= 2);
            WBE_DEBUG_ASSERT(p_token.value.rfind("--", 0) == 0);
            curr_option = make_ref<CLAASTNodeOperation>(global_allocator(), p_token.value.substr(2), false);
            p_operations.push_back(curr_option);
            parse_state = ParseState::GET_OPERAND;
            break;
        default:
            throw std::runtime_error("Failed to parse CLA: invalid token: " + p_token.value + ".");
    }
}

void CLAParser::parse_get_operand(const CLAToken& p_token, std::string& p_utility_name, std::vector<Ref<CLAASTNode>>& p_operations) {
    switch (p_token.type) {
        case CLAToken::Type::OPTION_SHORT:
        case CLAToken::Type::OPTION_LONG:
            parse_state = ParseState::GET_OPTION;
            process_token(p_token, p_utility_name, p_operations);
            break;
        case CLAToken::Type::OPERAND: {
            WBE_DEBUG_ASSERT(curr_option != MEM_NULL);
            curr_option->push_argument(p_token.value);
            parse_state = ParseState::GET_OPERAND;
            break;
        }
        default:
            throw std::runtime_error("Failed to parse CLA: invalid token: " + p_token.value + ".");
    }
}

}
