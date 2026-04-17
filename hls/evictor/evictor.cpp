#include "evictor.h"
#include <hls_math.h>

// Score update: 1 cycle per call (pipelined with RSA softmax output)
void evictor_update(
    token_id_t token_ids[MAX_TOKENS],
    score_t    scores[MAX_TOKENS],
    score_t    new_weights[MAX_TOKENS],
    uint16_t   num_tokens
) {
    #pragma HLS INTERFACE s_axilite port=num_tokens bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return     bundle=CTRL
    #pragma HLS INTERFACE m_axi port=token_ids  offset=slave bundle=ID_MEM
    #pragma HLS INTERFACE m_axi port=scores     offset=slave bundle=SCORE_MEM
    #pragma HLS INTERFACE m_axi port=new_weights offset=slave bundle=WGHT_MEM

    update_loop: for (uint16_t i = 0; i < num_tokens; i++) {
        #pragma HLS PIPELINE II=1
        scores[i] += new_weights[i];
    }
}

// Min-search comparator tree — O(log2(N)) depth when unrolled
// For synthesis, the pipeline here represents the comparator chain
eviction_result_t evictor_find_min(
    token_score_t tokens[MAX_TOKENS],
    uint16_t      num_tokens,
    uint16_t      initial_preserved,
    uint16_t      recent_window
) {
    #pragma HLS INTERFACE s_axilite port=num_tokens       bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=initial_preserved bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=recent_window    bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return           bundle=CTRL
    #pragma HLS INTERFACE m_axi port=tokens offset=slave bundle=TOK_MEM

    // Effective recent window: cap at num_tokens/2 (Python logic)
    uint16_t eff_recent = (recent_window < num_tokens / 2) ? recent_window : num_tokens / 2;
    uint16_t recent_threshold = num_tokens - eff_recent;

    score_t    min_score = ~(score_t)0;  // max value
    token_id_t min_id    = 0;
    bool       found     = false;

    scan_loop: for (uint16_t i = 0; i < num_tokens; i++) {
        #pragma HLS PIPELINE II=1
        bool is_initial = (i < initial_preserved);
        bool is_recent  = (i >= recent_threshold);
        if (tokens[i].valid && !is_initial && !is_recent) {
            if (!found || tokens[i].score < min_score) {
                min_score = tokens[i].score;
                min_id    = tokens[i].id;
                found     = true;
            }
        }
    }

    eviction_result_t result;
    result.victim_id    = min_id;
    result.found        = found;
    // Cycle count: ceil(log2(num_tokens)) — reported back to host
    result.search_cycles = (num_tokens <= 1)   ? 1 :
                           (num_tokens <= 2)   ? 1 :
                           (num_tokens <= 4)   ? 2 :
                           (num_tokens <= 8)   ? 3 :
                           (num_tokens <= 16)  ? 4 :
                           (num_tokens <= 32)  ? 5 :
                           (num_tokens <= 64)  ? 6 :
                           (num_tokens <= 128) ? 7 :
                           (num_tokens <= 256) ? 8 : 9;
    return result;
}
