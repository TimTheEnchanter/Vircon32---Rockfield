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
