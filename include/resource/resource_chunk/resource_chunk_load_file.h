#include "core/allocator/stack_allocator.h"
#include "core/parser/parser.h"
#include "resource/resource_chunk/resource_chunk.h"
#include "resource/resource_unit_loader/ru_loader_binary.h"
#include "resource/resource_unit_loader/ru_loader_image.h"
#include "resource/resource_unit_loader/ru_loader_text.h"
#include "utils/utils.h"
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace WhiteBirdEngine {

/**
 * @class ResourceChunk
 * @brief Chunk of resources. Used for containing multiple resources to be loaded and deloaded at once.
 * @todo Test
 */
class ResourceChunkLoadFile : public ResourceChunk<ResourceChunkLoadFile> {
public:
    ~ResourceChunkLoadFile() {}
    ResourceChunkLoadFile(const ResourceChunkLoadFile&) = delete;
    ResourceChunkLoadFile(ResourceChunkLoadFile&&) = delete;
    ResourceChunkLoadFile& operator=(const ResourceChunkLoadFile&) = delete;
    ResourceChunkLoadFile& operator=(ResourceChunkLoadFile&&) = delete;

    /**
     * @brief Constructor
     *
     * @param p_parser The parser to get resource from.
     */
    template <ParserConcept ParserType>
    ResourceChunkLoadFile(const Parser<ParserType>& p_parser, StackAllocator& p_allocator);

    MemID get_resource(const std::string& p_key) const {
        return resource_map.at(dynam_hash(p_key.c_str()));
    }

    size_t get_size() const {
        return total_size;
    }

    template <ResourceUnitConcept T>
    MemID push_res_unit(const std::string& p_res_key, StackAllocator& p_allocator, const T& p_res) {
        using PureT = std::remove_cvref<T>;
        total_size += sizeof(PureT);
        MemID result = p_allocator.allocate(sizeof(PureT));
        *(p_allocator.get_obj<PureT>(result)) = p_res;
        resource_map[dynam_hash(p_res_key.c_str())] = result;
        return result;
    }

    MemID push_data(size_t p_res_size, void* p_res, StackAllocator& p_allocator) {
        total_size += p_res_size;
        MemID result = p_allocator.allocate(p_res_size);
        memcpy(p_allocator.get(result), p_res, p_res_size);
        return result;
    }

    bool is_already_registered(const std::string& p_res_key) {
        return resource_map.contains(dynam_hash(p_res_key.c_str()));
    }

private:
    size_t total_size;
    std::unordered_map<HashCode, MemID> resource_map;

    enum class RUType {
        AUDIO,
        BINARY,
        FONT,
        IMAGE,
        TEXT,
        VIDEO
    };

    RUType get_res_type(const std::string& p_type);

    template <ParserDataConcept ParserDataType>
    void load_ru_image(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_audio(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_video(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_text(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_font(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
    template <ParserDataConcept ParserDataType>
    void load_ru_binary(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator);
};

inline ResourceChunkLoadFile::RUType ResourceChunkLoadFile::get_res_type(const std::string& p_type) {
    if (p_type.starts_with("audio.")) {
        return RUType::AUDIO;
    }
    if (p_type.starts_with("binary.")) {
        return RUType::BINARY;
    }
    if (p_type.starts_with("font.")) {
        return RUType::FONT;
    }
    if (p_type.starts_with("image.")) {
        return RUType::IMAGE;
    }
    if (p_type.starts_with("text.")) {
        return RUType::TEXT;
    }
    if (p_type.starts_with("video.")) {
        return RUType::VIDEO;
    }
    throw std::runtime_error("Failed to get resource type. Type: " + p_type + " not recognized.");
}

template <ParserConcept ParserType>
ResourceChunkLoadFile::ResourceChunkLoadFile(const Parser<ParserType>& p_parser, StackAllocator& p_allocator) {
    std::vector<std::string> keys = p_parser.get_all_keys();
    for (auto& key : keys) {
        switch (get_res_type(key)) {
            case RUType::AUDIO:
                load_ru_audio(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::BINARY:
                load_ru_binary(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::FONT:
                load_ru_font(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::IMAGE:
                load_ru_image(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::TEXT:
                load_ru_text(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
            case RUType::VIDEO:
                load_ru_video(p_parser.get_data<ParserType::DataType>>(key), p_allocator);
                break;
        }
    }
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_image(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    RULoaderImage loader;
    loader.load<ParserDataType>(*this, p_parser_data);
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_audio(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_video(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_text(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    RULoaderText loader;
    loader.load<ParserDataType>(*this, p_parser_data.get_data());
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_font(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    // TODO
}

template <ParserDataConcept ParserDataType>
void ResourceChunkLoadFile::load_ru_binary(const ParserData<ParserDataType>& p_parser_data, StackAllocator& p_allocator) {
    RULoaderBinary loader;
    loader.load<ParserDataType>(*this, p_parser_data.get_data());
}

}
