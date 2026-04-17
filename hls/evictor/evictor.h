// Systolic Evictor: min-search comparator tree for AERP eviction.
// ceil(log2(N)) cycles for min-search. Matches Python SystolicEvictor.
#ifndef EVICTOR_H
#define EVICTOR_H

#include <ap_int.h>
#include <stdint.h>

#define MAX_TOKENS      512   // max KV cache capacity
#define SCORE_BITS      32    // importance score accumulator width

typedef ap_ufixed<SCORE_BITS, 16> score_t;  // 16.16 fixed-point importance score
typedef uint16_t token_id_t;

struct token_score_t {
    token_id_t id;
    score_t    score;
    bool       valid;
    bool       is_initial;   // protected: never evict
    bool       is_recent;    // protected: never evict
};

struct eviction_result_t {
    token_id_t victim_id;
    bool       found;        // false if all tokens are protected
    uint8_t    search_cycles; // ceil(log2(N)) — reported to simulator
};

// Accumulate one attention weight into score registers (1 cycle, pipelined)
void evictor_update(
    token_id_t token_ids[MAX_TOKENS],
    score_t    scores[MAX_TOKENS],
    score_t    new_weights[MAX_TOKENS],
    uint16_t   num_tokens
);

// Find minimum-score non-protected token (ceil(log2(N)) cycles)
eviction_result_t evictor_find_min(
    token_score_t tokens[MAX_TOKENS],
    uint16_t      num_tokens,
    uint16_t      initial_preserved,
    uint16_t      recent_window
);

#endif
