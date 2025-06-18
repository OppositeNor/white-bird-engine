from build import WBEBuilder


def register(p_builder : WBEBuilder):
    p_builder.register_channel("WBE_TEST_LABEL")
    p_builder.register_channel("WBE_TEST_LABEL_MACRO")
    p_builder.register_channel("WBE_TEST_LABEL_1")
    p_builder.register_channel("WBE_TEST_LABEL_2")
    p_builder.register_channel("WBE_TEST_LABEL_3")


