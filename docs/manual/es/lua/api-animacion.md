# Animación

| Función | Qué hace |
|---|---|
| `animScene(name [, loop])`, `stopScene()`, `animSceneFrame(f)` | Animaciones de toda la escena. |
| `animObject(root, "clip" [, loop])`, `animObjectStop`, `animObjectFinished`, `animObjectSpeed` | Clips de animación de un objeto y sus hijos. |
| `animClip(armature, clip [, loop])`, `animFinished`, `animSpeed`, `animTransition` | Animaciones de un esqueleto (caminar, saltar…) y la transición suave entre ellas. |
| `animLayer(esq, n, clip [, weight, mode])` y familia | Mezclar varias animaciones a la vez en capas (1 a 16), por ejemplo correr + disparar. |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | Dónde está un hueso (para poner un arma en la mano). |
| `animate(mesh, n [, frame])` | Cambia la animación de vértices de una malla. |
