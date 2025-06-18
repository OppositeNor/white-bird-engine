import json
import sys
import importlib.util
from channel_registrator import ChannelRegistrator

class WBEBuilder:
    def __init__(self, p_metadata_path : str) -> None:
        self.metadata_path = p_metadata_path
        self.channel_registrator = ChannelRegistrator()

    def load_script_config(self, p_load_path : str) -> None:
        print("Loading configuration scripts...")
        spec = importlib.util.spec_from_file_location("config", p_load_path)
        if spec is None:
            raise RuntimeError("Failed to load spec from path: " + p_load_path)
        module_config = importlib.util.module_from_spec(spec)
        if spec.loader is not None:
            spec.loader.exec_module(module_config)
            module_config.register(self)

    def register_channel(self, p_channel_path) -> None:
        self.channel_registrator.register_channel(p_channel_path)

    def dump(self) -> None:
        print("Exporting...")
        metadata : dict = {}
        metadata["channels"] = self.channel_registrator.channels
        self.write_to_file(self.metadata_path, json.dumps(metadata, sort_keys=True))

    def write_to_file(self, p_path : str, p_str : str) -> None:
        with open(p_path, "w") as channel_file:
            channel_file.write(p_str)

    def checks(self) -> None:
        print("Checking...")
        self.check_hashing_collision("channels", self.channel_registrator.channels)

    def check_hashing_collision(self, p_dict_name : str, p_dictionary : dict) -> None:
        hash_val_set = set()
        dict_rev = dict()
        for channel in p_dictionary.keys():
            if p_dictionary[channel] in hash_val_set:
                raise RuntimeError("Hasing collision of \"" + p_dict_name + "\" detected! Hash values of: \"" + channel +
                                   "\" and \"", dict_rev[p_dictionary[channel]], "\" are identical: ", p_dictionary[channel])
            dict_rev[p_dictionary[channel]] = channel
            hash_val_set.add(p_dictionary[channel])


if __name__ == "__main__":
    if (len(sys.argv) < 2):
        print("Usage: " + sys.argv[0] + " <res_path> [config1] [config2] ...")
        exit(-1)

    print("White Bird Engine build script start.")
    builder = WBEBuilder(sys.argv[1])
    for i in range(2, len(sys.argv)):
        builder.load_script_config(sys.argv[i])
    builder.checks()
    builder.dump() # TODO
    print("White Bird Engine build script finish.")

