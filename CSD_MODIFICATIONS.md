# CSD Modification Record

This record identifies the modification boundary relative to upstream
SimpleSSD. It is part of the source provenance for the CSD extension.

## Base

- Upstream repository: `SimpleSSD/SimpleSSD`
- Upstream branch: `2.0`
- Base commit: `2be371619a6533821741540ee2d8ce742c2dad2d`
- Working release date: 2026-09-21

## Functional changes

- Added `csd/` configuration, physical payload storage, and processing-unit
  implementation.
- Added CSD configuration parsing and validation.
- Added NVMe vendor-specific `read_compute` dispatch and completion handling.
- Added FTL and PAL payload access required for physical CSD reads.
- Added CSD statistics, payload accounting, and cache restrictions.
- Preserved the original SimpleSSD behavior when CSD is disabled.

The exact file-level change set is recorded by Git.

- Public CSD copyright identifier: `DXain14`.
- Before public release, the release owner should retain an internal record
  mapping this public identifier to the legally authorized human or
  organization owner.
