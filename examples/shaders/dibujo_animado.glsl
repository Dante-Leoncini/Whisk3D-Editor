// DIBUJO ANIMADO (shader escrito de Whisk3D)
// La luz en tres tonos fijos en vez de un degradado, como en los dibujos animados. Usa el Color y la textura.

vec4 Fragmento() {
    float l = dot(w3d_Normal(), w3d_HaciaLuz());
    float tono = l > 0.1 ? 1.0 : (l > 0.1 ? 0.65 : 0.35);
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * tono, base.a);
}
