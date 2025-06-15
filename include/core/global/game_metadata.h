#ifndef __WBE_GAME_METADATA_H__
#define __WBE_GAME_METADATA_H__

#include "utils/defs.h"
#include "utils/interface/singleton.h"
#include "utils/utils.h"
#include <string>
#include <unordered_map>
namespace WhiteBirdEngine {

/**
 * @class GameMetadata
 * @brief Metadata of the game.
 * Including channel ids, type ids, etc.
 * @todo Test
 */
class GameMetadata : public Singleton<GameMetadata> {
public:
    GameMetadata() = default;
    ~GameMetadata() {}
    
    /**
     * @class MetadataBlock
     * @brief Block of the metadata.
     *
     */
    struct MetadataBlock {
        std::unordered_map<std::string, HashCode> hash_code_map;
        std::unordered_map<HashCode, std::string> hash_code_map_rev;

        HashCode operator[](const std::string& p_key) const {
            return hash_code_map.at(p_key);
        }
        const std::string& operator[](HashCode p_hash_code) const {
            return hash_code_map_rev.at(p_hash_code);
        }

        /**
         * @brief Add a hashing pair
         *
         * @param p_key The key of the pair.
         * @param p_hash_code The hash code of the pair.
         */
        void add_pair(const std::string& p_key, HashCode p_hash_code) {
            WBE_DEBUG_ASSERT(hash_code_map.find(p_key) == hash_code_map.end());
            hash_code_map.insert(std::pair(p_key, p_hash_code));
            hash_code_map_rev.insert(std::pair(p_hash_code, p_key));
        }
    };

    /**
     * @brief Get the metadata of the channels.
     *
     * @return The metadata of the channels.
     */
    MetadataBlock& get_channel_metadata() {
        return channel_metadata;
    }

    /**
     * @brief Get the metadata of the channels.
     *
     * @return The metadata of the channels.
     */
    const MetadataBlock& get_channel_metadata() const {
        return channel_metadata;
    }

    /**
     * @brief Get the metadata of the types.
     *
     * @return The metadata of the types.
     */
    MetadataBlock& get_type_metadata() {
        return type_metadata;
    }

    /**
     * @brief Get the metadata of the types.
     *
     * @return The metadata of the types.
     */
    const MetadataBlock& get_type_metadata() const {
        return type_metadata;
    }


private:
    MetadataBlock channel_metadata;
    MetadataBlock type_metadata;
};

}

#endif
