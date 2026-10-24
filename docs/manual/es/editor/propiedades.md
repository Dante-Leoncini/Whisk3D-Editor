# Propiedades

El panel con todos los datos del objeto activo, organizado en pestañas (iconos). Las pestañas que aparecen dependen del tipo de objeto. Click en el título de una tarjeta la pliega. En los campos numéricos: click para tipear (acepta cuentas), arrastrar para deslizar el valor. El rombo de la derecha pone o quita un keyframe.

| Pestaña | Qué tiene |
|---|---|
| Archivo / Render (diskette) | **Archivo**: abrir, guardar, guardar como, guardar versión, extraer assets. **Render**: ruta, tamaño, pases extra, fondo, **Trazado de rayos** (rayos, muestras, pases), Renderizar imagen y Renderizar animación. **Juego**: plataforma (Windows, Symbian, Linux .deb, AppImage, WebGL, Android, Android 2.3), ventana, orientación, icono, física, sonido, volumen, **resolución** (el porcentaje de la pantalla al que dibuja el juego: en Android 2.3 el sistema lo estira, y en esos teléfonos dibujar menos píxeles da más cuadros) y Compilar juego (con un teléfono conectado por adb lo instala y lo abre). **Exportar**: OBJ, glTF, GLB. **Ajustes**: idioma, antialiasing, gráficos, skin, logs, escala, **Atajos de teclado…** y Guardar cambios. |
| Objeto | Posición, rotación (Euler, cuaternión o eje-ángulo), escala, nombre, visible, renderizar; animaciones y paleta del objeto. |
| Contextual | Depende del tipo: partes de malla y **material** (textura, iluminación, color de vértice, transparencia, reflejos, normal map, mezcla, profundidad, capas de textura; Rugosidad y Metálico con trazado de rayos), luz, cámara, niebla, partículas, física, hitbox, esqueleto y huesos, elementos 2D, prefabs, rutinas. |
| Malla 3D | La malla usada (y cuántos la comparten), mapas UV, capas de color, grupos de vértices, grupos UV. Con un recurso elegido en la Biblioteca: sus datos y su ubicación (dentro o fuera del .w3d). |
| Modificadores | Ver [Modificadores](modificadores.md). |
| Transformar | Solo en Modo Edición: centro de la selección y de sus UV, editables. |
| Esqueleto 2D | Esqueletos y huesos 2D de la malla. |
| Restricciones | Copiar posición, Copiar rotación, Billboard (mirar a la cámara), Child Of. |
| Scripts | Los scripts Lua del objeto y sus propiedades (ver el [documento de Lua](../lua/README.md)). |
| Animación | La animación activa, capas de mezcla (Mix), valores exactos del keyframe elegido y cortes de cámara. |

## Rutinas

Una rutina es una lista de pasos de dibujo (trasladar, rotar, fijar textura, dibujar una malla, llamar a otra rutina, Llamar Lua…) que se arma en Propiedades. Es la forma más rápida de dibujar en el N95.

| Función | Descripción | Atajo |
|---|---|---|
| Añadir / Quitar paso | Agrega pasos agrupados por tema, o quita el elegido. | Botones; <kbd>X</kbd> o <kbd>Supr</kbd> quita |
| Agarrar paso | Reordena el paso con las flechas; <kbd>Enter</kbd> confirma. | <kbd>G</kbd> |
| Insertar keyframe | Anima los valores del paso en el frame actual. | <kbd>I</kbd> |
| Repetir | Cuántas veces corre la lista seguida (bucle de rutina). 1 = rutina normal. | Campo Repetir |
| Modo de render / Usa | Una lista distinta por modo de sombreado, o la misma para todos. | Desplegables |
