# Hand-written shaders

A **hand-written shader** is a text file (`.glsl`) that decides how a material is drawn, pixel by pixel. It's for effects the regular material doesn't have: frosted glass that shows what's behind it blurred, moving water, heat haze, cartoon shading, a pulsing glow…

You only write your own part: a `Fragmento()` function (the color of each pixel) and, if you want, a `Vertice()` one (to move the vertices). Whisk3D adds everything else.

## Where it works

The same file works the same way on every graphics backend:

| Backend | `@vertice` | `@fragmento` |
|---|---|---|
| OpenGL ES 2.0: Android, browser (WebGL), Nokia N8, PC with **Graphics → OpenGL ES 2.0** | On the GPU | On the GPU |
| Vulkan (PC) | On the GPU | On the GPU |
| PC OpenGL (the default one), OpenGL ES 1.1 (Nokia N95), Nintendo 3DS, Dreamcast | **On the CPU** | No: the material is drawn as usual |

On a backend without shaders nothing breaks: the **`@vertice` runs on the CPU** (the wavy water moves just the same on the N95) and the color comes from the normal material. The Material card says so: *"This graphics backend has no shaders: the @vertice section runs on the CPU and the rest of the material is drawn as usual"*.

### `@vertice` on the CPU

Where there are no shaders, Whisk3D includes an interpreter that reads the **same text** of `@vertice` and runs it vertex by vertex before drawing. You don't have to write anything different. It understands what is used to move vertices:

- `float`, `int`, `bool`, `vec2`, `vec3`, `vec4`; constants (`const float VELOCIDAD = 2.0;`) and your own functions (with `in`, `out`, `inout`);
- `if` / `else`, `for` (with `break` and `continue`), `return`, the ternary `? :`;
- operators, components (`pos.y`, `n.xz`, also for writing) and constructors (`vec3(...)`);
- GLSL's number functions: `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `pow`, `exp`, `log`, `sqrt`, `abs`, `sign`, `floor`, `ceil`, `fract`, `mod`, `min`, `max`, `clamp`, `mix`, `step`, `smoothstep`, `length`, `distance`, `dot`, `cross`, `normalize`, `reflect`, `refract`;
- `w3d_tiempo`, `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`.

It doesn't understand matrices, arrays, `while` or `#define`: if you use them, the error says so with its line (and on the GPU it would work). It runs once per frame over all the mesh's vertices: on the N95 it's best used on small meshes (a few hundred vertices, like the example water). It works if **all** parts of the mesh use the same shader; otherwise the mesh is drawn without moving.

> **Why the same file works everywhere:** the file has nothing specific to a backend. Each backend prepends its own part (the `w3d_*` inputs, the precision, the `main`). OpenGL ES 2.0 compiles it directly; Vulkan turns it into SPIR-V internally. If Whisk3D adds another backend tomorrow, your shader still works.

## Using a shader in a material

1. Select the object and open the **Material** tab in Properties.
2. At the very bottom, check **Advanced: custom shader**. If the material has no file yet, a new one (`shader.glsl`) is created from a template, used right away and opened in the IDE.
3. In **Shader** you choose the file:
   - the `.glsl` files already in the project;
   - **New shader**: another file with the template;
   - **Load from disk...**: a `.glsl` from your computer. It is copied into the project, like textures and scripts.
