#!/usr/bin/env python3
# ============================================================================
#  logo_inicio.py - las capas del logo de la PANTALLA DE INICIO (ui/ViewPorts/PopUp/HomePopup.cpp)
# ----------------------------------------------------------------------------
#  El engranaje se arma COMO EN LA INTRO DE LAS DEMOS (conversiones/intro_demos/make_intro_w3d.py): el reflejo QUIETO,
#  arriba la mascara del engranaje GIRANDO que lo recorta multiplicando (sobre la cabecera negra, lo de afuera queda
#  negro), arriba el borde girando y en el medio el emblema quieto. A la derecha, el texto "Whisk3D" (negro con borde
#  blanco). Las capas son las de la intro (conversiones/intro_demos), copiadas tal cual a res/home:
#    reflejo.png   <- reflejo.png        el verde con las franjas de luz (opaco)
#    mascara.png   <- logo_mascara.png   blanco = engranaje, negro = afuera (el borde de la imagen es negro: va en Clamp)
#    borde.png     <- solo_borde.png     el contorno del engranaje
#    centro.png    <- centro_logo.png    el circulo negro con el emblema
#    texto.png     <- whisk3d_texto.png  "Whisk3D" con la geometria en el 3 (sin los margenes vacios)
#  Si cambian, se vuelve a correr:
#    python3 tools/logo_inicio.py
# ============================================================================
import os
import shutil

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIGEN = os.path.join(os.path.dirname(RAIZ), "conversiones", "intro_demos")
SALIDA = os.path.join(RAIZ, "res", "home")
CAPAS = [("reflejo.png", "reflection.png"), ("logo_mascara.png", "mask.png"), ("solo_borde.png", "border.png"),
         ("centro_logo.png", "center.png")]


def main():
    os.makedirs(SALIDA, exist_ok=True)
    for origen, destino in CAPAS:
        shutil.copyfile(os.path.join(ORIGEN, origen), os.path.join(SALIDA, destino))
        print("   res/home/" + destino)
    from PIL import Image
    im = Image.open(os.path.join(ORIGEN, "whisk3d_texto.png")).convert("RGBA")
    caja = im.split()[3].getbbox()
    if caja: im = im.crop(caja)
    im.save(os.path.join(SALIDA, "text.png"), optimize=True)
    print("   res/home/texto.png (%dx%d)" % im.size)


if __name__ == "__main__":
    main()
