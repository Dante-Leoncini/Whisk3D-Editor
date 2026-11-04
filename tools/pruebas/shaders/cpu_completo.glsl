// (prueba_shaders / prueba_shaders_fijo) el @vertice con lo que entiende el interprete de la CPU: constantes, funciones,
// inout, for con continue, swizzles (leer y escribir), ternaria y funciones de GLSL. La cuenta da 3: el cubo sube 3 m.
// En la GPU (GLES 2.0 / Vulkan) y en la CPU (GLES 1.1, GL fijo) tiene que dar lo mismo
@vertice
const float UNO = 1.0;
float Suma(float a, float b) { return a + b; }
void Doblar(inout vec2 v) { v *= 2.0; }
void Vertice(inout vec3 pos, inout vec3 normal) {
    float s = 0.0;
    for (int i = 0; i < 3; i++) { if (i == 1) continue; s += UNO; }   // 2
    vec2 d = vec2(0.25, 0.5);
    Doblar(d);                                                           // (0.5, 1.0)
    s = Suma(s, d.y);                                                    // 3
    vec3 q = vec3(1.0);
    q.xz = vec2(clamp(-5.0, 0.0, 1.0), max(2.0, 1.0));                   // (0, 1, 2)
    s *= (q.z > 1.5) ? 1.0 : 0.0;                                        // 3
    s += length(normalize(vec3(3.0, 0.0, 4.0))) - 1.0;                   // + 0
    s += floor(fract(2.75) * 4.0) - 3.0;                                 // + 0
    pos.y += s;
    normal = normalize(normal);
}

@fragmento
vec4 Fragmento() {
    return vec4(0.0, 1.0, 0.0, 1.0);
}
