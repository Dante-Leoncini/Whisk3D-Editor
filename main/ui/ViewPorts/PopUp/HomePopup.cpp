// ============================================================================
//  HomePopup.cpp - la pantalla de inicio (ver el .h)
// ============================================================================
#include "w3dGraphics.h"
#include "w3dTexture.h"
#include "W3dLang.h"
#include "HomePopup.h"
#include "ViewPorts/LayoutInput.h"   // LayoutKey
#include "WhiskUI/core/UI.h"
#include "WhiskUI/text/bitmapText.h"
#include "WhiskUI/draw/icons.h"
#include "WhiskUI/draw/glesdraw.h"    // W3dDrawStrip4
#include "objects/Textures.h"
#include "render/RenderOptions.h"   // g_redraw
#include "config/W3dRecent.h"
#include "config/W3dLayouts.h"        // los layouts guardados: la lista de "Nuevo archivo"
#include "config/W3dProfile.h"       // W3dNowMs (el giro del engranaje)
#include "config/w3dVersion.h"
#include "w3dFilesystem.h"
#include "WhiskUI/widgets/card.h"     // CardUV: el relleno liso
#include <cstring>
#include <cstdio>

HomePopup* homePopup = NULL;

static const int kRecent = 5;
static const char* kPage = "whisk3d.com.ar";

HomePopup::HomePopup() : PopUpBase("Start") {
    texReflection = texMask = texBorder = texCenter = texText = 0;
    texTextW = texTextH = 0;
    sel = 0; pressed = -1; t0 = 0;
    showLayouts = false;
    rect = new Card(NULL, 1, 1);
    width = height = headerHeight = sideLogo = rowHeight = yRows = yRecentTitle = 0;
    yLogo = colX = colW = 0;
}
HomePopup::~HomePopup() { delete rect; }

// las texturas del logo, de res/ (en Android salen del APK, en la web del paquete)
static unsigned int LoadLogo(const char* name, int* pw = NULL, int* ph = NULL) {
    unsigned int id = 0;
    int w = 0, h = 0;
#ifdef W3D_SYMBIAN
    // (Symbian: GetResDir() es "": todo va en la carpeta privada, la de la skin -> <privada>\inicio\, ver los .pkg)
    extern std::string g_skinDir;
    const std::string path = g_skinDir + "inicio\\" + name;
#else
    const std::string path = w3dFileSystem::GetResDir() + "/home/" + name;
#endif
    if (!w3dEngine::LoadTexture(path.c_str(), id, &w, &h)) return 0;
    if (pw) *pw = w;
    if (ph) *ph = h;
    return id;
}

void HomePopup::Open() {
    recent = W3dRecent(kRecent);
    showLayouts = false;
    if (!texReflection) texReflection = LoadLogo("reflection.png");
    if (!texMask) texMask = LoadLogo("mask.png");
    if (!texBorder) texBorder = LoadLogo("border.png");
    if (!texCenter) texCenter = LoadLogo("center.png");
    if (!texText) texText = LoadLogo("text.png", &texTextW, &texTextH);
    // "Nuevo archivo" RESALTADO al abrir: con el teclado se arranca con un OK / Enter. Con el DEDO (Android, un Symbian
    // tactil: HomePopupNoHighlight) sin resaltado, que parecia apretado
#if defined(__ANDROID__)
    sel = -1;
#else
    sel = 0;
#endif
    pressed = -1;
    t0 = W3dNowMs();
    PopUpActive = this;
    g_redraw = true;
}

// al cerrarse se LIBERA todo lo que trajo: las texturas del logo y la lista
void HomePopup::Close() {
    PopUpBase::Close();
    unsigned int* tex[] = { &texReflection, &texMask, &texBorder, &texCenter };
    for (int i = 0; i < 4; i++) if (*tex[i]) { w3dEngine::DeleteTexture(*tex[i]); *tex[i] = 0; }
    if (texText) { w3dEngine::DeleteTexture(texText); texText = 0; }
    std::vector<std::string>().swap(recent);
    std::vector<std::string>().swap(layouts);
    showLayouts = false;
    pressed  = -1;
    g_redraw = true;
}

