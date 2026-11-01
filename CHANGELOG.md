# Changelog

## 1.0 (2026-10-31)

First release.

### Game
- Asteroids-style play: rotate, thrust, fire with auto-fire, and hyperspace.
- A shield on Y, with an energy meter that recharges while it's down.
- Big and small saucers. Small saucers aim at you, more accurately each wave.
- Power-ups dropped by rocks and saucers: spread shot, rapid fire, and a shield refill.
- Waves escalate: large rocks split into three from wave 3, and armored rocks that take two hits appear from wave 4. Each new rule is announced under the wave banner.
- Extra life every 10,000 points, a heartbeat that speeds up during each wave, and wave banners.
- Pause menu with Resume, Options and Quit to Title.
- Title screen, a top-five high score table with initials entry, and an attract mode that replays the best recorded game.

### Options
- Music and effects volume, Easy/Normal/Hard difficulty, and screen shake and screen flashes on or off. All saved to the memory card.
- High scores remember the difficulty they were set on, and are tagged E or H in the table.

### Memory card
- Saves the high score table, the attract demo and the options.
- Never writes to a card that belongs to another game.
- Checksums reject damaged data.
- Loads saves from earlier development versions: an old single high score, or an old table.

### Sound and music
- 19 synthesized sound effects.
- Three chiptune tracks (title, in-game and game over), played by a small note sequencer on the console (`chipmusic.h`). The whole cartridge is about 1.8 MB.

### vector.h
- Glowing vector lines drawn by stretching and rotating a 1-pixel texture strip, with all glows drawn before all sharp lines.
- Shapes, shapes that wrap around screen edges, circles, arcs, dashed lines, convex fills, rectangles, a 20-segment vector font, a global fade, and a camera offset for screen shake.
- Tuned for the Vircon32 CPU: direct GPU port writes when flushing, precomputed font geometry, and bit-operation color math.

### Tools
- `make_sounds.py` and `make_music.py` generate all the audio.
- `tools/headless.cpp` runs a ROM without a window on the official emulator's console code. It supports scripted input, frame and audio capture, memory cards, and reports dropped frames.
- `tools/gen_demo.py` generates the built-in attract demo.
