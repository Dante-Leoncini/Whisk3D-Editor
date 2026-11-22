# Sahne: bulma, oluşturma ve yok etme

| Fonksiyon | Ne yapar |
|---|---|
| `find(name [, root]) → obj \| nil` | Outliner'da bir nesneyi adıyla arar (ya da yalnızca `root` içinde). |
| `selected() → { obj, … }` | Editörde seçili nesneler (editör eklentisi gibi çalışan [düğmeler](scripts-y-objetos.md#düğmeler) için). |
| `getName(obj) → text` | Nesnenin adı. |
| `getType(obj) → text` | Ne olduğu: `"mesh"`, `"light"`, `"camera"`, `"empty"`, `"armature"`, `"text"`, `"image"`, `"button"`, `"hitbox"`, `"prefab"`, `"rutina"`… (yoksa `""`). |
| `instantiate(prefab [, x, y, z [, rotY [, parent]]]) → obj \| nil` | O konumda bir prefab'ın kopyasını oluşturur (rotY derece cinsinden). `"Libreria/Escena"`, bağlı bir kütüphanedekini oluşturur. `parent` verilirse o nesneye bağlı olur (konumu ve dönüşü ona göre). |
| `destroy(obj) → bool` | Nesneyi karenin sonunda siler. |
| `importW3D(path) → obj \| nil` | Oyun çalışırken sahneye başka bir .w3d dosyası ekler. |
| `streamLoad(obj)` / `streamUnload(obj)` / `streamAuto(obj)` | Seviyenin bir bölümünü elle yükler ya da boşaltır (streaming) veya onu otomatiğe döndürür. |
| `isLoaded(obj) → bool` | O bölümün yüklenip yüklenmediği. |
