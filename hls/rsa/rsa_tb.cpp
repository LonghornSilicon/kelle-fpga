#include "rsa.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

// Python timing model: ceil(M/R)*ceil(N/C)*(K+R-1)
static int expected_cycles(int M, int K, int N) {
    int tiles_m = (M + RSA_ROWS - 1) / RSA_ROWS;
    int tiles_n = (N + RSA_COLS - 1) / RSA_COLS;
    return tiles_m * tiles_n * (K + RSA_ROWS - 1);
}

// Reference CPU matmul using INT4 quantised weights
static void ref_matmul(act_t* input, wpacked_t* weights, acc_t* output,
                       int M, int K, int N) {
    memset(output, 0, M * N * sizeof(acc_t));
    for (int m = 0; m < M; m++) {
        for (int k = 0; k < K; k++) {
            act_t a = input[m * K + k];
            for (int n = 0; n < N; n++) {
                int flat = k * N + n;
                wpacked_t packed = weights[flat / 2];
                int8_t w = (flat % 2 == 0) ? (int8_t)(packed & 0x0F) : (int8_t)((packed >> 4) & 0x0F);
                // Sign-extend from 4 bits
                if (w & 0x8) w |= 0xF0;
                output[m * N + n] += (acc_t)a * (acc_t)w;
            }
        }
    }
}

static int run_test(const char* name, int M, int K, int N) {
    printf("\n[TEST] %s: M=%d K=%d N=%d\n", name, M, K, N);

    // Allocate
    act_t*     inp  = new act_t[M * K];
    wpacked_t* wgt  = new wpacked_t[K * N / 2 + 1];
    acc_t*     out  = new acc_t[M * N];
    acc_t*     ref  = new acc_t[M * N];
    rsa_config_t cfg; cfg.M = M; cfg.K = K; cfg.N = N;

    // Fill with deterministic pseudo-random values
    for (int i = 0; i < M * K; i++)        inp[i] = (act_t)((i * 7 + 3) & 0x7F);
    for (int i = 0; i < (K * N + 1) / 2; i++) wgt[i] = (wpacked_t)((i * 13 + 5) & 0xFF);

    // Run reference
    ref_matmul(inp, wgt, ref, M, K, N);

    // Run HLS kernel
    rsa_matmul(inp, wgt, out, cfg);

    // Compare
    int errors = 0;
    for (int i = 0; i < M * N; i++) {
        if (out[i] != ref[i]) {
            if (errors < 5)
                printf("  MISMATCH at [%d]: got %d expected %d\n", i, (int)out[i], (int)ref[i]);
            errors++;
        }
    }

    int cyc = expected_cycles(M, K, N);
    printf("  Expected cycles (Python model): %d\n", cyc);
    printf("  Result: %s (%d/%d elements correct)\n",
           errors == 0 ? "PASS" : "FAIL", M * N - errors, M * N);

    delete[] inp; delete[] wgt; delete[] out; delete[] ref;
    return errors;
}

int main() {
    int total_errors = 0;
    total_errors += run_test("QKV projection (decode)",  1,   768, 2304);
    total_errors += run_test("Attention scores (kv=128)",1,    64,  128);
    total_errors += run_test("FFN up (decode)",          1,   768, 3072);
    total_errors += run_test("QKV projection (prefill)", 32, 768, 2304);

    printf("\n===== RSA testbench: %s =====\n", total_errors == 0 ? "ALL PASS" : "FAILURES DETECTED");
    return total_errors;
}
