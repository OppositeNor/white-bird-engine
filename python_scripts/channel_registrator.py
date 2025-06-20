from utils import hash_str

class ChannelRegistrator:
    def __init__(self) -> None:
        self.channels = {}

    def register_channel(self, p_channel_name : str) -> None:
        self.channels[p_channel_name] = hash_str(p_channel_name)

