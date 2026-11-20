# Installation

[Documentation index](../README.md) · [User guide](guide.md)

Choose a package from [GitHub Releases](https://github.com/juraj8/jPDF-Desk/releases)
or [build from source](../development/build.md).

## Linux

Install a DEB/RPM package, run an AppImage, or use the standalone executable.
All require a compatible Linux system. Physical printing requires CUPS.

On Ubuntu 22.04 or 24.04 (x86-64), choose the release asset ending in
`-ubuntu22.04.deb`, not the Debian 13 build. In the directory containing the
Ubuntu download, install it with `sudo apt install ./*-ubuntu22.04.deb` so APT
can resolve its dependencies.

## Windows

Run the installer or standalone `.exe`. The installer adds **Open with** and
**Settings → Apps → Default apps** entries without changing your default PDF app.

## macOS

Use the Apple Silicon `.app` bundle. CI bundles are unsigned and not notarized.

## Snap

Install the downloaded or locally built Snap using its actual filename:

```sh
sudo snap install --dangerous ./jpdf-desk_VERSION_amd64.snap
jpdf-desk
```

Connect removable-drive or printing access when needed:

```sh
sudo snap connect jpdf-desk:removable-media
sudo snap connect jpdf-desk:cups
```

Printing requires the CUPS Snap service. Confinement still applies; assets stay
in the Snap's per-user data directory.

## Updates

jPDF Desk checks GitHub for newer stable releases at startup and when you open
**About…**. The check sends the app version, not document data, and does not
install updates. Follow the release link to download and install a new package.
