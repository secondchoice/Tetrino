# Tetrino

A tiny, fairly accurate Tetris engine.

## Install

### Terminal version

Compile:

```bash
cmake -Bbuild -S.
cmake --build build --target Tetrino
```

Run (assumes a VT100-compatible terminal):

```bash
./build/Tetrino
```

### SDL2 version

Install SDL2, e.g. using `brew`:

```bash
brew install sdl2
brew install sdl2_ttf
```

Compile `TetrinoSDL`:

```bash
cmake -Bbuild -S.
cmake --build build --target TetrinoSDL
```

Run:

```bash
cd build ; ./TetrinoSDL
```
