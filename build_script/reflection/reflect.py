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
from build_script.reflection.code_gen import WBECodeGenerator, WBEGenFileInfo
from build_script.utils import hash_str
from build_script.reflection.metadata_types import WBEMetadata

class WBEReflector:
    """Reflector. Used for managing all the reflections.

    Attributes: 
        metadata_path: The path to the metadata output file.
        metadata: The metadata of the engine.
    """

    def __init__(self, metadata_path : str, gen_file_infos : list[WBEGenFileInfo]) -> None:
        """Constructor.

        Args:
            metadata_path: The path to the metadata output file.
            gen_file_infos: The list of code generation information.
        """
        self.metadata_path = metadata_path
        self.metadata = WBEMetadata()
        self._gen_file_infos = gen_file_infos

    def load_script_config(self, load_path : str) -> None:
        """Load config scripts.

        Args:
            load_path: The path to load the config script.

        Raises:
            RuntimeError: If the config script is not runnable.
        """
        print("WBEReflect: Loading configuration script: " + load_path)
        if not os.path.exists(load_path):
            raise FileNotFoundError(f"File not found: {load_path}")
        spec = importlib.util.spec_from_file_location("config", load_path)
        if spec is None:
            raise RuntimeError(f"Failed to load spec from path: {load_path}")
        module_config = importlib.util.module_from_spec(spec)
        if spec.loader is not None:
            spec.loader.exec_module(module_config)
            module_config.register(self)

    def register_channel(self, channel_name) -> None:
        """Register a channel.

        Args:
            channel_path (): The channel to register.
        """
        self.metadata.channels[channel_name] = hash_str(channel_name)

    def register_class(self, class_obj) -> None:
        """Register a class.

        Args:
            class_obj (): The class to register.
        """
        self.metadata.classes.append(class_obj.model_dump())

    def register_component(self, component) -> None:
        """Register a component.

        Args:
            component (): The component to register.
        """
        self.metadata.components.append(component.model_dump())

    def register_components_headers(self, components_headers) -> None:
        self.metadata.components_headers = list(components_headers)

    def dump(self) -> None:
        """Dump the metadata to the metadata file."""
        print("WBEReflect: Exporting metadata...")
        self._write_to_file(self.metadata_path, json.dumps(self.metadata.__dict__, indent=4))
        # Generate code
        generator = WBECodeGenerator({
            "metadata" : self.metadata
        }, self._gen_file_infos)
        generator.generate()

    def checks(self) -> None:
        """Do checkings."""
        print("WBEReflect: Checking...")
        self._check_hashing_collision("channels", self.metadata.channels)

    def _write_to_file(self, path : str, str_content : str) -> None:
        with open(path, "w+") as channel_file:
            channel_file.write(str_content)

    def _check_hashing_collision(self, dict_name : str, dictionary : dict) -> None:
        hash_val_set = set()
        dict_rev = dict()
        for channel in dictionary.keys():
            if dictionary[channel] in hash_val_set:
                raise RuntimeError(f"Hasing collision of \"{dict_name}\" detected! Hash values of: \"{channel}\" "
                    f"and \"{dict_rev[dictionary[channel]]}\" are identical: {dictionary[channel]}")
            dict_rev[dictionary[channel]] = channel
            hash_val_set.add(dictionary[channel])



