# Whisk3D en Symbian (Nokia N95 y S60)

Backend OpenGL ES 1.1. El objetivo insignia del proyecto: correr en un teléfono de 2007.

## Instalar (usuario final)

Necesitás tener instalado **PyS60**, o el programa no se ejecuta. El instalador está en la carpeta `Dependencies` del repo.

La última versión está siempre en la carpeta `sis` como `Whisk3D_gcce.sisx` (puede no funcionar del todo; está en desarrollo constante). La versión **verificada** está en `releases`, con la fecha en el nombre, referenciando los videos/imágenes de demostración de [Instagram](https://www.instagram.com/dante_leoncini).

> Ojo: algunas features van y vienen entre versiones. Por ejemplo el modelado 3D se sacó en las últimas; si querés esa función, usá una versión anterior como `Whisk3D_gcce_beta_24-08-15_FERNET.sisx`.

Si el instalador falla, probablemente tengas que "hackear" el teléfono para ignorar los certificados de Symbian: [Tutorial Hack](https://www.youtube.com/watch?v=UJJICzbk3TA).

## Compilar

El proyecto de Symbian está en esta carpeta:

- `group/Whisk3D.mmp` — el proyecto (fuentes, includes, libs). Comparte casi todo el código con el resto de las plataformas (`main/`, `libs/Whisk3DCore`, `libs/WhiskUI`); lo específico del teléfono está en `src/`.
- `src/` — la capa de Symbian: app framework (`Whisk3D*.cpp`), input (teclado/keypad, mouse virtual, HID bluetooth), carga de texturas por ICL, y el reloj/glue.
- `data/`, `sis/` — recursos e instaladores.

Se compila con el SDK de S60 3rd Edition + **Carbide.c++** (o `bldmake` + `abld` a mano). La toolchain es vieja (**C++03**: sin `nullptr`, `auto`, range-for, `vector::data`...) y no todo el subsistema nuevo (FBX, skinning) está portado — mirá los `#ifndef W3D_SYMBIAN` del código.
