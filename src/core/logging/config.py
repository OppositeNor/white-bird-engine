# Copyright (c) 2025 OppositeNor.
# Licensed under GPL-3.0
from reflection.reflect import WBEReflector

# TODO: replace with reflection

def register(p_builder : WBEReflector):
    p_builder.register_channel("WBE_CHANNEL_UNKNOWN")
    p_builder.register_channel("WBE_CHANNEL_GLOBAL")
    p_builder.register_channel("WBE_CHANNEL_USER")

