from reflect import WBEBuilder


def register(p_builder : WBEBuilder):
    p_builder.register_channel("WBE_CHANNEL_UNKNOWN")
    p_builder.register_channel("WBE_CHANNEL_GLOBAL")
    p_builder.register_channel("WBE_CHANNEL_USER")

