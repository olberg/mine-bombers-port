# Mine Bombers Port — Knowledge Base

Documentation for the port/rebuild of **Mine Bombers 3.11** (1995-2001, Skitso Productions) from DOS to C99 + Raylib.

New to the codebase? Start with the **[Architecture overview](architecture.md)**.

## Reference

Core reverse-engineering documentation extracted from the decompiled original.

- **[Formats](reference/formats/file-formats.md)** — Binary file format specs (.SPY, .MNE, .VOC, .DAT, etc.)
- **[Rendering](reference/rendering/graphics-pipeline.md)** — VGA Mode X pipeline, sprite blitting, palette management
- **[Gameplay](reference/gameplay/game-mechanics.md)** — Maps, tile types, weapons, scoring, economy
- **[Input](reference/input/input-system.md)** — Keyboard/joystick architecture and port abstraction
- **[AI](reference/ai/ai-behavior.md)** — Monster AI pathfinding, combat logic, bot personalities
- **[Audio](reference/audio/music-system.md)** — S3M music playback and scheduling
- **[Testing](reference/testing/dosbox-setup.md)** — Running the original in DOSBox for side-by-side fidelity comparison
