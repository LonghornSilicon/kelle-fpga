#include "evictor.h"
#include <cstdio>
#include <cmath>

int main() {
    const int N = 128;
    token_score_t tokens[MAX_TOKENS] = {};

    // Set up 128 tokens: first 10 initial, last 64 recent, middle evictable
    for (int i = 0; i < N; i++) {
        tokens[i].id       = i;
        tokens[i].score    = (score_t)(100 - i);  // token 0 = highest score
        tokens[i].valid    = true;
        tokens[i].is_initial = (i < 10);
        tokens[i].is_recent  = (i >= 64);
    }
    // Make token 55 the clear minimum in evictable range [10,64)
    tokens[55].score = (score_t)1;

    eviction_result_t r = evictor_find_min(tokens, N, 10, 64);

    printf("Victim: token %d (score=%d)\n", (int)r.victim_id, (int)tokens[r.victim_id].score.to_int());
    printf("Search cycles: %d (expected: %d)\n", (int)r.search_cycles, (int)ceil(log2(N)));
    printf("Found: %s\n", r.found ? "yes" : "no");

    bool pass = r.found && r.victim_id == 55 && r.search_cycles == 7;
    printf("Result: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
