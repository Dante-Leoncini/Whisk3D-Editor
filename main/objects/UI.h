#ifndef UI_OBJECT_H
#define UI_OBJECT_H
#include "objects/Objects.h"
#include "WhiskUI/theme/colores.h"   // la paleta arranca con los colores de Whisk3D
#include <vector>

// un color CON NOMBRE de la paleta del UI: los componentes lo referencian por indice
// (editas el color de la paleta y cambia en todos lados a la vez)
struct PaletaColor {
    std::string nombre;
    float rgba[4];
};

// una PALETA completa (un "tema"): cambiar la activa recolorea toda la interfaz
struct Paleta {
    std::string nombre;
    std::vector<PaletaColor> colores;
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
class UI : public Object {
public:
    bool verEn3D;        // ver los elementos 2D DENTRO de la escena 3D (con su profundidad Z).
                         // Apagado por defecto: lo normal es verla como overlay (simula la ventana).
    bool igualQueRender; // true (default): el lienzo es el tamano del render. false: responsive.
    float ancho, alto;   // el lienzo propio (solo cuenta en responsive)
    int resPreset;       // preset elegido, en lineas: 2160/1080/720/480/240 (etiqueta del boton)
    int aspectoPreset;   // 0 = 16:9, 1 = 4:3, 2 = 1:1
    float opacidad;      // 0..1: atenua la interfaz ENTERA (los hijos la heredan)
    float color[4];      // fondo de la ventana (TRANSPARENTE por defecto)
    float escalaGlobal;  // multiplica TODO lo que este en px (como el GlobalScale del editor):
                         // x1 = N95, x2/x3/x4 = como se ve en PC y pantallas mas grandes
    // las PALETAS ("temas"): los componentes referencian colores POR INDICE contra la
    // paleta ACTIVA; cambiar de paleta recolorea todo. OJO con los reserve: el panel
    // guarda punteros adentro (max 8 paletas de 32 colores).
    std::vector<Paleta> paletas;
    int paletaActiva;
    // como se ACOMODAN los hijos: 0 = libremente (cada uno con su ancla y su posicion),
    // 1 = filas, 2 = columnas (se reparten el 100% del area interior, sin posicion editable)
    int   layoutHijos;
    int   layoutAjuste;   // 0 = estirar (100% por peso); 1 = minimo (tamano natural)
    int   layoutAlign;    // con minimo y sin expandir: 0 inicio, 1 centro, 2 fin
    float gap;            // espacio entre hijos cuando hay filas/columnas
    bool  padGapPx;       // true: padding y gap en PIXELES (default); false: proporcionales
                          // al LADO MENOR de la ventana (mismo tamano en X que en Y)
    bool  recortaX;       // overflow: recortar lo que se sale de la ventana, por eje
    bool  recortaY;
    bool  conScroll;      // permitir scrollear el contenido recortado
    float scrollX, scrollY;   // desplazamiento del contenido (px de lienzo)
    float padIzq, padDer;   // margen interior POR LADO: encoge el area de anclaje
    float padArr, padAba;
    bool  padUni;           // el panel edita los 4 lados con UN solo valor

    UI(Object* parent = NULL, Vector3 pos = Vector3(0,0,0))
        : Object(parent, "UI", pos) {
        verEn3D = false;
        igualQueRender = true;
        ancho = 640.0f; alto = 480.0f;
        resPreset = 480; aspectoPreset = 1;
        opacidad = 1.0f;
        padIzq = padDer = padArr = padAba = 0.0f;
        padUni = true;
        color[0] = color[1] = color[2] = 0.0f; color[3] = 0.0f;
        // arranca con la PALETA DE WHISK3D (los colores reales del skin cargado)
        paletas.reserve(8);
        paletaActiva = 0;
        NuevaPaleta("Whisk3D");
        AgregarPaleta("Fondo",         ListaColores[ColorID::background]);
        AgregarPaleta("Blanco",        ListaColores[ColorID::blanco]);
        AgregarPaleta("Accent",        ListaColores[ColorID::accent]);
        AgregarPaleta("Accent oscuro", ListaColores[ColorID::accentDark]);
        AgregarPaleta("Negro",         ListaColores[ColorID::negro]);
        AgregarPaleta("Gris",          ListaColores[ColorID::gris]);
        AgregarPaleta("Gris UI",       ListaColores[ColorID::grisUI]);
        AgregarPaleta("Gris linea",    ListaColores[ColorID::grisLinea]);
        escalaGlobal = 1.0f;
        layoutHijos = 0; layoutAjuste = 0; layoutAlign = 0; gap = 0.0f; padGapPx = true;
        recortaX = false; recortaY = false;
        conScroll = false; scrollX = 0.0f; scrollY = 0.0f;
    }

    // aplica el preset resolucion+aspecto respetando la orientacion actual
    void AplicarPreset() {
        float ratio = (aspectoPreset == 0) ? (16.0f / 9.0f)
                    : (aspectoPreset == 1) ? (4.0f / 3.0f) : 1.0f;
        float mayor = (float)(int)(resPreset * ratio + 0.5f);
        if (alto > ancho) { ancho = (float)resPreset; alto = mayor; }   // vertical
        else              { ancho = mayor; alto = (float)resPreset; }   // horizontal
    }
    void Rotar() { float t = ancho; ancho = alto; alto = t; }   // horizontal <-> vertical

    // los COLORES de la paleta activa (la que manda para todo el arbol)
    std::vector<PaletaColor>& Colores() {
        if (paletas.empty()) NuevaPaleta("Whisk3D");
        if (paletaActiva < 0 || paletaActiva >= (int)paletas.size()) paletaActiva = 0;
        return paletas[paletaActiva].colores;
    }
    void NuevaPaleta(const std::string& nombre) {
        if (paletas.size() >= 8) return;   // reserve del ctor: punteros estables
        Paleta pa;
        pa.nombre = nombre;
        pa.colores.reserve(32);
        if (!paletas.empty())              // nueva = COPIA de la activa (para hacer un tema)
            pa.colores = Colores();
        paletas.push_back(pa);
        paletaActiva = (int)paletas.size() - 1;
    }
    void AgregarPaleta(const std::string& nombre, const float* c) {
        std::vector<PaletaColor>& cs = Colores();
        if (cs.size() >= 32) return;       // reserve: punteros estables
        PaletaColor pc;
        pc.nombre = nombre;
        for (int i = 0; i < 4; i++) pc.rgba[i] = c ? c[i] : 1.0f;
        cs.push_back(pc);
    }

    ObjectType getType() override { return ObjectType::ui; }
    void RenderObject() override {}   // nada en el 3D: vive en el Editor 2D
};
#endif
