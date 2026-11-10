# Matemática, escena y formatos

**Vectores, matrices de 4x4 y cuaterniones** propios (`math/`), que compilan igual en la PC y en los sistemas retro. Encima, la **escena** (`objects/`): objetos con jerarquía padre e hijo, mallas, materiales, luces (direccional, puntual y spot), cámaras y esqueletos. Y las **animaciones** (`animation/`): fotogramas clave, esqueletos, vértices, flipbooks y locomoción.

Los formatos propios están pensados para que una máquina chica los lea rápido:

| Formato | Qué es |
|---|---|
| `.w3d` | El proyecto: un ZIP estándar (se abre con cualquier descompresor). Se escribe sin comprimir y se lee también comprimido. Sin ZIP64 |
| `.w3dm` | La geometría en texto por líneas: legible, editable a mano y fácil de leer para el N95 |
| `.w3db` | La misma malla en binario, ya lista para memoria: abrirla es copiar arreglos |
| `.w3dt` | Una textura lista para la GPU (16 bits y mipmaps hechos) |
| `.w3dpack` | Un contenedor de assets cifrado con ChaCha20; la clave la pone la aplicación |
| Pak embebido | Los archivos de un juego compilado, ofuscados con XOR. Es contra curiosos, no criptografía |
| glTF / GLB | Un lector y renderer en un solo header (`io/w3dGltf.h`), para usar fuera del editor |
