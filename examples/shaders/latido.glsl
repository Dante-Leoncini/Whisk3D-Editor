// LATIDO (shader escrito de Whisk3D)
// Un brillo rojo que late, sobre el color del material con su luz. Usa w3d_time (los segundos): en el editor es
// el de la linea de tiempo (se mueve con Play), en el juego el reloj.

vec4 Fragment() {
    float latido = 0.5 + 0.5 * sin(w3d_time * 4.0);
    vec3 luz = w3d_Light(w3d_Normal());
    return vec4(w3d_color.rgb * luz + vec3(1.0, 0.2, 0.1) * latido * 0.5, 1.0);
}
