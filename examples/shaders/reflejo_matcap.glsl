// REFLEJO MATCAP (shader escrito de Whisk3D)
// Un "matcap" es una TEXTURA con una esfera pintada (texturas/matcap.png): cromo, arcilla, plastico, lo que sea.
// Cada pixel del objeto toma el color de la esfera en el punto que mira para el mismo lado. Es el reflejo mas
// barato que hay (una sola lectura de textura): anda bien hasta en un celular viejo. El reflejo sigue a la camara
// (no al mundo): para uno que se quede quieto, ver reflejo_entorno.glsl.
//
// Del material usa la textura (el matcap) y el Color (lo tine).

vec4 Fragmento() {
    vec3 n = w3d_Normal();                 // la normal de la VISTA: x a la derecha, y para arriba
    // el punto de la esfera pintada que mira para ahi (la v de la textura arranca ARRIBA de la imagen: y al reves)
    vec2 uv = vec2(n.x, -n.y) * 0.49 + 0.5;
    vec3 matcap = w3d_Textura(uv).rgb;
    return vec4(matcap * w3d_color.rgb, 1.0);
}
