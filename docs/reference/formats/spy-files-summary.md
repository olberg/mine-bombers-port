# SPY Files Summary

The 21 SPY images shipped with the original game, and what each contains.
Format details: [SPY Format Analysis](spy-format-analysis.md).

## Screens

| File | Content |
|------|---------|
| TITLEBE.SPY | Title screen — "Mine Bombers" logo |
| MAIN3.SPY | Main menu background — stone panel with button bars, vertical "MINE BOMBERS" text |
| OPTIONS5.SPY | Options menu screen |
| PLAYERS.SPY | Player selection screen |
| LEVSELEC.SPY | Level selection screen |
| IDENTIFW.SPY | Player identification screen |
| SHOPPIC.SPY | Shop screen background |
| KEYS.SPY | Key configuration screen |
| CODES.SPY | Cheat/codes menu |
| FINAL.SPY | Podium screen with slots for player money and scores |
| GAMEOVER.SPY | Single-player game over screen |
| CONGRATU.SPY | Win screen for single-player campaign |
| HALLOFFA.SPY | Hall of Fame (leaderboard) |

## Info Screens

| File | Content |
|------|---------|
| INFO1.SPY | All weapons with names (shop reference?) |
| INFO2.SPY | Credits screen |
| INFO3.SPY | All tools with names (shop reference?) |

## Sprite Sheets

| File | Content |
|------|---------|
| SIKA.SPY | **In-game sprite sheet** ("sika" is Finnish for "pig") — all gameplay sprites: tiles, characters, items, weapons, effects, and the menu cursor (shovel). This is what `load_tile_resources()` loads — string at `0x1030:0x7972` resolves to `sika.spy` (MB.EXE offset 0x01AA73) |
| SHAPET.SPY | Tile type help/reference chart — tile sprites with text descriptions; NOT the gameplay sprite sheet |

## Editor

| File | Content |
|------|---------|
| MINEDIT2.SPY | Map editor screen |
| MINEDI33.SPY | Map editor v3.3 screen |
| EDITHELP.SPY | Tile editor help screen |
