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
#ifndef WBE_FILE_MODULE_HH
#define WBE_FILE_MODULE_HH

#include "platform/file_system/path.hh"
#include <string_view>
namespace WhiteBirdEngine {

class DynamicModule final {
public:
    DynamicModule(const Path& p_module_path);
    ~DynamicModule();
    DynamicModule(const DynamicModule&) = delete;
    DynamicModule(DynamicModule&&) = delete;
    DynamicModule& operator=(const DynamicModule&) = delete;
    DynamicModule& operator=(DynamicModule&&) = delete;

    template <typename FuncType>
    FuncType get_function(std::string_view p_function_symbol) {
        return FuncType(get_function_helper(p_function_symbol));
    }

private:
#ifdef __unix__
    using ModuleHandle = void*;
    static constexpr ModuleHandle MODULE_HANDLE_INVALID = nullptr;
#else
#error "Module type not declared"
#endif
    ModuleHandle handle = MODULE_HANDLE_INVALID;
    Path module_path;
    void* get_function_helper(std::string_view p_function_symbol);
};

} // namespace WhiteBirdEngine

#endif
