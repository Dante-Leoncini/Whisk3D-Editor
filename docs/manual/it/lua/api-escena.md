# La scena: cercare, creare e distruggere

| Funzione | Cosa fa |
|---|---|
| `find(name [, root]) → obj \| nil` | Cerca un oggetto per nome nell'outliner (o solo dentro `root`). |
| `selected() → { obj, … }` | Gli oggetti selezionati nell'editor (per i [pulsanti](scripts-y-objetos.md#pulsanti) che funzionano come componenti aggiuntivi dell'editor). |
| `getName(obj) → text` | Il nome dell'oggetto. |
| `getType(obj) → text` | Che cos'è: `"mesh"`, `"light"`, `"camera"`, `"empty"`, `"armature"`, `"text"`, `"image"`, `"button"`, `"hitbox"`, `"prefab"`, `"rutina"`… (`""` se non esiste). |
| `instantiate(prefab [, x, y, z [, rotY [, parent]]]) → obj \| nil` | Crea una copia di un prefab in quella posizione (rotY in gradi). `"Libreria/Escena"` crea quella di una libreria collegata. Con `parent` resta appesa a quell'oggetto (posizione e rotazione relative a esso). |
| `destroy(obj) → bool` | Elimina l'oggetto alla fine del fotogramma. |
| `importW3D(path) → obj \| nil` | Aggiunge alla scena un altro file .w3d mentre il gioco è in esecuzione. |
| `streamLoad(obj)` / `streamUnload(obj)` / `streamAuto(obj)` | Carica o scarica a mano una parte del livello (streaming), o la riporta in automatico. |
| `isLoaded(obj) → bool` | Se quella parte è già caricata. |
