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

