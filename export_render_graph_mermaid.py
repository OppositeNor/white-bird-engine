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
"""Export the render graphs of a GPU timestamp report as Mermaid flowcharts.

The report is the JSON file TimestampQueryPluginVK writes when the renderer terminates (by default
gpu_timestamp_report.json next to the executable). Every registered render graph becomes one Markdown section holding a
Mermaid flowchart:

- Nodes are tasks, colored by queue (blue for graphics, yellow for compute), labeled with their mean GPU time.
- Parallel resource edges between the same two tasks are merged into one arrow. The label lists each resource, marked
  W when the next task writes it and R when it only reads it.
- Solid arrows stay on one queue (pipeline barriers); red dashed arrows cross queues (timeline semaphores).
"""
import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any


GRAPHICS_CLASS: str = "classDef graphics fill:#dbeafe,stroke:#1d4ed8,color:#0f172a"
COMPUTE_CLASS: str = "classDef compute fill:#fde68a,stroke:#b45309,color:#0f172a"
CROSS_QUEUE_STYLE: str = "stroke:#b91c1c,stroke-width:2px,stroke-dasharray:6 4"


def mermaid_node_id(p_task_id: str) -> str:
    """Return a Mermaid-safe node identifier for a task id."""
    return "task_" + re.sub(r"[^A-Za-z0-9_]", "_", p_task_id)


def format_timing(p_timing: dict[str, Any] | None) -> str:
    """Return the node label line for a task's timing statistics."""
    if p_timing is None:
        return "not sampled"
    return f"{p_timing['mean_ms']:.3f} ms (max {p_timing['max_ms']:.3f})"


def export_graph(p_graph: dict[str, Any], p_include_timing: bool) -> str:
    """Return the Markdown section of one render graph."""
    tasks: list[dict[str, Any]] = p_graph["tasks"]
    edges: list[dict[str, Any]] = p_graph["edges"]
    queue_by_task: dict[str, str] = {task["task_id"]: task["queue"] for task in tasks}

    # Merge parallel edges, keeping the first-declared order of task pairs.
    merged: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for edge in edges:
        merged.setdefault((edge["from"], edge["to"]), []).append(edge)

    lines: list[str] = ["flowchart TD"]
    for task in tasks:
        label: str = task["task_id"]
        if p_include_timing:
            label += "<br/>" + format_timing(task.get("timing"))
        lines.append(f'    {mermaid_node_id(task["task_id"])}["{label}"]:::{task["queue"]}')

    cross_queue_links: list[int] = []
    for link_index, ((from_task, to_task), pair_edges) in enumerate(merged.items()):
        resources: list[str] = sorted(
            f'{edge["resource"]} {"W" if edge["to_task_writes"] else "R"}' for edge in pair_edges)
        label = "<br/>".join(resources)
        lines.append(f'    {mermaid_node_id(from_task)} -->|"{label}"| {mermaid_node_id(to_task)}')
        if queue_by_task[from_task] != queue_by_task[to_task]:
            cross_queue_links.append(link_index)

    lines.append(f"    {GRAPHICS_CLASS}")
    lines.append(f"    {COMPUTE_CLASS}")
    if cross_queue_links:
        lines.append(f"    linkStyle {','.join(str(index) for index in cross_queue_links)} {CROSS_QUEUE_STYLE}")

    graphics_count: int = sum(1 for task in tasks if task["queue"] == "graphics")
    summary: str = (
        f"{len(tasks)} tasks ({graphics_count} graphics, {len(tasks) - graphics_count} compute), {len(edges)} resource "
        f"edges merged into {len(merged)} arrows, {len(cross_queue_links)} of which cross queues."
    )
    return "\n".join([f"## {p_graph['key']}", "", summary, "", "```mermaid", *lines, "```", ""])


def export_report(p_report: dict[str, Any], p_graph_keys: list[str], p_include_timing: bool) -> str:
    """Return the Markdown document of the selected render graphs of a timestamp report."""
    graphs: list[dict[str, Any]] = p_report.get("render_graphs", [])
    if not graphs:
        raise RuntimeError("The report has no render_graphs; it was written by a renderer without graph metadata.")
    if p_graph_keys:
        known: set[str] = {graph["key"] for graph in graphs}
        missing: list[str] = [key for key in p_graph_keys if key not in known]
        if missing:
            raise RuntimeError(f"The report has no render graph {', '.join(missing)}; it has {', '.join(sorted(known))}.")
        graphs = [graph for graph in graphs if graph["key"] in p_graph_keys]
    header: str = (
        f"GPU timestamps of {p_report['sampled_frames']} sampled frames on {p_report['device']}.\n"
        if p_include_timing else "")
    return header + "\n".join(export_graph(graph, p_include_timing) for graph in graphs)


def main() -> None:
    parser = argparse.ArgumentParser(description="Export the render graphs of a GPU timestamp report as Mermaid flowcharts.")
    parser.add_argument("report", type=Path, help="The gpu_timestamp_report.json written by TimestampQueryPluginVK.")
    parser.add_argument("-o", "--output", type=Path, help="The Markdown file to write; standard output when omitted.")
    parser.add_argument("-g", "--graph", action="append", default=[], help="Export only this graph key; may be repeated.")
    parser.add_argument("--no-timing", action="store_true", help="Leave the GPU times out of the task labels.")
    args = parser.parse_args()

    report: dict[str, Any] = json.loads(args.report.read_text(encoding="utf-8"))
    document: str = export_report(report, args.graph, not args.no_timing)
    if args.output is None:
        sys.stdout.write(document)
    else:
        args.output.write_text(document, encoding="utf-8")


if __name__ == "__main__":
    main()
