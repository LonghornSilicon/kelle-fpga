#include "aerp.h"
#include <cstdio>
#include <cstring>

int main() {
    aerp_token_t tokens[AERP_MAX_TOKENS];
    memset(tokens, 0, sizeof(tokens));

    // Pre-fill 128 CACHED tokens (at capacity N'=128)
    for (int i = 0; i < 128; i++) {
        tokens[i].token_id    = i;
        tokens[i].position    = i;
        tokens[i].status      = STATUS_CACHED;
        tokens[i].kv_bytes    = AERP_KV_BYTES;
        tokens[i].input_bytes = AERP_INPUT_BYTES;
        tokens[i].valid       = true;
    }

    // Insert token 128: capacity is full, victim=50, low importance → full eviction
    aerp_result_t r1 = aerp_insert(tokens, 128, 128, 128, 128,
                                    (4*1024*1024 - 128*AERP_KV_BYTES),
                                    50, true, false);
    printf("Test 1 (full eviction): action=%d evicted=%d  [expect action=1 evicted=50]\n",
           (int)r1.action, (int)r1.evicted_id);

    // Insert token 129: victim=60, high importance → recompute
    aerp_result_t r2 = aerp_insert(tokens, 129, 129, 129, 128,
                                    (4*1024*1024 - 128*AERP_KV_BYTES),
                                    60, true, true);
    printf("Test 2 (recompute):     action=%d             [expect action=2]\n",
           (int)r2.action);

    bool pass = (r1.action == 1 && r1.evicted_id == 50 && r2.action == 2);
    printf("Result: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
