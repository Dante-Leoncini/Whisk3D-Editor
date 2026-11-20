// ============================================================================
//  InicioPopup.cpp - la pantalla de inicio (ver el .h)
// ============================================================================
#include "w3dGraphics.h"
#include "w3dTexture.h"
#include "W3dLang.h"
#include "InicioPopup.h"
#include "ViewPorts/LayoutInput.h"   // LayoutKey
#include "WhiskUI/core/UI.h"
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/draw/glesdraw.h"    // W3dDrawStrip4
#include "objects/Textures.h"
#include "render/OpcionesRender.h"   // g_redraw
#include "config/W3dRecientes.h"
#include "config/W3dLayouts.h"        // los layouts guardados: la lista de "Nuevo archivo"
#include "config/W3dProfile.h"       // W3dNowMs (el giro del engranaje)
#include "config/w3dVersion.h"
#include "w3dFilesystem.h"
#include "WhiskUI/widgets/card.h"     // CardUV: el relleno liso
#include <cstring>
#include <cstdio>

InicioPopup* inicioPopup = NULL;

static const int kRecientes = 5;
static const char* kPagina = "whisk3d.com.ar";

InicioPopup::InicioPopup() : PopUpBase("Inicio") {
    texReflejo = texMascara = texBorde = texCentro = texTexto = 0;
    texTextoW = texTextoH = 0;
    sel = 0; apretada = -1; t0 = 0;
    verLayouts = false;
    rect = new Card(NULL, 1, 1);
    ancho = alto = altoCabecera = ladoLogo = altoFila = yFilas = yTituloRecientes = 0;
    yLogo = colX = colW = 0;
}
InicioPopup::~InicioPopup() { delete rect; }

// las texturas del logo, de res/ (en Android salen del APK, en la web del paquete)
static unsigned int CargarLogo(const char* nombre, int* pw = NULL, int* ph = NULL) {
    unsigned int id = 0;
    int w = 0, h = 0;
#ifdef W3D_SYMBIAN
    // (Symbian: GetResDir() es "": todo va en la carpeta privada, la de la skin -> <privada>\inicio\, ver los .pkg)
    extern std::string g_skinDir;
    const std::string ruta = g_skinDir + "inicio\\" + nombre;
#else
    const std::string ruta = w3dFileSystem::GetResDir() + "/inicio/" + nombre;
#endif
    if (!w3dEngine::LoadTexture(ruta.c_str(), id, &w, &h)) return 0;
    if (pw) *pw = w;
    if (ph) *ph = h;
    return id;
}

void InicioPopup::Abrir() {
    recientes = W3dRecientes(kRecientes);
    verLayouts = false;
    if (!texReflejo) texReflejo = CargarLogo("reflejo.png");
    if (!texMascara) texMascara = CargarLogo("mascara.png");
    if (!texBorde) texBorde = CargarLogo("borde.png");
    if (!texCentro) texCentro = CargarLogo("centro.png");
    if (!texTexto) texTexto = CargarLogo("texto.png", &texTextoW, &texTextoH);
    // "Nuevo archivo" RESALTADO al abrir: con el teclado se arranca con un OK / Enter. Con el DEDO (Android, un Symbian
    // tactil: InicioPopupSinResaltar) sin resaltado, que parecia apretado
#if defined(__ANDROID__)
    sel = -1;
#else
    sel = 0;
#endif
    apretada = -1;
    t0 = W3dNowMs();
    PopUpActive = this;
    g_redraw = true;
}

// al cerrarse se LIBERA todo lo que trajo: las texturas del logo y la lista
void InicioPopup::Cerrar() {
    PopUpBase::Cerrar();
    unsigned int* tex[] = { &texReflejo, &texMascara, &texBorde, &texCentro };
    for (int i = 0; i < 4; i++) if (*tex[i]) { w3dEngine::DeleteTexture(*tex[i]); *tex[i] = 0; }
    if (texTexto) { w3dEngine::DeleteTexture(texTexto); texTexto = 0; }
    std::vector<std::string>().swap(recientes);
    std::vector<std::string>().swap(layouts);
    verLayouts = false;
    apretada = -1;
    g_redraw = true;
}

