// (prueba_shaders) la seccion @vertice mueve la malla 3 metros para arriba (Y, en el espacio del objeto)
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    pos.y += 3.0;
}

@fragmento
vec4 Fragmento() {
    return vec4(0.0, 1.0, 0.0, 1.0);
}
