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
