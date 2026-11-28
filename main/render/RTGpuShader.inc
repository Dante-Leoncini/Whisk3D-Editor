// ============================================================================
//  RTGpuShader.inc - los compute shaders del trazado de rayos por GPU (RTGpu.cpp). GLSL 4.50 de Vulkan; el de trazar
//  es el MISMO algoritmo que RayTracer.cpp (Trazar, Visibilidad, LightIn, Radiancia, FirstImpact): si se cambia uno,
//  cambiar el otro. La recursion de los reflejos va con una pila (GLSL no tiene recursion).
// ============================================================================
// (sin #version: RTGpu.cpp pone "#version 450", o "#version 460" + GL_EXT_ray_query + W3D_RTX con los nucleos RTX)
static const char* kRTCommonGpu = R"GLSL(
layout(local_size_x = 8, local_size_y = 8) in;
struct Vert { vec4 p; vec4 n; vec4 t; };          // p.w = u, n.w = v
struct Mat { vec4 m0; vec4 m1; vec4 m2; vec4 m3; vec4 m4; vec4 m5; vec4 m6; };
// m0 = difuso.rgb alfa | m1 = emision.rgb rugosidad | m2 = metalico f0 banderas ior | m3 = tex nmTex emTex rugTex | m4 = metTex alfaTex recorte transmision
// m5 = (sin uso) | m6 = -, -, -, la fuerza de la emision
struct Luz { vec4 p; vec4 d; vec4 c; vec4 amb; vec4 att; vec4 area; };
// p = pos.xyz direccional | d = dir.xyz (la del sol, o hacia donde alumbra un foco o una de area) radio | c = color.rgb rayos |
// amb = ambiente.rgb, coseno del cono del foco (-1 = sin cono) | att = constante lineal cuadratica, exponente del foco |
// area = la LUZ DE AREA (Light.h): su medio ancho (xyz, en el mundo) y su medio alto (w; negativo = elipse). 0 = no es de area
struct Nodo { vec4 a; vec4 b; vec4 c; };          // a = bmin izq, b = bmax der, c = primero cantidad
layout(std430, binding = 0) readonly buffer BVerts { Vert verts[]; };
layout(std430, binding = 1) readonly buffer BTris { ivec4 tris[]; };
layout(std430, binding = 2) readonly buffer BMats { Mat mats[]; };
layout(std430, binding = 3) readonly buffer BLuces { Luz luces[]; };
layout(std430, binding = 4) readonly buffer BNodos { Nodo nodos[]; };
layout(std430, binding = 5) readonly buffer BOrden { int orden[]; };
layout(std430, binding = 6) readonly buffer BTexInfo { ivec4 texInfo[]; };
layout(std430, binding = 7) readonly buffer BTexels { uint texels[]; };
layout(std430, binding = 8) buffer BAcum { vec4 acum[]; };
layout(std430, binding = 9) buffer BImg { uint img[]; };
layout(std430, binding = 10) buffer BAlb { vec4 aovAlb[]; };
layout(std430, binding = 11) buffer BNrm { vec4 aovNrm[]; };
layout(std430, binding = 12) buffer BRayos { uint rayosTotal[]; };   // los rayos trazados (para medir)
// LAS CAUSTICAS (RayTracer.cpp, PhotonsShoot / CausticsIn): los fotones del pase, ordenados por celda, y la grilla
// (causInfo = radio, 1/lado de la celda, celdas, prendidas; celdas[k] = el primer foton de la celda k)
struct Foton { vec4 p; vec4 pw; vec4 n; };
layout(std430, binding = 13) readonly buffer BFotones { Foton fotones[]; };
layout(std430, binding = 14) readonly buffer BCaus { vec4 causInfo; int celdas[]; };
uint gRayos = 0u;
layout(push_constant) uniform PC {
    vec4 camPos;    // w = sx de la lente (ndc = e.x / sx [/ ez] * zoom + pan)
    vec4 camDer;    // w = sy
    vec4 camArr;    // w = zoom
    vec4 camAdel;   // w = 1 ortografica
    ivec4 dims;     // ancho alto paso samples
    ivec4 info;     // luces nodos capas-transparentes (0 = no hay) aov
    vec4 bg;        // w = el radio de la LENTE (m: la profundidad de campo; 0 = agujerito)
    vec4 lente;     // pan x, pan y, la SEMILLA de esta imagen (entera), la distancia ENFOCADA (m)
} pc;

uint gRng;
float Azar() { gRng ^= gRng << 13; gRng ^= gRng >> 17; gRng ^= gRng << 5; return float(gRng & 0xFFFFFFu) / 16777216.0; }

