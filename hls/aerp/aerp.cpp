#include "aerp.h"

aerp_result_t aerp_insert(
    aerp_token_t  tokens[AERP_MAX_TOKENS],
    uint16_t      num_tokens,
    uint16_t      new_token_id,
    uint16_t      new_position,
    uint16_t      capacity,
    uint32_t      edram_budget_bytes,
    uint16_t      victim_id,
    bool          victim_found,
    bool          should_recompute
) {
    #pragma HLS INTERFACE s_axilite port=num_tokens        bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=new_token_id      bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=new_position      bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=capacity          bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=edram_budget_bytes bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=victim_id         bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=victim_found      bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=should_recompute  bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return            bundle=CTRL
    #pragma HLS INTERFACE m_axi port=tokens offset=slave bundle=TOK_MEM

    aerp_result_t result;
    result.evicted_id = 0;
    result.did_evict  = false;
    result.action     = 0;

    // Count CACHED tokens
    uint16_t num_cached = 0;
    count_loop: for (uint16_t i = 0; i < num_tokens; i++) {
        #pragma HLS PIPELINE II=1
        if (tokens[i].valid && tokens[i].status == STATUS_CACHED)
            num_cached++;
    }

    // Tier 1: KV capacity check — evict from CACHED tokens only
    if (num_cached >= capacity && victim_found) {
        // Find and update victim token
        uint16_t victim_slot = 0;
        find_victim: for (uint16_t i = 0; i < num_tokens; i++) {
            #pragma HLS PIPELINE II=1
            if (tokens[i].valid && tokens[i].token_id == victim_id)
                victim_slot = i;
        }
        if (should_recompute) {
            tokens[victim_slot].status   = STATUS_RECOMPUTE;
            tokens[victim_slot].kv_bytes = 0;
            result.action = 2;  // recompute
        } else {
            tokens[victim_slot].valid  = false;
            result.evicted_id = victim_id;
            result.did_evict  = true;
            result.action = 1;  // evicted
        }
    }

    // Tier 2: eDRAM byte budget — purge cheapest RECOMPUTE if overflow
    uint32_t recompute_bytes = 0;
    uint16_t cheapest_slot   = 0;
    bool     cheapest_found  = false;
    // (score tracking omitted here — driven by evictor in real system)
    budget_loop: for (uint16_t i = 0; i < num_tokens; i++) {
        #pragma HLS PIPELINE II=1
        if (tokens[i].valid && tokens[i].status == STATUS_RECOMPUTE)
            recompute_bytes += tokens[i].input_bytes;
    }
    if (recompute_bytes > edram_budget_bytes) {
        // Evict first RECOMPUTE token (simplified: real design uses lowest score)
        purge_loop: for (uint16_t i = 0; i < num_tokens; i++) {
            #pragma HLS PIPELINE II=1
            if (tokens[i].valid && tokens[i].status == STATUS_RECOMPUTE && !cheapest_found) {
                cheapest_slot = i;
                cheapest_found = true;
            }
        }
        if (cheapest_found) {
            tokens[cheapest_slot].valid = false;
            if (!result.did_evict) {
                result.evicted_id = tokens[cheapest_slot].token_id;
                result.did_evict  = true;
                result.action = 3;  // recompute_overflow
            }
        }
    }

    // Insert new token as CACHED
    // Find first invalid slot (or append)
    insert_loop: for (uint16_t i = 0; i <= num_tokens; i++) {
        #pragma HLS PIPELINE II=1
        if (i == num_tokens || !tokens[i].valid) {
            tokens[i].token_id    = new_token_id;
            tokens[i].position    = new_position;
            tokens[i].status      = STATUS_CACHED;
            tokens[i].kv_bytes    = AERP_KV_BYTES;
            tokens[i].input_bytes = AERP_INPUT_BYTES;
            tokens[i].valid       = true;
            break;
        }
    }

    return result;
}
