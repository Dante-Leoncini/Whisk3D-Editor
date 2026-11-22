// (prueba_shaders_fijo) el @vertex con w3d_time: sube lo que dice la linea de tiempo (cuadro / FPS)
@vertex
void Vertex(inout vec3 pos, inout vec3 normal) {
    pos.y += w3d_time;
}
