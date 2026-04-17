#include "rsa.h"

// Unpack one INT4 from a packed byte (nibble = 0 for low, 1 for high)
static weight4_t unpack_int4(wpacked_t packed, int nibble) {
    #pragma HLS INLINE
    if (nibble == 0)
        return (weight4_t)(packed & 0x0F);
    else
        return (weight4_t)((packed >> 4) & 0x0F);
}

// Weight-stationary systolic array matmul.
// Timing: ceil(M/ROWS) * ceil(N/COLS) * (K + ROWS - 1) cycles  [matches Python rsa.py]
void rsa_matmul(
    act_t     input[MAX_M * MAX_K],
    wpacked_t weights[MAX_K * MAX_N / 2],
    acc_t     output[MAX_M * MAX_N],
    rsa_config_t cfg
) {
    #pragma HLS INTERFACE m_axi port=input   offset=slave bundle=ACT_MEM   depth=4096
    #pragma HLS INTERFACE m_axi port=weights offset=slave bundle=WGT_MEM   depth=8388608
    #pragma HLS INTERFACE m_axi port=output  offset=slave bundle=OUT_MEM   depth=2097152
    #pragma HLS INTERFACE s_axilite port=cfg    bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return bundle=CTRL

    // On-chip tile buffers
    act_t  a_buf[RSA_ROWS][MAX_K];
    acc_t  c_buf[RSA_ROWS][RSA_COLS];

    #pragma HLS ARRAY_PARTITION variable=a_buf complete dim=1
    #pragma HLS ARRAY_PARTITION variable=c_buf complete dim=1
    #pragma HLS ARRAY_PARTITION variable=c_buf complete dim=2

    const int M = cfg.M, K = cfg.K, N = cfg.N;

    // Tile loop over output rows (M dimension)
    for (int m0 = 0; m0 < M; m0 += RSA_ROWS) {
        int m_tile = (M - m0 < RSA_ROWS) ? (M - m0) : RSA_ROWS;

        // Load input tile: a_buf[i][k] = input[(m0+i)*K + k]
        load_a: for (int i = 0; i < RSA_ROWS; i++) {
            for (int k = 0; k < K; k++) {
                #pragma HLS PIPELINE II=1
                a_buf[i][k] = (i < m_tile) ? input[(m0 + i) * K + k] : (act_t)0;
            }
        }

        // Tile loop over output columns (N dimension)
        for (int n0 = 0; n0 < N; n0 += RSA_COLS) {
            int n_tile = (N - n0 < RSA_COLS) ? (N - n0) : RSA_COLS;

            // Clear accumulator tile
            clear_c: for (int i = 0; i < RSA_ROWS; i++) {
                #pragma HLS UNROLL
                for (int j = 0; j < RSA_COLS; j++) {
                    #pragma HLS UNROLL
                    c_buf[i][j] = 0;
                }
            }

            // Inner product over K: each cycle one weight column is broadcast.
            // Pipeline II=1 → one K-step per cycle → matches Python timing model.
            k_loop: for (int k = 0; k < K; k++) {
                #pragma HLS PIPELINE II=1
                #pragma HLS DEPENDENCE variable=c_buf inter false

                // Fetch weight column w[k][n0 .. n0+COLS-1], unpacked from INT4
                weight4_t w_col[RSA_COLS];
                #pragma HLS ARRAY_PARTITION variable=w_col complete

                for (int j = 0; j < RSA_COLS; j++) {
                    #pragma HLS UNROLL
                    int n = n0 + j;
                    if (j < n_tile) {
                        int flat  = k * N + n;
                        int byte  = flat / 2;
                        int nib   = flat % 2;
                        w_col[j] = unpack_int4(weights[byte], nib);
                    } else {
                        w_col[j] = 0;
                    }
                }

                // 32×32 PE array: each PE accumulates one output element
                pe_i: for (int i = 0; i < RSA_ROWS; i++) {
                    #pragma HLS UNROLL
                    pe_j: for (int j = 0; j < RSA_COLS; j++) {
                        #pragma HLS UNROLL
                        c_buf[i][j] += (acc_t)a_buf[i][k] * (acc_t)w_col[j];
                    }
                }
            }

            // Write output tile back
            write_c: for (int i = 0; i < RSA_ROWS; i++) {
                for (int j = 0; j < RSA_COLS; j++) {
                    #pragma HLS PIPELINE II=1
                    int m = m0 + i, n = n0 + j;
                    if (i < m_tile && j < n_tile)
                        output[m * N + n] = c_buf[i][j];
                }
            }
        }
    }
}
