# Copyright 2025 OppositeNor
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import hashlib

def to_uint32(x):
    """Convert a uint64 integer to uint32 to match the usage in the C++ file.

    Args:
        x (): The value to be converted.

    Returns:
        The converted value.
    """
    return x & 0xFFFFFFFF

def hash_str(p_str : str) -> int:
    """Hash a string.

    Args:
        p_str: The string to hash.

    Returns:
        The hash value of the string.
    """
    if (len(p_str) != 0):
        return to_uint32(ord(p_str[0]) + to_uint32(33 * hash_str(p_str[1:])))
    else:
        return 5381

def hash_file(p_path : str):
    """Hash a file.

    Args:
        p_path: The path to the file to get the hashcode.

    Returns: The hashcode of the file's content.
        
    """
    hasher = hashlib.sha256()
    with open(p_path, "rb") as f:
        hasher.update(f.read())
    return hasher.hexdigest()

def hash_str_sha256(p_str : str) -> str:
    """Hash a string with sha256, has smaller collision posibility compared to hash_str method.

    Args:
        p_str: The string to be hashed.

    Returns:
        The hashed result.
    """
    hasher = hashlib.sha256()
    hasher.update(p_str.encode("utf-8"))
    return hasher.hexdigest()
