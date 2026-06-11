#ifndef RESULTS_H
#define RESULTS_H

#include "game/player.h"

typedef enum {
    RESULTS_ACTIVE,
    RESULTS_DONE
} ResultsResult;

/* Initialize results screen with end-of-match scoring data. The original
 * shows this once per MATCH (post-match block, seg_1000:7323-7325), not
 * after every round. */
void results_init(const Player players[], int num_players);

/* Apply the results screen's match-stats writes: matches played +1 for
 * all players, matches won +1 for the rank-0 player (seg_1000:6093/6115/
 * 6157). Call once per completed (non-aborted) MP match, before merging
 * records via player_db_merge_match_stats. */
void results_accumulate_match_stats(Player players[], int num_players);

/* Rank = number of players with a strictly higher metric; rank+1 ==
 * num_players → 3. Metric switches on the winner-by option
 * (option_toggle[3]): 0 = wallet (FUN_1000_96c6), nonzero = round wins
 * (FUN_1000_9640). 0=winner, 1/2=draw, 3=loser. */
int results_compute_rank(int player_idx, const Player players[],
                         int num_players);

/* Update results screen. Returns RESULTS_DONE when user dismisses. */
ResultsResult results_update(void);

/* Draw the results screen. */
void results_draw(void);

/* Clean up results screen resources. */
void results_cleanup(void);

#endif