void InicioPopupAbrir() {
    if (!inicioPopup) inicioPopup = new InicioPopup();
    if (PopUpActive && PopUpActive != inicioPopup) PopUpActive->Cerrar();
    inicioPopup->Abrir();
}
void InicioPopupCerrar() { if (inicioPopup && PopUpActive == inicioPopup) inicioPopup->Cerrar(); }
bool InicioPopupAbierta() { return inicioPopup && PopUpActive == inicioPopup; }
void InicioPopupSinResaltar() { if (inicioPopup) { inicioPopup->sel = -1; g_redraw = true; } }

// un RECTANGULO LISO (las lineas finas y el resaltado). Card es un 9-slice con esquinas de 2*GlobalScale: mas fino
// que eso se rompe (las lineas salian gruesas y claras). Las 4 esquinas toman el texel del medio de la tarjeta.
static void RectLiso(int x0, int y0, int w, int h) {
    const GLfloat u = CardUV[5 * 2], v = CardUV[5 * 2 + 1];   // (el vertice 5 de la grilla 4x4: adentro del relleno)
    GLshort q[8] = { (GLshort)x0, (GLshort)y0, (GLshort)(x0 + w), (GLshort)y0,
                     (GLshort)x0, (GLshort)(y0 + h), (GLshort)(x0 + w), (GLshort)(y0 + h) };
    GLfloat uv[8] = { u, v, u, v, u, v, u, v };
    W3dDrawStrip4(q, uv);
}

// ---- el armado: una tarjeta AL MEDIO de la pantalla (con el borde redondeado y verde de las tarjetas de Whisk3D); el
//      ancho, el que entre (en un telefono parado, casi todo). Detras, el editor atenuado (kVeloInicio) ----
static const float kVeloInicio = 0.6f;   // lo negro del velo: el proyecto de fondo queda al 40%
void InicioPopup::Layout() {
    altoFila = RenglonHeightGS + 2 * gapGS;                          // filas comodas para el dedo
    ancho = 46 * CharacterWidthGS;
    if (ancho > MenuPantallaW - 4 * gapGS) ancho = MenuPantallaW - 4 * gapGS;
    colX = 0; colW = ancho;
    const int pad = borderGS + gapGS * 2;
    const int pie = gapGS * 2 + RenglonHeightGS + pad;                // abajo del logo: la pagina y la version
    const int rotulos = verLayouts ? 1 : (recientes.empty() ? 0 : 1);   // el de recientes, o "Nuevo archivo" en la de layouts
    const int filasAlto = (Filas() + rotulos) * altoFila + borderGS;
    // el LOGO: tres filas de alto, o lo que deje la pantalla (un telefono acostado, una ventana chica), con un minimo;
    // y a lo ancho, que deje lugar al texto de la derecha
    const int disponible = MenuPantallaH - 8 * gapGS;
    ladoLogo = 3 * altoFila;
    if (pad + ladoLogo + pie + filasAlto > disponible) ladoLogo = disponible - pad - pie - filasAlto;
    if (ladoLogo > colW * 38 / 100) ladoLogo = colW * 38 / 100;
    if (ladoLogo < altoFila + altoFila / 2) ladoLogo = altoFila + altoFila / 2;
    yLogo = pad;
    altoCabecera = pad + ladoLogo + pie;
    yFilas = altoCabecera;                                            // la fila 0 (Nuevo archivo); abajo, Abrir...
    yTituloRecientes = yFilas + 2 * altoFila;                         // el rotulo "Archivos recientes" (si hay alguno)
    alto = FilaY(UltimaFila()) + altoFila + borderGS;
    popUpWindow->Resize(ancho, alto);
    x = (MenuPantallaW - ancho) / 2;
    y = (MenuPantallaH - alto) / 2;
    if (y < 2 * gapGS) y = 2 * gapGS;
}
// EL ORDEN en pantalla: Nuevo archivo, Abrir... y despues el rotulo "Archivos recientes" con la lista (los numeros de
// fila no cambian: 0 = Nuevo archivo, 1..N = los recientes, N+1 = Abrir...)
int InicioPopup::FilaY(int fila) const {
    if (verLayouts) return yFilas + altoFila + (fila < 0 ? 0 : fila) * altoFila;   // abajo del rotulo "Nuevo archivo"
    const int n = (int)recientes.size();
    if (fila <= 0) return yFilas;
    if (fila > n) return yFilas + altoFila;                          // Abrir..., abajo de Nuevo archivo
    return yTituloRecientes + altoFila + (fila - 1) * altoFila;      // los recientes, abajo de su rotulo
}
int InicioPopup::UltimaFila() const {   // la de mas abajo
    if (verLayouts) return Filas() - 1;    // (Ir atras)
    return recientes.empty() ? 1 : (int)recientes.size();
}
// las filas en el orden en que se ven (las flechas del teclado van asi)
// (la lista de layouts ya va en orden)
static int OrdenVisual(int fila, int n, bool lay) { return lay ? fila : (fila == 0 ? 0 : (fila == n + 1 ? 1 : fila + 1)); }
static int FilaDeOrden(int orden, int n, bool lay) { return lay ? orden : (orden == 0 ? 0 : (orden == 1 ? n + 1 : orden - 1)); }
int InicioPopup::FilaEn(int mx, int my) const {
    if (!Contains(mx, my) || mx < x + colX || mx >= x + colX + colW) return -1;   // (solo en la columna)
    const int ly = my - y;
    for (int f = 0; f < Filas(); f++) {
        const int fy = FilaY(f);
        if (ly >= fy && ly < fy + altoFila) return f;
    }
    return -1;
}

