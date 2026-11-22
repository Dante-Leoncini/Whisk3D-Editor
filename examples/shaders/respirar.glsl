// RESPIRAR (shader escrito de Whisk3D: solo @vertex, anda tambien en el N95)
// El objeto se infla y se desinfla: cada vertice se corre sobre su normal. Sirve para un corazon, un globo, un enemigo
// que "late". No necesita vertices de mas: anda con cualquier malla.

@vertex
void Vertex(inout vec3 pos, inout vec3 normal) {
    float respiro = 0.5 + 0.5 * sin(w3d_time * 3.0);         // 0..1
    pos += normal * respiro * 0.12;
}

@fragment
vec4 Fragment() {
    vec4 base = w3d_color * w3d_Texture(w3d_uv);
    return vec4(base.rgb * w3d_Light(w3d_Normal()), base.a);
}
