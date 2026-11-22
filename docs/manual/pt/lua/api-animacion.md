# Animação

| Função | O que faz |
|---|---|
| `animScene(name [, loop])`, `stopScene()`, `animSceneFrame(f)` | Animações da cena inteira. |
| `animObject(root, "clip" [, loop])`, `animObjectStop`, `animObjectFinished`, `animObjectSpeed` | Clipes de animação de um objeto e seus filhos. |
| `animClip(armature, clip [, loop])`, `animFinished`, `animSpeed`, `animTransition` | Animações de uma armação (andar, pular…) e a transição suave entre elas. |
| `animLayer(esq, n, clip [, weight, mode])` e família | Misturar várias animações ao mesmo tempo em camadas (1 a 16), por exemplo correr + atirar. |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | Onde está um osso (para colocar uma arma na mão). |
| `animate(mesh, n [, frame])` | Muda a animação de vértices de uma malha. |
