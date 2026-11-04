// (prueba_shaders) lo que esta detras, sin tocar: el objeto se vuelve invisible si la copia de la pantalla anda
vec4 Fragmento() {
    return w3d_Pantalla(w3d_PuntoPantalla());
}
