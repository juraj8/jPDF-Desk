# Installation

[Documentation index](../README.md) · [User guide](guide.md)

Use an available release package or [build from source](../development/build.md).
Not every platform has prebuilt downloads.

## Linux

Install a DEB/RPM package, run an AppImage, or use the standalone executable.
All require a compatible Linux system. Physical printing requires CUPS.

## Windows

Run the installer or standalone `.exe`. The installer adds **Open with** and
**Settings → Apps → Default apps** entries without changing your default PDF app.

## macOS

Use the Apple Silicon `.app` bundle. CI bundles are unsigned and not notarized.

## Snap

Install a locally built Snap using its actual filename:

```sh
sudo snap install --dangerous ./jpdf-desk_1.0.0_amd64.snap
jpdf-desk
```

Connect removable-drive or printing access when needed:

```sh
sudo snap connect jpdf-desk:removable-media
sudo snap connect jpdf-desk:cups
```

Printing requires the CUPS Snap service. Confinement still applies; assets stay
in the Snap's per-user data directory.
