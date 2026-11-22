# Scenes and cutscenes

| Function | What it does |
|---|---|
| `changeScene(name [, restart])` | Switches to another scene (menu, level 2…) at the end of the frame. |
| `playScene(name [, "funcion"])` | Plays a 3D scene as a cutscene with the game paused; when it ends, it calls that function. |
| `stopScene3D()`, `scenePlaying()` | Stops the cutscene / tells you which one is playing. |
| `draw3D([on])` | Turns all the 3D off or on (for example, behind an opaque menu). |
