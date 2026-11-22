# Split screen (two players on the same device)

| Function | What it does |
|---|---|
| `setViews(n [, "lado" \| "up"])` | `2` splits the screen in two (side by side or one above the other); `1` goes back to the full screen. The 2D interface (the HUD) is drawn once, on top of both. |
| `setCameraView(i, camera)` | The camera of view `i` (1 or 2). If not set, the active one. |
| `view() → i, n` | The view being drawn (1 or 2; 0 outside drawing) and how many there are. |
| `beforeView(i)` | (Your script defines it.) The engine calls it before drawing each view, in the script that called `setViews`. That's where you turn whatever faces the camera (billboards, particles) toward THAT camera, or hide what that view shouldn't see. Don't move anything that advances over time: that goes in `update`. |

Each half is a full image with its own camera: the game's frame (the camera's aspect ratio) is split in two.

## A gamepad for each player

`button`, `buttonPressed`, `stick` and `trigger` accept the **gamepad number** (1 to 4, in the order they were first moved):
`button("a", 2)`, `stick("left", 1)`. Without a number, any of them (all together), as always. The keyboard has no number:
split it up yourself (for example, player 1 with the arrows and the numbers, player 2 with WASD and the letters; on a
Nokia with a QWERTY keyboard the letters come through too).

```lua
function update(dt)
  local x1, y1 = stick("left", 1)
  local x2, y2 = stick("left", 2)
  if key("w") then y2 = -1 end
  move(jugador1, x1 * dt * 5, y1 * dt * 5, 0)
  move(jugador2, x2 * dt * 5, y2 * dt * 5, 0)
end
```
