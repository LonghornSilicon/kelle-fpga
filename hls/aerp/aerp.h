// AERP: Attention-based Eviction and Recomputation Policy.
// Two-tier capacity: (1) KV count <= N', (2) recompute byte budget.
// Matches Python aerp.py logic exactly.
#ifndef AERP_H
#define AERP_H

#include <ap_int.h>
#include <stdint.h>

#define AERP_MAX_TOKENS    512
#define AERP_KV_BYTES      3072   // kv_bytes_per_token_per_layer for OPT-125M INT4
                                   // = 2 * 12 heads * 64 head_dim * (4/8) bytes...
                                   // actually INT4 KV: 2*12*64*0.5 = 768 bytes
#define AERP_INPUT_BYTES   1536   // input_bytes_per_token = d_model*2 = 768*2

typedef ap_uint<2> token_status_t;
#define STATUS_CACHED    0
#define STATUS_RECOMPUTE 1
#define STATUS_EVICTED   2

struct aerp_token_t {
    uint16_t      token_id;
    uint16_t      position;
    token_status_t status;
    uint32_t      kv_bytes;
    uint32_t      input_bytes;
    bool          valid;
};

struct aerp_result_t {
    uint16_t evicted_id;
    bool     did_evict;
    uint8_t  action;     // 0=none 1=evicted 2=recompute 3=recompute_overflow
};

aerp_result_t aerp_insert(
    aerp_token_t  tokens[AERP_MAX_TOKENS],
    uint16_t      num_tokens,
    uint16_t      new_token_id,
    uint16_t      new_position,
    uint16_t      capacity,           // N' (KV capacity)
    uint32_t      edram_budget_bytes, // eDRAM bytes available for recompute vecs
    uint16_t      victim_id,          // from evictor_find_min
    bool          victim_found,
    bool          should_recompute    // from evictor importance_ratio >= 0.5
);

#endif
