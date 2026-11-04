# examples — escenas de ejemplo

Escenas de ejemplo (`.w3d`) que cubren distintas features de Whisk3D. Sirven como **pruebas rápidas de regresión**: al terminar un cambio, abrís estas escenas y confirmás que todo sigue andando (que no se rompió el modelado, los modificadores, las texturas, la animación, etc.).

> ⚠️ **Están viejas**: usan un formato `.w3d` anterior. La idea a futuro es **regenerarlas con el `.w3d` nuevo** para que vuelvan a abrir bien y sigan sirviendo de test. Hasta entonces, algunas pueden fallar al cargar — eso es esperable.

## Qué prueba cada una

| Escena | Qué ejercita |
|---|---|
| `Test_01_basico.w3d` | escena mínima (cubo, cámara, luz) |
| `Test_02_outliner.w3d` | jerarquía / outliner (colecciones, parenting) |
| `Test_03_array.w3d` | modificador **Array** |
| `Test_04_mirror.w3d` | modificador **Mirror** |
| `Test_05_gamepad.w3d` | control con gamepad |
| `Test_06_vertexColor.w3d` | colores por vértice |
| `Test_07_Textura_Transparente.w3d` | textura con alpha / transparencia |
| `Test_08_Textura_Animada.w3d` | textura animada |
| `Test_09_Vertex_Animation.w3d` | animación por vértices |
| `Test_10_Normal_Animation.w3d` | animación de normales |
| `shaders/Ejemplos_Shaders.w3d` | todos los **shaders escritos** de ejemplo (vidrio esmerilado, vidrio plano, reflejo de entorno, matcap, latido, dibujo animado, agua) y la **Transmisión** / IOR del trazado de rayos. Es del formato nuevo. Se regenera con `tools/pruebas/ejemplo_shaders.w3s` |
| `shaders/Ejemplos_Vertex_N95.w3d` | los shaders de **vértices** (`@vertice`: bandera, respirar, torcer, saltar, agua) en mallas chicas: en el N95 y el GL fijo los corre la CPU, en GLES 2.0 / Vulkan la GPU. Se mueven con Play. Se regenera con `tools/pruebas/ejemplo_vertex_n95.w3s` |

## `assets/`

Recursos que usan las escenas (texturas, modelos `.obj/.mtl` como `monkey`, y los frames de las animaciones por vértice/normal). Si movés o renombrás una escena, revisá que sus rutas a `assets/` sigan válidas.

## Cómo usarlas

Abrí una escena desde Whisk3D (o pasándola como argumento al ejecutable) y verificá que carga y se ve/comporta como corresponde. Es un smoke-test manual; más adelante se pueden automatizar con el harness de tests (`main/test/W3dScript.cpp`).

## `shaders/`

Los [shaders escritos](../docs/manual/es/editor/shaders.md) de ejemplo, como archivos `.glsl` sueltos para que los pruebes en tus proyectos (arrastralos desde el explorador a la ventana, o **Cargar del disco...** en el material), sus texturas (`shaders/texturas/`) y el proyecto `Ejemplos_Shaders.w3d` con todos puestos en objetos.
