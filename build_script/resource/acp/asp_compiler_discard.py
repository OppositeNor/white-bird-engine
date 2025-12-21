from typing import override
from build_script.resource.acp.acp_compiler import WBEACPCompiler


class WBEACPCompilerDiscard(WBEACPCompiler):
    """ACP compiler that will discard files.

    Attributes: 
        discarded: The extensions that will be discarded.
    """
    def __init__(self, discarded : list[str]) -> None:
        """Constructor.

        Args:
            discarded: The extensions that will be discarded.
        """
        self.discarded = discarded

    @override
    def get_supported_file_extensions(self) -> list[str]:
        return self.discarded

    @override
    def compile(self, res_path: str) -> None:
        _ = res_path
        pass

