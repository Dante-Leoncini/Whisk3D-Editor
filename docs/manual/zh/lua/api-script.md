# 脚本与通信

| 函数 | 作用 |
|---|---|
| `yo() → obj` | 拥有该脚本的对象。 |
| `objeto(prop) → obj \| nil` | 在 `propiedades` 的 `prop` 字段中选择的对象。 |
| `opcion(prop) → texto` | 在 `propiedades` 的下拉列表中选择的选项；如果没有选择，就是列表中的第一个（默认值）。 |
| `propiedad(prop) → valor` | 该对象自己的数字、是/否或文本（或者声明的默认值）。 |
| `parametro(prop [, def]) → número` | 与 `propiedad` 类似，但总是返回数字；如果不存在则返回 `def`（或 0）。 |
| `objetivo() → obj \| nil` | 所属对象的目标（“target”）。 |
| `compartido(clave) → valor` | 读取所有脚本共用的共享空间。 |
| `setCompartido(clave, valor)` | 向共享空间写入一个数字、是/否或文本；传入 `nil` 会删除它。 |