void HomePopupOpen() {
    if (!homePopup) homePopup = new HomePopup();
    if (PopUpActive && PopUpActive != homePopup) PopUpActive->Close();
    homePopup->Open();
}
void HomePopupClose() { if (homePopup && PopUpActive == homePopup) homePopup->Close(); }
bool HomePopupIsOpen() { return homePopup && PopUpActive == homePopup; }
void HomePopupNoHighlight() { if (homePopup) { homePopup->sel = -1; g_redraw = true; } }

// un RECTANGULO LISO (las lineas finas y el resaltado). Card es un 9-slice con esquinas de 2*GlobalScale: mas fino
// que eso se rompe (las lineas salian gruesas y claras). Las 4 esquinas toman el texel del medio de la tarjeta.
static void SmoothRect(int x0, int y0, int w, int h) {
    const GLfloat u = CardUV[5 * 2], v = CardUV[5 * 2 + 1];   // (el vertice 5 de la grilla 4x4: adentro del relleno)
    GLshort q[8] = { (GLshort)x0, (GLshort)y0, (GLshort)(x0 + w), (GLshort)y0,
                     (GLshort)x0, (GLshort)(y0 + h), (GLshort)(x0 + w), (GLshort)(y0 + h) };
    GLfloat uv[8] = { u, v, u, v, u, v, u, v };
    W3dDrawStrip4(q, uv);
}

// ---- el armado: una tarjeta AL MEDIO de la pantalla (con el borde redondeado y verde de las tarjetas de Whisk3D); el
//      ancho, el que entre (en un telefono parado, casi todo). Detras, el editor atenuado (kVeilStart) ----
static const float kVeilStart = 0.6f;   // lo negro del velo: el proyecto de fondo queda al 40%
void HomePopup::Layout() {
    rowHeight = RowHeightGS + 2 * gapGS;                          // filas comodas para el dedo
    width = 46 * CharacterWidthGS;
    if (width > MenuScreenW - 4 * gapGS) width = MenuScreenW - 4 * gapGS;
    colX = 0; colW = width;
    const int pad  = borderGS + gapGS * 2;
    const int foot = gapGS * 2 + RowHeightGS + pad;                // abajo del logo: la pagina y la version
    const int captions = showLayouts ? 1 : (recent.empty() ? 0 : 1);   // el de recientes, o "Nuevo archivo" en la de layouts
    const int rowsHeight = (Rows() + captions) * rowHeight + borderGS;
    // el LOGO: tres filas de alto, o lo que deje la pantalla (un telefono acostado, una ventana chica), con un minimo;
    // y a lo ancho, que deje lugar al texto de la derecha
    const int available = MenuScreenH - 8 * gapGS;
    sideLogo = 3 * rowHeight;
    if (pad + sideLogo + foot + rowsHeight > available) sideLogo = available - pad - foot - rowsHeight;
    if (sideLogo > colW * 38 / 100) sideLogo = colW * 38 / 100;
    if (sideLogo < rowHeight + rowHeight / 2) sideLogo = rowHeight + rowHeight / 2;
    yLogo = pad;
    headerHeight = pad + sideLogo + foot;
    yRows = headerHeight;                                         // la fila 0 (Nuevo archivo); abajo, Abrir...
    yRecentTitle = yRows + 2 * rowHeight;                         // el rotulo "Archivos recientes" (si hay alguno)
    height = RowY(LastRow()) + rowHeight + borderGS;
    popUpWindow->Resize(width, height);
    x = (MenuScreenW - width) / 2;
    y = (MenuScreenH - height) / 2;
    if (y < 2 * gapGS) y = 2 * gapGS;
}
// EL ORDEN en pantalla: Nuevo archivo, Abrir... y despues el rotulo "Archivos recientes" con la lista (los numeros de
// fila no cambian: 0 = Nuevo archivo, 1..N = los recientes, N+1 = Abrir...)
int HomePopup::RowY(int row) const {
    if (showLayouts) return yRows + rowHeight + (row < 0 ? 0 : row) * rowHeight;   // abajo del rotulo "Nuevo archivo"
    const int n = (int)recent.size();
    if (row <= 0) return yRows;
    if (row > n) return yRows + rowHeight;                        // Abrir..., abajo de Nuevo archivo
    return yRecentTitle + rowHeight + (row - 1) * rowHeight;      // los recientes, abajo de su rotulo
}
int HomePopup::LastRow() const {   // la de mas abajo
    if (showLayouts) return Rows() - 1;    // (Ir atras)
    return recent.empty() ? 1 : (int)recent.size();
}
// las filas en el orden en que se ven (las flechas del teclado van asi)
// (la lista de layouts ya va en orden)
static int OrderVisual(int row, int n, bool lay) { return lay ? row : (row == 0 ? 0 : (row == n + 1 ? 1 : row + 1)); }
static int OrderRow(int order, int n, bool lay) { return lay ? order : (order == 0 ? 0 : (order == 1 ? n + 1 : order - 1)); }
int HomePopup::RowIn(int mx, int my) const {
    if (!Contains(mx, my) || mx < x + colX || mx >= x + colX + colW) return -1;   // (solo en la columna)
    const int ly = my - y;
    for (int f = 0; f < Rows(); f++) {
        const int fy = RowY(f);
        if (ly >= fy && ly < fy + rowHeight) return f;
    }
    return -1;
}

