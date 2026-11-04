// (prueba_shaders) el error esta en el renglon 4 del archivo: el mensaje tiene que decirlo
@fragmento
vec4 Fragmento() {
    vec3 c = esto_no_existe;
    return vec4(c, 1.0);
}
