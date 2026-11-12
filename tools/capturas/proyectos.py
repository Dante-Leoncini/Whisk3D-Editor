"""Los proyectos de ejemplo de los tutoriales: los .w3d que se descargan al final de cada uno.

Como las capturas, no se hacen a mano: los arma el editor de verdad (con el harness) y quedan en
docs/manual/<idioma>/tutoriales/ejemplos/. Si cambia el formato del .w3d o el script del tutorial, se vuelven a armar.

    python3 tools/capturas/proyectos.py                 # en castellano
    python3 tools/capturas/proyectos.py --idiomas es,en
"""
import os
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
import capturas  # noqa: E402
import recetas  # noqa: E402,F401  (registra las recetas en el orden de siempre, como capturas.py)
import recetas_script  # noqa: E402

idiomas = ["es"]
if "--idiomas" in sys.argv:
    idiomas = sys.argv[sys.argv.index("--idiomas") + 1].split(",")
for idi in idiomas:
    destino = os.path.join(capturas.RAIZ, "docs", "manual", idi, "tutoriales", "ejemplos", "girar-cubo.w3d")
    recetas_script.proyecto_ejemplo(idi, destino)
    print("%s  %.1f KB" % (os.path.relpath(destino, capturas.RAIZ), os.path.getsize(destino) / 1024.0))
