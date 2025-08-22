from reflection.reflect import WBEReflector


def register(p_builder : WBEReflector):
    p_builder.register_channel("WBE_CHANNEL_UNKNOWN")
    p_builder.register_channel("WBE_CHANNEL_GLOBAL")
    p_builder.register_channel("WBE_CHANNEL_USER")

