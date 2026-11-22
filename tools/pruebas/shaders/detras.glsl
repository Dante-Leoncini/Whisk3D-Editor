// (prueba_shaders) lo que esta detras, sin tocar: el objeto se vuelve invisible si la copia de la pantalla anda
vec4 Fragment() {
    return w3d_Screen(w3d_ScreenPoint());
}