// la LISTA DE LAYOUTS (o de vuelta a la tarjeta de siempre). Se lee layouts.txt cada vez: uno recien guardado aparece
void HomePopup::ShowLayouts(bool show, bool keyboard) {
    showLayouts = show;
    if (show) layouts = W3dLayoutsNames();
    else std::vector<std::string>().swap(layouts);
    // (por teclado queda resaltada la primera: Por defecto, o Nuevo archivo al volver; con el dedo, ninguna)
    sel = keyboard ? 0 : -1;
    pressed  = -1;
    g_redraw = true;
}

void HomePopup::Pick(int row, bool keyboard) {
    if (showLayouts) {
        const int L = (int)layouts.size();
        if (row == L + 1) { ShowLayouts(false, keyboard); return; }   // Ir atras
        if (row < 0 || row > L) return;
        const std::string name = (row == 0) ? std::string() : layouts[(size_t)(row - 1)];
        Close();
        extern void NewProjectWithLayout(const std::string&);
        NewProjectWithLayout(name);
        return;
    }
    const int n = (int)recent.size();
    if (row < 0 || row > n + 1) return;
    if (row == 0 && !W3dLayoutsNames().empty()) { ShowLayouts(true, keyboard); return; }   // a elegir el layout
    std::string path;
    if (row >= 1 && row <= n) path = recent[(size_t)(row - 1)];
    Close();   // (libera el logo; lo que sigue abre otra cosa)
    if (row == 0) { extern void NewProjectFrom(); NewProjectFrom(); }
    else if (row <= n) { extern void OpenProjectFrom(const std::string&); OpenProjectFrom(path); }
    else { extern void ProjectOpenDialog(); ProjectOpenDialog(); }
}

