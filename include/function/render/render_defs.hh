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
#ifndef __WBE_RENDER_DEFS_HH__
#define __WBE_RENDER_DEFS_HH__

#ifdef WBE_REFLECTION_PARSER

__attribute__((annotate("WBE_DEFINE_RENDER_PASS")))
void define_wbe_render_pass(const char* pass_class_name, const char* pass_name, const char* pass_type) {}

// Define a render pass.
#define WBE_RENDER_PASS(PassClassName, pass_name, RENDER_API)\
        define_wbe_render_pass(#PassClassName, #pass_name, #RENDER_API);\
        virtual const char* get_pass_name() override {\
            return #pass_name;\
        }

#else

// Define a render pass.
#define WBE_RENDER_PASS(PassClassName, pass_name, RENDER_API)\
virtual const char* get_pass_name() override {\
    return #pass_name;\
}

#endif



#endif
