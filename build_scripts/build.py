import json
import os
import sys
import importlib.util
from channel_registrator import ChannelRegistrator

class WBEBuilder:
    def __init__(self, p_metadata_dir : str) -> None:
        self.metadata_dir = p_metadata_dir
        self.channel_registrator = ChannelRegistrator()

    def load_script_config(self, p_load_path : str) -> None:
        spec = importlib.util.spec_from_file_location("config", p_load_path)
        if spec is None:
            raise RuntimeError("Failed to load spec from path: " + p_load_path)
        module_config = importlib.util.module_from_spec(spec)
        if spec.loader is not None:
            spec.loader.exec_module(module_config)
            module_config.register(self)

    def register_channel(self, p_channel_path) -> None:
        self.channel_registrator.register_channel(p_channel_path)

    def output_resources(self) -> None:
        self.write_to_file(os.path.join(self.metadata_dir, "channels.json"),
                           self.serialize_channel_registrator())

    def write_to_file(self, p_path : str, p_str : str) -> None:
        with open(p_path, "w") as channel_file:
            channel_file.write(json.dumps(p_str, sort_keys=True))

    def serialize_channel_registrator(self) -> str:
        return json.dumps(self.channel_registrator.channels, sort_keys=True)


if __name__ == "__main__":
    if (len(sys.argv) < 2):
        print("Usage: " + sys.argv[0] + " <res_path> [config1] [config2] ...")
        exit(-1)

    print("White Bird Engine build script start")
    builder = WBEBuilder(sys.argv[1])
    for i in range(2, len(sys.argv)):
        builder.load_script_config(sys.argv[i])
    builder.output_resources() # TODO

