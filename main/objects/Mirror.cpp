#include "w3dGraphics.h" // abstraccion de graficos (independencia de OpenGL)
#include "Mirror.h"
#include "objects/Mesh.h"   // w3dDecalesInline: la sombra del target se dibuja en el pase del espejo
#include "Culling.h"    // W3dBoundsSubarbol: el AABB en MUNDO del subarbol del target
#include <sstream>

// Constructor
Mirror::Mirror(Object* parent, Vector3 pos, bool x, bool y, bool z)
    : Object(parent, "Mirror", pos)
{
    mirrorX = x;
    mirrorY = y;
    mirrorZ = z;
    // defaults que antes eran inicializadores in-class (C++11): en C++03/RVCT van aca.
    RenderChildrens = true;
    sinProfundidad = false;
    usaLimites = false;
    limU0 = limU1 = limV0 = limV1 = 0.0f;
    recorte = false;
    recorteDeclarado = false;
    hundimientoMaximo = 0.0f;
}

// Devuelve el tipo de objeto
ObjectType Mirror::getType() {
    return ObjectType::mirror;
}

// UNA sola fuente de verdad de la reflexion: mundo(espejo) * Scale(-ejes). Render
// y medicion tienen que salir de aca (si divergen, el test mide otra cosa).
void Mirror::MatrizEspejo(Matrix4& out) const {
    Matrix4 W; GetWorldMatrix(W);
    Matrix4 S; S.Identity();
    S.m[0]  = mirrorX ? -1.0f : 1.0f;
    S.m[5]  = mirrorY ? -1.0f : 1.0f;
    S.m[10] = mirrorZ ? -1.0f : 1.0f;
    out = W * S;
}

// punto fijo del eje Y: y = m13 + m5*y  ->  y = m13 / (1 - m5). Con mirrorY (m5=-1)
// da m13/2, que es el "z: 2*h" que declaran los .w3d de una lamina de agua.
float Mirror::PlanoY() const {
    Matrix4 R; MatrizEspejo(R);
    float d = 1.0f - R.m[5];
    return (d > 1e-6f || d < -1e-6f) ? (R.m[13] / d) : R.m[13];
}

// ---------------------------------------------------------------------------
//  EL PLANO DEL ESPEJO, EN MUNDO (ver Mirror.h)
//
//  MatrizEspejo es W * S, con S negando uno de los ejes LOCALES. O sea: el plano
//  de reflexion es el plano local perpendicular a ese eje que pasa por el origen
//  local, llevado a mundo por W.
//   * la NORMAL es la columna de W del eje negado, normalizada;
//   * un PUNTO del plano es el medio entre el origen y su reflejo -- y el reflejo
//     del origen es, justamente, la traslacion de R;
//   * EjeU/EjeV son las otras dos columnas, en el orden X, Y, Z sin la normal.
//     Para el espejo horizontal (mirrorY) eso da U = X y V = Z, que es exactamente
//     lo que se medía antes a mano.
//
//  Si hay mas de un eje negado la transformacion ya no es una reflexion de plano
//  (es una rotacion); se toma el primero para que el recorte siga siendo algo
//  razonable en vez de romperse.
// ---------------------------------------------------------------------------
static Vector3 ColumnaW(const Matrix4& W, int c) {
    return Vector3(W.m[c*4+0], W.m[c*4+1], W.m[c*4+2]);
}
int Mirror::EjeEspejado() const {
    if (mirrorX) return 0;
    if (mirrorY) return 1;
    if (mirrorZ) return 2;
    return 1;
}
Vector3 Mirror::Normal() const {
    Matrix4 W; GetWorldMatrix(W);
    Vector3 n = ColumnaW(W, EjeEspejado());
    return n.Normalized();
}
Vector3 Mirror::PuntoDelPlano() const {
    Matrix4 R; MatrizEspejo(R);
    // R * (0,0,0) = la traslacion de R; el punto medio con el origen cae en el plano
    return Vector3(R.m[12] * 0.5f, R.m[13] * 0.5f, R.m[14] * 0.5f);
}
Vector3 Mirror::EjeU() const {
    Matrix4 W; GetWorldMatrix(W);
    const int e = EjeEspejado();
    return ColumnaW(W, (e == 0) ? 1 : 0).Normalized();   // X negado -> U = Y; si no, U = X
}
Vector3 Mirror::EjeV() const {
    Matrix4 W; GetWorldMatrix(W);
    const int e = EjeEspejado();
    return ColumnaW(W, (e == 2) ? 1 : 2).Normalized();   // Z negado -> V = Y; si no, V = Z
}

