#include "game/player_db.h"
#include "game/player.h"
#include "game/config.h"
#include <stdio.h>
#include <string.h>

void player_db_init_defaults(PlayerDatabase *db)
{
    memset(db, 0, sizeof(*db));
    for (int i = 0; i < PLAYER_DB_SLOTS; i++) {
        db->records[i].exists = 1;
    }
}

bool player_db_load(PlayerDatabase *db, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        player_db_init_defaults(db);
        return false;
    }

    /* Check file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size != PLAYER_DB_SIZE) {
        fclose(f);
        player_db_init_defaults(db);
        return false;
    }

    fseek(f, 0, SEEK_SET);
    size_t read = fread(db->records, PLAYER_RECORD_SIZE, PLAYER_DB_SLOTS, f);
    fclose(f);

    if (read != PLAYER_DB_SLOTS) {
        player_db_init_defaults(db);
        return false;
    }

    return true;
}

bool player_db_save(const PlayerDatabase *db, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;

    size_t written = fwrite(db->records, PLAYER_RECORD_SIZE, PLAYER_DB_SLOTS, f);
    fclose(f);

    return written == PLAYER_DB_SLOTS;
}

/* Extract player name from Pascal String[24] format.
 * rec->name[0] = length byte, rec->name[1..24] = characters.
 * Copies into a C string (null-terminated, max 20 chars for Player.name). */
static void extract_pascal_name_impl(char *dest, int dest_size, const char *pascal_str)
{
    int name_len = (uint8_t)pascal_str[0];
    if (name_len > 24) name_len = 24;
    if (name_len > dest_size - 1) name_len = dest_size - 1;
    memcpy(dest, &pascal_str[1], name_len);
    dest[name_len] = '\0';

    /* Trim trailing spaces */
    for (int i = name_len - 1; i >= 0; i--) {
        if (dest[i] == ' ' || dest[i] == '\0')
            dest[i] = '\0';
        else
            break;
    }
}

void player_init_from_record(Player *p, const PlayerRecord *rec, int player_num)
{
    player_init_defaults(p, player_num);

    /* Starting wallet: the original seeds it in
     * process_menu_selection (seg_1010:7169-7183), which runs at PLAY
     * *before* player select — MP gets the Starting Cash option
     * (sign-extended into the 32-bit wallet; factory default 750),
     * SP gets a fixed 250 (0xFA) regardless of the option. Applied
     * here, before the cheat check, so a Lottery name still overrides
     * it — same ordering as the original. */
    p->cash = (g_config.num_players < 2) ? 250 : g_config.starting_cash;

    extract_pascal_name_impl(p->name, sizeof(p->name), rec->name);

    /* Check for cheat code names and apply bonuses (all players, not bot-only) */
    CheatCode cheat = player_detect_cheat(p->name);
    if (cheat != CHEAT_NONE) {
        player_apply_cheat(p, cheat);
    }
}

void player_init_from_record_slot(Player *p, const PlayerRecord *rec, int player_num,
                                  int slot_index)
{
    player_init_from_record(p, rec, player_num);
    p->record_slot = (int16_t)slot_index;
}

void player_db_merge_match_stats(PlayerDatabase *db, const Player *p)
{
    /* FUN_1000_15c7 (seg_1000:917-928): record.stats[i] += match-stats
     * block dword i, for all 10 dwords, once per match (MP only). The
     * original also recomputes an anti-tamper checksum byte over the
     * record (seg_1000:929-940) — not yet ported; whether anything
     * verifies it is an open question for the .DAT round-trip phase. */
    if (p->record_slot < 0 || p->record_slot >= PLAYER_DB_SLOTS) return;

    PlayerRecord *rec = &db->records[p->record_slot];
    for (int i = 0; i < 10; i++) {
        rec->stats[i] += p->match_stats[i];
    }
}

char *player_record_name(char *dest, int dest_size, const PlayerRecord *rec)
{
    extract_pascal_name_impl(dest, dest_size, rec->name);
    return dest;
}

CheatCode player_detect_cheat(const char *name)
{
    if (!name) return CHEAT_NONE;

    /* Case-insensitive comparison against known cheat code names */
    static const struct { const char *name; CheatCode id; } cheats[] = {
        { "Lottery",  CHEAT_LOTTERY },
        { "Skitso",   CHEAT_SKITSO },
        { "Rambo",    CHEAT_RAMBO },
        { "Invis",    CHEAT_INVIS },
        { "Pyroman",  CHEAT_PYROMAN },
        { "Mutation", CHEAT_MUTATION },
    };

    for (int i = 0; i < 6; i++) {
        const char *a = name;
        const char *b = cheats[i].name;
        bool match = true;
        while (*a && *b) {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca += 32;
            if (cb >= 'A' && cb <= 'Z') cb += 32;
            if (ca != cb) { match = false; break; }
            a++; b++;
        }
        if (match && *b == '\0') return cheats[i].id;
    }

    return CHEAT_NONE;
}

void player_apply_cheat(Player *p, CheatCode cheat)
{
    switch (cheat) {
    case CHEAT_LOTTERY:
        p->cash = 50000;
        break;
    case CHEAT_SKITSO:
        /* FUN_1000_302f (seg_1000:1962-1968): set all weapon slots to 50,
         * then override offsets 0xD2=1, 0xD4=1, 0xD6=1 (dig upgrade counts). */
        for (int i = 0; i < WEAPON_CYCLE_USED; i++) {
            p->weapons[i] = 50;
        }
        p->rockpick = 1;
        p->lg_rockpick = 1;
        p->powerdrill = 1;
        p->bonus_stat = player_dig_upgrade_bonus(p);
        break;
    case CHEAT_RAMBO:
        p->health = 32000;
        p->max_health = 32000;
        break;
    case CHEAT_INVIS:
        /* Original (FUN_1000_3095, seg_1000:1984-2014) copies floor sprite
         * (DAT_1038_0494 at sheet 0,0) to all 16 player sprite slots, making
         * the player blend into the background. Port: skip drawing entirely. */
        p->cheat_visual = CHEAT_INVIS;
        break;
    case CHEAT_PYROMAN:
        /* 1000 rockets — decompiled sets offset 0xC0 = 1000 (seg_1000:2022) */
        {
            int idx = player_weapon_index(WEAPON_ROCKET_DIR);
            if (idx >= 0) p->weapons[idx] = 1000;
        }
        break;
    case CHEAT_MUTATION:
        /* Original (FUN_1000_3129, seg_1000:2028-2038) copies monster sprite
         * set 11 (DAT_1038_2352 at Y=70,X=160) into the player's sprite data.
         * DAT_1038_2392 is a pre-made copy of 2352 (seg_1010:4873).
         * Port: render using monster sprite set index 10 (Mon 3). */
        p->cheat_visual = CHEAT_MUTATION;
        break;
    default:
        break;
    }
}
