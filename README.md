---
![Moirae](assets/logo.svg)
---

# Moirae

[Moirae](https://github.com/delhivery/moirae) is a logistics planner with the
objective to automate routing and transportation. It has a comprehensive,
flexible ecosystem of tools, libraries and algorithms that lets developers easily
build, deploy and integrate a routing planner in their own logistics platform.

Moirae was originally designed to automate shipment routing in Delhivery's own
transportation network. However, the system is general enough to be applicable in
a wide variety of other domains, as well.

Moirae provides a stable C++ interface.

## Install

See the [install guide](Install.md) to use a docker container] or
[build from source](Build.md).

To install the current release

```bash
layman -a moirae
emerge -av moirae
```

## Architecture

At a high level, moirae consists of a graph library, a stream processor, one or
more solvers and tools to request a particular solver for given dataset.

See the [design document](Architecture.md) for additional details.

## Contribution guidelines

**If you want to contribute to moirae, be sure to review the
[contribution guidelines](Contributing.md)**

## Available Solvers

- Planner
  - Uncapacitated
    - Single source, single target shortest path
    - Pareto optimal single source, single target shortest paths
    - Single source, single target critical path
  - Capacitated
    - Multiple source, multiple sink shortest paths
- Executors
  - Greedy prioritization
  - VRP with pickups, drops and time windows

## TODO

- Graph in its own subproject/namespace
- Modules support
