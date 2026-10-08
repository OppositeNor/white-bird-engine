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
"""Export the render graphs of a GPU timestamp report as Mermaid flowcharts or SVG images.

The report is the JSON file TimestampQueryPluginVK writes when the renderer terminates (by default
gpu_timestamp_report.json next to the executable). Every selected render graph is drawn the same way in both formats:

- Nodes are tasks, colored by queue (blue for graphics, yellow for compute_1, green for compute_2), labeled with their
  mean GPU time.
- Parallel resource edges between the same two tasks are merged into one arrow. The label lists each resource, marked
  W when the next task writes it and R when it only reads it.
- Solid arrows stay on one queue (pipeline barriers); red dashed arrows cross queues (timeline semaphores).

The mermaid format writes one Markdown section per graph. The svg format lays every graph out with Graphviz (the dot
executable must be on PATH) and writes one SVG file per graph.
"""
import argparse
import json
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


# Fill and stroke colors of each queue name TimestampQueryPluginVK reports, in report order.
QUEUE_COLORS: dict[str, tuple[str, str]] = {
    "graphics": ("#dbeafe", "#1d4ed8"),
    "compute_1": ("#fde68a", "#b45309"),
    "compute_2": ("#bbf7d0", "#15803d"),
}
TEXT_COLOR: str = "#0f172a"
CROSS_QUEUE_COLOR: str = "#b91c1c"


@dataclass
class GraphNode:
    """A task of the drawn graph."""
    node_id: str
    task_id: str
    queue: str
    timing_label: str | None


@dataclass
class GraphArrow:
    """The merged edges between one pair of tasks."""
    from_node_id: str
    to_node_id: str
    resources: list[str]
    cross_queue: bool


@dataclass
class Graph:
    """A render graph ready to be drawn in any format."""
    key: str
    summary: str
    nodes: list[GraphNode]
    arrows: list[GraphArrow]


def node_id(p_task_id: str) -> str:
    """Return an identifier for a task id that is safe in Mermaid and Graphviz."""
    return "task_" + re.sub(r"[^A-Za-z0-9_]", "_", p_task_id)


def format_timing(p_timing: dict[str, Any] | None) -> str:
    """Return the node label line for a task's timing statistics."""
    if p_timing is None:
        return "not sampled"
    return f"{p_timing['mean_ms']:.3f} ms (max {p_timing['max_ms']:.3f})"


def build_graph(p_graph: dict[str, Any], p_include_timing: bool) -> Graph:
    """Merge the parallel edges of a report graph and label its tasks."""
    tasks: list[dict[str, Any]] = p_graph["tasks"]
    edges: list[dict[str, Any]] = p_graph["edges"]
    queue_by_task: dict[str, str] = {task["task_id"]: task["queue"] for task in tasks}
    for task_id, queue in queue_by_task.items():
        if queue not in QUEUE_COLORS:
            raise ValueError(f"Task '{task_id}' of graph '{p_graph['key']}' runs on unknown queue '{queue}'.")

    nodes: list[GraphNode] = [
        GraphNode(
            node_id=node_id(task["task_id"]),
            task_id=task["task_id"],
            queue=task["queue"],
            timing_label=format_timing(task.get("timing")) if p_include_timing else None,
        )
        for task in tasks
    ]

    # Merge parallel edges, keeping the first-declared order of task pairs.
    merged: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for edge in edges:
        merged.setdefault((edge["from"], edge["to"]), []).append(edge)
    arrows: list[GraphArrow] = [
        GraphArrow(
            from_node_id=node_id(from_task),
            to_node_id=node_id(to_task),
            resources=sorted(f'{edge["resource"]} {"W" if edge["to_task_writes"] else "R"}' for edge in pair_edges),
            cross_queue=queue_by_task[from_task] != queue_by_task[to_task],
        )
        for (from_task, to_task), pair_edges in merged.items()
    ]

    queue_counts: list[str] = [
        f"{count} {queue}" for queue in QUEUE_COLORS if (count := sum(1 for node in nodes if node.queue == queue)) > 0]
    cross_queue_count: int = sum(1 for arrow in arrows if arrow.cross_queue)
    summary: str = (
        f"{len(nodes)} tasks ({', '.join(queue_counts)}), {len(edges)} resource "
        f"edges merged into {len(arrows)} arrows, {cross_queue_count} of which cross queues."
    )
    return Graph(key=p_graph["key"], summary=summary, nodes=nodes, arrows=arrows)


def to_mermaid(p_graph: Graph, p_direction: str) -> str:
    """Return the Markdown section holding the Mermaid flowchart of a graph."""
    lines: list[str] = [f"flowchart {p_direction}"]
    for node in p_graph.nodes:
        label: str = node.task_id if node.timing_label is None else f"{node.task_id}<br/>{node.timing_label}"
        lines.append(f'    {node.node_id}["{label}"]:::{node.queue}')
    for arrow in p_graph.arrows:
        lines.append(f'    {arrow.from_node_id} -->|"{"<br/>".join(arrow.resources)}"| {arrow.to_node_id}')
    for queue, (fill, stroke) in QUEUE_COLORS.items():
        lines.append(f"    classDef {queue} fill:{fill},stroke:{stroke},color:{TEXT_COLOR}")
    cross_queue_links: list[str] = [str(index) for index, arrow in enumerate(p_graph.arrows) if arrow.cross_queue]
    if cross_queue_links:
        lines.append(
            f"    linkStyle {','.join(cross_queue_links)} stroke:{CROSS_QUEUE_COLOR},stroke-width:2px,stroke-dasharray:6 4")
    return "\n".join([f"## {p_graph.key}", "", p_graph.summary, "", "```mermaid", *lines, "```", ""])


