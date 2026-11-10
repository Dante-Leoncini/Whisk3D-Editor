# Bölünmüş ekran (aynı cihazda iki oyuncu)

| Fonksiyon | Ne yapar |
|---|---|
| `setVistas(n [, "lado" \| "arriba"])` | `2` ekranı ikiye böler (yan yana ya da biri diğerinin üstünde); `1` tam ekrana döner. 2D arayüz (HUD) ikisinin üstüne bir kez çizilir. |
| `setVistaCamara(i, camara)` | `i` görünümünün (1 ya da 2) kamerası. Ayarlanmazsa etkin kamera. |
| `vista() → i, n` | O an çizilen görünüm (1 ya da 2; çizimin dışında 0) ve kaç görünüm olduğu. |
| `antesDeVista(i)` | (Onu senin script'in tanımlar.) Motor onu, her görünümü çizmeden önce, `setVistas`'ı çağıran script'te çağırır. Orada kameraya bakan şeyleri (tabelalar, parçacıklar) O kameraya doğru çevirir ya da o görünümün görmemesi gerekenleri gizlersin. Zamanla ilerleyen hiçbir şeyi hareket ettirme: o, `actualizar`'a gider. |

Her yarı kendi kamerasıyla tam bir görüntüdür: oyunun karesi (kameranın en-boy oranı) ikiye bölünür.

## Herkese bir kumanda

`boton`, `botonApretado`, `stick` ve `gatillo` **kumanda numarasını** kabul eder (1'den 4'e, ilk kez hareket ettirildikleri
sırayla): `boton("a", 2)`, `stick("izq", 1)`. Numara verilmezse her zamanki gibi herhangi biri (hepsi birlikte). Klavyenin
numarası yoktur: onu sen paylaştır (örneğin 1. oyuncu oklar ve rakamlarla, 2. oyuncu WASD ve harflerle; QWERTY klavyeli
bir Nokia'da harfler de gelir).

```lua
function actualizar(dt)
  local x1, y1 = stick("izq", 1)
  local x2, y2 = stick("izq", 2)
  if tecla("w") then y2 = -1 end
  mover(jugador1, x1 * dt * 5, 0, y1 * dt * 5)
  mover(jugador2, x2 * dt * 5, 0, y2 * dt * 5)
end
```
