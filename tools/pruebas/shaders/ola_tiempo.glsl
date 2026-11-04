// (prueba_shaders_fijo) el @vertice con w3d_tiempo: sube lo que dice la linea de tiempo (cuadro / FPS)
@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    pos.y += w3d_tiempo;
}