struct Hit { float t; float b1; float b2; int tri; };
// LO QUE HACE CADA OBJETO CON LA LUZ (RTTri::fl de RayTracer.cpp, en los 8 bits altos de tris[].w; los de abajo son el
// material): un rayo de SOMBRA solo ve lo que PROYECTA, uno de REBOTE solo lo que REBOTA, y en lo que no RECIBE las
// lamparas llegan enteras. Antes la GPU no lo sabia: el cuadro del fondo de la camara (sin proyectar sombras) le tapaba
// el sol a toda la escena y el render salia sin sol ni sombras
const int RT_PROYECTA = 1, RT_REBOTA = 2, RT_RECIBE = 4;
int MaterialDe(int tri) { return tris[tri].w & 0xFFFFFF; }
int BanderasDe(int tri) { return (tris[tri].w >> 24) & 255; }
bool PasaDeLargo(int tri, int mascara) { return mascara != 0 && (BanderasDe(tri) & mascara) != mascara; }
bool RayoTri(int tri, vec3 o, vec3 d, float tMin, float tMax, inout Hit h) {
    ivec4 T = tris[tri];
    vec3 p0 = verts[T.x].p.xyz, p1 = verts[T.y].p.xyz, p2 = verts[T.z].p.xyz;
    vec3 e1 = p1 - p0, e2 = p2 - p0;
    vec3 pv = cross(d, e2);
    float det = dot(e1, pv);
    if (det > -1e-9 && det < 1e-9) return false;
    float inv = 1.0 / det;
    vec3 tv = o - p0;
    float b1 = dot(tv, pv) * inv; if (b1 < 0.0 || b1 > 1.0) return false;
    vec3 qv = cross(tv, e1);
    float b2 = dot(d, qv) * inv; if (b2 < 0.0 || b1 + b2 > 1.0) return false;
    float t = dot(e2, qv) * inv;
    if (t <= tMin || t >= tMax) return false;
    h.t = t; h.b1 = b1; h.b2 = b2; h.tri = tri;
    return true;
}
bool RayoCaja(vec3 bmin, vec3 bmax, vec3 o, vec3 invD, float tMax) {
    vec3 ta = (bmin - o) * invD, tb = (bmax - o) * invD;
    vec3 mn = min(ta, tb), mx = max(ta, tb);
    float t0 = max(max(mn.x, mn.y), max(mn.z, 0.0)), t1 = min(min(mx.x, mx.y), min(mx.z, tMax));
    return t0 <= t1;
}
// donde entra el rayo a la caja (1e30 = no la toca antes de tMax)
float EntraCaja(vec3 bmin, vec3 bmax, vec3 o, vec3 invD, float tMax) {
    vec3 ta = (bmin - o) * invD, tb = (bmax - o) * invD;
    vec3 mn = min(ta, tb), mx = max(ta, tb);
    float t0 = max(max(mn.x, mn.y), max(mn.z, 0.0)), t1 = min(min(mx.x, mx.y), min(mx.z, tMax));
    return t0 <= t1 ? t0 : 1e30;
}
#ifdef W3D_RTX
// CON LOS NUCLEOS DE TRAZADO (VK_KHR_ray_query): la busqueda la hace el hardware sobre su propia estructura (la
// escena entera en un BLAS, con los triangulos en el MISMO orden: el indice de primitiva es el del triangulo). Opaco
// salvo cuando hay que saltear el triangulo ya atravesado (las capas transparentes)
layout(binding = 15) uniform accelerationStructureEXT gTlas;
// (mascara: RT_PROYECTA / RT_REBOTA, o 0 = ve todo; lo que no la tiene le pasa de largo)
bool TrazarM(vec3 o, vec3 d, float tMin, float tMax, bool sombra, out Hit mejor, int excluido, int mascara) {
    gRayos++;
    mejor.t = tMax; mejor.b1 = 0.0; mejor.b2 = 0.0; mejor.tri = -1;
    if (pc.info.y == 0) return false;
    const bool filtra = excluido >= 0 || mascara != 0;   // (hay triangulos que saltear: cada candidato pasa por aca)
    uint flags = filtra ? gl_RayFlagsNoOpaqueEXT : gl_RayFlagsOpaqueEXT;
    if (sombra && !filtra) flags |= gl_RayFlagsTerminateOnFirstHitEXT;
    rayQueryEXT q;
    rayQueryInitializeEXT(q, gTlas, flags, 0xFF, o, tMin, d, tMax);
    while (rayQueryProceedEXT(q)) {
        if (rayQueryGetIntersectionTypeEXT(q, false) == gl_RayQueryCandidateIntersectionTriangleEXT) {
            int tri = rayQueryGetIntersectionPrimitiveIndexEXT(q, false);
            if (tri == excluido || PasaDeLargo(tri, mascara)) continue;
            rayQueryConfirmIntersectionEXT(q);
            if (sombra) rayQueryTerminateEXT(q);
        }
    }
    if (rayQueryGetIntersectionTypeEXT(q, true) == gl_RayQueryCommittedIntersectionNoneEXT) return false;
    vec2 b = rayQueryGetIntersectionBarycentricsEXT(q, true);
    mejor.t = rayQueryGetIntersectionTEXT(q, true); mejor.b1 = b.x; mejor.b2 = b.y;
    mejor.tri = rayQueryGetIntersectionPrimitiveIndexEXT(q, true);
    return true;
}
#else
bool TrazarM(vec3 o, vec3 d, float tMin, float tMax, bool sombra, out Hit mejor, int excluido, int mascara) {
    gRayos++;
    mejor.t = tMax; mejor.b1 = 0.0; mejor.b2 = 0.0; mejor.tri = -1;
    if (pc.info.y == 0) return false;
    vec3 invD = vec3(1.0 / (abs(d.x) > 1e-12 ? d.x : 1e-12), 1.0 / (abs(d.y) > 1e-12 ? d.y : 1e-12), 1.0 / (abs(d.z) > 1e-12 ? d.z : 1e-12));
    int pila[64]; int sp = 0; pila[sp++] = 0;
    bool hay = false;
    while (sp > 0) {
        Nodo n = nodos[pila[--sp]];
        if (!RayoCaja(n.a.xyz, n.b.xyz, o, invD, mejor.t)) continue;
        int cant = int(n.c.y);
        if (cant > 0) {
            int primero = int(n.c.x);
            for (int i = 0; i < cant; i++) {
                int tri = orden[primero + i];
                if (tri == excluido || PasaDeLargo(tri, mascara)) continue;
                Hit h;
                if (RayoTri(tri, o, d, tMin, mejor.t, h)) { mejor = h; hay = true; if (sombra) return true; }
            }
        } else {
            // los hijos que toca, el mas cercano arriba de la pila (se recorre primero)
            int izq = int(n.a.w), der = int(n.b.w);
            float ti = izq >= 0 ? EntraCaja(nodos[izq].a.xyz, nodos[izq].b.xyz, o, invD, mejor.t) : 1e30;
            float td = der >= 0 ? EntraCaja(nodos[der].a.xyz, nodos[der].b.xyz, o, invD, mejor.t) : 1e30;
            if (ti > td) { int x = izq; izq = der; der = x; float y = ti; ti = td; td = y; }
            if (td < 1e29 && sp < 63) pila[sp++] = der;
            if (ti < 1e29 && sp < 63) pila[sp++] = izq;
        }
    }
    return hay;
}
#endif
bool Trazar(vec3 o, vec3 d, float tMin, float tMax, bool sombra, out Hit mejor, int excluido) {
    return TrazarM(o, d, tMin, tMax, sombra, mejor, excluido, 0);
}
vec4 Texel(ivec4 I, int x, int y) { return unpackUnorm4x8(texels[I.x + y * I.y + x]); }
vec4 Muestrear(int ti, vec2 uv) {
    ivec4 I = texInfo[ti];   // desde, ancho, alto, filtrado
    uv -= floor(uv);
    if (I.w != 0) {   // BILINEAL entre los 4 texeles de alrededor (repetido en los bordes)
        vec2 f = uv * vec2(I.yz) - 0.5;
        vec2 b = floor(f), a = f - b;
        int x0 = int(b.x) % I.y, y0 = int(b.y) % I.z;
        if (x0 < 0) x0 += I.y; if (y0 < 0) y0 += I.z;
        int x1 = (x0 + 1) % I.y, y1 = (y0 + 1) % I.z;
        return mix(mix(Texel(I, x0, y0), Texel(I, x1, y0), a.x), mix(Texel(I, x0, y1), Texel(I, x1, y1), a.x), a.y);
    }
    int x = clamp(int(uv.x * float(I.y)), 0, I.y - 1), y = clamp(int(uv.y * float(I.z)), 0, I.z - 1);
    return Texel(I, x, y);
}
vec2 UVEn(Hit h) {
    ivec4 T = tris[h.tri]; float b0 = 1.0 - h.b1 - h.b2;
    return vec2(verts[T.x].p.w, verts[T.x].n.w) * b0 + vec2(verts[T.y].p.w, verts[T.y].n.w) * h.b1 + vec2(verts[T.z].p.w, verts[T.z].n.w) * h.b2;
}
struct M { vec3 dif; vec3 em; float alfa; float rug; float met; float f0; int tex; int nm; bool sinLuz; bool transp; bool aditivo; float recorte; float ior; float trans; };
// el borde del RECORTADO: una rampa corta alrededor del umbral (con el bilineal queda suave; ver RayTracer.cpp)
float Recortar(float a, float umbral) { return clamp((a - umbral) * 8.0 + 0.5, 0.0, 1.0); }
M MaterialEn(Hit h) {
    Mat r = mats[MaterialDe(h.tri)];
    M m;
    m.dif = r.m0.rgb; m.alfa = r.m0.a; m.em = r.m1.rgb; m.rug = r.m1.a; m.met = r.m2.x; m.f0 = r.m2.y;
    int b = int(r.m2.z); m.sinLuz = (b & 1) != 0; m.transp = (b & 2) != 0; m.aditivo = (b & 4) != 0;
    m.tex = int(r.m3.x); m.nm = int(r.m3.y); m.recorte = r.m4.z; m.ior = r.m2.w; m.trans = m.aditivo ? 0.0 : r.m4.w;
    int emT = int(r.m3.z), rugT = int(r.m3.w), metT = int(r.m4.x), alfaT = int(r.m4.y);
    if (emT >= 0 || rugT >= 0 || metT >= 0 || alfaT >= 0) {
        vec2 uv = UVEn(h);
        if (emT >= 0) m.em = Muestrear(emT, uv).rgb * r.m6.w;   // (x la fuerza de la emision)
        if (rugT >= 0) m.rug = Muestrear(rugT, uv).r;
        if (metT >= 0) m.met = Muestrear(metT, uv).r;
        if (alfaT >= 0) { vec4 t = Muestrear(alfaT, uv); m.alfa = (t.a < 0.999) ? t.a : t.r; m.transp = true; }
    }
    return m;
}
float OpacidadEn(Hit h) {
    M m = MaterialEn(h);
    if (!m.transp && m.trans <= 0.0) return 1.0;
    if (m.aditivo) return 0.0;
    float a = m.transp ? m.alfa : 1.0;
    if (m.transp && m.tex >= 0 && a > 0.0) a *= Muestrear(m.tex, UVEn(h)).a;
    if (m.recorte > 0.0) a = Recortar(a, m.recorte);
    return a * (1.0 - m.trans);   // (el vidrio deja pasar la luz)
}
// la normal INTERPOLADA del impacto (sin dar vuelta)
vec3 NormalLisa(Hit h) {
    ivec4 T = tris[h.tri]; float b0 = 1.0 - h.b1 - h.b2;
    return normalize(verts[T.x].n.xyz * b0 + verts[T.y].n.xyz * h.b1 + verts[T.z].n.xyz * h.b2);
}
// FRESNEL (Schlick, como RayTracer.cpp): cuanto refleja un dielectrico con ese coseno
float Fresnel(float f0, float c) { float k = 1.0 - clamp(abs(c), 0.0, 1.0); float k2 = k * k; return f0 + (1.0 - f0) * k2 * k2 * k; }
// lo que DEJA PASAR la superficie, por canal (TransmitIn de RayTracer.cpp): el hueco del alfa entero; el VIDRIO con su
// color y menos lo que refleja; con las causticas prendidas, la parte que el vidrio con IOR dobla la traen los fotones
vec3 TransmiteEn(Hit h, vec3 d) {
    M m = MaterialEn(h);
    if (!m.transp && m.trans <= 0.0) return vec3(0.0);
    if (m.aditivo) return vec3(1.0);
    float a = m.transp ? m.alfa : 1.0;
    vec4 tx = vec4(1.0);
    if (m.tex >= 0 && ((m.transp && a > 0.0) || m.trans > 0.0)) tx = Muestrear(m.tex, UVEn(h));
    if (m.transp) a *= tx.a;
    if (m.recorte > 0.0) a = Recortar(a, m.recorte);
    float vidrio = a * m.trans;
    if (vidrio > 0.0 && m.ior > 1.0001) vidrio *= causInfo.w > 0.5 ? 0.0 : 1.0 - Fresnel(m.f0, dot(NormalLisa(h), d));   // (IOR 1: pasa todo)
    return vec3(1.0 - a) + vidrio * m.dif * tx.rgb;
}
// LA SOMBRA CON TRANSPARENCIAS EN UN SOLO RECORRIDO: lo que deja pasar cada superficie se MULTIPLICA (el orden no
// importa), asi que no hace falta ir de la mas cercana a la siguiente (eran hasta 8 recorridos por rayo de sombra):
// se visita todo lo que corta el rayo hasta la luz y se corta apenas no pasa nada
#ifdef W3D_RTX
vec3 VisibilidadUnaPasada(vec3 o, vec3 d, float tMax) {
    gRayos++;
    vec3 vis = vec3(1.0);
    rayQueryEXT q;
    rayQueryInitializeEXT(q, gTlas, gl_RayFlagsNoOpaqueEXT, 0xFF, o, 1e-4, d, tMax);
    while (rayQueryProceedEXT(q)) {
        if (rayQueryGetIntersectionTypeEXT(q, false) != gl_RayQueryCandidateIntersectionTriangleEXT) continue;
        Hit h;
        vec2 b = rayQueryGetIntersectionBarycentricsEXT(q, false);
        h.t = rayQueryGetIntersectionTEXT(q, false); h.b1 = b.x; h.b2 = b.y;
        h.tri = rayQueryGetIntersectionPrimitiveIndexEXT(q, false);
        if (PasaDeLargo(h.tri, RT_PROYECTA)) continue;   // (no proyecta sombras: no tapa)
        vis *= TransmiteEn(h, d);
        if (max(max(vis.r, vis.g), vis.b) < 0.004) { rayQueryTerminateEXT(q); return vec3(0.0); }
    }
    return vis;
}
#else
vec3 VisibilidadUnaPasada(vec3 o, vec3 d, float tMax) {
    gRayos++;
    vec3 invD = vec3(1.0 / (abs(d.x) > 1e-12 ? d.x : 1e-12), 1.0 / (abs(d.y) > 1e-12 ? d.y : 1e-12), 1.0 / (abs(d.z) > 1e-12 ? d.z : 1e-12));
    int pila[64]; int sp = 0; pila[sp++] = 0;
    vec3 vis = vec3(1.0);
    while (sp > 0) {
        Nodo n = nodos[pila[--sp]];
        if (!RayoCaja(n.a.xyz, n.b.xyz, o, invD, tMax)) continue;
        int cant = int(n.c.y);
        if (cant > 0) {
            int primero = int(n.c.x);
            for (int i = 0; i < cant; i++) {
                Hit h;
                int tri = orden[primero + i];
                if (PasaDeLargo(tri, RT_PROYECTA)) continue;   // (no proyecta sombras: no tapa)
                if (!RayoTri(tri, o, d, 1e-4, tMax, h)) continue;
                vis *= TransmiteEn(h, d);
                if (max(max(vis.r, vis.g), vis.b) < 0.004) return vec3(0.0);
            }
        } else {
            int izq = int(n.a.w), der = int(n.b.w);
            if (izq >= 0 && sp < 63) pila[sp++] = izq;
            if (der >= 0 && sp < 63) pila[sp++] = der;
        }
    }
    return vis;
}
#endif
// la superficie que se esta iluminando NO RECIBE sombras (RT_RECIBE): las lamparas le llegan enteras (ver Radiancia)
bool gSinSombras = false;
vec3 Visibilidad(vec3 o, vec3 d, float tMax) {   // (por canal: lo que pasa por un vidrio trae su color)
    if (gSinSombras) return vec3(1.0);
    Hit h;
    if (pc.info.z == 0) return TrazarM(o, d, 1e-4, tMax, true, h, -1, RT_PROYECTA) ? vec3(0.0) : vec3(1.0);
    if (pc.info.y == 0) return vec3(1.0);
    return VisibilidadUnaPasada(o, d, tMax);
}
// LAS CAUSTICAS en 'p' (cara con normal 'n'): los fotones de las 27 celdas de alrededor (CausticsIn de RayTracer.cpp)
uint CeldaFoton(ivec3 c) { return uint(c.x) * 73856093u ^ uint(c.y) * 19349663u ^ uint(c.z) * 83492791u; }
vec3 CausticasEn(vec3 p, vec3 n) {
    if (causInfo.w < 0.5) return vec3(0.0);
    float r = causInfo.x, inv = causInfo.y; uint H = uint(causInfo.z);
    ivec3 ic = ivec3(floor(p * inv));
    vec3 suma = vec3(0.0); uint vistos[27]; int nv = 0;
    for (int dz = -1; dz <= 1; dz++) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        uint k = CeldaFoton(ic + ivec3(dx, dy, dz)) & (H - 1u);
        bool rep = false; for (int s = 0; s < nv; s++) if (vistos[s] == k) rep = true;
        if (rep) continue;
        vistos[nv++] = k;
        for (int i = celdas[k], i1 = celdas[k + 1u]; i < i1; i++) {
            Foton f = fotones[i];
            vec3 e = f.p.xyz - p; float d2 = dot(e, e);
            if (d2 >= r * r || dot(f.n.xyz, n) < 0.5) continue;
            suma += f.pw.rgb * (1.0 - sqrt(d2) / r);
        }
    }
    return suma * (3.0 / (3.14159265 * r * r));
}
vec3 DirAzarEsfera() {
    float z = 1.0 - 2.0 * Azar();
    float r = sqrt(max(1.0 - z * z, 0.0)), a = 6.2831853 * Azar();
    return vec3(r * cos(a), r * sin(a), z);
}
// la DISTANCIA de una luz (como RayTracer.cpp): se apaga suave hasta 0 justo ahi. D <= 0 = sin limite
float VentanaDistancia(float d, float D) {
    if (D <= 0.0) return 1.0;
    float x = d / D; if (x >= 1.0) return 0.0;
    float x2 = x * x; float w = 1.0 - x2 * x2; return w * w;
}
// LA LUZ DE AREA (como AreaIn de RayTracer.cpp): la difusa EXACTA sin nada en el medio (el factor de forma del poligono
// recortado por el horizonte del punto) por la parte de la lampara que ven los rayos de sombra, y el brillo con los
// mismos rayos. L.area = el medio ancho (xyz) y el medio alto (w; negativo = elipse); L.d.xyz = hacia donde alumbra
vec3 AristaArea(vec3 a, vec3 b) {
    vec3 cr = cross(a, b); float l = length(cr);
    return l > 1e-12 ? cr * (atan(l, dot(a, b)) / (l * 6.2831853)) : vec3(0.0);
}
void AristaAreaH(vec3 a, vec3 b, vec3 n, inout vec3 F, inout vec3 sal, inout vec3 ent) {
    float za = dot(n, a), zb = dot(n, b);
    if (za >= 0.0 && zb >= 0.0) { F += AristaArea(a, b); return; }
    if (za < 0.0 && zb < 0.0) return;
    vec3 x = normalize(za < zb ? a * zb - b * za : b * za - a * zb);
    if (za >= 0.0) { F += AristaArea(a, x); sal = x; } else { F += AristaArea(x, b); ent = x; }
}
// (el disco/la elipse: [-1,1]^2 -> el circulo sin deformar los estratos, Shirley-Chiu)
vec2 DiscoConcentrico(vec2 u) {
    if (u.x == 0.0 && u.y == 0.0) return u;
    float r, a;
    if (abs(u.x) > abs(u.y)) { r = u.x; a = 0.78539816 * (u.y / u.x); }
    else { r = u.y; a = 1.5707963 - 0.78539816 * (u.x / u.y); }
    return r * vec2(cos(a), sin(a));
}
void LuzArea(Luz L, vec3 po, vec3 n, vec3 haciaCam, float expo, bool conBrillo, out vec3 lambert, out vec3 spec) {
    lambert = vec3(0.0); spec = vec3(0.0);
    vec3 c = L.p.xyz, nl = L.d.xyz, ax = L.area.xyz;
    bool elip = L.area.w < 0.0;
    vec3 ay = normalize(cross(ax, nl)) * abs(L.area.w);
    float S = (elip ? 3.14159265 : 4.0) * length(ax) * abs(L.area.w);
    float h = dot(po - c, nl);
    if (h <= 0.0 || S <= 0.0) return;   // (detras: alumbra de un solo lado)
    vec3 F = vec3(0.0), sal = vec3(0.0), ent = vec3(0.0);
    if (!elip) {
        vec3 v0 = normalize(c + ax + ay - po), v1 = normalize(c - ax + ay - po), v2 = normalize(c - ax - ay - po), v3 = normalize(c + ax - ay - po);
        AristaAreaH(v0, v1, n, F, sal, ent); AristaAreaH(v1, v2, n, F, sal, ent); AristaAreaH(v2, v3, n, F, sal, ent); AristaAreaH(v3, v0, n, F, sal, ent);
    } else {   // (la elipse: el poligono de 12 lados con su misma superficie, 1.02333 = sqrt(pi / 3))
        vec3 va = normalize(c + ax * 1.0233267 - po);
        for (int k = 1; k <= 12; k++) {
            float a = float(k) * 0.52359878;
            vec3 vb = normalize(c + (ax * cos(a) + ay * sin(a)) * 1.0233267 - po);
            AristaAreaH(va, vb, n, F, sal, ent); va = vb;
        }
    }
    if (dot(sal, sal) > 0.5 && dot(ent, ent) > 0.5) F += AristaArea(sal, ent);
    float E = max(dot(n, F), 0.0) * 3.14159265 / S;
    if (E <= 0.0) return;
    int nRayos = max(int(L.c.w), 1);
    int sx = max(int(ceil(sqrt(float(nRayos)))), 1), sy = (nRayos + sx - 1) / sx, celdas = sx * sy;
    int desde = min(int(Azar() * float(celdas)), celdas - 1);
    float gMax = 6.2831853 * float(nRayos) / S;   // (un pedazo de la lampara no tapa mas que medio cielo)
    float sg = 0.0; vec3 sgv = vec3(0.0);
    for (int r = 0; r < nRayos; r++) {
        int celda = (r + desde) % celdas;
        vec2 u = vec2((float(celda % sx) + Azar()) / float(sx), (float(celda / sx) + Azar()) / float(sy)) * 2.0 - 1.0;
        if (elip) u = DiscoConcentrico(u);
        vec3 a = c + ax * u.x + ay * u.y - po;
        float dist = length(a); if (dist < 1e-6) continue;
        vec3 ld = a / dist;
        float ndl = dot(n, ld), cl = -dot(ld, nl);
        if (ndl <= 0.0 || cl <= 0.0) continue;
        float g = min(cl / (dist * dist), gMax);
        vec3 vis = Visibilidad(po, ld, dist);
        sg += ndl * g; sgv += ndl * g * vis;
        if (conBrillo && max(max(vis.r, vis.g), vis.b) > 0.0) {
            float nh = dot(n, normalize(ld + haciaCam));
            if (nh > 0.0) spec += pow(nh, expo) * (expo + 2.0) * 0.125 * vis * g;
        }
    }
    spec /= float(nRayos);
    vec3 parte;
    if (sg > 0.0) parte = sgv / sg;
    else {   // (ningun rayo cayo del lado del punto: uno en la direccion media de la luz que llega)
        vec3 d = normalize(F); float dn = -dot(d, nl);
        parte = (dn > 1e-4) ? Visibilidad(po, d, h / dn) : vec3(0.0);
    }
    lambert = E * parte;
}
// soloBrillo: ademas, el brillo de las lamparas solo (el VIDRIO lo muestra aunque no tenga difuso)
vec3 LuzEn(vec3 p, vec3 n, vec3 haciaCam, vec3 alb, M m, out vec3 soloBrillo) {
    soloBrillo = vec3(0.0);
    if (m.sinLuz) return alb;
    vec3 luz = vec3(0.0), brillo = vec3(0.0);
    float rug = max(m.rug, 0.05);
    float rugA = rug * rug;   // (rugosidad PERCEPTUAL, la de Blender/glTF: alfa = r^2; ver LightIn en RayTracer.cpp)
    float expo = min(2.0 / (rugA * rugA) - 2.0, 8192.0);
    float pesoBrillo = (m.rug <= 0.0) ? 0.0 : (m.met + (1.0 - m.met) * m.f0);
    vec3 colBrillo = m.met * alb + vec3(1.0 - m.met);
    vec3 po = p + n * 1e-3;
    const int kPorImpacto = 4;
    int N = pc.info.x;
    for (int i = 0; i < N; i++) luz += luces[i].amb.rgb;   // (el ambiente de todas: no tira rayos)
    int K = N > kPorImpacto ? kPorImpacto : N;
    float pesoMuestra = float(N) / float(max(K, 1));
    for (int s = 0; s < K; s++) {
        int i = N > kPorImpacto ? min(int(Azar() * float(N)), N - 1) : s;
        Luz L = luces[i];
        if (dot(L.area.xyz, L.area.xyz) > 0.0) {   // LUZ DE AREA (sin la atenuacion C/L/Q: la caida es la de la superficie)
            vec3 la, sp; LuzArea(L, po, n, haciaCam, expo, pesoBrillo > 0.0, la, sp);
            float w = VentanaDistancia(length(L.p.xyz - po), -L.p.w);
            luz += L.c.rgb * la * w * pesoMuestra; brillo += L.c.rgb * sp * w * pesoMuestra;
            continue;
        }
        vec3 lambert = vec3(0.0), spec = vec3(0.0); float att = 1.0;
        bool dirl = L.p.w > 0.5; float radio = L.d.w;
        float distMax = dirl ? 0.0 : -L.p.w;   // (la distancia de la luz: 0 = sin limite)
        int nRayos = int(L.c.w);
        for (int r = 0; r < nRayos; r++) {
            vec3 ldir; float dist;
            if (dirl) {
                ldir = L.d.xyz;
                if (radio > 0.0) ldir = normalize(L.d.xyz + DirAzarEsfera() * tan(radio * 3.14159265 / 180.0));
                dist = 1e30;
            } else {
                vec3 lp = L.p.xyz;
                if (radio > 0.0) lp += DirAzarEsfera() * radio;
                vec3 a = lp - po; dist = length(a); ldir = (dist > 1e-9) ? a / dist : n;
            }
            float nl = dot(n, ldir);
            if (nl <= 0.0) continue;
            if (distMax > 0.0 && dist >= distMax) continue;
            float cono = 1.0;
            if (!dirl && L.amb.w > -1.0) {   // FOCO: afuera del cono no llega
                float sd = -dot(ldir, L.d.xyz);
                if (sd < L.amb.w) continue;
                if (L.att.w > 0.0) cono = pow(sd, L.att.w);
            }
            // (lo que casi no se ve no paga el rayo de sombra: menos de medio tono de 8 bits)
            if (!dirl) {
                float at0 = min(1.0 / (L.att.x + L.att.y * dist + L.att.z * dist * dist), 1.0) * VentanaDistancia(dist, distMax);
                if (nl * cono * at0 * max(max(L.c.r, L.c.g), L.c.b) * pesoMuestra < 0.002) continue;
            }
            vec3 vis = Visibilidad(po, ldir, dist) * cono;
            if (max(max(vis.r, vis.g), vis.b) <= 0.0) continue;
            lambert += nl * vis;
            if (pesoBrillo > 0.0) {
                float nh = dot(n, normalize(ldir + haciaCam));
                if (nh > 0.0) spec += pow(nh, expo) * (expo + 2.0) * 0.125 * vis;
            }
        }
        if (max(max(lambert.r, lambert.g), lambert.b) <= 0.0 && max(max(spec.r, spec.g), spec.b) <= 0.0) continue;
        lambert /= float(nRayos); spec /= float(nRayos);
        if (!dirl) { float dd = length(L.p.xyz - po); att = min(1.0 / (L.att.x + L.att.y * dd + L.att.z * dd * dd), 1.0) * VentanaDistancia(dd, distMax); }
        luz += L.c.rgb * lambert * att * pesoMuestra; brillo += L.c.rgb * spec * att * pesoMuestra;
    }
    if (!gSinSombras) luz += CausticasEn(p, n);   // LAS CAUSTICAS: la luz que llego doblada por un vidrio
    soloBrillo = colBrillo * brillo * pesoBrillo;
    return alb * luz * (1.0 - m.met) + soloBrillo + m.em;
}
vec3 NormalEn(Hit h, vec3 d) {
    ivec4 T = tris[h.tri]; float b0 = 1.0 - h.b1 - h.b2;
    vec3 n = normalize(verts[T.x].n.xyz * b0 + verts[T.y].n.xyz * h.b1 + verts[T.z].n.xyz * h.b2);
    if (dot(n, d) > 0.0) n = -n;   // dos caras
    return n;
}
)GLSL";

