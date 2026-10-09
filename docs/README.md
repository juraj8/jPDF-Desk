# Documentation

[Project overview](../README.md)

## Usage

- [Installation](usage/installation.md)
- [User guide](usage/guide.md)

## Development

- [Build, test, and package](development/build.md)
- [GUI architecture](development/gui.md)
- [PDF libraries](development/libraries.md)
- [CI setup](development/ci.md)

## Website

In **Settings → Pages**, select **Deploy from a branch → release → /docs**.
The site uses `docs/index.html`, `docs/style.css`, and `docs/assets/`; no build step is needed.

Preview from the repository root:

```sh
python3 -m http.server 8000 --directory docs
```

Open `http://localhost:8000/`.
