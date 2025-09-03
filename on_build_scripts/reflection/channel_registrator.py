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
from utils import hash_str

class WBEChannelRegistrator:
    """Channel registrator. Used to keep track on the registrated channels.

    Attributes: 
        channels: The channels that are registered.
    """

    def __init__(self) -> None:
        self.channels = {}

    def register_channel(self, p_channel_name : str) -> None:
        """Register a channel.

        Args:
            p_channel_name: The channel to register.
        """
        self.channels[p_channel_name] = hash_str(p_channel_name)

