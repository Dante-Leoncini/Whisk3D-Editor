# Animazione

| Funzione | Cosa fa |
|---|---|
| `animScene(name [, loop])`, `stopScene()`, `animSceneFrame(f)` | Animazioni di tutta la scena. |
| `animObject(root, "clip" [, loop])`, `animObjectStop`, `animObjectFinished`, `animObjectSpeed` | Clip di animazione di un oggetto e dei suoi figli. |
| `animClip(armature, clip [, loop])`, `animFinished`, `animSpeed`, `animTransition` | Animazioni di un'armatura (camminare, saltare…) e la transizione morbida tra di esse. |
| `animLayer(esq, n, clip [, weight, mode])` e famiglia | Mescolare più animazioni contemporaneamente a livelli (da 1 a 16), per esempio correre + sparare. |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | Dove si trova un osso (per mettere un'arma in mano). |
| `animate(mesh, n [, frame])` | Cambia l'animazione dei vertici di una mesh. |
