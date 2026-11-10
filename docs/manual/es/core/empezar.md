# Usar el Core directamente

Para quien programa en C o C++ y quiere un juego sin el editor. Hace falta:

- **Las fuentes del Core.** Los directorios de includes son la raíz del Core más `base/`, `graphics/`, `io/` y `thirdparty/`.
- **Un backend de `graphics/`**, el del sistema (por ejemplo `graphics/opengl/w3dGraphics.cpp`), y `base/w3dlog.cpp`. Después, los módulos que se usen (audio, archivos, física...).
- **Una ventana con su contexto.** Los ejemplos usan SDL2. Con el backend de shaders hay que llamar a `w3dEngine::GLES2Init(SDL_GL_GetProcAddress)` una vez, después de crear el contexto.

Lo mínimo, del ejemplo `simple_cube`:

```cpp
gfx::Viewport(0, 0, w, h);
gfx::ClearColor(0.10f, 0.12f, 0.16f, 1.0f);
gfx::Clear(gfx::ColorBuffer | gfx::DepthBuffer);
gfx::MatrixMode(gfx::Projection);  gfx::LoadIdentity();
gfx::Perspective(45.0f, (float)w / (float)h, 0.1f, 100.0f);
gfx::MatrixMode(gfx::ModelView);   gfx::LoadIdentity();
gfx::Translatef(0.0f, 0.0f, -5.0f);
gfx::Rotatef(angulo, 0.0f, 1.0f, 0.0f);
gfx::EnableArray(gfx::VertexArray); gfx::VertexPointer3f(0, cubeVerts);
gfx::EnableArray(gfx::NormalArray); gfx::NormalPointer3b(cubeNormals);
gfx::DrawTriangles(36, cubeIdx);
```

Los ejemplos se van subiendo a [Whisk3D-Examples](https://github.com/Dante-Leoncini/Whisk3D-Examples): `simple_cube` (un cubo con una luz), `textured_cube` (con una textura que carga `LoadTexture`) y `normal_map_cube` (relieve falso con DOT3). Ahí también está `game/w3drun`, el runtime que corre un juego hecho en el editor (escena, interfaz y Lua) sin el editor.

El estado actual, sin vueltas:

- **El Core todavía no está listo para producción.** Se sigue reescribiendo y cambia seguido.
- **Tiene lógica del editor adentro** (modo edición, modificadores, modos de render) que se está sacando. `tools/prueba_independencia.sh` mide cuánto le falta para usarse solo.
- **Los ejemplos se escribieron para una versión anterior** de la organización en carpetas: su CMake nombra `w3dGraphics.cpp` en la raíz del Core, que hoy está en `graphics/opengl/`. Puede hacer falta ajustar las rutas.
- **Lo que viene:** esqueletos, un sistema estilo BSP para mapas, física opcional, más backends retro, más ejemplos y documentación.

Las dudas, en el grupo de [Telegram](https://t.me/Whisk3D).

Si no hace falta bajar a C++, el camino corto es el editor: el juego se programa en [Lua](../lua/README.md) y la tarjeta **Juego** de [Propiedades](../editor/propiedades.md) lo compila para cada sistema. Por debajo usa el mismo Core.