static const char* kRTGpuTrace = R"GLSL(
// un rayo de camara con sus capas transparentes; los REFLEJOS del metal van a una pila (hasta 4 rebotes)
int gRebotes = 0;   // los rebotes de la luz (pc.dims.w >> 8)
int gEnvMat = -1;   // el material extra del FONDO 360 (((pc.info.w >> 1) & 0x7FFF) - 1; -1 = el color de fondo)
float gLuzFondo = 1.0;   // LA LUZ DEL FONDO (pc.info.w >> 16, en %): cuanto ilumina el fondo
// EL FONDO en una direccion: el color, o el punto de la textura 360 (como RayTracer.cpp: FondoEn)
vec3 Fondo(vec3 d) {
    if (gEnvMat < 0) return pc.bg.rgb;
    Mat r = mats[gEnvMat];
    float x = r.m0.x * d.x + r.m0.y * d.y + r.m0.z * d.z;
    float y = r.m0.w * d.x + r.m1.x * d.y + r.m1.y * d.z;
    float z = clamp(r.m1.z * d.x + r.m1.w * d.y + r.m2.x * d.z, -1.0, 1.0);
    vec2 uv = vec2(fract(0.5 + atan(y, x) * 0.15915494), acos(z) * 0.31830989);
    return Muestrear(int(r.m3.x), uv).rgb;
}
vec3 Radiancia(vec3 o0, vec3 d0) {
    vec3 total = vec3(0.0);
    vec3 pO[8]; vec3 pD[8]; vec3 pT[8]; int pP[8]; int sp = 0;
    pO[0] = o0; pD[0] = d0; pT[0] = vec3(1.0); pP[0] = 0; sp = 1;
    while (sp > 0) {
        sp--;
        vec3 o = pO[sp], d = pD[sp], thr = pT[sp]; int prof = pP[sp];
        float peso = 1.0; float tMin = 1e-4; int excl = -1;
        // (pasadas las capas transparentes que se permiten, lo de atras queda negro)
        int capas = max(pc.info.z, 1);
        // (lo TOTALMENTE transparente en ese punto no gasta capa: el pelo son muchas tarjetas; 'vuelta' es el tope)
        int capa = 0;
        for (int vuelta = 0; vuelta < 256 && capa < capas; vuelta++) {
            Hit h;
            // (los rayos de la camara ven todo; un REBOTE o un reflejo, solo lo que rebota la luz)
            if (!TrazarM(o, d, tMin, 1e30, false, h, excl, prof > 0 ? RT_REBOTA : 0)) { total += thr * peso * Fondo(d) * (prof > 0 ? gLuzFondo : 1.0); break; }
            ivec4 T = tris[h.tri];
            M m = MaterialEn(h);
            float b0 = 1.0 - h.b1 - h.b2;
            vec3 p = o + d * h.t;
            vec3 alb = m.dif;
            float alfa = m.transp ? m.alfa : 1.0;
            if (m.tex >= 0) { vec4 tx = Muestrear(m.tex, UVEn(h)); alb *= tx.rgb; if (m.transp) alfa *= tx.a; }
            if (m.recorte > 0.0) alfa = Recortar(alfa, m.recorte);
            if (alfa > 0.0) capa++;
            if (alfa > 0.0) {
                vec3 n = NormalEn(h, d);
                vec3 nLisa = n;   // (la de antes del normal map: la usa el Fresnel)
                if (m.nm >= 0) {   // NORMAL MAP (tangent-space), base T/B/N del impacto
                    vec4 tx = Muestrear(m.nm, UVEn(h));
                    vec3 Tg = verts[T.x].t.xyz * b0 + verts[T.y].t.xyz * h.b1 + verts[T.z].t.xyz * h.b2;
                    float mano = verts[T.x].t.w < 0.0 ? -1.0 : 1.0;
                    Tg -= n * dot(n, Tg);
                    if (dot(Tg, Tg) > 1e-12) {
                        Tg = normalize(Tg);
                        vec3 B = cross(n, Tg) * mano;
                        vec3 np = Tg * (tx.r * 2.0 - 1.0) + B * (tx.g * 2.0 - 1.0) + n * (tx.b * 2.0 - 1.0);
                        if (dot(np, np) > 1e-12) n = normalize(np);
                    }
                }
                // FRESNEL (Schlick, como Blender; ver Radiance en RayTracer.cpp): lo que no es metal refleja su F0 de frente
                // y casi todo de costado; el metal, su color (blanco de costado). Lo reflejado no es difuso
                // (con la normal LISA: la del normal map puede quedar de canto a la camara, F = 1 y el difuso a cero)
                float cosV = clamp(-dot(d, nLisa), 0.0, 1.0);
                float borde = pow(1.0 - cosV, 5.0);
                float fDie = (m.sinLuz || m.aditivo || m.trans > 0.0) ? 0.0 : m.f0 + (1.0 - m.f0) * borde;
                vec3 kRefl = m.met * (alb + (vec3(1.0) - alb) * borde) + vec3((1.0 - m.met) * fDie);
                alb *= 1.0 - (1.0 - m.met) * fDie;
                vec3 soloBrillo;
                gSinSombras = (BanderasDe(h.tri) & RT_RECIBE) == 0;
                vec3 col = LuzEn(p, n, -d, alb, m, soloBrillo);
                gSinSombras = false;
                // LA LUZ DEL FONDO 360 sin rebotes: un rayo al cielo pesado por el coseno (si no lo tapa nada)
                if ((gEnvMat >= 0 || dot(pc.bg.rgb, vec3(1.0)) > 0.0) && gLuzFondo > 0.0 && gRebotes == 0 && !m.sinLuz && m.met < 1.0) {
                    vec3 sd = normalize(n + DirAzarEsfera());
                    if (dot(sd, n) <= 0.0) sd = n;
                    Hit hs;
                    if (!TrazarM(p + n * 1e-3, sd, 1e-4, 1e30, true, hs, -1, RT_PROYECTA)) col += (1.0 - m.met) * alb * Fondo(sd) * gLuzFondo;
                }
                float w = m.aditivo ? peso : peso * alfa * (1.0 - m.trans);
                total += thr * w * col;
                // (la parte de VIDRIO no tiene difuso, pero las lamparas se le reflejan: el brillo que lo hace ver de vidrio)
                if (!m.aditivo && m.trans > 0.0) total += thr * (peso * alfa * m.trans) * soloBrillo;
                // EL REFLEJO con ese Fresnel: el metal hasta 4 rebotes; lo que no es metal en los 2 primeros y con
                // probabilidad (pesando en proporcion: mismo promedio sin duplicar los rayos)
                bool metal = m.met > 0.0;
                float prob = metal ? 1.0 : min(0.25 + 2.0 * fDie, 1.0);
                if (max(max(kRefl.r, kRefl.g), kRefl.b) > 0.0 && prof < (metal ? 4 : 2) && sp < 8 && (prob >= 1.0 || Azar() < prob)) {
                    vec3 r = d - n * (2.0 * dot(d, n));
                    if (m.rug > 0.0) {
                        r = normalize(r + DirAzarEsfera() * (m.rug * m.rug));
                        if (dot(r, n) <= 0.0) r = normalize(r - n * (2.0 * dot(r, n)));
                    }
                    pO[sp] = p + n * 1e-3; pD[sp] = r; pT[sp] = thr * w * kRefl / prob; pP[sp] = prof + 1; sp++;
                }
                // REBOTES DE LA LUZ (como RayTracer.cpp): la parte difusa sigue en una direccion pesada por el coseno
                if (gRebotes > 0 && prof < gRebotes && !m.sinLuz && m.met < 1.0 && sp < 8) {
                    vec3 r = normalize(n + DirAzarEsfera());
                    if (dot(r, n) <= 0.0) r = n;
                    pO[sp] = p + n * 1e-3; pD[sp] = r; pT[sp] = thr * w * (1.0 - m.met) * alb; pP[sp] = prof + 1; sp++;
                }
                if (!m.aditivo && m.trans <= 0.0) peso *= (1.0 - alfa);
            }
            // TRANSMISION (como RayTracer.cpp): lo que el alfa saca pasa derecho; la parte de vidrio REFLEJA (Fresnel) o pasa
            // refractada y TENIDA de su color
            if (m.trans > 0.0 && !m.aditivo) {
                float derecho = 1.0 - alfa, vidrio = alfa * m.trans, pasa = derecho + vidrio;
                peso *= pasa;
                if (peso * max(max(thr.r, thr.g), thr.b) < 0.004) break;
                if (Azar() * pasa < vidrio) {
                    vec3 nr = NormalLisa(h);
                    bool entra = dot(nr, d) < 0.0;
                    if (!entra) nr = -nr;
                    float c = -dot(nr, d);
                    float F = m.ior > 1.0001 ? Fresnel(m.f0, c) : 0.0;   // (IOR 1: transparente perfecto, no refleja)
                    vec3 pasaDir = d; bool refleja = false;
                    if (m.ior > 1.0001) {
                        float eta = entra ? 1.0 / m.ior : m.ior, k = 1.0 - eta * eta * (1.0 - c * c);
                        if (k < 0.0) refleja = true;   // (reflexion total interna)
                        else { pasaDir = normalize(d * eta + nr * (eta * c - sqrt(k))); if (!entra) F = Fresnel(m.f0, sqrt(k)); }
                    }
                    if (!refleja && Azar() < F) refleja = true;
                    if (refleja) d = normalize(d + nr * (2.0 * c));
                    else { d = pasaDir; thr *= alb; }
                    if (m.rug > 0.0) {   // VIDRIO ESMERILADO (como RayTracer.cpp)
                        vec3 dr = normalize(d + DirAzarEsfera() * (m.rug * m.rug));
                        if (dot(dr, nr) * dot(d, nr) > 0.0) d = dr;
                    }
                    if (peso * max(max(thr.r, thr.g), thr.b) < 0.004) break;
                    o = p; tMin = 1e-4; excl = h.tri;
                    continue;
                }
                tMin = h.t * (1.0 - 1e-5); excl = h.tri;
                continue;
            }
            if (!m.transp || peso < 0.004) break;
            if (m.ior > 1.0001 && !m.aditivo && m.recorte <= 0.0) {
                // REFRACCION (agua, vidrio): el rayo se dobla al entrar (o salir) y sigue desde aca
                vec3 nr = normalize(verts[T.x].n.xyz * b0 + verts[T.y].n.xyz * h.b1 + verts[T.z].n.xyz * h.b2);
                bool entra = dot(nr, d) < 0.0;
                if (!entra) nr = -nr;
                vec3 rd = refract(d, nr, entra ? 1.0 / m.ior : m.ior);
                d = (dot(rd, rd) < 1e-8) ? reflect(d, nr) : normalize(rd);   // (reflexion total interna)
                o = p; tMin = 1e-4; excl = h.tri;
                continue;
            }
            tMin = h.t * (1.0 - 1e-5); excl = h.tri;
        }
    }
    return total;
}
// LA LENTE (la profundidad de campo, como RayoLente de RayTracer.cpp): desde un punto del disco hacia el plano enfocado
void RayoLente(inout vec3 o, inout vec3 d) {
    if (pc.bg.w <= 0.0 || pc.camAdel.w > 0.5) return;
    float dd = dot(d, pc.camAdel.xyz);
    if (dd <= 1e-4) return;
    vec3 P = o + d * (pc.lente.w / dd);
    float r = sqrt(Azar()) * pc.bg.w, a = Azar() * 6.2831853;
    o = o + pc.camDer.xyz * (r * cos(a)) + pc.camArr.xyz * (r * sin(a));
    d = normalize(P - o);
}
void RayoPixel(float px, float py, out vec3 o, out vec3 d) {
    float w = float(pc.dims.x), h = float(pc.dims.y);
    float ndcX = px / w * 2.0 - 1.0, ndcY = 1.0 - py / h * 2.0;
    float ex = (ndcX - pc.lente.x) / pc.camArr.w * pc.camPos.w, ey = (ndcY - pc.lente.y) / pc.camArr.w * pc.camDer.w;
    if (pc.camAdel.w > 0.5) { o = pc.camPos.xyz + pc.camDer.xyz * ex + pc.camArr.xyz * ey; d = pc.camAdel.xyz; }
    else { o = pc.camPos.xyz; d = normalize(pc.camAdel.xyz + pc.camDer.xyz * ex + pc.camArr.xyz * ey); }
}
void main() {
    // pc.dims.w = muestras (8 bits) | rebotes (8) | BLOQUE de la vista previa (4: 0 = pase de verdad) | PRIMERA FILA (12):
    // el visor traza un pase de a FRANJAS de filas (cada cuadro las que entran en su tiempo: la UI sigue fluida) y, antes,
    // la VISTA PREVIA pixelada (un rayo por bloque de 6, 4 y 2 pixeles, como la CPU). El render a archivo, pases enteros
    int blk = (pc.dims.w >> 16) & 15;
    int row0 = (pc.dims.w >> 20) & 0xFFF;
    ivec2 q = ivec2(gl_GlobalInvocationID.xy) + ivec2(0, row0);
    if (q.x >= pc.dims.x || q.y >= pc.dims.y) return;
    int i = q.y * pc.dims.x + q.x;
    gRng = uint(q.x * 1973 + q.y * 9277 + pc.dims.z * 26699 + 1) ^ (uint(pc.lente.z) * 2654435761u);
    if (gRng == 0u) gRng = 0x9E3779B9u;
    int ns = max(pc.dims.w & 255, 1);
    gRebotes = (pc.dims.w >> 8) & 255;
    gEnvMat = ((pc.info.w >> 1) & 0x7FFF) - 1;
    gLuzFondo = float(pc.info.w >> 16) * 0.01;
    if (blk > 1) {   // LA VISTA PREVIA: un rayo al medio del bloque pinta el bloque entero (no toca lo acumulado)
        if ((q.x % blk) != 0 || (q.y % blk) != 0) return;
        vec3 o, d; RayoPixel(float(q.x) + float(blk) * 0.5, float(q.y) + float(blk) * 0.5, o, d);
        uint px = packUnorm4x8(vec4(clamp(Radiancia(o, d), 0.0, 1.0), 1.0));
        for (int yy = 0; yy < blk && q.y + yy < pc.dims.y; yy++)
            for (int xx = 0; xx < blk && q.x + xx < pc.dims.x; xx++) img[(q.y + yy) * pc.dims.x + q.x + xx] = px;
        atomicAdd(rayosTotal[0], gRayos);
        return;
    }
    vec3 c = vec3(0.0);
    for (int s = 0; s < ns; s++) {
        vec3 o, d; RayoPixel(float(q.x) + Azar(), float(q.y) + Azar(), o, d); RayoLente(o, d);
        c += min(Radiancia(o, d), vec3(2.0));
    }
    vec4 a = (pc.dims.z == 0 ? vec4(0.0) : acum[i]) + vec4(c / float(ns), 0.0);
    acum[i] = a;
    img[i] = packUnorm4x8(vec4(clamp(a.rgb / float(pc.dims.z + 1), 0.0, 1.0), 1.0));
    // el ALBEDO y la NORMAL del primer impacto (para el denoiser), PROMEDIADOS EN CADA PASE con un punto al azar adentro
    // del pixel, como el color. Antes salian del CENTRO del pixel y solo en el primer pase: el denoiser divide el color
    // por el albedo, limpia y lo vuelve a multiplicar, y ese albedo sin suavizar le devolvia a los bordes los DIENTES DE
    // SIERRA que las muestras repartidas en el pixel ya habian borrado (el antialiasing de los 16 pases se perdia)
    if ((pc.info.w & 1) != 0) {
        vec3 o, d; RayoPixel(float(q.x) + Azar(), float(q.y) + Azar(), o, d);
        // (la primera superficie que TAPA al menos la mitad: lo transparente y lo recortado se atraviesa, si no el
        // denoiser borroneaba lo que se ve a traves de un vidrio o de las hojas)
        Hit h; bool hay = false; float tMin = 1e-4; int excl = -1;
        for (int capa = 0; capa < 64; capa++) {
            hay = Trazar(o, d, tMin, 1e30, false, h, excl);
            if (!hay || pc.info.z == 0 || capa + 1 >= pc.info.z || OpacidadEn(h) >= 0.5) break;
            tMin = h.t * (1.0 - 1e-5); excl = h.tri;
        }
        vec3 alb, nrm;
        if (!hay) { alb = Fondo(d); nrm = vec3(0.0); }
        else {
            M m = MaterialEn(h);
            alb = m.dif;
            if (m.tex >= 0) alb *= Muestrear(m.tex, UVEn(h)).rgb;
            nrm = NormalEn(h, d);
        }
        // (el promedio de los pases hechos: pc.dims.z es el pase, 0 = el primero)
        float k = 1.0 / float(pc.dims.z + 1);
        if (pc.dims.z == 0) { aovAlb[i] = vec4(alb, 0.0); aovNrm[i] = vec4(nrm, 0.0); }
        else { aovAlb[i] = vec4(mix(aovAlb[i].rgb, alb, k), 0.0); aovNrm[i] = vec4(mix(aovNrm[i].xyz, nrm, k), 0.0); }
    }
    atomicAdd(rayosTotal[0], gRayos);
}
)GLSL";

