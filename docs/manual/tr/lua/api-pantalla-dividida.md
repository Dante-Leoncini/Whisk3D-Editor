# Bölünmüş ekran (aynı cihazda iki oyuncu)

| Fonksiyon | Ne yapar |
|---|---|
| `setViews(n [, "lado" \| "up"])` | `2` ekranı ikiye böler (yan yana ya da biri diğerinin üstünde); `1` tam ekrana döner. 2D arayüz (HUD) ikisinin üstüne bir kez çizilir. |
| `setCameraView(i, camera)` | `i` görünümünün (1 ya da 2) kamerası. Ayarlanmazsa etkin kamera. |
| `view() → i, n` | O an çizilen görünüm (1 ya da 2; çizimin dışında 0) ve kaç görünüm olduğu. |
| `beforeView(i)` | (Onu senin script'in tanımlar.) Motor onu, her görünümü çizmeden önce, `setViews`'ı çağıran script'te çağırır. Orada kameraya bakan şeyleri (tabelalar, parçacıklar) O kameraya doğru çevirir ya da o görünümün görmemesi gerekenleri gizlersin. Zamanla ilerleyen hiçbir şeyi hareket ettirme: o, `update`'a gider. |

Her yarı kendi kamerasıyla tam bir görüntüdür: oyunun karesi (kameranın en-boy oranı) ikiye bölünür.

## Herkese bir kumanda

`button`, `buttonPressed`, `stick` ve `trigger` **kumanda numarasını** kabul eder (1'den 4'e, ilk kez hareket ettirildikleri
sırayla): `button("a", 2)`, `stick("left", 1)`. Numara verilmezse her zamanki gibi herhangi biri (hepsi birlikte). Klavyenin
numarası yoktur: onu sen paylaştır (örneğin 1. oyuncu oklar ve rakamlarla, 2. oyuncu WASD ve harflerle; QWERTY klavyeli
bir Nokia'da harfler de gelir).

```lua
function update(dt)
  local x1, y1 = stick("left", 1)
  local x2, y2 = stick("left", 2)
  if key("w") then y2 = -1 end
  move(jugador1, x1 * dt * 5, y1 * dt * 5, 0)
  move(jugador2, x2 * dt * 5, y2 * dt * 5, 0)
end
```
