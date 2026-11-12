# Sahne: bulma, oluşturma ve yok etme

| Fonksiyon | Ne yapar |
|---|---|
| `buscar(nombre [, raiz]) → obj \| nil` | Outliner'da bir nesneyi adıyla arar (ya da yalnızca `raiz` içinde). |
| `seleccionados() → { obj, … }` | Editörde seçili nesneler (editör eklentisi gibi çalışan [düğmeler](scripts-y-objetos.md#düğmeler) için). |
| `nombre(obj) → texto` | Nesnenin adı. |
| `tipo(obj) → texto` | Ne olduğu: `"mesh"`, `"luz"`, `"camara"`, `"vacio"`, `"esqueleto"`, `"texto"`, `"imagen"`, `"boton"`, `"hitbox"`, `"prefab"`, `"rutina"`… (yoksa `""`). |
| `instanciar(prefab [, x, y, z [, rotY [, padre]]]) → obj \| nil` | O konumda bir prefab'ın kopyasını oluşturur (rotY derece cinsinden). `"Libreria/Escena"`, bağlı bir kütüphanedekini oluşturur. `padre` verilirse o nesneye bağlı olur (konumu ve dönüşü ona göre). |
| `destruir(obj) → bool` | Nesneyi karenin sonunda siler. |
| `importarW3D(ruta) → obj \| nil` | Oyun çalışırken sahneye başka bir .w3d dosyası ekler. |
| `cargar(obj)` / `descargar(obj)` / `cargaAuto(obj)` | Seviyenin bir bölümünü elle yükler ya da boşaltır (streaming) veya onu otomatiğe döndürür. |
| `cargado(obj) → bool` | O bölümün yüklenip yüklenmediği. |
