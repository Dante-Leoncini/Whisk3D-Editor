// VIDRIO ESMERILADO (shader escrito de Whisk3D)
// Lo que hay detras del objeto se ve BORROSO y un poco doblado, como a traves de un vidrio arenado.
// Anda igual en GLES 2.0 (Android, web, N8) y en Vulkan: el motor pone el resto.
//
// Usa los valores del material (asi el trazado de rayos y este shader dicen lo mismo):
//   Transmision  0 = vidrio opaco del color del material, 1 = se ve todo lo de atras
//   IOR          cuanto se dobla lo de atras (1 = nada, 1.5 = vidrio)
//   Rugosidad    cuanto se borronea (0 = vidrio liso, 1 = muy esmerilado)

@fragmento
vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // LA REFRACCION: se lee la pantalla corrida segun la normal (de frente casi nada, en los bordes mas)
    vec2 p = w3d_PuntoPantalla() + n.xy * (w3d_ior - 1.0) * 0.15;

    // LO BORROSO: el promedio de 13 lecturas en dos anillos alrededor del punto. La rugosidad abre los anillos
    vec2 r = w3d_Pixel() * (1.0 + 18.0 * w3d_rugosidad);
    vec3 c = w3d_Pantalla(p).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  0.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 0.0, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4,  1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.4, -1.4)).rgb;
    c += w3d_Pantalla(p + r * vec2( 2.5,  1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2(-1.0,  2.5)).rgb;
    c += w3d_Pantalla(p + r * vec2(-2.5, -1.0)).rgb;
    c += w3d_Pantalla(p + r * vec2( 1.0, -2.5)).rgb;
    c /= 13.0;

    // lo de atras sale un poco tenido del color del vidrio
    vec3 atras = c * mix(vec3(1.0), w3d_color.rgb, 0.35);
    // la superficie del vidrio con su luz (lo que se ve con poca transmision)
    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 col = mix(superficie, atras, w3d_transmision);

    // el brillo de la luz y el borde mas claro (de costado un vidrio refleja mas: Fresnel)
    vec3 medio = normalize(w3d_HaciaLuz() + ojo);
    float brillo = pow(max(dot(n, medio), 0.0), 80.0) * (1.0 - w3d_rugosidad);
    float borde = pow(1.0 - max(dot(n, ojo), 0.0), 3.0);
    col += w3d_luzColor.rgb * brillo + vec3(0.2) * borde;

    return vec4(col, 1.0);
}