// ---------------------------------------------------------------------------
//  hundimientoMaximo: DEPRECADO (ver Mirror.h). La reflexion es GEOMETRICA
//  PURA: el objeto sube y el reflejo baja la distancia exacta, siempre. La
//  matriz por target queda como alias de MatrizEspejo para que el harness siga
//  midiendo por la misma puerta que el render.
// ---------------------------------------------------------------------------
void Mirror::MatrizEspejoTarget(Object* tg, Matrix4& out) const {
    (void)tg;
    MatrizEspejo(out);
}


// ---- targets ----
int Mirror::TargetsVivos() const {
    int n = target ? 1 : 0;
    for (size_t i = 0; i < targetsExtra.size(); i++) if (targetsExtra[i]) n++;
    return n;
}

Object* Mirror::TargetVivo(int i) const {
    if (target) { if (i == 0) return target; i--; }
    for (size_t k = 0; k < targetsExtra.size(); k++) {
        if (!targetsExtra[k]) continue;
        if (i == 0) return targetsExtra[k];
        i--;
    }
    return NULL;
}

void Mirror::SetTargetsTexto(const std::string& lista) {
    std::vector<std::string> nombres;
    std::stringstream ss(lista);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        size_t a = tok.find_first_not_of(" \t\r\n\"");
        size_t b = tok.find_last_not_of(" \t\r\n\"");
        if (a == std::string::npos) continue;
        nombres.push_back(tok.substr(a, b - a + 1));
    }
    if (nombres.empty()) return;
    SetTarget(nombres[0]);                 // el primero ocupa el slot historico
    targetsExtraNombre.clear();
    targetsExtra.clear();
    for (size_t i = 1; i < nombres.size(); i++) {
        targetsExtraNombre.push_back(nombres[i]);
        targetsExtra.push_back(NULL);
    }
}

std::string Mirror::TargetsTexto() const {
    std::string s = target ? target->name : targetName;
    for (size_t i = 0; i < targetsExtraNombre.size(); i++) {
        const std::string n = (i < targetsExtra.size() && targetsExtra[i])
                            ? targetsExtra[i]->name : targetsExtraNombre[i];
        if (n.empty()) continue;
        if (!s.empty()) s += ", ";
        s += n;
    }
    return s;
}

// Recarga los targets (el principal por la puerta de Target; los extra igual,
// con los mismos dos chequeos: ni yo mismo ni un ancestro -> recursion infinita)
void Mirror::Reload() {
    ReloadTarget(this);
    for (size_t i = 0; i < targetsExtra.size() && i < targetsExtraNombre.size(); i++) {
        targetsExtra[i] = NULL;
        Object* o = FindObjectByName(SceneCollection, targetsExtraNombre[i]);
        if (!o || o == this || IsMyAncestor(this, o)) continue;
        targetsExtra[i] = o;
    }
}

// ---------------------------------------------------------------------------
//  RECORTE DEL REFLEJO
//
//  El recorte correcto de un espejo es POR DONDE SE VE, no por donde esta la
//  geometria reflejada: la SILUETA del agua EN PANTALLA (la mascara de
//  estencil, que ademas se marca CON z-test para que lo que este DELANTE del
//  agua -- un puente que cruza el estanque -- la tape). Con la mascara, esa
//  silueta es TODO el recorte: el reflejo del que salta baja y se sigue viendo
//  adentro del agua, y el del que camina por la orilla aparece recortado por el
//  borde -- como en un espejo de verdad.
//
//  Los PLANOS DE MUNDO (el semiespacio bajo la superficie + las 4 paredes del
//  rectangulo) quedan SOLO como fallback para un contexto sin estencil: cortan
//  el reflejo por su posicion en el mundo, que para el salto y la orilla es la
//  respuesta equivocada (era el bug que tapaba el parche `hundimientoMaximo`,
//  hoy deprecado), pero sin estencil no hay nada mejor.
//
//  El plano de recorte va en el espacio de la MATRIZ ACTUAL, que en RenderObject
//  es la de MUNDO del espejo: un plano de mundo P se pasa como W^T * P.
// ---------------------------------------------------------------------------
static void PlanoAEspacioLocal(const Matrix4& W, const float* mundo, float* out) {
    // (W^T * P): si p_mundo = W * p_local, entonces P.p_mundo = (W^T P).p_local
    for (int i = 0; i < 4; i++)
        out[i] = W.m[i*4+0]*mundo[0] + W.m[i*4+1]*mundo[1]
               + W.m[i*4+2]*mundo[2] + W.m[i*4+3]*mundo[3];
}