// ---- dibujo ----
void HomePopup::Render() {
    // EL VELO: todo el editor atenuado detras (el proyecto se ve, mas oscuro)
    x = 0; y = 0;
    popUpWindow->Resize(MenuScreenW, MenuScreenH);
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);   // el atlas de la UI (los rectangulos, los iconos y las letras)
    w3dEngine::Color4f(0.0f, 0.0f, 0.0f, kVeilStart);
    SmoothRect(0, 0, MenuScreenW, MenuScreenH);
    endView();
    // LA TARJETA, adelante
    Layout();
    initView();
    w3dEngine::BindTexture(Textures[0]->iID);
    const int pad = borderGS + gapGS * 2;
    const float* background = ColorList[static_cast<int>(ColorID::background)];
    const float* green      = ColorList[static_cast<int>(ColorID::accent)];
    w3dEngine::Color4f(background[0], background[1], background[2], 1.0f);
    popUpWindow->Render(false);
    // LA CABECERA NEGRA (con las esquinas redondeadas de la tarjeta)
    w3dEngine::Color4f(0.0f, 0.0f, 0.0f, 1.0f);
    rect->Resize(width, headerHeight); rect->RenderObject(false);
    // el logo y, a su derecha, "Whisk3D": los dos juntos, centrados. El texto, de un poco menos de la mitad del alto del
    // logo (o lo que entre a lo ancho de la columna)
    const int sep = gapGS * 3;
    int hText = sideLogo * 4 / 10, wText = 0;
    if (texText && texTextH > 0) {
        wText = hText * texTextW / texTextH;
        const int free = colW - 2 * pad - sideLogo - sep;
        if (wText > free) { wText = free; hText = wText * texTextH / texTextW; }
    } else {
        hText = 2 * LetterHeightGS;                                  // (sin la textura: la letra de la UI, al doble)
        wText = 2 * 7 * CharacterWidthGS;
    }
    const int x0 = (width - (sideLogo + sep + wText)) / 2;
    const float cx = (float)(x0 + sideLogo / 2), cy = (float)(yLogo + sideLogo / 2);
    GLshort v[8] = { 0, 0, (GLshort)sideLogo, 0, 0, (GLshort)sideLogo, (GLshort)sideLogo, (GLshort)sideLogo };
    GLfloat uv[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    w3dEngine::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
    // EL ENGRANAJE, como en la intro de las demos (conversiones/intro_demos/make_intro_w3d.py): GIRA la forma (la
    // mascara y el borde, a 67,5 grados por segundo, como en la intro y en el sitio) y el REFLEJO QUEDA QUIETO
    const float ang = (float)((W3dNowMs() - t0) * 0.0675);
    if (texReflection && texMask) {
        // 1) el reflejo, quieto: el cuadrado verde con las franjas de luz (opaco, sobre la cabecera negra)
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)x0, (GLfloat)yLogo, 0);
        w3dEngine::BindTexture(texReflection);
        W3dDrawStrip4(v, uv);
        w3dEngine::PopMatrix();
        // 2) la mascara girando, MULTIPLICANDO: lo de afuera del engranaje queda negro (como la cabecera). Su quad es 1,5
        //    veces el cuadrado (las UV de -0,25 a 1,25 con la textura en Clamp: su borde es negro) para que, girada,
        //    lo tape siempre entero; y se recorta (scissor) al cuadrado del reflejo para no oscurecer nada mas
        const int glY = W3dScreenHeight - (y + yLogo + sideLogo);
        w3dEngine::Scissor(x + x0, glY, sideLogo, sideLogo);
        w3dEngine::BindTexture(texMask);
        w3dEngine::TexWrap(false);
        w3dEngine::BlendMode(1);
        const GLshort m = (GLshort)(sideLogo * 3 / 4);
        GLshort vm[8] = { (GLshort)-m, (GLshort)-m, m, (GLshort)-m, (GLshort)-m, m, m, m };
        GLfloat uvm[8] = { -0.25f, -0.25f, 1.25f, -0.25f, -0.25f, 1.25f, 1.25f, 1.25f };
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(cx, cy, 0);
        w3dEngine::Rotatef(ang, 0, 0, 1);
        W3dDrawStrip4(vm, uvm);
        w3dEngine::PopMatrix();
        w3dEngine::BlendAlpha();
        w3dEngine::Scissor(x, W3dScreenHeight - y - popUpWindow->height, popUpWindow->width, popUpWindow->height);
    }
    if (texBorder) {       // 3) el borde, girando con la mascara
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(cx, cy, 0);
        w3dEngine::Rotatef(ang, 0, 0, 1);
        w3dEngine::Translatef(-sideLogo * 0.5f, -sideLogo * 0.5f, 0);
        w3dEngine::BindTexture(texBorder);
        W3dDrawStrip4(v, uv);
        w3dEngine::PopMatrix();
    }
    if (texCenter) {      // 4) el emblema del medio, quieto
        w3dEngine::PushMatrix();
        w3dEngine::Translatef(cx - sideLogo * 0.5f, cy - sideLogo * 0.5f, 0);
        w3dEngine::BindTexture(texCenter);
        W3dDrawStrip4(v, uv);
        w3dEngine::PopMatrix();
    }
    // "Whisk3D" a la derecha del logo, a la altura de su centro
    const int xText = x0 + sideLogo + sep, yText = yLogo + (sideLogo - hText) / 2;
    if (texText) {
        GLshort vt[8] = { 0, 0, (GLshort)wText, 0, 0, (GLshort)hText, (GLshort)wText, (GLshort)hText };
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)xText, (GLfloat)yText, 0);
        w3dEngine::BindTexture(texText);
        W3dDrawStrip4(vt, uv);
        w3dEngine::PopMatrix();
    }
    w3dEngine::BindTexture(Textures[0]->iID);
    if (!texText) {
        SetColorID(ColorID::white);
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)xText, (GLfloat)yText, 0);
        w3dEngine::Scalef(2.0f, 2.0f, 1.0f);
        RenderBitmapText("Whisk3D", textAlign::left, wText);
        w3dEngine::PopMatrix();
    }
    // abajo del logo, en los bordes de la columna: la pagina a la izquierda y la version a la derecha
    const int yFoot = yLogo + sideLogo + gapGS * 2 + (RowHeightGS - LetterHeightGS) / 2;
    const int wShow = (int)strlen(W3dVersion()) * CharacterWidthGS;   // (la letra es de ancho fijo)
    const int wPage = (int)strlen(kPage) * CharacterWidthGS;
    // el margen, el de las opciones; en una pantalla angosta (un telefono parado) se achica para que entren las dos
    int margin = (colW - wPage - wShow - 2 * CharacterWidthGS) / 2;
    if (margin > pad) margin = pad;
    if (margin < 0) margin = 0;
    SetColorID(ColorID::grayUI);
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(colX + margin), (GLfloat)yFoot, 0);
    RenderBitmapText(kPage, textAlign::left, colW - 2 * margin - wShow - CharacterWidthGS);
    w3dEngine::PopMatrix();
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)(colX + colW - margin - wShow), (GLfloat)yFoot, 0);
    RenderBitmapText(W3dVersion(), textAlign::left, wShow + CharacterWidthGS);
    w3dEngine::PopMatrix();

    // LAS OPCIONES, en la columna del medio, cada una separada con una linea
    w3dEngine::PushMatrix();
    w3dEngine::Translatef((GLfloat)colX, 0, 0);
    const int n = (int)recent.size();
    const int colText = pad + IconSizeGS + gapGS * 2;
    const int wTxt = colW - colText - pad;
    struct Row {
        static void Line(int yLine, int w) {   // la linea que separa las opciones: fina, gris tenue
            SetColorID(ColorID::grayUI, 0.35f);
            SmoothRect(0, yLine, w, GlobalScale > 2 ? GlobalScale / 2 : 1);
        }
    };
    for (int f = 0; f < Rows(); f++) {
        const int fy = RowY(f);
        if (f == sel || f == pressed) {   // el resaltado
            SetColorID(ColorID::accent, f == pressed ? 0.5f : 0.25f);
            SmoothRect(0, fy, colW, rowHeight);
        }
        const int L = (int)layouts.size();
        const int icon = showLayouts ? (f <= L ? (int)IconType::grid : (int)IconType::undo)
                        : (f == 0) ? (int)IconType::plus : (f <= n) ? (int)IconType::archive : (int)IconType::folder;
        const int ty = fy + (rowHeight - IconSizeGS) / 2;
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)ty, 0);
        SetColorID(ColorID::accent);
        W3dDrawStrip4(IconMesh, IconsUV[(size_t)icon]->uvs);
        w3dEngine::PopMatrix();
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)colText, (GLfloat)(fy + (rowHeight - LetterHeightGS) / 2), 0);
        if (showLayouts) {   // Por defecto, los guardados e Ir atras
            SetColorID(ColorID::white);
            const std::string t = (f == 0) ? std::string(T("Default")) : (f <= L) ? layouts[(size_t)(f - 1)] : std::string(T("Go back"));
            RenderBitmapText(t, textAlign::left, wTxt);
        } else if (f == 0 || f == n + 1) {
            SetColorID(ColorID::white);
            RenderBitmapText(T(f == 0 ? "New file" : "Open..."), textAlign::left, wTxt);
        } else {
            // el nombre del archivo y, en gris, su carpeta
            const std::string& r = recent[(size_t)(f - 1)];
            const size_t b = r.find_last_of("/\\");
            const std::string name = (b == std::string::npos) ? r : r.substr(b + 1);
            const std::string dir  = (b == std::string::npos) ? std::string() : r.substr(0, b);
            SetColorID(ColorID::white);
            const int wName = ((int)name.size() + 1) * CharacterWidthGS;
            RenderBitmapText(name, textAlign::left, wTxt);
            if (!dir.empty() && wName < wTxt) {
                SetColorID(ColorID::grayUI);
                w3dEngine::Translatef((GLfloat)wName, 0, 0);
                RenderBitmapText(dir, textAlign::left, wTxt - wName);
            }
        }
        w3dEngine::PopMatrix();
        if (f != LastRow()) Row::Line(fy + rowHeight - 1, colW);   // (la de mas abajo no: abajo ya esta el borde)
    }
    // la lista de layouts: arriba, el rotulo "Nuevo archivo" (lo que se esta por hacer)
    if (showLayouts) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)(yRows + (rowHeight - LetterHeightGS) / 2), 0);
        SetColorID(ColorID::grayUI);
        RenderBitmapText(T("New file"), textAlign::left, colW - 2 * pad);
        w3dEngine::PopMatrix();
        Row::Line(yRows + rowHeight - 1, colW);
    }
    // despues de "Abrir...": el rotulo de los recientes (si no hay ninguno, ni el rotulo)
    else if (n > 0) {
        w3dEngine::PushMatrix();
        w3dEngine::Translatef((GLfloat)pad, (GLfloat)(yRecentTitle + (rowHeight - LetterHeightGS) / 2), 0);
        SetColorID(ColorID::grayUI);
        RenderBitmapText(T("Recent files"), textAlign::left, colW - 2 * pad);
        w3dEngine::PopMatrix();
        Row::Line(yRecentTitle + rowHeight - 1, colW);
    }
    w3dEngine::PopMatrix();
    // el borde verde: es la ventana con el foco
    w3dEngine::Color4f(green[0], green[1], green[2], 1.0f);
    popUpWindow->RenderBorder(false);
    endView();
    if (texMask) g_redraw = true;   // el engranaje sigue girando mientras se vea
}

