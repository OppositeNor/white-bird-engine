def hash_str(p_str : str) -> int:
    if (len(p_str) != 0):
        return ord(p_str[0]) + 33 * hash_str(p_str[1:])
    else:
        return 5381

