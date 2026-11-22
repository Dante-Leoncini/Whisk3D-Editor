// VIDRIO PLANO ESMERILADO (shader escrito de Whisk3D)
// Para un PLANO: una ventana, una mampara de baño, una pantalla de vidrio. Lo que hay detras se ve borroso.
//
// Del material usa:
//   Rugosidad    cuanto se borronea (0 = vidrio liso, 1 = muy esmerilado)
//   la textura   DONDE esta esmerilado: blanco = borroso, negro = liso (texturas/escarcha.png tiene manchas, una
//                ventana limpia y gotas que bajan). Sin textura, todo el vidrio es esmerilado
//   Color        el tinte del vidrio

@fragment
vec4 Fragment() {
    // cuanto se borronea ESTE punto
    float esmerilado = w3d_Texture(w3d_uv).r * w3d_roughness;

    // lo de atras, promediado en dos anillos de 8 lecturas alrededor del punto (17 en total)
    vec2 p = w3d_ScreenPoint();
    vec2 r = w3d_Pixel() * (1.0 + 26.0 * esmerilado);
    vec3 c = w3d_Screen(p).rgb;
    c += w3d_Screen(p + r * vec2( 0.0,  0.0)).rgb;
    c += w3d_Screen(p + r * vec2(-0.0,  0.0)).rgb;
    c += w3d_Screen(p + r * vec2( 0.0,  1.0)).rgb;
    c += w3d_Screen(p + r * vec2( 0.0, -1.0)).rgb;
    c += w3d_Screen(p + r * vec2( 0.7,  0.7)).rgb;
    c += w3d_Screen(p + r * vec2(-0.7,  0.7)).rgb;
    c += w3d_Screen(p + r * vec2( 0.7, -0.7)).rgb;
    c += w3d_Screen(p + r * vec2(-0.7, -0.7)).rgb;
    c += w3d_Screen(p + r * vec2( 1.85,  0.77)).rgb;
    c += w3d_Screen(p + r * vec2(-0.77,  1.85)).rgb;
    c += w3d_Screen(p + r * vec2(-1.85, -0.77)).rgb;
    c += w3d_Screen(p + r * vec2( 0.77, -1.85)).rgb;
    c += w3d_Screen(p + r * vec2( 0.77,  1.85)).rgb;
    c += w3d_Screen(p + r * vec2(-1.85,  0.77)).rgb;
    c += w3d_Screen(p + r * vec2(-0.77, -1.85)).rgb;
    c += w3d_Screen(p + r * vec2( 1.85, -0.77)).rgb;
    c /= 10.0;

    // el tinte del vidrio, y lo arenado queda un poco mas claro
    vec3 col = mix(c, c * w3d_color.rgb, 0.5) + vec3(0.10) * esmerilado;

    // el brillo de la luz, solo en lo liso
    vec3 medio = normalize(w3d_ToLight() + w3d_ToCamera());
    float brillo = pow(max(dot(w3d_Normal(), medio), 0.0), 20.0) * (1.0 - esmerilado);
    col += w3d_lightColor.rgb * brillo * 0.6;
    return vec4(col, 0.5);
}
