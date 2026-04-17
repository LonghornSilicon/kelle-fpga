"""
Generate test vectors for HLS C++ testbenches from the Python Kelle simulator.

For each representative matmul shape in OPT-125M decode, produces:
  - input.bin     : INT8 activations, row-major
  - weights.bin   : INT4 packed weights (2 per byte), row-major [K][N]
  - expected.bin  : INT32 reference outputs, row-major
  - meta.json     : shape, expected cycle count, energy estimate

Usage:
  python test_vectors/gen_vectors.py
  # Outputs to test_vectors/<shape_name>/
"""

import struct, json, os, math, pathlib

ROOT = pathlib.Path(__file__).parent

# OPT-125M representative shapes (name, M, K, N)
SHAPES = [
    ("qkv_decode",    1,  768, 2304),
    ("attn_score_128",1,   64,  128),
    ("attn_output_128",1,  64,  128),
    ("ffn_up_decode", 1,  768, 3072),
    ("ffn_down_decode",1, 3072, 768),
    ("qkv_prefill32", 32, 768, 2304),
]

RSA_ROWS = RSA_COLS = 32

def expected_cycles(M, K, N):
    return math.ceil(M/RSA_ROWS) * math.ceil(N/RSA_COLS) * (K + RSA_ROWS - 1)

def int4_to_packed(weights_int4):
    """Pack list of int4 values (as python ints, -8..7) into bytes (2 per byte)."""
    assert len(weights_int4) % 2 == 0
    out = bytearray(len(weights_int4) // 2)
    for i in range(0, len(weights_int4), 2):
        lo = int(weights_int4[i]) & 0x0F
        hi = int(weights_int4[i+1]) & 0x0F
        out[i // 2] = lo | (hi << 4)
    return bytes(out)

def generate(name, M, K, N):
    out_dir = ROOT / name
    out_dir.mkdir(exist_ok=True)

    # Deterministic pseudo-random (matches rsa_tb.cpp)
    inp  = [(i * 7 + 3) & 0x7F for i in range(M * K)]   # INT8, 0..127
    wgt_packed_bytes = bytearray((K * N + 1) // 2)
    for i in range(len(wgt_packed_bytes)):
        wgt_packed_bytes[i] = (i * 13 + 5) & 0xFF

    # Unpack weights for reference computation
    def unpack_w(k, n):
        flat = k * N + n
        byte = wgt_packed_bytes[flat // 2]
        nibble = (flat % 2)
        raw = (byte & 0x0F) if nibble == 0 else ((byte >> 4) & 0x0F)
        return raw - 16 if raw >= 8 else raw  # sign-extend 4-bit

    # Reference matmul
    ref = [0] * (M * N)
    for m in range(M):
        for k in range(K):
            a = inp[m * K + k]
            if a >= 64: a -= 128  # sign-extend INT8
            for n in range(N):
                ref[m * N + n] += a * unpack_w(k, n)

    # Write files
    with open(out_dir / "input.bin", "wb") as f:
        f.write(struct.pack(f"{M*K}b", *[x - 256 if x >= 128 else x for x in inp]))
    with open(out_dir / "weights.bin", "wb") as f:
        f.write(bytes(wgt_packed_bytes))
    with open(out_dir / "expected.bin", "wb") as f:
        f.write(struct.pack(f"{M*N}i", *ref))
    with open(out_dir / "meta.json", "w") as f:
        json.dump({
            "name": name, "M": M, "K": K, "N": N,
            "expected_cycles": expected_cycles(M, K, N),
            "rsa_rows": RSA_ROWS, "rsa_cols": RSA_COLS,
        }, f, indent=2)

    print(f"  {name:25s}  M={M:3d} K={K:4d} N={N:4d}  "
          f"cycles={expected_cycles(M,K,N):7d}  "
          f"out_dir={out_dir.relative_to(ROOT.parent)}")

if __name__ == "__main__":
    print("Generating test vectors for Kelle HLS blocks...")
    print(f"  {'Shape':<25}  {'M':>3} {'K':>5} {'N':>5}  {'Cycles':>7}")
    print("  " + "-"*65)
    for args in SHAPES:
        generate(*args)
    print("\nDone. Load these into HLS testbenches with fread().")
