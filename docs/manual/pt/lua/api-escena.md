# A cena: buscar, criar e destruir

| Função | O que faz |
|---|---|
| `find(name [, root]) → obj \| nil` | Busca um objeto pelo nome no Outliner (ou só dentro de `root`). |
| `selected() → { obj, … }` | Os objetos selecionados no editor (para os [botões](scripts-y-objetos.md#botões) que funcionam como complementos do editor). |
| `getName(obj) → text` | O nome do objeto. |
| `getType(obj) → text` | O que ele é: `"mesh"`, `"light"`, `"camera"`, `"empty"`, `"armature"`, `"text"`, `"image"`, `"button"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` se não existir). |
| `instantiate(prefab [, x, y, z [, rotY [, parent]]]) → obj \| nil` | Cria uma cópia de um prefab nessa posição (rotY em graus). `"Libreria/Escena"` cria a de uma biblioteca vinculada. Com `parent`, ela fica pendurada nesse objeto (posição e rotação relativas a ele). |
| `destroy(obj) → bool` | Apaga o objeto no fim do quadro. |
| `importW3D(path) → obj \| nil` | Adiciona à cena outro arquivo .w3d enquanto o jogo roda. |
| `streamLoad(obj)` / `streamUnload(obj)` / `streamAuto(obj)` | Carrega ou descarrega manualmente uma parte do nível (streaming), ou a devolve ao modo automático. |
| `isLoaded(obj) → bool` | Se essa parte já está carregada. |
