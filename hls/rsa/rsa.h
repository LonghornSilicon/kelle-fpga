// Weight-stationary 32x32 systolic array for matrix multiply.
// Timing matches Python: ceil(M/ROWS)*ceil(N/COLS)*(K+ROWS-1) cycles at II=1.
#ifndef RSA_H
#define RSA_H

#include <ap_int.h>
#include <hls_stream.h>
#include <stdint.h>

// Array dimensions (match Python HardwareConfig)
#define RSA_ROWS  32
#define RSA_COLS  32

// OPT-125M worst-case dimensions
#define MAX_K     4096
#define MAX_N     4096
#define MAX_M     512    // max prefill length

// Data types
typedef ap_int<4>  weight4_t;   // INT4 weight (sub-byte)
typedef ap_uint<8> wpacked_t;   // two INT4 packed into one byte
typedef ap_int<8>  act_t;       // INT8 activation
typedef ap_int<32> acc_t;       // INT32 accumulator (no overflow for 768-length dot product)

// Interface structs
struct rsa_config_t {
    uint16_t M;   // rows of input (1 for decode, prompt_len for prefill)
    uint16_t K;   // inner dimension (d_model, head_dim, d_ffn)
    uint16_t N;   // output columns
};

// Top-level function
void rsa_matmul(
    act_t     input[MAX_M * MAX_K],    // row-major [M][K]
    wpacked_t weights[MAX_K * MAX_N / 2], // INT4 packed: weights[k*N+n], 2 per byte
    acc_t     output[MAX_M * MAX_N],   // row-major [M][N]
    rsa_config_t cfg
);

#endif
