// SALTAR (shader escrito de Whisk3D: solo @vertice, anda tambien en el N95)
// Rebota y se aplasta al tocar el piso, como en los dibujos animados: sube con un seno "rectificado" y se ensancha
// cuando esta abajo. Todo en el @vertice: el objeto no se mueve de verdad (su posicion queda igual).

@vertice
void Vertice(inout vec3 pos, inout vec3 normal) {
    float fase = abs(sin(w3d_tiempo * 3.0));                   // 0 en el piso, 1 arriba
    float aplaste = smoothstep(0.25, 0.0, fase) * 0.35;         // solo cerca del piso
    pos.y = pos.y * (1.0 - aplaste) + fase * 1.2;
    pos.xz *= 1.0 + aplaste * 0.6;
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
