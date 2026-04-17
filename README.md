# Kelle-FPGA

Vivado HLS implementation of the Kelle LLM accelerator (MICRO 2025, arXiv 2510.16040).

Target board: **Xilinx ZCU104** (xczu7ev-ffvc1156-2-e, Zynq UltraScale+)
HLS tool: **Vitis HLS 2023.2** (or Vivado HLS 2020.2+)

Reference simulator: [LonghornSilicon/kelle-simulator](https://github.com/LonghornSilicon/kelle-simulator)

## Blocks

| Block | Source | Function |
|-------|--------|----------|
| `rsa` | `hls/rsa/` | 32x32 weight-stationary systolic array (INT4/INT8) |
| `evictor` | `hls/evictor/` | Min-search comparator tree for AERP eviction |
| `aerp` | `hls/aerp/` | Two-tier KV cache capacity + byte-budget management |

## Quick start

### 1. Generate test vectors from Python simulator
```bash
cd ..
python test_vectors/gen_vectors.py
```

### 2. Run C simulation only (fast, no synthesis)
```bash
vitis_hls -f scripts/csim_only.tcl
```

### 3. Full synthesis + co-simulation + IP export
```bash
vitis_hls -f scripts/run_hls.tcl
```

### 4. Open in GUI
```bash
vitis_hls
# File > Open Project > hls_rsa/
```

## Expected results (OPT-125M, ZCU104 @ 300 MHz)

From C simulation (testbench PASS/FAIL):

| Shape | M | K | N | Expected cycles |
|-------|---|---|---|----------------|
| QKV decode | 1 | 768 | 2304 | 57,528 |
| Attn scores kv=128 | 1 | 64 | 128 | 380 |
| FFN up decode | 1 | 768 | 3072 | 76,704 |

After synthesis, compare DSP/BRAM usage against ZCU104 budget:
- DSP target: < 1,728 (ZCU104 total)
- BRAM target: < 312 RAMB36 (for weight buffer + KV cache)

## Project roadmap

- [x] RSA systolic array — HLS csim + synthesis
- [x] Systolic evictor — HLS csim + synthesis
- [x] AERP logic — HLS csim + synthesis
- [ ] kelle_top integration wrapper
- [ ] Vivado block design (PS + PL via AXI)
- [ ] Hardware validation on ZCU104
- [ ] RTL rewrite of RSA critical path