// ---- entrada: se elige AL SOLTAR (el dedo o el boton) donde se apoyo ----
bool HomePopup::Click(int mx, int my) {
    Layout();
    if (!Contains(mx, my)) return false;   // afuera: se cierra (y el toque no sigue)
    pressed = RowIn(mx, my);
    if (pressed >= 0) sel = pressed;
    g_redraw = true;
    return true;
}
bool HomePopup::Motion(int mx, int my) {
    Layout();
    const int f = RowIn(mx, my);
    if (pressed >= 0 && f != pressed) pressed = -1;   // se corrio: ya no elige
    if (f >= 0 && f != sel) { sel = f; g_redraw = true; }
    return true;
}
void HomePopup::Release() {
    const int f = pressed;
    pressed = -1;
    if (f >= 0) Pick(f);
}
bool HomePopup::Key(int key) {
    switch (key) {
        case LayoutKey::Up:
        case LayoutKey::Down: {   // en el orden en que se ven: Nuevo archivo, Abrir..., los recientes
            const int n = (int)recent.size();
            int o = (sel < 0) ? -1 : OrderVisual(sel, n, showLayouts);
            // (sin nada elegido: abajo = el primero, arriba = el ultimo)
            if (o < 0) o = (key == LayoutKey::Down) ? 0 : Rows() - 1;
            else o += (key == LayoutKey::Down) ? 1 : -1;
            // DA LA VUELTA: arriba desde el primero va al ultimo reciente, abajo desde el ultimo vuelve al primero
            if (o < 0) o = Rows() - 1;
            if (o > Rows() - 1) o = 0;
            sel = OrderRow(o, n, showLayouts);
            g_redraw = true;
            return true;
        }
        case LayoutKey::Enter:
        case LayoutKey::Accept: if (sel >= 0) Pick(sel, true); return true;
        case LayoutKey::Cancel: if (showLayouts) ShowLayouts(false, true); else Close(); return true;   // (de la lista, vuelve)
    }
    return true;
}