// EL DENOISER EN LA GPU SIN IDA Y VUELTA (el trazado tambien en la GPU): la LUZ sale directo de lo acumulado (el
// promedio dividido por el albedo) y al final se vuelve a multiplicar y queda en la imagen de 8 bits que se muestra
static const char* kRTGpuDemod = R"GLSL(
#version 450
layout(local_size_x = 8, local_size_y = 8) in;
layout(std430, binding = 0) readonly buffer BAccum { vec4 accum[]; };
layout(std430, binding = 1) buffer BSal { vec4 sal[]; };
layout(std430, binding = 2) readonly buffer BAlb { vec4 alb[]; };
layout(std430, binding = 3) readonly buffer BNrm { vec4 nrm[]; };
layout(push_constant) uniform PC { ivec4 dims; vec4 sigma; } pc;   // dims = ancho alto pasos -
void main() {
    ivec2 q = ivec2(gl_GlobalInvocationID.xy);
    if (q.x >= pc.dims.x || q.y >= pc.dims.y) return;
    int i = q.y * pc.dims.x + q.x;
    vec3 c = clamp(accum[i].rgb / float(max(pc.dims.z, 1)), 0.0, 1.0);
    sal[i] = vec4(c / max(alb[i].rgb, vec3(0.02)), 0.0);
}
)GLSL";
static const char* kRTGpuRemod = R"GLSL(
#version 450
layout(local_size_x = 8, local_size_y = 8) in;
layout(std430, binding = 0) readonly buffer BEnt { vec4 ent[]; };
layout(std430, binding = 1) buffer BImg { uint img[]; };
layout(std430, binding = 2) readonly buffer BAlb { vec4 alb[]; };
layout(std430, binding = 3) readonly buffer BNrm { vec4 nrm[]; };
layout(push_constant) uniform PC { ivec4 dims; vec4 sigma; } pc;
void main() {
    ivec2 q = ivec2(gl_GlobalInvocationID.xy);
    if (q.x >= pc.dims.x || q.y >= pc.dims.y) return;
    int i = q.y * pc.dims.x + q.x;
    img[i] = packUnorm4x8(vec4(clamp(ent[i].rgb * max(alb[i].rgb, vec3(0.02)), 0.0, 1.0), 1.0));
}
)GLSL";

