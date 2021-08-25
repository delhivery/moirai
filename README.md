---
![Moirae](assets/logo.svg)
---

# Moirae

[Moirae](https://github.com/delhivery/moirai) is a logistics planner with the
objective to automate routing and transportation. It has a comprehensive,
flexible ecosystem of tools, libraries and algorithms that lets developers easily
build, deploy and integrate a routing planner in their own logistics platform.

Moirai was originally designed to automate shipment routing in Delhivery's own
transportation network. However, the system is general enough to be applicable in
a wide variety of other domains, as well.

Moirai provides a stable C++ interface.

## Install

See the [moirai install guide](Install.md) to use a [Docker container](Docker.md)
or [build from source](Build.md).

To install the current release

```bash
layman -a moirai
emerge -av moirai
```

## Architecture

At a high level, moirae consists of a graph library, a stream processor, one or
more solvers and tools to request a particular solver for given dataset.

See the [design document](Architecture.md) for additional details.

## Contribution guidelines

\*\* If you awnt to contribute to moirai, be sure to review the
[contribution guidelines](Contributing.md)

## TODO

- Graph in its own subproject/namespace
- Modules support
