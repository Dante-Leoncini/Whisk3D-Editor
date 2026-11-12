# Axes, units and objects

- **3D:** in scripts the **Z axis points up**, the same as in the Properties panel: X and Y are the floor (the width and the depth) and Z is the height. What you see in the panel is what the script reads and writes. Positions, rotations and scales are relative to the object's parent. Rotations are in **degrees**.
- **2D (screen and UI):** canvas pixels, with (0, 0) at the **center**; +x to the right and +y **down**.
- **Objects:** the functions that find or return objects (`yo`, `buscar`, `objeto`…) give you a value that has no fields or methods: you pass it to other functions. Two objects can be compared with `==`. If the object was destroyed, it behaves like `nil`.
- **Tolerance:** almost every function accepts `nil` or an object of the wrong type without breaking anything: the ones that read return 0, `""` or `false`, and the ones that write do nothing.
- **Indices:** as in Lua, almost everything starts at 1 (vertices, fingers, layers, routine steps). The exception is the routine memories (`memoria`), which go from 0 to 255.
