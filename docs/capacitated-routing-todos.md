# Capacitated Routing Roadmap

This is the engineering roadmap for capacity-aware shipment routing in Moirae. The goal is simple: route each shipment/meta-shipment through the scheduled network without violating SLA when feasible,
while respecting edge capacity and minimizing hop count.

## Target Formulation

This roadmap implements the practical path-based scheduled service-network assignment approach:

- choose exactly one feasible path per shipment/meta-shipment
- respect capacity on every scheduled edge departure selected by those paths
- minimize lexicographically:
  SLA breach/lateness, then hop count, then slack/travel/wait-time tie-breakers

The implementation is intentionally near-real-time: it optimizes over the
current rolling horizon and generated candidate set. It does not claim global
optimality over all future shipments or all possible paths.

The checked items describe the current implementation slice. Moirae now has
online capacity checks, owner reservations, bounded candidate generation,
bounded rolling-horizon assignment, optional RocksDB state, optional HiGHS
solves, price feedback hooks, and simple forecast protection. Full global
exactness, branch-and-price, and dynamic discretization engines remain future
research work.

## 1. Capacity Ingestion + RocksDB State

- [x] Add capacity buckets keyed by scheduled edge and concrete departure time.
- [x] Ingest capacity records from the existing input stream.
- [x] Support capacity records by numeric edge id or route edge code.
- [x] Support total, reserved, consumed, protected, and price fields.
- [x] Persist capacity state through snapshot files.
- [x] Persist capacity state through RocksDB when compiled with RocksDB support.
- [x] Restore buckets before reservations during startup replay.
- [x] Decide whether incoming capacity records are always full snapshots or may be deltas.
- [x] Add a dedicated replay/rebuild mode for local RocksDB state.
- [x] Add invariants for invalid buckets such as `reserved + consumed > capacity`.

## 2. Reservation Lifecycle Hardening

- [x] Track reservations by stable owner id.
- [x] Make same-owner re-reservation idempotent.
- [x] Atomically replace an owner's prior reservation when recomputing a path.
- [x] Release reservations by owner.
- [x] Consume reservations by owner.
- [x] Persist and restore owner reservations.
- [x] Wire actual shipment/trip lifecycle events to release or consume reservations.
- [x] Expire stale reservations outside the mutable horizon.
- [x] Preserve original shipment/meta-shipment payloads for recomputation and audit.
- [x] Add duplicate-message and out-of-order-message tests.

## 3. Candidate Path Generation

- [x] Generate candidate paths from the existing scheduled graph solver.
- [x] Generate alternatives by excluding edges from already discovered paths.
- [x] Carry concrete capacity instances on each candidate path.
- [x] Bypass path-cache lookups when capacity-aware routing is enabled.
- [x] Add tests for direct and alternate candidate generation.
- [ ] Replace the compact edge-exclusion alternative search with bounded
  K-shortest path generation.
- [x] Generate candidate sets for critical/ultimate paths, not only forward earliest paths.
- [ ] Add candidate diversity controls by route, hub, departure day, and slack.
- [x] Add hard caps and metrics for candidate-generation latency.

## 4. Rolling-Horizon HiGHS Optimizer

- [x] Add a path-based assignment optimizer over generated candidates.
- [x] Use HiGHS when available.
- [x] Fall back to a deterministic greedy optimizer when HiGHS is unavailable or incomplete.
- [x] Enforce edge-departure capacity constraints inside the candidate assignment.
- [x] Optimize over bounded runtime batches.
- [x] Add `MOIRAI_ROLLING_HORIZON_MAX_BATCH_SIZE`.
- [x] Add `MOIRAI_ROLLING_HORIZON_WINDOW_MS`.
- [x] Commit optimizer-selected reservations.
- [x] Add tests where the horizon assignment gives scarce capacity to the SLA-constrained shipment.
- [x] Model the lexicographic objective explicitly: SLA feasibility, then hop count, then tie-breakers.
- [x] Add tests proving the lexicographic ordering cannot trade an SLA-feasible lower-hop path for a higher-hop path unless required by capacity.
- [ ] Add cross-batch reoptimization for uncommitted shipments.
- [x] Freeze near-departure decisions so late reoptimization cannot invalidate operationally committed paths.
- [x] Add infeasibility handling when no candidate can satisfy capacity.

## 5. Dual-Price Feedback Into Online Solver

- [x] Store scarcity prices on capacity buckets.
- [x] Feed capacity prices into online path scoring.
- [x] Feed protected-capacity penalties into online path scoring.
- [x] Keep physical ETA/ETD timestamps separate from queue objective scores.
- [x] Write optimizer-derived prices back into the capacity store.
- [x] Add tests showing price steering away from scarce direct capacity.
- [ ] Derive prices from LP relaxation duals where useful.
- [x] Smooth price updates across horizons to avoid oscillation.
- [ ] Add price metrics by edge, route, lane, and departure bucket.
- [x] Evaluate whether prices should influence critical/ultimate searches differently from earliest searches.

## 6. Forecast / Protection Layer

- [x] Support explicit protected capacity on capacity records.
- [x] Support forecast aliases: `forecast_protected`, `forecast_units`, and `forecast_demand`.
- [x] Add configurable reserve-ratio protection.
- [x] Add min/max protected-unit clamps.
- [x] Prevent normal searches from consuming protected capacity by default.
- [x] Allow protected-capacity override when explicitly configured.
- [x] Add tests for explicit and policy-derived protected capacity.
- [ ] Replace static protection ratios with lane/time/destination-specific protection.
- [ ] Add historical demand features by edge departure, day of week, destination, and cutoff bucket.
- [ ] Add scenario-based protection for future critical shipment bursts.
- [ ] Track protection quality beyond current capacity-saved/used/deferral
  counters: capacity wasted and SLA saves still need production attribution.

## Later Research Tracks

- [ ] Column generation: use the current solver as the pricing problem and the HiGHS assignment as the restricted master.
- [ ] Branch-and-price for bounded offline windows where exactness matters.
- [ ] Dynamic discretization discovery for larger planning/replanning windows.
- [ ] Local-search or matheuristic neighborhoods for hot lanes and near-SLA shipments.
