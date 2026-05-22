# hyprscreenpicker

A HyprToolkit picker for `xdg-desktop-portal-hyprland` screen sharing.

## Features

- Pick screens, windows, or regions for sharing.
- Uses `hyprctl` for monitor data and `XDPH_WINDOW_SHARING_LIST` for windows.
- Uses `slurp` for region selection when available.
- Supports restore-token opt-in with `--allow-token`.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Requires CMake, a C++23 compiler, `pkg-config`, HyprToolkit, and its runtime dependencies.

## xdg-desktop-portal-hyprland

Set `custom_picker_binary` in your xdph config. By default, XDPH reads
`$XDG_CONFIG_HOME/hypr/xdph.conf`, or `~/.config/hypr/xdph.conf` when
`XDG_CONFIG_HOME` is unset:

```ini
screencopy {
    custom_picker_binary = /home/falk/Work/hyprscreenpicker/build/hyprscreenpicker
}
```

Then restart `xdg-desktop-portal-hyprland`.
