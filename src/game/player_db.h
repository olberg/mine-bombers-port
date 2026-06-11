#ifndef PLAYER_DB_H
#define PLAYER_DB_H

#include <stdint.h>
#include <stdbool.h>
#include "game/player.h"

#define PLAYER_DB_SLOTS    32
#define PLAYER_RECORD_SIZE 101  /* 0x65 bytes on disk */
#define PLAYER_DB_SIZE     3232 /* 32 x 101 = 0xCA0 */

/* Player record layout decoded from decompiled FUN_1000_19e8 (seg_1000:1053)
 * and FUN_1000_15c7 (seg_1000:867).
 *
 * Name is a Pascal String[24]: byte 0 = length, bytes 1-24 = characters.
 * Stats are 10 x uint32_t (little-endian) at bytes 26-65, accumulated from
 * runtime player data after each round (FUN_1000_15c7).
 * Bytes 66-100: weapon inventory bar data (35 bytes). */
#pragma pack(push, 1)
typedef struct {
    uint8_t  exists;              /* byte 0: 0 = record EXISTS, nonzero = empty
                                     slot. Verified against the shipped
                                     PLAYERS.DAT ("Plr 1"/"Plr 2" have 0x00,
                                     the 30 empty slots 0x01) and
                                     FUN_1000_1892, which prints the name when
                                     byte0 == '\0'. The old comment here had
                                     it inverted. */
    char     name[25];            /* bytes 1-25: Pascal String[24] (name[0]=length, name[1..24]=chars) */
    uint32_t stats[10];           /* bytes 26-65: 10 cumulative stat counters (see STAT_* indices) */
    uint8_t  weapons[35];         /* bytes 66-100: weapon inventory bar data */
} PlayerRecord;
#pragma pack(pop)

/* Stat field indices into PlayerRecord.stats[]. FUN_1000_15c7 adds the
 * player's 40-byte match-stats block dword-for-dword into these
 * (record.stats[i] += block[i]), so the indices ARE the block offsets / 4.
 * Known writers: [0] results screen all players, [1] results
 * screen rank-0 only, [2]+[5] FUN_1000_a17c, [4] treasure pickup.
 * The old names here (rounds/wins/kills/deaths/score/cash/bombs/time)
 * were guesses and several were wrong. */
#define STAT_MATCHES     0   /* matches played (bytes 26-29) */
#define STAT_MATCH_WINS  1   /* matches won    (bytes 30-33) */
#define STAT_ROUNDS      2   /* rounds played  (bytes 34-37) */
#define STAT_ROUND_WINS  3   /* round wins (bytes 38-41): the results screen
                                ASSIGNS the match's round-win count into
                                match-stats dword 3 for every slot
                                (seg_1000:6374-6393), so the record
                                accumulates career round wins at merge.
                                Shown in player-select column A row 4. */
#define STAT_TREASURES   4   /* treasures collected (bytes 42-45) */
#define STAT_MONEY       5   /* money earned, cumulative (bytes 46-49) */
#define STAT_UNKNOWN6    6   /* no identified writer (bytes 50-53) */
#define STAT_UNKNOWN7    7   /* weapons placed (block +0x1C, seg_1000:2781-2791)
                                — writer identified but not yet wired in port */
#define STAT_DEATHS      8   /* deaths (bytes 58-61) — both death paths
                                increment block +0x20: seg_1000:5994-5998
                                and seg_1010:5401-5409 */
#define STAT_TILES_WALKED 9  /* tiles walked (block +0x24): incremented on each
                                tile-center crossing onto a passable tile,
                                FUN_1000_5073 head seg_1000:3351-3359 */

typedef struct {
    PlayerRecord records[PLAYER_DB_SLOTS];
} PlayerDatabase;

/* Initialize all records to defaults (byte0=1 = all slots empty, matching
 * the file the original creates when PLAYERS.DAT is missing). */
void player_db_init_defaults(PlayerDatabase *db);

/* Load player database from file. Returns true on success.
 * If file is missing or wrong size, initializes defaults. */
bool player_db_load(PlayerDatabase *db, const char *path);

/* Save player database to file. Returns true on success. */
bool player_db_save(const PlayerDatabase *db, const char *path);

/* Initialize runtime Player struct from a database record. */
void player_init_from_record(Player *p, const PlayerRecord *rec, int player_num);

/* Initialize runtime Player from a record, also storing the record slot index
 * for later stat persistence. Use this instead of player_init_from_record
 * when you need to update the record after gameplay. */
void player_init_from_record_slot(Player *p, const PlayerRecord *rec, int player_num,
                                  int slot_index);

/* Merge the player's match-stats block into the persistent record:
 * record.stats[i] += p->match_stats[i]. The original calls this ONCE per
 * match (post-match block, seg_1000:7329-7339), multiplayer only — never
 * after individual rounds, and never in single-player.
 * Decompiled ref: FUN_1000_15c7 (seg_1000:867-955). */
void player_db_merge_match_stats(PlayerDatabase *db, const Player *p);

/* Extract player name from a record's Pascal string into a C string.
 * dest must be at least 25 bytes. Returns dest. */
char *player_record_name(char *dest, int dest_size, const PlayerRecord *rec);

/* Cheat code IDs: special player names that grant stat bonuses.
 * Applied at player select time for ANY player (not bot-only).
 * From decompiled FUN_1000_318d (seg_1000:3276). */
typedef enum {
    CHEAT_NONE = 0,
    CHEAT_LOTTERY,   /* cash = 50000 */
    CHEAT_SKITSO,    /* all 27 weapon slots = 50, dig upgrades = 1 */
    CHEAT_RAMBO,     /* health = 32000 */
    CHEAT_INVIS,     /* invisible sprite (copies transparent sprites) */
    CHEAT_PYROMAN,   /* rockets (0xC0) = 1000 */
    CHEAT_MUTATION   /* mutated sprites (copies alternate sprite data) */
} CheatCode;

/* Check if a player name matches a known cheat code.
 * Returns CHEAT_NONE if no match. */
CheatCode player_detect_cheat(const char *name);

/* Apply cheat code stat bonuses to a player. */
void player_apply_cheat(Player *p, CheatCode cheat);

#endif
