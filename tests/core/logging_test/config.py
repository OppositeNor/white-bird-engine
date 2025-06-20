from reflection.reflect import WBEReflector


def register(p_builder : WBEReflector):
    p_builder.register_channel("WBE_TEST_LABEL")
    p_builder.register_channel("WBE_TEST_LABEL_MACRO")
    p_builder.register_channel("WBE_TEST_LABEL_1")
    p_builder.register_channel("WBE_TEST_LABEL_2")
    p_builder.register_channel("WBE_TEST_LABEL_3")


