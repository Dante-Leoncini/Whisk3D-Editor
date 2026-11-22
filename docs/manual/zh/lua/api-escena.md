# 场景：查找、创建和销毁

| 函数 | 作用 |
|---|---|
| `find(name [, root]) → obj \| nil` | 按名称在 Outliner 中查找对象（或只在 `root` 内部查找）。 |
| `selected() → { obj, … }` | 编辑器中所选的对象（用于作为编辑器插件使用的[按钮](scripts-y-objetos.md#按钮)）。 |
| `getName(obj) → text` | 对象的名称。 |
| `getType(obj) → text` | 它是什么：`"mesh"`、`"light"`、`"camera"`、`"empty"`、`"armature"`、`"text"`、`"image"`、`"button"`、`"hitbox"`、`"prefab"`、`"rutina"`……（不存在时返回 `""`）。 |
| `instantiate(prefab [, x, y, z [, rotY [, parent]]]) → obj \| nil` | 在该位置创建预制体的一个副本（rotY 以度为单位）。`"Libreria/Escena"` 会创建已链接库中那个预制体的副本。传入 `parent` 时，副本会挂在该对象下（位置和旋转都相对于它）。 |
| `destroy(obj) → bool` | 在帧结束时删除该对象。 |
| `importW3D(path) → obj \| nil` | 在游戏运行时，把另一个 .w3d 文件添加到场景中。 |
| `streamLoad(obj)` / `streamUnload(obj)` / `streamAuto(obj)` | 手动加载或卸载关卡的某一部分（流式加载），或将其恢复为自动。 |
| `isLoaded(obj) → bool` | 该部分是否已经加载。 |
