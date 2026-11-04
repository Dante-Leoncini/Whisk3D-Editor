// BANDERA (shader escrito de Whisk3D: solo @vertice, anda tambien en el N95)
// Una bandera que flamea: los vertices se corren en Z con una onda que avanza por X. Cerca del asta (X chica) casi no
// se mueve; en la punta, mas. Para que se note, el plano necesita vertices (modificador Subdivision en Simple).
// En GLES 1.1 (N95) y el GL fijo esta seccion corre en la CPU; el @fragmento lo usan GLES 2.0 y Vulkan.

@vertice
const float ALTO = 0.25;        // cuanto se ondula en la punta (metros)
const float VELOCIDAD = 4.0;

void Vertice(inout vec3 pos, inout vec3 normal) {
    float desdeAsta = clamp((pos.x + 1.0) * 0.5, 0.0, 1.0);    // 0 en el asta, 1 en la punta (plano de -1 a 1)
    float onda = sin(pos.x * 3.0 - w3d_tiempo * VELOCIDAD) + 0.5 * sin(pos.y * 2.0 - w3d_tiempo * 2.5);
    pos.z += onda * ALTO * desdeAsta;
    // la normal se inclina con la pendiente de la onda (para que la luz siga los pliegues)
    float pendiente = cos(pos.x * 3.0 - w3d_tiempo * VELOCIDAD) * 3.0 * ALTO * desdeAsta;
    normal = normalize(normal + vec3(-pendiente, 0.0, 0.0));
}

@fragmento
vec4 Fragmento() {
    vec4 base = w3d_color * w3d_Textura(w3d_uv);
    return vec4(base.rgb * w3d_Luz(w3d_Normal()), base.a);
}
