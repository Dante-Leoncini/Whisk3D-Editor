// (prueba_shaders) w3d_tiempo hecho color: en el editor es el de la linea de tiempo (cuadro / FPS)
vec4 Fragmento() {
    return vec4(fract(w3d_tiempo), 0.0, 0.0, 1.0);
}
