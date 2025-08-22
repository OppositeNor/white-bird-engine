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