def dot_string(p_text: str) -> str:
    """Return p_text as a quoted Graphviz string, with \\n kept as a centered line break."""
    return '"' + p_text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def to_dot(p_graph: Graph, p_direction: str) -> str:
    """Return the Graphviz source of a graph."""
    lines: list[str] = [
        f"digraph {dot_string(p_graph.key)} {{",
        f"    graph [rankdir={p_direction}, label={dot_string(p_graph.key + chr(10) + p_graph.summary)}, labelloc=t, "
        'fontname="Helvetica", fontsize=24, nodesep=0.35, ranksep=0.8, bgcolor="white", pad=0.5];',
        f'    node [shape=box, style="rounded,filled", fontname="Helvetica", fontsize=14, fontcolor="{TEXT_COLOR}", '
        "penwidth=1.5];",
        '    edge [fontname="Helvetica", fontsize=10, color="#475569", fontcolor="#334155", arrowsize=0.8];',
    ]
    for node in p_graph.nodes:
        label: str = node.task_id if node.timing_label is None else f"{node.task_id}\n{node.timing_label}"
        fill, stroke = QUEUE_COLORS[node.queue]
        lines.append(f'    {node.node_id} [label={dot_string(label)}, fillcolor="{fill}", color="{stroke}"];')
    for arrow in p_graph.arrows:
        style: str = (
            f', color="{CROSS_QUEUE_COLOR}", fontcolor="{CROSS_QUEUE_COLOR}", style=dashed, penwidth=2'
            if arrow.cross_queue else "")
        lines.append(
            f"    {arrow.from_node_id} -> {arrow.to_node_id} [label={dot_string(chr(10).join(arrow.resources))}{style}];")
    lines.append("}")
    return "\n".join(lines) + "\n"


def render_svg(p_graph: Graph, p_direction: str) -> bytes:
    """Lay a graph out with Graphviz and return the SVG image."""
    dot_path: str | None = shutil.which("dot")
    if dot_path is None:
        raise RuntimeError("The svg format needs the Graphviz dot executable on PATH.")
    result = subprocess.run([dot_path, "-Tsvg"], input=to_dot(p_graph, p_direction).encode("utf-8"), capture_output=True)
    if result.returncode != 0:
        raise RuntimeError(f"Graphviz failed to lay out {p_graph.key}: {result.stderr.decode('utf-8', 'replace')}")
    return result.stdout


def select_graphs(p_report: dict[str, Any], p_graph_keys: list[str]) -> list[dict[str, Any]]:
    """Return the report graphs named by p_graph_keys, or every graph when it is empty."""
    graphs: list[dict[str, Any]] = p_report.get("render_graphs", [])
    if not graphs:
        raise RuntimeError("The report has no render_graphs; it was written by a renderer without graph metadata.")
    if not p_graph_keys:
        return graphs
    known: set[str] = {graph["key"] for graph in graphs}
    missing: list[str] = [key for key in p_graph_keys if key not in known]
    if missing:
        raise RuntimeError(f"The report has no render graph {', '.join(missing)}; it has {', '.join(sorted(known))}.")
    return [graph for graph in graphs if graph["key"] in p_graph_keys]


def svg_output_path(p_output: Path, p_graph_key: str, p_graph_count: int) -> Path:
    """Return the SVG file of a graph: p_output itself for one graph, otherwise suffixed with the graph key."""
    if p_graph_count == 1:
        return p_output
    return p_output.with_name(f"{p_output.stem}_{p_graph_key}{p_output.suffix or '.svg'}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Export the render graphs of a GPU timestamp report.")
    parser.add_argument("report", type=Path, help="The gpu_timestamp_report.json written by TimestampQueryPluginVK.")
    parser.add_argument("-f", "--format", choices=["mermaid", "svg"], default="mermaid",
                        help="mermaid writes Markdown with Mermaid flowcharts; svg writes one Graphviz SVG per graph.")
    parser.add_argument("-o", "--output", type=Path,
                        help="The output file. Optional for mermaid (standard output); required for svg, where several "
                             "graphs are written to <stem>_<graph key>.svg.")
    parser.add_argument("-g", "--graph", action="append", default=[], help="Export only this graph key; may be repeated.")
    parser.add_argument("-d", "--direction", choices=["TB", "LR"], default="TB",
                        help="TB lays the graph out top to bottom, LR left to right (horizontal).")
    parser.add_argument("--no-timing", action="store_true", help="Leave the GPU times out of the task labels.")
    args = parser.parse_args()

    report: dict[str, Any] = json.loads(args.report.read_text(encoding="utf-8"))
    graphs: list[Graph] = [build_graph(graph, not args.no_timing) for graph in select_graphs(report, args.graph)]

    if args.format == "svg":
        if args.output is None:
            raise RuntimeError("The svg format needs --output.")
        for graph in graphs:
            path: Path = svg_output_path(args.output, graph.key, len(graphs))
            path.write_bytes(render_svg(graph, args.direction))
            print(f"Wrote {path}.")
        return

    header: str = (
        f"GPU timestamps of {report['sampled_frames']} sampled frames on {report['device']}.\n"
        if not args.no_timing else "")
    document: str = header + "\n".join(to_mermaid(graph, args.direction) for graph in graphs)
    if args.output is None:
        sys.stdout.write(document)
    else:
        args.output.write_text(document, encoding="utf-8")


if __name__ == "__main__":
    main()
