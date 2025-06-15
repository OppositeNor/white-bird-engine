def to_uint32(x):
    return x & 0xFFFFFFFF

def hash_str(p_str : str) -> int:
    if (len(p_str) != 0):
        return to_uint32(ord(p_str[0]) + to_uint32(33 * hash_str(p_str[1:])))
    else:
        return 5381