// ---------------------------------------------------------------------------
//  EL GATE: cuando SE VE el reflejo de un target
//
//  Reporte del dueno: "el reflejo del personaje y del enemigo en el agua tiene que estar
//  no solo al tocar el agua. si salta tambien y si esta al borde del agua
//  tambien. solo cuando ya no se llega a reflejar deja de verse realmente".
//
//  El gate viejo preguntaba por PuntoFoco()/RadioFoco(), que para todo lo que no
//  es una Mesh (un Empty que agrupa al personaje, por ejemplo) valen "el origen" y
//  "cero": o sea que en la practica era "el ORIGEN del objeto esta adentro del
//  rectangulo". Con el personaje parado en la orilla el origen caia afuera y el reflejo
//  desaparecia ENTERO de golpe.
//
//  Ahora se pregunta lo que hay que preguntar: si la PROYECCION del objeto sobre
//  el plano del agua (su AABB de MUNDO aplastado en X/Z, que la reflexion en Y no
//  mueve) INTERSECTA el rectangulo del cuerpo de agua. Con eso:
//    - saltando alto: sigue intersectando en X/Z -> se ve (mas chico y mas hondo);
//    - en la orilla:  intersecta parcialmente    -> se ve RECORTADO por los planos;
//    - ya afuera:     no intersecta              -> recien ahi se apaga.
//  El unico caso vertical que apaga es "todo el cuerpo hundido": su reflejo cae
//  ARRIBA del agua y el plano de recorte lo borraria igual.
//
//  Devuelve false = no hay nada que dibujar de ese target.
// ---------------------------------------------------------------------------
bool Mirror::TargetSeRefleja(Object* tg) const {
    if (!tg || !tg->visible) return false;
    if (!Recorta()) return true;    // espejo sin recorte: todo target visible se refleja

    // NADA de descartar por el rectangulo: un espejo refleja tambien lo que
    // esta AL LADO suyo (el que camina por la orilla se ve en el agua aunque su
    // planta no pise el rectangulo). El unico descarte fisico es "el cuerpo
    // ENTERO quedo del otro lado del plano" (hundido): su imagen caeria ARRIBA
    // del agua, y un agua no refleja lo que tiene abajo.
    Vector3 mn, mx;
    if (!W3dBoundsSubarbol(tg, mn, mx)) {
        // sin geometria medible (un Empty pelado): se cae al criterio del
        // origen, que para un punto es exactamente lo correcto.
        Vector3 c = tg->PuntoFoco();
        const float r = tg->RadioFoco();
        mn = Vector3(c.x - r, c.y - r, c.z - r);
        mx = Vector3(c.x + r, c.y + r, c.z + r);
    }
    const Vector3 N = Normal(), P0 = PuntoDelPlano();
    float n1 = 0;
    for (int i = 0; i < 8; i++) {
        Vector3 c((i & 1) ? mx.x : mn.x, (i & 2) ? mx.y : mn.y, (i & 4) ? mx.z : mn.z);
        const float cn = N.Dot(c);
        if (i == 0 || cn > n1) n1 = cn;
    }
    return n1 > N.Dot(P0);
}

