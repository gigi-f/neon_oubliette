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

1. Download or build the Tracy Profiler GUI (available at [github.com/wolfpld/tracy](https://github.com/wolfpld/tracy)).
2. Launch the `tracy` (or `Tracy.exe`) application.
3. Click **Connect** (the engine should appear in the list if running on the same machine).

> [!TIP]
> Use the **Statistics** window in Tracy to identify which ECS systems are consuming the most frame time.
