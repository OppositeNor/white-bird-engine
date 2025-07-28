#include "resource/resource_unit_loader/ru_loader_ppm3.h"
#include "resource/resource_utils.h"
#include "utils/utils.h"
#include <sstream>

namespace WhiteBirdEngine {

std::unique_ptr<unsigned char[]> RULoaderPPM3::load_ppm3_content(const Path& p_path, bool p_flip_h, bool p_flip_v,
                                                                 uint32_t& p_width, uint32_t& p_height) {
    std::string str_content = load_text_file(static_cast<std::string>(p_path).c_str());
    std::stringstream content(remove_hashtag_comments(str_content));
    std::string header;
    content >> header;
    if (header != "P3") {
        throw std::runtime_error("Not a PPM3 file.");
    }
    content >> p_width >> p_height;
    double scaler;
    content >> scaler;
    scaler = 255.0 / scaler;
    std::unique_ptr<unsigned char[]> result = std::make_unique<unsigned char[]>(p_width * p_height * 3);
    for (uint32_t i = 0; i < p_width * p_height; ++i) {
        int r, g, b;
        content >> r >> g >> b;
        result[3 * i + 0] = static_cast<unsigned char>(r * scaler);
        result[3 * i + 1] = static_cast<unsigned char>(g * scaler);
        result[3 * i + 2] = static_cast<unsigned char>(b * scaler);
    }
    if (p_flip_h) {
        img_flip_h(result.get(), p_width, p_height, 3);
    }
    if (p_flip_v) {
        img_flip_v(result.get(), p_width, p_height, 3);
    }
    return result;
}


}
