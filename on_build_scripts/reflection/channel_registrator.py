# Copyright (c) 2025 OppositeNor.
# Licensed under GPL-3.0
from utils import hash_str

class ChannelRegistrator:
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

