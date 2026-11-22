// (prueba_shaders) la seccion @vertex mueve la malla 3 metros para arriba (Y, en el espacio del objeto)
@vertex
void Vertex(inout vec3 pos, inout vec3 normal) {
    pos.y += 3.0;
}

@fragment
vec4 Fragment() {
    return vec4(0.0, 1.0, 0.0, 1.0);
}