// la LISTA DE LAYOUTS (o de vuelta a la tarjeta de siempre). Se lee layouts.txt cada vez: uno recien guardado aparece
void InicioPopup::VerLayouts(bool ver, bool teclado) {
    verLayouts = ver;
    if (ver) layouts = W3dLayoutsNombres();
    else std::vector<std::string>().swap(layouts);
    // (por teclado queda resaltada la primera: Por defecto, o Nuevo archivo al volver; con el dedo, ninguna)
    sel = teclado ? 0 : -1;
    apretada = -1;
    g_redraw = true;
}

void InicioPopup::Elegir(int fila, bool teclado) {
    if (verLayouts) {
        const int L = (int)layouts.size();
        if (fila == L + 1) { VerLayouts(false, teclado); return; }   // Ir atras
        if (fila < 0 || fila > L) return;
        const std::string nombre = (fila == 0) ? std::string() : layouts[(size_t)(fila - 1)];
        Cerrar();
        extern void NuevoProyectoConLayout(const std::string&);
        NuevoProyectoConLayout(nombre);
        return;
    }
    const int n = (int)recientes.size();
    if (fila < 0 || fila > n + 1) return;
    if (fila == 0 && !W3dLayoutsNombres().empty()) { VerLayouts(true, teclado); return; }   // a elegir el layout
    std::string ruta;
    if (fila >= 1 && fila <= n) ruta = recientes[(size_t)(fila - 1)];
    Cerrar();   // (libera el logo; lo que sigue abre otra cosa)
    if (fila == 0) { extern void NuevoProyectoDesde(); NuevoProyectoDesde(); }
    else if (fila <= n) { extern void AbrirProyectoDesde(const std::string&); AbrirProyectoDesde(ruta); }
    else { extern void ProyectoAbrirDialogo(); ProyectoAbrirDialogo(); }
}

