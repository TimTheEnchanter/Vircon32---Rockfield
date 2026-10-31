# Rockfield + vector.h for Vircon32

**Rockfield.v32** is the ready-to-play cartridge. Load it in the Vircon32 emulator.

## Controls

| Input | Action |
|---|---|
| Left / Right | Rotate |
| Up or A | Thrust |
| B | Fire (hold for auto-fire) |
| X | Hyperspace |
| Start | Start game / pause |

## Saving the high score

Insert a memory card and Rockfield saves its high score when a game ends with a new record. The title screen shows the card's status.

- A blank card is claimed with the signature `ROCKFIELD VECTOR V1 `.
- A card that already belongs to another game is never written to.
- The save is three words right after the signature: a format version, the score, and a checksum. A damaged save is ignored and the default of 5000 is used instead.
- A card inserted while the title screen is showing is picked up straight away. If it holds a higher score than the current one, that score is used.

## Files

- `vector.h`: the vector graphics library. Its API is documented at the top of the file.
- `vector_lines.png`: the small texture the library draws its lines from.
- `Rockfield.c`, `Rockfield.xml`: the game source and its ROM definition.
- `make_sounds.py`: synthesizes all 16 sound effects into `sounds/`. It writes `.wav` files to listen to and `.vsnd` files for the ROM. It needs Python 3 with NumPy.
- `sounds/`: the generated sounds.

## Building

With the Vircon32 dev tools on your PATH:

```
python3 make_sounds.py
png2vircon vector_lines.png -o vector_lines.vtex
compile Rockfield.c -o obj/Rockfield.asm
assemble obj/Rockfield.asm -o obj/Rockfield.vbin
packrom Rockfield.xml -o Rockfield.v32
```

The order of the sounds in `Rockfield.xml` must match the `SND_` numbers in `Rockfield.c`. That is the same order as the `SOUNDS` list in `make_sounds.py`.

## Vircon32 C notes

- Arrays are declared as `float[ 10 ] name;`, not `float name[ 10 ];`.
- There is no `? :` operator.
- You can't add to an array name. Pass `&array[ n ]` instead of `array + n`.
