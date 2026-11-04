// TORCER (shader escrito de Whisk3D: solo @vertice, anda tambien en el N95)
// El objeto se retuerce alrededor de su eje vertical: cada vertice gira en Y un angulo que crece con la altura y va y
// viene con el tiempo. La normal gira igual (la luz sigue la torcedura). Mejor con una malla alta y con cortes.

@vertice
void Girar(inout vec3 v, float a) {
    float c = cos(a), s = sin(a);
    v = vec3(v.x * c - v.z * s, v.y, v.x * s + v.z * c);
}

void Vertice(inout vec3 pos, inout vec3 normal) {
    float angulo = pos.y * 1.2 * sin(w3d_tiempo * 1.5);
    Girar(pos, angulo);
    Girar(normal, angulo);
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