// ---- dibujo ----
void InicioPopup::Render() {
    // EL VELO: todo el editor atenuado detras (el proyecto se ve, mas oscuro)
    x = 0; y = 0;
    popUpWindow->Resize(MenuPantallaW, MenuPantallaH);
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);   // el atlas de la UI (los rectangulos, los iconos y las letras)
    w3dEngine::Color4f(0.0f, 0.0f, 0.0f, kVeloInicio);
    RectLiso(0, 0, MenuPantallaW, MenuPantallaH);
    endView();
    // LA TARJETA, adelante
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);
    const int pad = borderGS + gapGS * 2;
    const float* fondo = ListaColores[static_cast<int>(ColorID::background)];
    const float* verde = ListaColores[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(fondo[0], fondo[1], fondo[2], 1.0f);
    popUpWindow->Render(false);
    // LA CABECERA NEGRA (con las esquinas redondeadas de la tarjeta)
    w3dEngine::Color4f(0.0f, 0.0f, 0.0f, 1.0f);
    rect->Resize(ancho, altoCabecera); rect->RenderObject(false);
    // el logo y, a su derecha, "Whisk3D": los dos juntos, centrados. El texto, de un poco menos de la mitad del alto del
    // logo (o lo que entre a lo ancho de la columna)
    const int sep = gapGS * 3;
    int hTexto = ladoLogo * 4 / 10, wTexto = 0;
    if (texTexto && texTextoH > 0) {
        wTexto = hTexto * texTextoW / texTextoH;
        const int libre = colW - 2 * pad - ladoLogo - sep;
        if (wTexto > libre) { wTexto = libre; hTexto = wTexto * texTextoH / texTextoW; }
    } else {
        hTexto = 2 * LetterHeightGS;                                  // (sin la textura: la letra de la UI, al doble)
        wTexto = 2 * 7 * CharacterWidthGS;
    }
    const int x0 = (ancho - (ladoLogo + sep + wTexto)) / 2;
    const float cx = (float)(x0 + ladoLogo / 2), cy = (float)(yLogo + ladoLogo / 2);
    GLshort v[8] = { 0, 0, (GLshort)ladoLogo, 0, 0, (GLshort)ladoLogo, (GLshort)ladoLogo, (GLshort)ladoLogo };
    GLfloat uv[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    // EL ENGRANAJE, como en la intro de las demos (conversiones/intro_demos/make_intro_w3d.py): GIRA la forma (la
    // mascara y el borde, a 67,5 grados por segundo, como en la intro y en el sitio) y el REFLEJO QUEDA QUIETO
    const float ang = (float)((W3dNowMs() - t0) * 0.0675);
    if (texReflejo && texMascara) {
        // 1) el reflejo, quieto: el cuadrado verde con las franjas de luz (opaco, sobre la cabecera negra)
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)x0, (GLfloat)yLogo, 0);
        w3dEngine::BindTexture(texReflejo);
        W3dDrawStrip4(v, uv);
        w3dEngine::PopMatrix();
        // 2) la mascara girando, MULTIPLICANDO: lo de afuera del engranaje queda negro (como la cabecera). Su quad es 1,5
        //    veces el cuadrado (las UV de -0,25 a 1,25 con la textura en Clamp: su borde es negro) para que, girada,
        //    lo tape siempre entero; y se recorta (scissor) al cuadrado del reflejo para no oscurecer nada mas
        const int glY = W3dPantallaAlto - (y + yLogo + ladoLogo);
        w3dEngine::Scissor(x + x0, glY, ladoLogo, ladoLogo);
        w3dEngine::BindTexture(texMascara);
        w3dEngine::TexWrap(false);
        w3dEngine::BlendMode(1);
        const GLshort m = (GLshort)(ladoLogo * 3 / 4);
        GLshort vm[8] = { (GLshort)-m, (GLshort)-m, m, (GLshort)-m, (GLshort)-m, m, m, m };
        GLfloat uvm[8] = { -0.25f, -0.25f, 1.25f, -0.25f, -0.25f, 1.25f, 1.25f, 1.25f };
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(cx, cy, 0);
        w3dEngine::Rotatef(ang, 0, 0, 1);
        W3dDrawStrip4(vm, uvm);
        w3dEngine::PopMatrix();
        w3dEngine::BlendAlpha();
        w3dEngine::Scissor(x, W3dPantallaAlto - y - popUpWindow->height, popUpWindow->width, popUpWindow->height);
    }
    if (texBorde) {       // 3) el borde, girando con la mascara
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(cx, cy, 0);
        w3dEngine::Rotatef(ang, 0, 0, 1);
        w3dEngine::Translatef(-ladoLogo * 0.5f, -ladoLogo * 0.5f, 0);
        w3dEngine::BindTexture(texBorde);
        W3dDrawStrip4(v, uv);
        w3dEngine::PopMatrix();
    }
    if (texCentro) {      // 4) el emblema del medio, quieto
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(cx - ladoLogo * 0.5f, cy - ladoLogo * 0.5f, 0);
        w3dEngine::BindTexture(texCentro);
        W3dDrawStrip4(v, uv);
        w3dEngine::PopMatrix();
    }
    // "Whisk3D" a la derecha del logo, a la altura de su centro
    const int xTexto = x0 + ladoLogo + sep, yTexto = yLogo + (ladoLogo - hTexto) / 2;
    if (texTexto) {
        GLshort vt[8] = { 0, 0, (GLshort)wTexto, 0, 0, (GLshort)hTexto, (GLshort)wTexto, (GLshort)hTexto };
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)xTexto, (GLfloat)yTexto, 0);
        w3dEngine::BindTexture(texTexto);
        W3dDrawStrip4(vt, uv);
        w3dEngine::PopMatrix();
    }
    w3dEngine::BindTexture(Textures[0]->iID);
    if (!texTexto) {
        SetColorID(ColorID::blanco);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)xTexto, (GLfloat)yTexto, 0);
        w3dEngine::Scalef(2.0f, 2.0f, 1.0f);
        RenderBitmapText("Whisk3D", textAlign::left, wTexto);
        w3dEngine::PopMatrix();
    }
    // abajo del logo, en los bordes de la columna: la pagina a la izquierda y la version a la derecha
    const int yPie = yLogo + ladoLogo + gapGS * 2 + (RenglonHeightGS - LetterHeightGS) / 2;
    const int wVer = (int)strlen(W3dVersion()) * CharacterWidthGS;   // (la letra es de ancho fijo)
    const int wPag = (int)strlen(kPagina) * CharacterWidthGS;
    // el margen, el de las opciones; en una pantalla angosta (un telefono parado) se achica para que entren las dos
    int margen = (colW - wPag - wVer - 2 * CharacterWidthGS) / 2;
    if (margen > pad) margen = pad;
    if (margen < 0) margen = 0;
    SetColorID(ColorID::grisUI);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(colX + margen), (GLfloat)yPie, 0);
    RenderBitmapText(kPagina, textAlign::left, colW - 2 * margen - wVer - CharacterWidthGS);
    w3dEngine::PopMatrix();
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(colX + colW - margen - wVer), (GLfloat)yPie, 0);
    RenderBitmapText(W3dVersion(), textAlign::left, wVer + CharacterWidthGS);
    w3dEngine::PopMatrix();

    // LAS OPCIONES, en la columna del medio, cada una separada con una linea
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)colX, 0, 0);
    const int n = (int)recientes.size();
    const int colTexto = pad + IconSizeGS + gapGS * 2;
    const int wTxt = colW - colTexto - pad;
    struct Fila {
        static void Linea(int yLinea, int w) {   // la linea que separa las opciones: fina, gris tenue
            SetColorID(ColorID::grisUI, 0.35f);
            RectLiso(0, yLinea, w, GlobalScale > 2 ? GlobalScale / 2 : 1);
        }
    };
    for (int f = 0; f < Filas(); f++) {
        const int fy = FilaY(f);
        if (f == sel || f == apretada) {   // el resaltado
            SetColorID(ColorID::accent, f == apretada ? 0.5f : 0.25f);
            RectLiso(0, fy, colW, altoFila);
        }
        const int L = (int)layouts.size();
        const int icono = verLayouts ? (f <= L ? (int)IconType::cuadricula : (int)IconType::deshacer)
                        : (f == 0) ? (int)IconType::mas : (f <= n) ? (int)IconType::archive : (int)IconType::carpeta;
        const int ty = fy + (altoFila - IconSizeGS) / 2;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)ty, 0);
        SetColorID(ColorID::accent);
        W3dDrawStrip4(IconMesh, IconsUV[(size_t)icono]->uvs);
        w3dEngine::PopMatrix();
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)colTexto, (GLfloat)(fy + (altoFila - LetterHeightGS) / 2), 0);
        if (verLayouts) {   // Por defecto, los guardados e Ir atras
            SetColorID(ColorID::blanco);
            const std::string t = (f == 0) ? std::string(T("Default")) : (f <= L) ? layouts[(size_t)(f - 1)] : std::string(T("Go back"));
            RenderBitmapText(t, textAlign::left, wTxt);
        } else if (f == 0 || f == n + 1) {
            SetColorID(ColorID::blanco);
            RenderBitmapText(T(f == 0 ? "New file" : "Open..."), textAlign::left, wTxt);
        } else {
            // el nombre del archivo y, en gris, su carpeta
            const std::string& r = recientes[(size_t)(f - 1)];
            const size_t b = r.find_last_of("/\\");
            const std::string nom = (b == std::string::npos) ? r : r.substr(b + 1);
            const std::string dir = (b == std::string::npos) ? std::string() : r.substr(0, b);
            SetColorID(ColorID::blanco);
            const int wNom = ((int)nom.size() + 1) * CharacterWidthGS;
            RenderBitmapText(nom, textAlign::left, wTxt);
            if (!dir.empty() && wNom < wTxt) {
                SetColorID(ColorID::grisUI);
                w3dEngine::Translatef((GLfloat)wNom, 0, 0);
                RenderBitmapText(dir, textAlign::left, wTxt - wNom);
            }
        }
        w3dEngine::PopMatrix();
        if (f != UltimaFila()) Fila::Linea(fy + altoFila - 1, colW);   // (la de mas abajo no: abajo ya esta el borde)
    }
    // la lista de layouts: arriba, el rotulo "Nuevo archivo" (lo que se esta por hacer)
    if (verLayouts) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)(yFilas + (altoFila - LetterHeightGS) / 2), 0);
        SetColorID(ColorID::grisUI);
        RenderBitmapText(T("New file"), textAlign::left, colW - 2 * pad);
        w3dEngine::PopMatrix();
        Fila::Linea(yFilas + altoFila - 1, colW);
    }
    // despues de "Abrir...": el rotulo de los recientes (si no hay ninguno, ni el rotulo)
    else if (n > 0) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)(yTituloRecientes + (altoFila - LetterHeightGS) / 2), 0);
        SetColorID(ColorID::grisUI);
        RenderBitmapText(T("Recent files"), textAlign::left, colW - 2 * pad);
        w3dEngine::PopMatrix();
        Fila::Linea(yTituloRecientes + altoFila - 1, colW);
    }
    w3dEngine::PopMatrix();
    // el borde verde: es la ventana con el foco
    w3dEngine::Color4f(verde[0], verde[1], verde[2], 1.0f);
    popUpWindow->RenderBorder(false);
    endView();
    if (texMascara) g_redraw = true;   // el engranaje sigue girando mientras se vea
}

