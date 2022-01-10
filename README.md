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

Install SDL2 cmake support:

```bash
git submodule init
git submodule update
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
