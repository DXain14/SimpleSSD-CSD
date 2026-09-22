# SimpleSSD CSD Extension

This repository is an unofficial research extension of
[SimpleSSD](https://github.com/SimpleSSD/SimpleSSD), an educational SSD
simulator for storage and full-system evaluations. It adds a simulated
Computational Storage Device (CSD) path to the upstream SimpleSSD 2.0 codebase.

The CSD implementation is a simulator model, not a claim of compatibility with
any particular commercial CSD device or NVMe vendor specification.

## CSD scope

The current CSD prototype provides:

- A vendor-specific NVMe `read_compute` command.
- A simulated SSD-controller processing unit.
- Dense FP16 GEMV with FP32 accumulation and FP32 output.
- A physical flash-array payload store reached through the FTL logical-to-
  physical mapping.
- Configurable internal read bandwidth, compute throughput, matrix size, and
  control-buffer limits.

The complete workload-generator and trace-replayer entry points are provided by
the companion SimpleSSD Standalone CSD extension. See [`CSD_USAGE.md`](CSD_USAGE.md)
for the core configuration and data-path description.

## Build

```bash
cmake -S . -B build \
  -DDRAMPOWER_SOURCE_DIR=/path/to/drampower/src
cmake --build build -j
```

The SimpleSSD core uses DRAMPower for DRAM modeling. The standalone companion
repository bundles a compatible DRAMPower source tree under
`lib/drampower/src`; use that path when the two repositories are checked out
side by side.

The standalone repository contains the end-to-end CSD tests and is the
recommended entry point for reproducing generator, trace, and stress workloads.

## Related work

This extension is related to research on computational-storage acceleration,
including:

> Xiurui Pan, Endian Li, Qiao Li, Shengwen Liang, Yizhou Shan, Ke Zhou,
> Yingwei Luo, Xiaolin Wang, and Jie Zhang. "InstAttention: In-Storage
> Attention Offloading for Cost-Effective Long-Context LLM Inference.”
> 2025 IEEE International Symposium on High Performance Computer Architecture.
> DOI: 10.1109/HPCA61900.2025.00113.

This repository does not implement the complete InstAttention system, including
its attention offload engine, SparF algorithm, GPU-CSD peer-to-peer path, or
KV-cache management system.

## License and provenance

SimpleSSD and this extension are distributed under the GNU GPLv3. See
[`LICENSE`](LICENSE), [`NOTICE.md`](NOTICE.md), and
[`CSD_MODIFICATIONS.md`](CSD_MODIFICATIONS.md) for provenance and third-party
notices.

The original SimpleSSD copyright and license notices remain in the source
files. This repository is not an official SimpleSSD release.

SimpleSSD uses open-source libraries:

- [inih](https://github.com/benhoyt/inih), located at `lib/inih`, under its
  New BSD license.
- [McPAT](https://github.com/HewlettPackard/mcpat), located at `lib/mcpat`;
  see its bundled notices and README.