// ---- entrada: se elige AL SOLTAR (el dedo o el boton) donde se apoyo ----
bool InicioPopup::Click(int mx, int my) {
    Layout();
    if (!Contains(mx, my)) return false;   // afuera: se cierra (y el toque no sigue)
    apretada = FilaEn(mx, my);
    if (apretada >= 0) sel = apretada;
    g_redraw = true;
    return true;
}
bool InicioPopup::Motion(int mx, int my) {
    Layout();
    const int f = FilaEn(mx, my);
    if (apretada >= 0 && f != apretada) apretada = -1;   // se corrio: ya no elige
    if (f >= 0 && f != sel) { sel = f; g_redraw = true; }
    return true;
}
void InicioPopup::Soltar() {
    const int f = apretada;
    apretada = -1;
    if (f >= 0) Elegir(f);
}
bool InicioPopup::Tecla(int tecla) {
    switch (tecla) {
        case LayoutKey::Up:
        case LayoutKey::Down: {   // en el orden en que se ven: Nuevo archivo, Abrir..., los recientes
            const int n = (int)recientes.size();
            int o = (sel < 0) ? -1 : OrdenVisual(sel, n, verLayouts);
            o += (tecla == LayoutKey::Down) ? 1 : -1;
            if (o < 0) o = 0;
            if (o > Filas() - 1) o = Filas() - 1;
            sel = FilaDeOrden(o, n, verLayouts);
            g_redraw = true;
            return true;
        }
        case LayoutKey::Enter:
        case LayoutKey::Accept: if (sel >= 0) Elegir(sel, true); return true;
        case LayoutKey::Cancel: if (verLayouts) VerLayouts(false, true); else Cerrar(); return true;   // (de la lista, vuelve)
    }
    return true;
}
