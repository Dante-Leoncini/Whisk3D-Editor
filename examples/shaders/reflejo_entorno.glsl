// REFLEJO DE ENTORNO (shader escrito de Whisk3D)
// El objeto refleja lo que lo rodea. Lo que rodea es la TEXTURA del material: una foto panoramica
// "equirectangular" (el doble de ancha que de alta: 360 grados de costado y 180 de arriba a abajo), como
// texturas/entorno.png. Cada pixel calcula hacia donde rebota la mirada y lee la foto en esa direccion.
//
// Del material usa:
//   la textura   el entorno
//   Color        el tinte del reflejo (un metal dorado refleja dorado)
//   Metalico     0 = plastico (refleja poco, mas de costado), 1 = metal (todo es reflejo)

@fragmento
const float PI = 3.14159265;

// una direccion del MUNDO -> donde cae en la foto panoramica
vec2 Panorama(vec3 d) {
    float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;      // la vuelta alrededor (0..1)
    float v = acos(clamp(d.y, -1.0, 1.0)) / PI;       // de arriba (0) a abajo (1): la v de Whisk3D arranca arriba
    return vec2(u, v);
}

vec4 Fragmento() {
    vec3 n = w3d_Normal();
    vec3 ojo = w3d_HaciaCamara();

    // hacia donde REBOTA la mirada (en el espacio de la camara) y esa direccion en el MUNDO: el entorno queda quieto
    // aunque la camara gire
    vec3 r = reflect(-ojo, n);
    vec3 entorno = w3d_Textura(Panorama(normalize(w3d_AMundo(r)))).rgb;

    // de costado todo refleja mas (Fresnel); un metal refleja siempre
    float fresnel = 0.04 + 0.96 * pow(1.0 - max(dot(n, ojo), 0.0), 5.0);
    float cuanto = mix(fresnel, 1.0, w3d_metalico);

    vec3 superficie = w3d_color.rgb * w3d_Luz(n);
    vec3 reflejo = entorno * mix(vec3(1.0), w3d_color.rgb, w3d_metalico);
    return vec4(mix(superficie, reflejo, cuanto), 1.0);
}
