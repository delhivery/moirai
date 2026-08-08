# Changelog

## Unreleased

- Add capacity-aware routing behind `MOIRAI_CAPACITY_ENABLED`, with capacity
  buckets keyed by scheduled edge and concrete departure minute.
- Add owner-aware reservation lifecycle support, including idempotent
  replacement, release, consume, expiry, freeze windows, and original-payload
  persistence for audit/replay.
- Add optional durable capacity state through snapshot files or RocksDB when
  the build finds RocksDB.
- Add bounded candidate path generation for capacity-aware forward, critical,
  and ultimate searches, with cache bypass while capacity checks are active.
- Add rolling-horizon candidate assignment with optional HiGHS support and a
  deterministic greedy fallback.
- Add dual-price and protected-capacity feedback hooks so online routing can
  steer away from scarce or forecast-protected departures.
- Add capacity/protection runtime configuration, developer documentation, and
  focused tests for capacity buckets, reservations, candidate generation,
  rolling assignment, forecast protection, and durable state.

## v1.0.0 - 2026-08-08

- Establish the v1.0.0 release tag on the stable scheduled-routing solver.
- Split route halt processing windows during route expansion: source-side
  loading uses half of the source halt window and target-side unloading uses
  half of the target halt window, rounded up.
- Preserve the existing route expansion behavior of dropping halt centers where
  `loading_allowed` is explicitly `false` before creating N choose 2 edges.
