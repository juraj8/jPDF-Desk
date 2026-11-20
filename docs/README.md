# Documentation

[Project overview](../README.md)

## Usage

- [Installation](usage/installation.md)
- [User guide](usage/guide.md)

## Development

- [Build, test, and package](development/build.md)
- [GUI architecture](development/gui.md)
- [PDF libraries](development/libraries.md)
- [Revocation library](development/revocation.md)
- [CI setup](development/ci.md)

## Website

In **Settings → Pages**, select **Deploy from a branch → release → /docs**.
The site uses `docs/index.html`, `docs/style.css`, and `docs/assets/`.
Keep `docs/.nojekyll` to publish static files without Jekyll processing.

Preview from the repository root:

```sh
python3 -m http.server 8000 --directory docs
```

Open `http://localhost:8000/`. To regenerate the sample PDF and preview, install
`reportlab` and `pymupdf`, then run `python3 script/create-demo-pdf.py`.
