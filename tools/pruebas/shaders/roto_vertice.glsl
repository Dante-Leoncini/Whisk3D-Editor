// (prueba_shaders_fijo) el error esta en el renglon 5 del archivo, adentro de @vertex
@vertex
void Vertex(inout vec3 pos, inout vec3 normal) {
    float a = 1.0;
    pos.y += esto_no_existe;
}
