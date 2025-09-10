#include <sstream>
#include <string>
#include <vector>

class WBEAllocPoolBehavTestClass {
public:
    WBEAllocPoolBehavTestClass(const std::string& p_pool_name, size_t p_total_size, size_t p_header_size)
        : pool_name(p_pool_name), total_size(p_total_size), header_size(p_header_size) {}

    std::string operator()(const std::vector<std::pair<bool, int64_t>>& p_chunk_status) {
        std::stringstream result;
        result << R"({"type":")" << pool_name << R"(","total_size":)" << total_size << R"(,"free_chunk_layout":[)";
        size_t tracker = 0;
        bool is_first = true;
        for (auto& status : p_chunk_status) {
            if (status.first) {
                if (status.second < 0) {
                    tracker = header_size;
                }
                else {
                    tracker += status.second + header_size;
                }
                continue;
            }
            if (!is_first) {
                result << ",";
            }
            is_first = false;
            if (status.second < 0) {
                result << R"({"begin":)" << tracker << R"(,"size":)" << total_size - tracker <<"}";
                tracker = total_size;
            }
            else {
                result << R"({"begin":)" << tracker << R"(,"size":)" << status.second <<"}";
                tracker += status.second;
            }
        }
        result << "]}";
        return result.str();
    }

    std::string pool_name;
    size_t total_size, header_size;
};

