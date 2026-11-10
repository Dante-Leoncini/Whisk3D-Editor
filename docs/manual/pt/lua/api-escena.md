# A cena: buscar, criar e destruir

| Função | O que faz |
|---|---|
| `buscar(nombre [, raiz]) → obj \| nil` | Busca um objeto pelo nome no Outliner (ou só dentro de `raiz`). |
| `nombre(obj) → texto` | O nome do objeto. |
| `tipo(obj) → texto` | O que ele é: `"mesh"`, `"luz"`, `"camara"`, `"vacio"`, `"esqueleto"`, `"texto"`, `"imagen"`, `"boton"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` se não existir). |
| `instanciar(prefab [, x, y, z [, rotY [, padre]]]) → obj \| nil` | Cria uma cópia de um prefab nessa posição (rotY em graus). `"Libreria/Escena"` cria a de uma biblioteca vinculada. Com `padre`, ela fica pendurada nesse objeto (posição e rotação relativas a ele). |
| `destruir(obj) → bool` | Apaga o objeto no fim do quadro. |
| `importarW3D(ruta) → obj \| nil` | Adiciona à cena outro arquivo .w3d enquanto o jogo roda. |
| `cargar(obj)` / `descargar(obj)` / `cargaAuto(obj)` | Carrega ou descarrega manualmente uma parte do nível (streaming), ou a devolve ao modo automático. |
| `cargado(obj) → bool` | Se essa parte já está carregada. |
