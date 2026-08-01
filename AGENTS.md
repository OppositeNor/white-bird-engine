# White Bird Engine Agent Guide

## Project Overview

This is a C++20 engine project.

Follow repository conventions and existing patterns.

---

## General Rules

- Make the smallest change that solves the problem.
- Do not modify unrelated code.
- Do not modify generated files.
- Do not modify dependencies.
- Preserve existing APIs unless explicitly required.

---

## Agent Workflow

For tasks:

1. Use repo-explorer when repository context is unclear.
2. Use investigator for debugging and root cause analysis.
3. Use main agent for implementation decisions.
4. Use validator before considering work complete.

Please utilize them as much as possible.

---

## Architecture

Layer dependency direction:

```

global
function
resource
core
platform

````

Higher layers may depend on lower layers.
Lower layers must not depend on higher layers.

---

## Environment

Use repository-provided scripts directly.

Build:

```sh
python build.py
````

Validation:

```sh
python validation.py
```

Do not:

* Create Python environments.
* Invoke cmake directly.
* Modify environment setup.

---

## Generated Files

Never edit generated files manually.

Generated locations:

```
src/per_target/<target>/generated/
include/per_target/<target>/generated/
```

---

## Special Constraints

Important project-specific constraints:

* No std smart pointers in engine code.
* Respect Rule of 6 for resource-owning classes.
* Public APIs in function/global must not expose templates.
* Follow existing naming and style conventions.
