# Анимация

| Функция | Что делает |
|---|---|
| `animScene(name [, loop])`, `stopScene()`, `animSceneFrame(f)` | Анимации всей сцены. |
| `animObject(root, "clip" [, loop])`, `animObjectStop`, `animObjectFinished`, `animObjectSpeed` | Клипы анимации объекта и его потомков. |
| `animClip(armature, clip [, loop])`, `animFinished`, `animSpeed`, `animTransition` | Анимации скелета (ходьба, прыжок…) и плавный переход между ними. |
| `animLayer(esq, n, clip [, weight, mode])` и родственные функции | Смешивание нескольких анимаций одновременно по слоям (от 1 до 16), например бег + стрельба. |
| `bonePoint(esq, "hueso" [, x, y, z]) → x, y, z` | Где находится кость (чтобы вложить оружие в руку). |
| `animate(mesh, n [, frame])` | Меняет вершинную анимацию меша. |
