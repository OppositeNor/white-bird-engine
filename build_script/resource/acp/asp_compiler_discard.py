from pathlib import Path
from typing import override
from build_script.resource.acp.acp_compiler import ManifestResource, WBEACPCompiler


class WBEACPCompilerDiscard(WBEACPCompiler):
    """ACP compiler that will discard resources by type.

    Attributes: 
        discarded: Resource types that will be discarded.
    """
    def __init__(self, discarded : list[str]) -> None:
        """Constructor.

        Args:
            discarded: Resource types that will be discarded.
        """
        self.discarded = discarded

    @override
    def get_supported_resource_types(self) -> list[str]:
        return self.discarded

    @override
    def compile(self, resource: ManifestResource, manifest_path: Path,
                res_dir: Path, res_output_dir: Path) -> ManifestResource:
        _ = manifest_path
        _ = res_dir
        _ = res_output_dir
        return dict(resource)

