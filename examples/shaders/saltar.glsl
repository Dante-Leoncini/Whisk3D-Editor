// SALTAR (shader escrito de Whisk3D: solo @vertex, anda tambien en el N95)
// Rebota y se aplasta al tocar el piso, como en los dibujos animados: sube con un seno "rectificado" y se ensancha
// cuando esta abajo. Todo en el @vertex: el objeto no se mueve de verdad (su posicion queda igual).

@vertex
void Vertex(inout vec3 pos, inout vec3 normal) {
    float fase = abs(sin(w3d_time * 3.0));                   // 0 en el piso, 1 arriba
    float aplaste = smoothstep(0.25, 0.0, fase) * 0.35;         // solo cerca del piso
    pos.y = pos.y * (1.0 - aplaste) + fase * 1.2;
    pos.xz *= 1.0 + aplaste * 0.6;
}

@fragment
vec4 Fragment() {
    vec4 base = w3d_color * w3d_Texture(w3d_uv);
    return vec4(base.rgb * w3d_Light(w3d_Normal()), base.a);
}
