# Animation

| Function | What it does |
|---|---|
| `animScene(name [, loop])`, `stopScene()`, `animSceneFrame(f)` | Animations of the whole scene. |
| `animObject(root, "clip" [, loop])`, `animObjectStop`, `animObjectFinished`, `animObjectSpeed` | Animation clips of an object and its children. |
| `animClip(armature, clip [, loop])`, `animFinished`, `animSpeed`, `animTransition` | Animations of an armature (walk, jump…) and the smooth transition between them. |
| `animLayer(esq, n, clip [, weight, mode])` and related functions | Blend several animations at once in layers (1 to 16), for example run + shoot. |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | Where a bone is (to put a weapon in a hand). |
| `animate(mesh, n [, frame])` | Changes a mesh's vertex animation. |
