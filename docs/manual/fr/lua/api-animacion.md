# Animation

| Fonction | Ce qu'elle fait |
|---|---|
| `animScene(name [, loop])`, `stopScene()`, `animSceneFrame(f)` | Animations de toute la scène. |
| `animObject(root, "clip" [, loop])`, `animObjectStop`, `animObjectFinished`, `animObjectSpeed` | Clips d'animation d'un objet et de ses enfants. |
| `animClip(armature, clip [, loop])`, `animFinished`, `animSpeed`, `animTransition` | Animations d'une armature (marcher, sauter…) et transition douce entre elles. |
| `animLayer(esq, n, clip [, weight, mode])` et ses variantes | Mélanger plusieurs animations à la fois en couches (1 à 16), par exemple courir + tirer. |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | La position d'un os (pour placer une arme dans la main). |
| `animate(mesh, n [, frame])` | Change l'animation de sommets d'un maillage. |
