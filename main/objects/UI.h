#ifndef UI_OBJECT_H
#define UI_OBJECT_H
#include "objects/Objects.h"
#include "WhiskUI/theme/colors.h"   // la paleta arranca con los colores de Whisk3D
#include <vector>

// un color CON NOMBRE de la paleta del UI: los componentes lo referencian por indice
// (editas el color de la paleta y cambia en todos lados a la vez)
struct PaletteColor {
    std::string name;
    float rgba[4];
};

// una PALETA completa (un "tema"): cambiar la activa recolorea toda la interfaz
struct Palette {
    std::string name;
    std::vector<PaletteColor> colors;
};

// =====================================================================
//  UI — la RAIZ de una interfaz 2D (el HUD de un juego, la pantalla de
//  un programa, una animacion 2D). Se edita en el viewport "Editor 2D";
//  en el viewport 3D no dibuja nada (es contenido 2D, no de la escena).
//
//  El lienzo que simula: por defecto "como el render" (el tamano de
//  Properties > Render, en vivo). En modo RESPONSIVE tiene tamano
//  propio, editable con presets (4k..240p + aspecto), el boton Rotar
//  (intercambia ancho y alto) o arrastrando el marco en el Editor 2D:
//  sirve para probar como se re-anclan los elementos en cada pantalla.
// =====================================================================
class UI;
// lo que hace el que DIBUJA cuando el recorrido del arbol llega a la UI que se muestra (el viewport del editor: la dibuja
// AHI, en su lugar del outliner). NULL = nadie: la UI se dibuja al final, como siempre
extern void (*g_uiInTree)(UI* u);
class UI : public Object {
public:
    bool inTree;        // DIBUJARLA EN SU LUGAR DEL OUTLINER (el arbol es el orden de dibujado: la UI, un Limpiar Z y
                         // un cubo encima; o un Postproceso despues que la afecta). Apagado: al final, encima de todo
                         // (los proyectos de antes; las UI nuevas nacen con el prendido)
    bool showIn3D;        // ver los elementos 2D DENTRO de la escena 3D (con su profundidad Z).
                         // Apagado por defecto: lo normal es verla como overlay (simula la ventana).
    bool sameAsRender;   // true (default): el lienzo es el tamano del render. false: responsive.
    float width, height; // el lienzo propio (solo cuenta en responsive)
    int resPreset;       // preset elegido, en lineas: 2160/1080/720/480/240 (etiqueta del boton)
    int aspectPreset;    // 0 = 16:9, 1 = 4:3, 2 = 1:1
    float opacity;       // 0..1: atenua la interfaz ENTERA (los hijos la heredan)
    float color[4];      // fondo de la ventana (TRANSPARENTE por defecto)
    float scaleGlobal;   // multiplica TODO lo que este en px (como el GlobalScale del editor):
                         // x1 = N95, x2/x3/x4 = como se ve en PC y pantallas mas grandes
    bool equalScaleEditor; // true = la escala la da el GlobalScale del editor (por plataforma: x1 N95, x2/x3 PC/
                            // Android), no el valor manual de arriba. Igual que reusamos GlobalScale al exportar.
    // las PALETAS ("temas"): los componentes referencian colores POR INDICE contra la
    // paleta ACTIVA; cambiar de paleta recolorea todo. OJO con los reserve: el panel
    // guarda punteros adentro (max 8 paletas de 32 colores).
    std::vector<Palette> palettes;
    int activePalette;
    // como se ACOMODAN los hijos: 0 = libremente (cada uno con su ancla y su posicion),
    // 1 = filas, 2 = columnas (se reparten el 100% del area interior, sin posicion editable)
    int   layoutChildren;
    int   layoutFit;      // 0 = estirar (100% por peso); 1 = minimo (tamano natural)
    int   layoutAlign;    // con minimo y sin expandir: 0 inicio, 1 centro, 2 fin
    int   distribution;   // reparto css del sobrante (solo con ajuste MINIMO):
                          // 0 gap clasico, 1 space-between, 2 space-around, 3 space-evenly
    float gap;            // espacio entre hijos cuando hay filas/columnas
    bool  padGapPx;       // true: padding y gap en PIXELES (default); false: proporcionales
                          // al LADO MENOR de la ventana (mismo tamano en X que en Y)
    bool  cropsX;       // overflow: recortar lo que se sale de la ventana, por eje
    bool  cropsY;
    bool  withScroll;      // permitir scrollear el contenido recortado
    float scrollX, scrollY;   // desplazamiento del contenido (px de lienzo)
    float padLeft, padRight;   // margen interior POR LADO: encoge el area de anclaje
    float padArr, padBottom;
    bool  padUni;           // el panel edita los 4 lados con UN solo valor
    // el NOMBRE DE ARCHIVO .w3dui del que vino esta escena (solo el nombre, sin
    // carpeta: "menu.w3dui"). Lo fija UI2DLoad; SaveW3D lo usa para volver a
    // escribir el MISMO archivo hermano al guardar el proyecto (formato .w3d JSON:
    // las escenas UI viven como .w3dui externos al lado del .w3d). Vacio = UI nueva
    // que nunca se guardo: al guardar toma "<nombre>.w3dui".
    std::string fileW3dui;

