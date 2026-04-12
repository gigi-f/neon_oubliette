# How to Use Tracy Profiler

## 1. Build with Tracy Enabled

Use the following commands to build the engine with profiling enabled:

```zsh
/opt/homebrew/bin/cmake -B build_tracy -DNEON_ENABLE_TRACY=ON
then:
/opt/homebrew/bin/cmake --build build_tracy -j$(sysctl -n hw.ncpu)
```

## 2. Run the Engine

Run the resulting binary:

```zsh
./build_tracy/bin/neon_oubliette
```

The engine will wait for a Tracy connection or buffer data locally.

## 3. Connect the Tracy GUI

run tracy in terminal

to convert the save: tracy-csvexport <filename>
