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
#ifndef __WBE_GAME_METADATA_HH__
#define __WBE_GAME_METADATA_HH__

#include "utils/defs.hh"
#include "utils/interface/singleton.hh"
#include "utils/utils.hh"
#include <stdexcept>
#include <string>
#include <unordered_map>
namespace WhiteBirdEngine {

/**
 * @class GameMetadata
 * @brief Metadata of the game.
 * Including label ids, type ids, etc.
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
            if (hash_code_map.find(p_key) == hash_code_map.end()) {
                throw std::runtime_error("Failed to find metadata with key: " + p_key);
            }
            return hash_code_map.at(p_key);
        }
        const std::string& operator[](HashCode p_hash_code) const {
            if (hash_code_map_rev.find(p_hash_code) == hash_code_map_rev.end()) {
                throw std::runtime_error("Failed to find metadata with hash code: " + std::to_string(p_hash_code));
            }
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