// ---------------------------------------------------------------------------
//  ORDEN DE PASADAS DEL ESPEJO (reporte del dueno: "algo me dice que estas
//  ignorando el test de profundidad [...] el zbuffer lo tenes que hacer si o si")
//
//  Antes el reflejo se dibujaba con `sinProfundidad`, que es LITERALMENTE apagar
//  el z-test: por eso se veia mal (el reflejo no se auto-ocultaba, y cualquier
//  cosa dibujada antes quedaba tapada por el). El z-test estaba apagado porque el
//  fondo OPACO del estanque esta apenas debajo de la superficie y el reflejo cae
//  mas hondo: con z-test no pasaba un solo pixel.
//
//  La solucion correcta es la clasica de espejo con ESTENCIL, en cuatro pases.
//  Todo lo demas de la escena sigue con el z-buffer prendido de punta a punta:
//
//    A) MARCAR  la lamina de agua en el estencil, CON z-test y sin escribir color
//       ni profundidad. Lo que este DELANTE del agua (el puente que cruza el
//       estanque) no la marca -> ahi no va a haber reflejo. [prueba (b)]
//    B) BORRAR  la profundidad SOLO adentro de la marca. Aca muere el motivo por
//       el que existia `sinProfundidad`: el fondo opaco del estanque deja de
//       ganarle al reflejo, sin tener que apagar el z-test. [prueba (c)]
//    C) REFLEJO dibujado adentro de la marca, CON z-test Y escritura de z (asi el
//       reflejo se auto-ocluye: el brazo de atras del personaje no se dibuja sobre el
//       de adelante), y con UN PLANO DE RECORTE DE USUARIO en la superficie
//       (GL_CLIP_PLANE0) que descarta todo lo que quede del lado de ARRIBA.
//       Los dos recortes son necesarios y no se solapan: la marca recorta EN
//       PANTALLA (por donde SE VE el agua) [prueba (a)] y el plano recorta EN EL
//       MUNDO (la mitad de abajo), que es lo unico que puede cortar el reflejo de
//       un cuerpo MEDIO SUMERGIDO -- su parte hundida se refleja hacia ARRIBA y
//       sale al aire por encima del cuerpo real. Las 4 paredes del rectangulo
//       siguen SIN usarse con mascara (cortaban por donde ESTA la geometria).
//    D) RESTAURAR la profundidad de la region al plano del agua (un poco hacia
//       ATRAS), para que la lamina translucida que se dibuja despues siga pasando
//       el z-test y para que lo que venga detras no se cuele por el agujero.
//
//  Y el ORDEN GENERAL de la escena, que es orden de ARBOL (Object::Render), queda
//  como pidio el dueno y como lo declara la escena que lo motivo:
//       cielo -> escenario SOLIDO -> ESPEJOS (A,B,C,D) -> agua/alpha -> personaje,
//       enemigos y demas entidades -> particulas -> HUD.
//
//  Sin buffer de estencil en el contexto (EstencilBits() == 0) se cae al camino
//  viejo: recorte por planos + `sinProfundidad`. Peor, pero nunca peor que antes.
// ---------------------------------------------------------------------------

