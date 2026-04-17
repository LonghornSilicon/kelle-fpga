# Kelle-FPGA Architecture

## Design approach

HLS-first: Vitis HLS C++ -> synthesised RTL -> Vivado integration.
The Python cycle-accurate simulator (kelle-simulator repo) is the
timing oracle. All HLS testbenches validate against its output.

Once synthesis results are available, hand-written RTL will replace
the HLS-generated output for the RSA critical path (15-30% better
area/power expected).

## RSA block (`hls/rsa/`)

32x32 weight-stationary systolic array.

**Timing model** (matches `rsa.py:_compute_cycles`):
```
cycles = ceil(M/32) * ceil(N/32) * (K + 31)
```
At II=1 on the K-loop, this is exact.

**Key HLS pragmas:**
- `#pragma HLS PIPELINE II=1` on `k_loop` -- one K-step per cycle
- `#pragma HLS UNROLL` on `pe_i` and `pe_j` -- all 1,024 PEs active simultaneously
- `#pragma HLS ARRAY_PARTITION complete` on `c_buf` -- all accumulators accessible in one cycle

**Expected synthesis (ZCU104 @ 300 MHz):**
- DSPs: ~1,024 (one per PE, fused MAC)
- BRAMs: ~32 for weight tile buffer
- Latency (QKV decode, M=1 K=768 N=2304): 57,528 cycles = 191 us

**INT4 packing:** Two INT4 weights packed per byte (low nibble first).
`unpack_int4()` extracts and sign-extends at load time.

## Evictor block (`hls/evictor/`)

Linear scan pipelined at II=1 -> O(N) cycles but with II=1 throughput.
Post-synthesis comparator tree depth = ceil(log2(N)) levels.

In hardware, the evictor runs in parallel with the RSA (5-7% overhead
per the Kelle paper Section 6). The scan loop here models this.

## AERP block (`hls/aerp/`)

Three sequential pipelined scans:
1. Count CACHED tokens
2. Find+update victim (Tier 1: KV capacity)
3. Count RECOMPUTE bytes + purge if over budget (Tier 2)
4. Insert new token

All loops at II=1. Total latency scales with num_tokens (~128 cycles
for N'=128, dominated by the scan loops).

## C-simulation results (native g++, validated 2026-04-17)

All three blocks pass without Vitis HLS. Run via `make csim` using `hls_stubs/` headers.

| Block | Test | Status | Notes |
|-------|------|--------|-------|
| RSA | QKV decode M=1,K=768,N=2304 | **PASS** | 57,528 cycles matches Python |
| RSA | Attn scores M=1,K=64,N=128 | **PASS** | 380 cycles matches Python |
| RSA | FFN up M=1,K=768,N=3072 | **PASS** | 76,704 cycles matches Python |
| RSA | QKV prefill M=32,K=768,N=2304 | **PASS** | 57,528 cycles matches Python |
| Evictor | N=128, victim=token55 | **PASS** | 7 cycles = ceil(log₂128) |
| AERP | Tier-1 full eviction | **PASS** | action=1, evicted=50 |
| AERP | Tier-1 recompute | **PASS** | action=2 |

INT4 sign-extension verified: `ap_int<4>` stubs correctly sign-extend nibble values ≥ 8,
matching the reference CPU matmul in `rsa_tb.cpp`.

## Pending: Vitis HLS synthesis

Blocked on: **Vitis HLS 2023.2** installation (~15 GB, Zynq UltraScale+ MPSoC device only).

Once installed, run:
```bash
vitis_hls -f scripts/csim_only.tcl   # C-sim under actual HLS (validates pragmas parse)
vitis_hls -f scripts/run_hls.tcl     # full synthesis + co-sim + IP export
```

Expected outputs:
- RSA: DSP count (target ~1,024), BRAM for weight tile, timing at 300 MHz
- Whether `#pragma HLS PIPELINE II=1` on `k_loop` closes — if not, add one register stage
- Actual LUT/FF numbers to verify ZCU104 fit

## Pending: Target board

**ZCU104** (xczu7ev-ffvc1156-2-e) — Zynq UltraScale+ EV, 300 MHz PL.
Board is not yet in hand. When available, the flow is:
1. Vivado block design: PS + PL connected via AXI4-Lite (control) + AXI4 (data)
2. Bitstream generation from the three IP blocks exported by `run_hls.tcl`
3. Hardware validation via Jupyter/PYNQ or bare-metal C on the A53

## Clock target

300 MHz (3.33 ns period). ZCU104 PL fabric comfortably achieves this
with BRAM and DSP-based datapaths. The RSA MAC chains are the
critical path -- if timing fails, reduce to 250 MHz or pipeline the
MAC with one additional register stage.

## ZCU104 resource budget (OPT-125M INT4)

| Resource | RSA | Evictor | AERP | Budget |
|----------|-----|---------|------|--------|
| DSP48 | ~1024 | 0 | 0 | 1,728 |
| BRAM36 | ~32 | ~1 | ~2 | 312 |
| LUT | ~5K | ~2K | ~3K | 504K |

Weight prefetch buffer (45 MB INT4): needs ~1,000 RAMB36 -- **exceeds ZCU104**.
Practical limit: ~20 MB on ZCU104. Use INT4 + smaller model or Alveo U50 for full OPT-125M.
