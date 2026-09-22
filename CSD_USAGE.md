# SimpleSSD CSD Usage

This document describes the CSD extension in this repository. It is intended
for simulator experiments and does not describe a production CSD device.

The SimpleSSD core uses DRAMPower for DRAM modeling. A standalone SimpleSSD
checkout must provide a compatible DRAMPower source directory through
`DRAMPOWER_SOURCE_DIR`; the companion standalone repository bundles one under
`lib/drampower/src`.

```bash
cmake -S . -B build \
  -DDRAMPOWER_SOURCE_DIR=/path/to/drampower/src
cmake --build build -j
```

## Supported operation

The current prototype adds a vendor-specific NVMe `read_compute` command for
dense GEMV:

```text
y = A * x
```

- Matrix `A` is stored as row-major FP16 payload bytes.
- Vector `x` is supplied in the command control buffer as FP16.
- The processing unit accumulates in FP32.
- Output `y` is written back as FP32.
- Matrix data is read through the FTL mapping and the simulated physical
  flash-array payload store.

The implementation does not provide complete attention offload, KV-cache
management, SparF, or GPU-CSD peer-to-peer DMA.

## Configuration

Add or update the `[csd]` section in the SSD configuration:

```ini
[csd]
Enable = 1
ReadComputeOpcode = 0xC0
PUComputeGFLOPS = 13.3
InternalReadBandwidth = 11200000000
MaxMatrixBytes = 268435456
MaxControlBytes = 67108864
RequireICLCacheOff = 1
```

When CSD is enabled, both ICL caches must be disabled:

```ini
[icl]
EnableReadCache = 0
EnableWriteCache = 0
```

The restriction preserves the payload-accurate data path: ordinary NVMe WRITE
payload bytes must reach the simulated physical flash array before
`read_compute` reads them.

## Data path

```text
NVMe WRITE
  -> host DMA
  -> HIL/ICL/FTL
  -> physical flash-array payload store

read_compute
  -> descriptor and vector DMA
  -> FTL logical-to-physical mapping
  -> physical matrix payload read
  -> FP16-to-FP32 GEMV
  -> FP32 output DMA
```

The standalone companion repository provides the request generator, trace
replayer, direct NVMe tests, negative tests, and stress tests.

## License

This extension is based on SimpleSSD and is distributed under GPLv3. Preserve
the original copyright and third-party notices when redistributing it.