4. The small button to the right of **Shader** opens the file in the **IDE** (the layout needs an IDE).
5. Write, and **Ctrl+S** saves and compiles. You can also press **Compile** in the material or in the IDE bar (it appears next to the file name when it's a `.glsl`).
6. The result shows below **Compile**: **Compiled**, or the error in red with its line.

You don't need to save the project to get started. If you haven't saved it yet, the shader is written to an editor work folder; when you save the project it goes inside the `.w3d`, in the `shaders/` folder, and the material and the IDE switch to that copy. From then on it travels with the project (to the phone, to the built game, to another computer). Uncheck it and the material goes back to being drawn as usual; the file stays selected.

If a shader doesn't compile, the object is drawn with its normal material until you fix it. The editor doesn't crash.

### From the Library: dragging a shader onto a mesh

The project's shaders appear in the **Library** (the Outliner in Library mode), under **Shaders**, with how many objects use them. From there:

- **Drag it onto a mesh** in the 3D Viewport: while you move it, the **mesh part** under the pointer is drawn with that shader, so you can see which part you're going to apply it to. If you move to another part or leave the mesh, it goes back to its material. When you drop it, that part's material switches to the shader (with the default material, the part gets a new material). **Ctrl+Z** undoes it.
- The same with a **material**: while you drag it, the part shows with it.
- **Open in IDE**, rename it, move it to another folder or take it out of the `.w3d`, like a script.
- A `.glsl` dragged **from your computer's file explorer** into the window enters the project; if you drop it on a mesh, it is also applied to that part.

## What the file looks like

```glsl
// comments go with // or /* */

@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    // optional: move the vertices
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
```

- **`@fragmento`** marks where the pixel code starts and **`@vertice`** where the vertex code starts. Both are optional: a file without marks is all fragment.
- **`vec4 Fragmento()`** is required: it returns the pixel color (red, green, blue, alpha; from 0 to 1).
- **`void Vertice(inout vec3 pos, inout vec3 normal)`** is optional. It receives the vertex position and normal in **object** space (Y up, in meters) and you can change them.
- You can write your own functions and constants (`const float VELOCIDAD = 2.0;`), above where you use them.
- The language is **GLSL ES 1.00**, the one in WebGL 1. You have `float`, `vec2/3/4`, `mat3/4`, `sin`, `cos`, `mix`, `clamp`, `pow`, `dot`, `normalize`, `reflect`, `refract`, `fract`, `smoothstep` and the rest of GLSL.
- **Don't** write `main`, or declare `uniform`, `attribute`, `varying` or `precision`: Whisk3D adds those.
- Errors give the line in **your file** (for example `0(4)` or `w3d.frag:4` is line 4).

## What you can use

### In `Fragmento()`

| Name | Type | What it is |
|---|---|---|
| `w3d_uv` | `vec2` | The UV coordinates of the point (from the first UV layer). |
| `w3d_Normal()` | `vec3` | The point's normal, already normalized, in camera space. If the back face is visible, it points the other way. |
| `w3d_normal` | `vec3` | The normal, not normalized or flipped. |
| `w3d_posVista` | `vec3` | The point's position in camera space (the camera is at 0,0,0 looking toward −Z). |
| `w3d_HaciaCamara()` | `vec3` | The direction from the point toward the camera. |
| `w3d_HaciaLuz()` | `vec3` | The direction from the point toward the first light. |
| `w3d_Luz(n)` | `vec3` | The light that reaches it with normal `n`: the ambient plus the first light (diffuse). |
| `w3d_luzColor` | `vec4` | The color of the first light (with its intensity). |
| `w3d_luzPos` | `vec4` | The position of the first light in camera space (`w` = 0: it's a direction, the sun). |
| `w3d_ambiente` | `vec4` | The ambient light. |
| `w3d_color` | `vec4` | The material's **Color** (with its alpha). |
| `w3d_emision` | `vec4` | The material's **Emission**. |
| `w3d_Textura(uv)` | `vec4` | The material's texture at those UVs (white if it has none). `v` starts at the **top** of the image (0 = the top row, 1 = the bottom one). |
| `w3d_colorVertice` | `vec4` | The vertex color (if the mesh has one and the material uses **Vertex color**). |
| `w3d_rugosidad`, `w3d_metalico` | `float` | The material's **Roughness** and **Metallic**. |
| `w3d_ior`, `w3d_transmision` | `float` | The material's **IOR** and **Transmission** (the ray tracing ones). |
| `w3d_tiempo` | `float` | Seconds, for animating. In the editor it is the **timeline** time (the current frame ÷ the FPS): paused, it stays still; with **Play** it advances; and by scrubbing the timeline you can see any moment. In the exported game it is the time since it started. |
| `w3d_PuntoPantalla()` | `vec2` | Where the pixel is in the view: (0,0) bottom left, (1,1) top right. |
| `w3d_Pixel()` | `vec2` | The size of one pixel in those same units. |
| `w3d_Pantalla(p)` | `vec4` | **What is already drawn** at point `p` of the view. See below. |
| `w3d_AMundo(v)` | `vec3` | Converts a direction from camera space to **world** space (Y up). Useful for reading an environment map that doesn't turn with the camera (see *Environment reflection*). |

### In `Vertice()`

`pos` and `normal` (of the object), and `w3d_tiempo`.

## Seeing what's behind: `w3d_Pantalla`

A material that reads the screen with `w3d_Pantalla` can show what's behind the object, and shift it, blur it or tint it. That's how you make glass, water, hot air or a force field.

- Meshes with a shader that reads the screen are drawn **last**, after the rest of the scene, so what's behind them is already drawn. It doesn't matter where they are in the Outliner.
- Before each one, the view is copied to a texture (one copy per mesh and per mesh part). On a PC or a modern phone it's cheap. On an old phone, better keep such objects few.
- **Particles** are drawn afterwards, so they don't show through the glass.
- The material should **not** be **Transparent**: the shader already blends what's behind and returns alpha 1.

## Examples

All of these are files in **`examples/shaders/`** in the repository, with their textures in `examples/shaders/texturas/`, and applied to objects in the project **`examples/shaders/Ejemplos_Shaders.w3d`**: open it and try them out. You can also drag the `.glsl` files into your project.

| File | What it does |
|---|---|
| `vidrio_esmerilado.glsl` | Glass for any shape: what's behind blurred and bent. |
| `vidrio_plano.glsl` | Frosted glass for a **plane** (a window, a glass partition); the texture says where it is frosted. |
| `reflejo_entorno.glsl` | Metal or plastic that reflects a panoramic photo of the surroundings (the texture). |
| `reflejo_matcap.glsl` | The cheapest reflection: a texture with a painted sphere. |
| `latido.glsl` | A pulsing glow (`w3d_tiempo`). |
| `dibujo_animado.glsl` | Light in three tones. |
| `agua.glsl` | Waves with `@vertice` and what's below shifted. |
| `bandera.glsl` | A flag waving (`@vertice` only: also works on the N95). |
| `respirar.glsl` | It inflates and deflates along its normals (`@vertice` only). |
| `torcer.glsl` | It twists around the vertical axis (`@vertice` only). |
| `saltar.glsl` | It bounces and squashes when it hits the floor (`@vertice` only). |

The vertex ones are applied to small meshes in **`examples/shaders/Ejemplos_Vertex_N95.w3d`**, made for the N95 (where `@vertice` runs on the CPU). They move with the timeline's **Play**.

### A pulsing color

```glsl
vec4 Fragmento() {
    float latido = 0.5 + 0.5 * sin(w3d_tiempo * 4.0);
    vec3 luz = w3d_Luz(w3d_Normal());
    return vec4(w3d_color.rgb * luz + vec3(1.0, 0.2, 0.1) * latido * 0.5, 1.0);
}
```

A shader that uses `w3d_tiempo` moves with the timeline's **Play** (like an animation); when paused it stays still and the editor doesn't redraw needlessly. In the exported game it moves on its own.

### Cartoon (three tones)

```glsl
vec4 Fragmento() {
    float l = dot(w3d_Normal(), w3d_HaciaLuz());
    float tono = l > 0.6 ? 1.0 : (l > 0.1 ? 0.65 : 0.35);
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * tono, base.a);
}
```

### Moving water (`@vertice`)

```glsl
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    float ola = sin(pos.x * 2.0 + w3d_tiempo * 2.0) * 0.1 + sin(pos.z * 3.0 + w3d_tiempo * 1.5) * 0.05;
    pos.y += ola;
}

@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 p = w3d_PuntoPantalla() + n.xy * 0.03;          // what's below, slightly shifted
    vec3 fondo = w3d_Pantalla(p).rgb;
    float borde = pow(1.0 - max(dot(n, w3d_HaciaCamara()), 0.0), 3.0);
    return vec4(mix(fondo * w3d_color.rgb, vec3(0.8, 0.9, 1.0), borde * 0.6), 1.0);
}
```

For the wave to show, the plane needs vertices: subdivide it in Edit Mode.

### Frosted glass

This is `examples/shaders/vidrio_esmerilado.glsl` (the glass sphere in the examples project). What's behind looks blurred and slightly bent:

```glsl
@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // refraction: the screen is read shifted according to the normal
    vec2 p = w3d_PuntoPantalla() + n.xy * (w3d_ior - 1.0) * 0.15;

    // the blur: the average of 13 reads around it; roughness widens the circle
    vec2 r = w3d_Pixel() * (1.0 + 18.0 * w3d_rugosidad);
    vec3 c = w3d_Pantalla(p).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 2.5,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  2.5)).rgb;
    c += w3d_Pantalla(p + r * vec2(-2.5, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0, -2.5)).rgb;
    c /= 13.0;

    vec3 atras = c * mix(vec3(1.0), w3d_color.rgb, 0.35);     // tinted by the glass color
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 col = mix(superficie, atras, w3d_transmision);

    // the light's highlight and the brighter edge
    vec3 medio = normalize(w3d_HaciaLuz() + ojo);
    float brillo = pow(max(dot(n, medio), 0.0), 80.0) * (1.0 - w3d_rugosidad);
    float borde = pow(1.0 - max(dot(n, ojo), 0.0), 3.0);
    col += w3d_luzColor.rgb * brillo + vec3(0.2) * borde;
    return vec4(col, 1.0);
}
```

It is controlled from the material: **Transmission** (0 = opaque glass in the material color, 1 = everything behind shows), **IOR** (how much what's behind is bent) and **Roughness** (how much it blurs). They're the same values **ray tracing** uses, so in Render Preview the same object is real glass.

### Flat frosted glass

For a **plane**: a window, a glass partition, a glass screen. It's `examples/shaders/vidrio_plano.glsl`. It averages 17 reads of what's behind in two rings; **Roughness** widens the rings. The material's **texture** says *where* it is frosted: white = blurred, black = clear. With `texturas/escarcha.png` you get smudges, a small clean window and drops running down; without a texture, the whole glass is frosted. Better turn off **Culling** on the plane so it can be seen from both sides.

```glsl
float esmerilado = w3d_Textura(w3d_uv).r * w3d_rugosidad;   // how much THIS point blurs
vec2 r = w3d_Pixel() * (1.0 + 26.0 * esmerilado);           // the radius of the reads
```

### Environment reflection (how to use a texture for reflections)

It's `examples/shaders/reflejo_entorno.glsl`. The **material's texture** is a **panoramic** photo of what surrounds the object: an *equirectangular* image, twice as wide as it is tall, covering 360° around and 180° from top to bottom (like `texturas/entorno.png`: sky, sun, buildings and a checkered floor). For each pixel:

1. `reflect(-ojo, n)` gives the direction the **view bounces** to, in camera space.
2. `w3d_AMundo(...)` converts that direction to the **world**, so the reflection stays still even when the camera turns.
3. `Panorama(...)` turns the direction into a point on the photo: the angle around (`atan`) is `u` and the angle from top to bottom (`acos`) is `v`.
4. It is blended with the surface according to **Fresnel** (more reflection at grazing angles) and **Metallic** (a metal reflects everything, tinted by its **Color**).

```glsl
@fragmento
const float PI = 3.14159265;

vec2 Panorama(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;      // the turn around (0..1)
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;       // from top (0) to bottom (1)
    return vec2(u, v);
}

vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();
    vec3 r = reflect(-ojo, n);
    vec3 entorno = w3d_Textura(Panorama(normalize(w3d_AMundo(r)))).rgb;
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(n, ojo), 0.0), 5.0);
    float cuanto = mix(fresnel, 1.0, w3d_metalico);
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 reflejo = entorno * mix(vec3(1.0), w3d_color.rgb, w3d_metalico);
    return vec4(mix(superficie, reflejo, cuanto), 1.0);
}
```

Any 2:1 panoramic photo works (360° photos from a phone, or HDRIs converted to PNG).

### Matcap reflection

It's `examples/shaders/reflejo_matcap.glsl`: a single texture read, so it runs on any GPU. The texture is a **painted sphere** (`texturas/matcap.png`); each pixel takes the color of the point on the sphere that faces the same way as its normal. The reflection follows the camera (not the world).

```glsl
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec2 uv = vec2(n.x, -n.y) * 0.49 + 0.5;   // (the texture's v starts at the top: y flipped)
    return vec4(w3d_Textura(uv).rgb * w3d_color.rgb, 1.0);
}
```

## In a routine: the **Shader** step

A hand-written shader is like a **GL state**: you set it and whatever is drawn afterwards uses it. In a **routine** (Add → Draw → **Shader**) the step has the `.glsl` **file**, the **On** checkbox and four numbers: **IOR**, **Transmission**, **Roughness** and **Metallic** (they arrive as `w3d_ior`, `w3d_transmision`, `w3d_rugosidad`, `w3d_metalico`; they can be animated or read from a memory).

```
Shader          vidrio_plano.glsl  On   (IOR 1.5, Roughness 0.6)
Vertex pointer  Ventana
Draw elements   Ventana
Shader          Off
Draw elements   Marco           ← no shader from here on
```

With **On**, the "Draw elements" that follow use the shader; with the checkbox off it is removed. When the routine ends it is removed automatically. It works on **OpenGL ES 2.0 and Vulkan**; on OpenGL ES 1.1 and fixed-function GL the step does nothing (a routine's draws point directly at the mesh data: `@vertice` on the CPU is for materials). A shader that reads the screen (`w3d_Pantalla`) sees what the routine and the scene drew **before** that step: you decide the order.

## With ray tracing

Ray tracing doesn't run your shader. It uses the material values: **Color**, **Roughness**, **Metallic**, **Transmission**, **IOR** and the texture. If your shader uses those same values (like the frosted glass), Material Preview and Render Preview agree.

**Transmission** (0 to 1, in the Material card with ray tracing on) says how much of the surface is glass: light passes through bent by the **IOR** and, with **Roughness**, comes out frosted. **Alpha** is something else: what is transparent through alpha (or its map) isn't there, and through it you see what's behind straight, unbent. With 50 % alpha and transmission 1, half passes straight and the other half refracted. With transmission 0, the IOR bends what is seen through the alpha, as in previous versions.

## What it doesn't do (yet)

- It uses a single light: the first one in the scene (or the preview's).
- It doesn't get the **fog** or the mirrors' **clipping planes**.
- In **Solid** and in the debug views (normals, alpha, vectors) the normal material shows.
- It has no parameters of its own: it uses the material's (color, roughness, metallic, transmission, IOR, emission).

## Common errors

| Message | What's going on |
|---|---|
| `undefined variable` / `undeclared identifier` | A misspelled name or one that doesn't exist (`w3d_Normal()` with parentheses?). |
| `cannot convert` / `wrong operand types` | Mixed types: `1` is an `int`, write `1.0`. A `vec3` can't be added to a `vec4`. |
| `Fragmento: no matching overloaded function` or it doesn't compile without `Fragmento` | The `vec4 Fragmento()` function is missing or returns another type. |
| `redefinition of main` | Don't write `main`: Whisk3D adds it. |
| What's behind doesn't show | The shader uses `w3d_Pantalla` but the material is **Transparent** with low alpha, or what's behind is drawn later (particles). |
