#!/usr/bin/env python3
"""Los dibujos de los manuales (tools/diagramas/*.html) -> docs/manual/<idioma>/<manual>/img/<nombre>.webp

Cada dibujo es una pagina HTML (con los logos de los engranajes al lado) que se fotografia con Chrome
headless, se recorta al contenido y se guarda en WebP sin perdida. Por ahora solo en castellano:
cuando se traduzca el manual, cada idioma va a tener su HTML (capas.<idioma>.html).

Uso:  python3 tools/diagramas/diagramas.py
"""
import os, shutil, subprocess, sys, tempfile

from PIL import Image

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.abspath(os.path.join(AQUI, "..", ".."))
# html -> (nombre de la imagen, manuales donde va: docs/manual/es/<manual>/img/)
DIBUJOS = {"capas": ("whisk3d-capas", ["editor", "core", "ui"]), "despacho": ("whisk3d-core-despacho", ["core"])}


def chrome():
    for n in ("google-chrome", "chromium", "chromium-browser"):
        p = shutil.which(n)
        if p:
            return p
    raise SystemExit("diagramas: no encuentro Chrome / Chromium")


def recortar(im, fondo=(18, 22, 28), margen=24):
    """Saca lo que sobra abajo (la ventana del navegador es mas alta que el dibujo)."""
    px = im.load()
    w, h = im.size
    y = h - 1
    while y > 0 and all(sum(abs(a - b) for a, b in zip(px[x, y][:3], fondo)) < 12 for x in range(0, w, 4)):
        y -= 1
    return im.crop((0, 0, w, min(h, y + margen)))


def main():
    navegador = chrome()
    for html, (nombre, manuales) in DIBUJOS.items():
        fuente = os.path.join(AQUI, html + ".html")
        tmp = tempfile.mkdtemp(prefix="w3d_diagrama_")
        png = os.path.join(tmp, "f.png")
        subprocess.run([navegador, "--headless=new", "--disable-gpu", "--hide-scrollbars", "--force-device-scale-factor=1",
                        "--window-size=1400,2000", "--screenshot=" + png, "file://" + fuente],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True, timeout=120)
        im = recortar(Image.open(png).convert("RGB"))
        for manual in manuales:
            salida = os.path.join(RAIZ, "docs", "manual", "es", manual, "img", nombre + ".webp")
            os.makedirs(os.path.dirname(salida), exist_ok=True)
            im.save(salida, "WEBP", lossless=True, method=6)
            print("%-22s %-7s %dx%d  %.1f KB" % (nombre, manual, im.width, im.height, os.path.getsize(salida) / 1024))
        shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
