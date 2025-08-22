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
