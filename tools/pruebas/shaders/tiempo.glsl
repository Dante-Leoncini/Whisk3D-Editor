// (prueba_shaders) w3d_time hecho color: en el editor es el de la linea de tiempo (cuadro / FPS)
vec4 Fragment() {
    return vec4(fract(w3d_time), 0.0, 0.0, 1.0);
}
