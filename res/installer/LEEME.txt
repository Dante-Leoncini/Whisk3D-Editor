IMAGENES DEL INSTALADOR DE WHISK3D (Windows / NSIS)
====================================================
FORMATO OBLIGATORIO: BMP de 24 bits (sin canal alfa, sin compresion). NSIS NO
acepta PNG ni JPG para estas. (El icono SI es .ico: res/Whisk3D.ico)

  header.bmp     150 x 57   - banner arriba a la derecha de cada pagina.
  welcome.bmp    164 x 314  - imagen grande (izquierda) de Bienvenida y Finalizar.
  uninstall.bmp  164 x 314  - igual pero para el DESINSTALADOR.

  res/Whisk3D.ico           - icono del instalador, desinstalador y del .w3d.
                              (se genera desde res/Whisk3D.png; multi-tamano)

Tip: en welcome/uninstall, NSIS suele mostrar el texto sobre la franja DERECHA
de la imagen, asi que dejá esa zona mas clara/neutra.
