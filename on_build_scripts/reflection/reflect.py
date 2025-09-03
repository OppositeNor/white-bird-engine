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
import json
import os
import importlib.util
from reflection.channel_registrator import WBEChannelRegistrator

class WBEReflector:
    """Reflector. Used for managing all the reflections.

    Attributes: 
        metadata_path: The path to the metadata output file.
        channel_registrator: The channel registrator.
    """

    def __init__(self, p_metadata_path : str) -> None:
        self.metadata_path = p_metadata_path
        self.channel_registrator = WBEChannelRegistrator()

    def load_script_config(self, p_load_path : str) -> None:
        """Load config scripts.

        Args:
            p_load_path: The path to load the config script.

        Raises:
            RuntimeError: If the config script is not runnable.
        """
        print("WBEReflect: Loading configuration script: " + p_load_path)
        if not os.path.exists(p_load_path):
            raise FileNotFoundError(f"File not found: {p_load_path}")
        spec = importlib.util.spec_from_file_location("config", p_load_path)
        if spec is None:
            raise RuntimeError(f"Failed to load spec from path: {p_load_path}")
        module_config = importlib.util.module_from_spec(spec)
        if spec.loader is not None:
            spec.loader.exec_module(module_config)
            module_config.register(self)

    def register_channel(self, p_channel_name) -> None:
        """Register a channel.

        Args:
            p_channel_path (): The channel to register.
        """
        self.channel_registrator.register_channel(p_channel_name)

    def dump(self) -> None:
        """Dump the metadata to the metadata file."""
        print("WBEReflect: Exporting metadata...")
        metadata : dict = {}
        metadata["channels"] = self.channel_registrator.channels
        self._write_to_file(self.metadata_path, json.dumps(metadata, sort_keys=True))

    def checks(self) -> None:
        """Do checkings."""
        print("WBEReflect: Checking...")
        self._check_hashing_collision("channels", self.channel_registrator.channels)

    def _write_to_file(self, p_path : str, p_str : str) -> None:
        with open(p_path, "w") as channel_file:
            channel_file.write(p_str)

    def _check_hashing_collision(self, p_dict_name : str, p_dictionary : dict) -> None:
        hash_val_set = set()
        dict_rev = dict()
        for channel in p_dictionary.keys():
            if p_dictionary[channel] in hash_val_set:
                raise RuntimeError(f"Hasing collision of \"{p_dict_name}\" detected! Hash values of: \"{channel}\" "
                    f"and \"{dict_rev[p_dictionary[channel]]}\" are identical: {p_dictionary[channel]}")
            dict_rev[p_dictionary[channel]] = channel
            hash_val_set.add(p_dictionary[channel])



