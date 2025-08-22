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
#include "resource/resource_unit_loader/ru_loader_ppm.hh"
#include <sstream>

namespace WhiteBirdEngine {

std::string RULoaderPPM::get_ppm_header(const Path& p_path) {
    std::string str_content = load_text_file(static_cast<std::string>(p_path).c_str());
    std::stringstream content(remove_hashtag_comments(str_content));
    std::string header;
    content >> header;
    return header;
}

}
