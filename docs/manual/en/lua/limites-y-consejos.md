# Limits and tips

## Which parts of Lua aren't available

- `string`, `table` and `math` are available, plus the basic functions (`print`, `pairs`, `ipairs`, `type`, `tostring`, `tonumber`, `pcall`, `assert`…).
- `io` and `os` are **not** available (a game doesn't touch files on the disk; that's why there's no `os.time`), and neither are `require`, `coroutine`, `utf8` or `debug`. To save data, use `getConfig` / `saveConfig`.
- Each script is independent: it can't see another script's variables. To communicate, use `setShared` / `getShared`.

## Errors

- If a function has an error, it's written to the log (`archivo.lua:LÍNEA: mensaje`) and that call stops, but the function is called again on the next frame: the same error repeats every frame until you fix it.
- If the file can't be read or has a syntax error, that script stays off until the next Play.
- In Whisk3D, `error("...")` only logs the message and the script keeps going. If you want to stop, use `assert(condicion, "mensaje")`.

## Things to keep in mind

- `update` only runs on **visible** objects in the active scene: hiding an object turns its logic off.
- `destroy`, `changeScene` and `playScene` are applied at the end of the frame, not right away.
- The game's text font doesn't draw accents or ñ: write `"MENU"` instead of `"MENÚ"`.
- `safeArea`, `uiBox` and `resolution` only have real values after the first frame has been drawn: don't use them in `start`.
- On Web and Android only the **first** light lights the scene.
- On the Nokia N95 there's no gamepad or mouse: use `key` with the phone keypad names. Integers are 32-bit.

## Making it run fast (especially on the phone)

- Find objects only once, in `start`, and keep them in `local` variables.
- Use the "many at once" versions (`setVertices`, `setVerticesColor`) instead of calling the single one a thousand times.
- Prefer the engine's functions (`scrollUV`, `linePoint`, `setRotation` with an order) over doing the math in Lua.
- Measure with `luaProfile` and `clockMs` before optimizing blindly.
