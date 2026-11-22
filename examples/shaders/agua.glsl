// AGUA (shader escrito de Whisk3D)
// Olas que se mueven (la seccion @vertex sube y baja los vertices) y lo de abajo visto a traves del agua, un poco
// corrido. Para que las olas se noten el plano necesita vertices: subdividilo (Modo Edicion, o el modificador
// Subdivision).

@vertex
void Vertex(inout vec3 pos, inout vec3 normal) {
    float ola = sin(pos.x * 2.0 + w3d_time * 2.0) * 0.1 + sin(pos.z * 3.0 + w3d_time * 1.5) * 0.05;
    pos.y += ola;
}

@fragment
vec4 Fragment() {
    vec3 n = w3d_Normal();
    vec2 p = w3d_ScreenPoint() + n.xy * 0.03;          // lo de abajo, un poco corrido
    vec3 fondo = w3d_Screen(p).rgb;
    float borde = pow(1.0 - max(dot(n, w3d_ToCamera()), 0.0), 3.0);
    return vec4(mix(fondo * w3d_color.rgb, vec3(0.8, 0.9, 1.0), borde * 0.6), 1.0);
}