    UI(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "UI", pos) {
        inTree   = true;
        showIn3D = false;
        sameAsRender = true;
        width = 640.0f; height = 480.0f;
        resPreset = 480; aspectPreset = 1;
        opacity = 1.0f;
        padLeft = padRight = padArr = padBottom = 0.0f;
        padUni  = true;
        color[0] = color[1] = color[2] = 0.0f; color[3] = 0.0f;
        // arranca con la PALETA DE WHISK3D (los colores reales del skin cargado)
        palettes.reserve(8);
        activePalette = 0;
        NewPalette("Whisk3D");
        AddPalette("Background",         ColorList[ColorID::background]);
        AddPalette("White",        ColorList[ColorID::white]);
        AddPalette("Accent",        ColorList[ColorID::accent]);
        AddPalette("Accent oscuro", ColorList[ColorID::accentDark]);
        AddPalette("Black",         ColorList[ColorID::black]);
        AddPalette("Gray",          ColorList[ColorID::gray]);
        AddPalette("Gris UI",       ColorList[ColorID::grayUI]);
        AddPalette("Gris linea",    ColorList[ColorID::grayLine]);
        scaleGlobal = 1.0f;
        equalScaleEditor = false;
        layoutChildren = 0; layoutFit = 0; layoutAlign = 0; distribution = 0;
        gap = 0.0f; padGapPx = true;
        cropsX = false; cropsY = false;
        withScroll = false; scrollX = 0.0f; scrollY = 0.0f;
    }

    // aplica el preset resolucion+aspecto respetando la orientacion actual
    void ApplyPreset() {
        float ratio = (aspectPreset == 0) ? (16.0f / 9.0f)
                    : (aspectPreset == 1) ? (4.0f / 3.0f) : 1.0f;
        float greater = (float)(int)(resPreset * ratio + 0.5f);
        if (height > width) { width = (float)resPreset; height = greater; }   // vertical
        else              { width = greater; height = (float)resPreset; }     // horizontal
    }
    void Rotate() { float t = width; width = height; height = t; }   // horizontal <-> vertical

    // los COLORES de la paleta activa (la que manda para todo el arbol)
    std::vector<PaletteColor>& Colors() {
        if (palettes.empty()) NewPalette("Whisk3D");
        if (activePalette < 0 || activePalette >= (int)palettes.size()) activePalette = 0;
        return palettes[activePalette].colors;
    }
    void NewPalette(const std::string& paletteName) {
        if (palettes.size() >= 8) return;   // reserve del ctor: punteros estables
        Palette pa;
        pa.name = paletteName;
        pa.colors.reserve(32);
        if (!palettes.empty())              // nueva = COPIA de la activa (para hacer un tema)
            pa.colors = Colors();
        palettes.push_back(pa);
        activePalette = (int)palettes.size() - 1;
    }
    void AddPalette(const std::string& colorName, const float* c) {
        std::vector<PaletteColor>& cs = Colors();
        if (cs.size() >= 32) return;       // reserve: punteros estables
        PaletteColor pc;
        pc.name = colorName;
        for (int i = 0; i < 4; i++) pc.rgba[i] = c ? c[i] : 1.0f;
        cs.push_back(pc);
    }

    ObjectType getType() W3D_OVERRIDE { return ObjectType::ui; }
    // nada en el 3D (vive en el Editor 2D)... salvo que se dibuje en su lugar del arbol: ahi la dibuja el que esta
    // dibujando (g_uiInTree), como un overlay en ese momento del recorrido
    void RenderObject() W3D_OVERRIDE { if (inTree && g_uiInTree) g_uiInTree(this); }
    bool DrawsSomething() W3D_OVERRIDE { return inTree && g_uiInTree; }   // (sino el pase 3D se saltea toda la interfaz)
};
#endif