// EL DENOISER A-TROUS: un filtro de 5x5 con huecos (paso 1, 2, 4, 8, 16) sobre la LUZ (el color dividido por el albedo,
// asi las texturas no se lavan), que no mezcla pixeles de normal, albedo o brillo muy distintos (los bordes quedan)
static const char* kRTGpuAtrous = R"GLSL(
#version 450
layout(local_size_x = 8, local_size_y = 8) in;
layout(std430, binding = 0) readonly buffer BEnt { vec4 ent[]; };
layout(std430, binding = 1) buffer BSal { vec4 sal[]; };
layout(std430, binding = 2) readonly buffer BAlb { vec4 alb[]; };
layout(std430, binding = 3) readonly buffer BNrm { vec4 nrm[]; };
layout(push_constant) uniform PC { ivec4 dims; vec4 sigma; } pc;   // dims = ancho alto paso -; sigma = luz normal albedo -
void main() {
    ivec2 q = ivec2(gl_GlobalInvocationID.xy);
    int W = pc.dims.x, H = pc.dims.y;
    if (q.x >= W || q.y >= H) return;
    int i = q.y * W + q.x;
    const float k[3] = float[3](3.0 / 8.0, 1.0 / 4.0, 1.0 / 16.0);
    vec3 c0 = ent[i].rgb, n0 = nrm[i].xyz, a0 = alb[i].rgb;
    float l0 = dot(c0, vec3(0.299, 0.587, 0.114));
    vec3 suma = vec3(0.0); float pesoT = 0.0;
    for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
        ivec2 p = q + ivec2(dx, dy) * pc.dims.z;
        if (p.x < 0 || p.y < 0 || p.x >= W || p.y >= H) continue;
        int j = p.y * W + p.x;
        vec3 c = ent[j].rgb;
        float l = dot(c, vec3(0.299, 0.587, 0.114));
        float wl = exp(-abs(l - l0) / (pc.sigma.x + 1e-4));
        float wn = (dot(n0, n0) < 0.5) ? 1.0 : pow(max(dot(n0, nrm[j].xyz), 0.0), pc.sigma.y);
        vec3 da = a0 - alb[j].rgb;
        float wa = exp(-dot(da, da) / (pc.sigma.z + 1e-6));
        float w = k[abs(dx)] * k[abs(dy)] * wl * wn * wa;
        suma += c * w; pesoT += w;
    }
    sal[i] = vec4(pesoT > 0.0 ? suma / pesoT : c0, 0.0);
}
)GLSL";
