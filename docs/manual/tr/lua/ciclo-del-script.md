# Motor script'ini ne zaman çağırır

Script'in özel adlara sahip fonksiyonlar tanımlar ve motor bunları tam zamanında kendisi çağırır. Hepsi isteğe bağlıdır: yalnızca ihtiyacın olanları yaz.

| Fonksiyon | Ne zaman çağrılır |
|---|---|
| `inicio()` | Bir kez, Play'e basıldığında (ya da nesne `instanciar` ile oluşturulur oluşturulmaz). Nesneleri bulmak ve değişkenleri hazırlamak için kullanılır. |
| `actualizar(dt)` | Her karede, nesne **görünür** olduğu ve etkin sahnede bulunduğu sürece. `dt`, önceki kareden bu yana geçen saniyedir (0.001 ile 0.1 arasında). |
| `alEntrar(otro, miHitbox, suHitbox)` | Bir hitbox başka birine dokunmaya başladığında. |
| `alQuedarse(otro, miHitbox, suHitbox)` | Birbirlerine dokunmaya devam ettikleri sürece her karede. |
| `alSalir(otro, miHitbox, suHitbox)` | Birbirlerine dokunmayı bıraktıklarında (biri gizlenirse ya da devre dışı bırakılırsa da). |
| `alTocar(otro, impulso, x, y, z, nx, ny, nz)` | İki **katı cisim** çarpıştığında. `x, y, z` temas noktası, `nx, ny, nz` ise diğerine doğru olan yöndür. |

Hitbox olaylarını hitbox'ın script'leri ve onun ebeveyn nesnesinin script'leri alır. Diğeri bir katı cisimse, `suHitbox` `nil` olarak gelir.

## dt ile bir şeyleri hareket ettirmek

Hızları her zaman `dt` ile çarp. Böylece oyun güçlü bir PC'de de yavaş bir telefonda da aynı hızda ilerler:

```lua
function actualizar(dt)
  mover(yo(), 3 * dt, 0, 0)   -- +X yönünde saniyede 3 birim
end
```

## Bir kare içindeki sıra

1. Fizik (hızlar, katı cisimler) ve çarpışma olayları: `alTocar`, `alSalir`, `alEntrar`, `alQuedarse`.
2. Tüm script'lerin `actualizar(dt)` fonksiyonu, Outliner'daki sırayla.
3. Animasyonlar (sahne, klipler, iskeletler).
4. Karenin sonu: bekleyen `destruir()` çağrıları uygulanır, örneklenen nesneler sahneye girer ve istenen `cambiarEscena()` yapılır.

## Oyunu durdurunca

Stop'a basınca tüm script'ler kapatılır, ortak alan silinir ve sahne Play'den önceki hâline döner. Oyun çalışırken bir script'i düzenlersen, o nesne script'i yeniden yükler: değişkenleri sıfırlanır ve `inicio()` yeniden çağrılır.
