# La scena: cercare, creare e distruggere

| Funzione | Cosa fa |
|---|---|
| `buscar(nombre [, raiz]) → obj \| nil` | Cerca un oggetto per nome nell'outliner (o solo dentro `raiz`). |
| `seleccionados() → { obj, … }` | Gli oggetti selezionati nell'editor (per i [pulsanti](scripts-y-objetos.md#pulsanti) che funzionano come componenti aggiuntivi dell'editor). |
| `nombre(obj) → texto` | Il nome dell'oggetto. |
| `tipo(obj) → texto` | Che cos'è: `"mesh"`, `"luz"`, `"camara"`, `"vacio"`, `"esqueleto"`, `"texto"`, `"imagen"`, `"boton"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` se non esiste). |
| `instanciar(prefab [, x, y, z [, rotY [, padre]]]) → obj \| nil` | Crea una copia di un prefab in quella posizione (rotY in gradi). `"Libreria/Escena"` crea quella di una libreria collegata. Con `padre` resta appesa a quell'oggetto (posizione e rotazione relative a esso). |
| `destruir(obj) → bool` | Elimina l'oggetto alla fine del fotogramma. |
| `importarW3D(ruta) → obj \| nil` | Aggiunge alla scena un altro file .w3d mentre il gioco è in esecuzione. |
| `cargar(obj)` / `descargar(obj)` / `cargaAuto(obj)` | Carica o scarica a mano una parte del livello (streaming), o la riporta in automatico. |
| `cargado(obj) → bool` | Se quella parte è già caricata. |