// Renderizado del mirror
void Mirror::RenderObject() {
    namespace gfx = w3dEngine;
    if (TargetsVivos() == 0 || (!mirrorX && !mirrorY && !mirrorZ)) return;

    Matrix4 W; GetWorldMatrix(W);
    Matrix4 Winv; bool hayInv = Matrix4::InvertirAfin(W, Winv);
    const bool recorta  = Recorta();
    const float planoY  = PlanoY();
    // el plano del espejo en MUNDO: normal, punto y los dos ejes en los que se
    // mide el rectangulo de recorte (ver Mirror.h)
    const Vector3 N = Normal(), U = EjeU(), V = EjeV(), P0 = PuntoDelPlano();

    // ---- 1) el RECORTE. Son DOS recortes, y hacen falta LOS DOS:
    //
    //   * EN PANTALLA lo recorta el ESTENCIL: la silueta del agua, marcada con
    //     z-test (pase A). Ese es el recorte que hace que el reflejo del que
    //     salta BAJE y se siga viendo adentro del agua, y que el del que camina
    //     por la orilla aparezca cortado por el borde -- como en un espejo de
    //     verdad. Los CUATRO PLANOS del rectangulo NO sirven para eso: cortan
    //     por DONDE ESTA la geometria reflejada y no por donde SE VE (ese era el
    //     bug que tapaba el parche `hundimientoMaximo`), asi que quedan SOLO
    //     como fallback para un contexto sin estencil.
    //
    //   * EN EL MUNDO lo recorta UN PLANO DE USUARIO sobre la superficie: el
    //     semiespacio de ABAJO. Reporte del dueno: "el recorte del reflejo esta
    //     mal y sale a traves del agua. cuando [el personaje] queda acostado en
    //     el agua, la espalda del reflejo se ve por encima del [personaje]
    //     original en vez de ser recortado."
    //     LA CAUSA: un cuerpo MEDIO SUMERGIDO. La reflexion manda p -> p', y lo
    //     que esta BAJO el plano se refleja HACIA ARRIBA: sale al aire, por
    //     encima del cuerpo real. El estencil no puede verlo (esa imagen cae
    //     justo adentro de la silueta del agua en pantalla) y el gate
    //     TargetSeRefleja tampoco (solo apaga el cuerpo ENTERO hundido).
    //     El arreglo barato y de GL 1.1 / GL ES 1.1 puro es el plano de recorte
    //     de usuario (glClipPlane / GL_CLIP_PLANE0): UN plano, prendido SOLO
    //     durante el pase del reflejo, que descarta todo lo que quede del lado
    //     de arriba. Sin planos disponibles (PlanosRecorteMax() == 0, el minimo
    //     que el estandar permite en GLES 1.1) PlanosRecorte es un no-op y el
    //     espejo sigue funcionando con el estencil solo, como antes.
    const bool conMascara = recorta && usaLimites && hayInv && gfx::EstencilBits() > 0;
    float planos[5 * 4];
    int nPlanos = 0;
    if (recorta) {
        // a) DETRAS DEL ESPEJO: -n.p + n.p0 >= 0. Es el semiespacio de ESTE lado
        //    del plano (bajo el agua, detras del vidrio). Sin esto, la parte del
        //    target que esta del otro lado del plano se refleja de este lado y
        //    se ve flotando fuera del recorte. Para el espejo horizontal
        //    (n = +Y, n.p0 = planoY) esto es exactamente el viejo
        //    { 0, -1, 0, planoY }. Va SIEMPRE: con mascara y sin ella.
        {
            const float p[4] = { -N.x, -N.y, -N.z, N.Dot(P0) };
            PlanoAEspacioLocal(W, p, &planos[nPlanos*4]); nPlanos++;
        }
        // b) las 4 paredes del rectangulo, EN EL PLANO DEL ESPEJO. SOLO en el
        //    fallback sin estencil (ver arriba: en pantalla recorta la mascara).
        if (usaLimites && !conMascara) {
            const float paredes[4][4] = {
                {  U.x,  U.y,  U.z, -limU0 },   // u >= limU0
                { -U.x, -U.y, -U.z,  limU1 },   // u <= limU1
                {  V.x,  V.y,  V.z, -limV0 },   // v >= limV0
                { -V.x, -V.y, -V.z,  limV1 },   // v <= limV1
            };
            for (int i = 0; i < 4 && nPlanos < 5; i++) {
                PlanoAEspacioLocal(W, paredes[i], &planos[nPlanos*4]); nPlanos++;
            }
        }
    }

    // ---- 2) mascara de estencil con la lamina de agua ----
    // El VALOR 1 se usa y se devuelve a 0 dentro de esta misma funcion, asi que
    // varios espejos en la misma escena no se pisan.
    float quad[12];
    if (conMascara) {
        // el rectangulo del agua, en el plano del espejo, pasado a LOCAL (la
        // matriz activa es la de mundo del espejo)
        // P(u,v) = N*(N.P0) + U*u + V*v, con {U, V, N} ortonormal. Para el espejo
        // horizontal da { limU0, planoY, limV1 }, que es el quad de siempre.
        const float dn = N.Dot(P0);
        const float uv[4][2] = {
            { limU0, limV1 }, { limU1, limV1 }, { limU1, limV0 }, { limU0, limV0 },
        };
        for (int i = 0; i < 4; i++) {
            Vector3 w(N.x * dn + U.x * uv[i][0] + V.x * uv[i][1],
                      N.y * dn + U.y * uv[i][0] + V.y * uv[i][1],
                      N.z * dn + U.z * uv[i][0] + V.z * uv[i][1]);
            Vector3 l = Winv * w;
            quad[i*3+0] = l.x; quad[i*3+1] = l.y; quad[i*3+2] = l.z;
        }
        // ---- PASE A: marcar SIN color y CON z-test: donde el agua no se ve
        // (tapada por el puente) no queda marca -> ahi no hay reflejo.
        gfx::EstencilLimpiar(0);
        gfx::EstencilMarcar(1);
        gfx::ColorMask(false, false, false, false);
        gfx::DepthMask(false);
        gfx::Enable(gfx::DepthTest);
        // el quad va por CLIENT ARRAY: si quedo un VBO bindeado de la ultima malla
        // dibujada, el puntero se interpreta como OFFSET adentro del VBO y la
        // mascara sale en cualquier lado (en una escena chica no se nota; en uno
        // con VBOs, el reflejo entero desaparece).
        gfx::UnbindVBOs();
        // y el z-test tiene que ser el normal: el multi-pass de Mesh deja
        // DepthFunc en EQUAL un rato y ahi la mascara no pasaria nunca.
        gfx::DepthFunc(gfx::DepthLEqual);
        // OFFSET DE PROFUNDIDAD (medido en una escena real): la lamina de agua del
        // nivel es una malla alpha que se dibuja DESPUES y que esta pegada al
        // terreno opaco de abajo. Sin offset la mascara queda COPLANAR con ese
        // terreno, pierde el z-test por redondeo y NO se marca ni un pixel: el
        // reflejo desaparecia del juego entero. El offset la corre hacia el ojo
        // lo justo para ganarle a lo coplanar y seguir perdiendo contra lo que
        // esta de verdad adelante (el puente cruza ~1 unidad mas arriba).
        // El FACTOR va en 0 a proposito: es la parte que escala con la PENDIENTE
        // del poligono, y el agua se ve casi de canto -> la pendiente es enorme y
        // con factor -1 el sesgo se volvia gigante y la mascara le ganaba tambien
        // a geometria que esta GENUINAMENTE ADELANTE. Solo hace falta el termino
        // constante (-4 unidades, el mismo sesgo del material `decal`).
        gfx::Enable(gfx::PolygonOffsetFill);
        gfx::PolygonOffset(0.0f, -4.0f);
        gfx::Disable(gfx::CullFace);       // el quad se marca se lo mire de donde se lo mire
        gfx::Disable(gfx::Texture2D);
        gfx::Disable(gfx::Lighting);
        gfx::DisableArray(gfx::NormalArray);
        gfx::DisableArray(gfx::TexCoordArray);
        gfx::DisableArray(gfx::ColorArray);
        gfx::EnableArray(gfx::VertexArray);
        gfx::VertexPointer3f(0, quad);
        static const unsigned char idx[6] = { 0, 1, 2, 0, 2, 3 };
        gfx::DrawTrianglesByte(6, idx);
        gfx::PolygonOffset(0.0f, 0.0f);
        gfx::Disable(gfx::PolygonOffsetFill);
        gfx::ColorMask(true, true, true, true);
        gfx::DepthFunc(gfx::DepthLess);    // como estaba
        gfx::EstencilDentro(1);            // de aca en mas se dibuja SOLO adentro

        // ---- PASE B: mandar la PROFUNDIDAD al FONDO adentro de la marca --------
        // Aca desaparece el motivo por el que existia `sinProfundidad`. El fondo
        // OPACO del estanque escribio z apenas debajo de la superficie y el
        // reflejo cae mas hondo todavia: con el z-test normal no pasaba UN SOLO
        // PIXEL, y por eso el motor terminaba apagando el z-test entero (que es
        // justo lo que el dueno vio como "ignoras el test de profundidad").
        // Dejando la z en el plano LEJANO solo adentro de la silueta del agua, el
        // reflejo compite contra un buffer limpio y usa el z-buffer NORMAL.
        //
        // OJO, ACA HAY UNA TRAMPA GRANDE: esto NO se puede hacer con
        // Clear(DepthBuffer). El glClear NO pasa por el test de estencil (spec GL
        // 2.1 §4.2.3: solo lo filtran el scissor y los writemasks), asi que
        // borraria la profundidad de TODA la ventana -- y despues el agua se
        // dibujaba encima de los postes que estaban delante de ella. Se redibuja
        // el mismo quad con DepthRange(1,1) + DepthFunc(ALWAYS): eso SI respeta
        // el estencil, y escribe el valor lejano exactamente en la silueta.
        gfx::DepthMask(true);
        gfx::DepthFunc(gfx::DepthAlways);
        gfx::DepthRange(1.0f, 1.0f);
        gfx::ColorMask(false, false, false, false);
        gfx::DrawTrianglesByte(6, idx);
        gfx::DepthRange(0.0f, 1.0f);
        gfx::ColorMask(true, true, true, true);
        gfx::DepthFunc(gfx::DepthLess);
    }

    // paridad IMPAR de ejes espejados = invertir el winding (con 2 ejes es una
    // rotacion y el frente queda como estaba)
    int nEjes = (mirrorX ? 1 : 0) + (mirrorY ? 1 : 0) + (mirrorZ ? 1 : 0);
    if (nEjes & 1) gfx::FrontFace(false);

    // ---- PASE C: el reflejo ------------------------------------------------
    // CON mascara: z-buffer NORMAL (test + escritura). La profundidad de la
    // region quedo limpia en el pase B, asi que el reflejo se dibuja entero y
    // ademas se AUTO-OCLUYE (el brazo de atras del personaje no se pinta sobre el de
    // adelante, que es lo que pasaba sin z).
    // SIN mascara (contexto sin estencil): camino viejo. `sinProfundidad` apaga
    // el z-test y w3dPaseEspejoSinZ evita que AplicarMaterial lo vuelva a
    // prender desde mat->depth_test.
    const bool zNormal = conMascara;
    bool zEstaba = false;
    if (zNormal) {
        zEstaba = true;
        gfx::Enable(gfx::DepthTest);
        gfx::DepthMask(true);
        gfx::DepthFunc(gfx::DepthLEqual);
    } else if (sinProfundidad) {
        zEstaba = gfx::IsEnabled(gfx::DepthTest);
        gfx::Disable(gfx::DepthTest);
        gfx::DepthMask(false);
        gfx::w3dPaseEspejoSinZ = true;
    }
    if (nPlanos > 0) gfx::PlanosRecorte(nPlanos, planos);

    // las CALCOMANIAS del target (la sombra del personaje) se dibujan ACA MISMO y no
    // en el pase diferido del final: aca esta puesta la matriz espejada y el
    // recorte por estencil/planos. Diferidas se dibujarian despues, sin espejar
    // y fuera de la mascara.
    const bool decalInlineAntes = w3dDecalesInline;
    w3dDecalesInline = true;

    const int nT = TargetsVivos();
    for (int t = 0; t < nT; t++) {
        Object* tg = TargetVivo(t);
        // EL GATE (ver TargetSeRefleja): "la proyeccion del objeto sobre el agua
        // intersecta el rectangulo", no "el origen esta adentro".
        if (!TargetSeRefleja(tg)) continue;

        gfx::PushMatrix();
        // La matriz del target tiene que ser la de MUNDO: con la LOCAL, un target
        // colgado de otro objeto se reflejaba en el lugar equivocado (y el
        // rectangulo, que se compara contra coordenadas de mundo, no cerraba).
        Matrix4 Mt; tg->GetWorldMatrix(Mt);
        // REFLEXION GEOMETRICA PURA: W * S * Mt, la misma matriz que devuelve
        // MatrizEspejoTarget (lo que mide el harness). El parche del
        // hundimientoMaximo que metia una T aca esta deprecado.
        // espejar CADA eje tildado (mismo idioma que Instance).
        gfx::Scalef(mirrorX ? -1.0f : 1.0f,
                    mirrorY ? -1.0f : 1.0f,
                    mirrorZ ? -1.0f : 1.0f);
        gfx::MultMatrix(Mt.m);
        tg->RenderObject();

        // los hijos POR LA PUERTA VIRTUAL (RenderHijos), no recorriendo Childrens
        // a mano: asi el reflejo de un target que sea (o contenga) un LOD dibuja
        // EL NIVEL ELEGIDO y no los dos superpuestos, y una Collection ordenada
        // por camara se refleja en su orden.
        if (RenderChildrens) tg->RenderHijos();
        gfx::PopMatrix();
    }

    w3dDecalesInline = decalInlineAntes;
    if (nPlanos > 0) gfx::PlanosRecorte(0, NULL);
    if (!zNormal && sinProfundidad) {
        gfx::w3dPaseEspejoSinZ = false;
        gfx::DepthMask(true);
        if (zEstaba) gfx::Enable(gfx::DepthTest);
    }
    if (nEjes & 1) gfx::FrontFace(true); // restaurar
    if (conMascara) {
        // ---- PASE D: devolver la profundidad de la region al plano del agua ----
        // El pase B dejo un agujero en el z-buffer con la forma del agua. Si se
        // deja asi, cualquier cosa dibujada DESPUES que este detras del estanque
        // se cuela por ese agujero. Se vuelve a estampar el quad, sin color, con
        // un offset POSITIVO (hacia ATRAS): asi la lamina translucida de agua --
        // que se dibuja despues, EN EL PLANO EXACTO -- sigue ganando el z-test,
        // pero el fondo lejano ya no pasa.
        //
        // EL SESGO TIENE QUE SER SENSIBLE A LA PENDIENTE (factor != 0), y esta es
        // la razon, medida: el quad del pase D es UNO SOLO y cubre el rectangulo
        // entero; la lamina que se dibuja despues son DECENAS DE TRIANGULOS CHICOS
        // sobre EL MISMO PLANO. Dos teselados distintos del mismo plano NO
        // interpolan la misma profundidad: el error crece con la PENDIENTE en
        // pantalla, y una lamina de agua se ve casi de canto, o sea con la
        // pendiente maxima. Con un sesgo puramente constante (el `units` de antes,
        // 4 ULPs) ese error se lo come: triangulo por triangulo la lamina PIERDE
        // el z-test y no se dibuja. Como debajo de la lamina no hay fondo que
        // dibujar, el pixel se queda con el color de borrado: EL AGUA SE VE NEGRA
        // A PARCHES, y solo mientras el espejo esta prendido. (Sintoma reportado:
        // "piso el agua y se rompe toda, el agua se vuelve negra".)
        //
        // El termino de PENDIENTE es exactamente la escala de ese error, que es
        // para lo que existe: `factor` multiplica la pendiente maxima del poligono
        // en z. Aca empuja HACIA ATRAS, asi que pasarse solo significa dejar pasar
        // geometria que estaba apenas DETRAS de la superficie -- justo lo que el
        // pase B ya habia borrado --, mientras que quedarse corto rompe la
        // superficie entera. En el pase A (que empuja hacia ADELANTE) la asimetria
        // es la contraria y por eso alla el factor va en 0.
        gfx::DepthFunc(gfx::DepthAlways);
        gfx::DepthMask(true);
        gfx::Enable(gfx::DepthTest);
        gfx::ColorMask(false, false, false, false);
        gfx::Enable(gfx::PolygonOffsetFill);
        gfx::PolygonOffset(4.0f, 64.0f);
        gfx::Disable(gfx::CullFace);
        gfx::Disable(gfx::Texture2D);
        gfx::Disable(gfx::Lighting);
        gfx::UnbindVBOs();
        gfx::DisableArray(gfx::NormalArray);
        gfx::DisableArray(gfx::TexCoordArray);
        gfx::DisableArray(gfx::ColorArray);
        gfx::EnableArray(gfx::VertexArray);
        gfx::VertexPointer3f(0, quad);
        static const unsigned char idxD[6] = { 0, 1, 2, 0, 2, 3 };
        gfx::DrawTrianglesByte(6, idxD);
        gfx::PolygonOffset(0.0f, 0.0f);
        gfx::Disable(gfx::PolygonOffsetFill);
        gfx::ColorMask(true, true, true, true);
        gfx::DepthFunc(gfx::DepthLess);

        gfx::EstencilApagar();
        gfx::EstencilLimpiar(0);   // dejar el buffer como estaba para el proximo espejo
    }
}

// Destructor
Mirror::~Mirror() {
}
