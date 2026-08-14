#ifndef IDE_H
#define IDE_H

#ifdef _WIN32
#ifndef W3D_SYMBIAN
    #include <windows.h>
#endif
#endif

#include "ViewPorts/ViewPorts.h"
#include "ViewPorts/WithBorder.h" // borde (verde si activo) como Outliner/Console
#include "ViewPorts/ScrollBar.h"  // Scrollable: barras v/h + rueda + drag (patron Console/Outliner)
#include <string>
#include <vector>

// ROLES estables de los botones de la barra del IDE (dispatch por rol via BarRolBtn,
// NO por indice: agregar/reordenar botones no rompe nada; mismo esquema que 2D/UV).
enum BarRolIDE {
    BRIDE_Script = 1, // selector desplegable: los .lua del proyecto (marca el abierto)
    BRIDE_Guardar,    // guarda el archivo (Ctrl+S)
    BRIDE_Refresh     // guarda + recarga los scripts EN VIVO si el juego corre
};

// =====================================================================
//  IDE — editor de TEXTO para los scripts lua del proyecto.
// =====================================================================
//  Lo basico de un editor: buffer multi-linea, cursor (flechas/Home/End/
//  PgUp/PgDn/click), seleccion (Shift+flechas y arrastre del mouse),
//  escribir (el texto entra por SDL_TEXTINPUT mientras el IDE tiene el
//  foco por hover: NO dispara atajos globales del editor), copiar/cortar/
//  pegar con el clipboard REAL de SDL (Ctrl+C/X/V), undo simple (Ctrl+Z,
//  snapshots del buffer agrupados por operacion), numeros de linea, y
//  RESALTADO de sintaxis lua por linea (lexer simple, ver LexLinea).
//
//  Barra: [selector de script] [Save] [Refresh]. Guardar es ATOMICO
//  (escribe a un .w3dtmp y renombra: el .lua nunca queda a medias).
//  Refresh con la sim corriendo recarga los lua_State de los objetos que
//  usan el script editado (SimScriptsCambiados: sus variables arrancan
//  de cero y corre inicio() de nuevo; el resto del juego NO se reinicia).
//
//  El estado de edicion es publico a proposito: el comando de test
//  'idetest' (main/test/W3dScript.cpp) maneja el buffer por esta API,
//  headless, sin GL ni teclado.
class IDE : public ViewportBase, public WithBorder, public Scrollable {
    public:
        // 1=3D 2=outliner 3=props 4=UV 5=timeline 6=2D 7=console 8=IDE
        int ViewportKind() const { return 8; }
        Scrollable* ComoScrollable() { return this; }

        // ---- buffer ----
        std::vector<std::string> lineas; // el texto (sin '\n'; siempre >= 1 linea)
        std::string archivo;             // ruta del .lua abierto ("" = ninguno)
        bool sucio;                      // cambios sin guardar (asterisco en el selector)

        // ---- cursor / seleccion ----
        int curLin, curCol;              // cursor (linea 0.., columna 0..len)
        bool haySel;                     // hay una seleccion activa
        int selLin, selCol;              // ANCLA de la seleccion (el otro extremo es el cursor)

        // ---- undo (snapshots del buffer, agrupados por tipo de operacion) ----
        struct EstadoTexto {
            std::vector<std::string> lineas;
            int curLin, curCol;
        };
        std::vector<EstadoTexto> pilaUndo;
        int undoUltimaOp;                // ultima op mutante (agrupa el tipeo seguido)

        // metricas del ultimo calculo de scroll (recalcular SOLO si cambia algo,
        // como lastContentRows del Outliner / lastCount de la Console)
        int lastLineas, lastMaxAncho;
        bool selDrag;                    // arrastre de seleccion con boton izquierdo en curso

        IDE();
        virtual ~IDE();

        // ================= API de edicion (headless: la usa idetest) =================
        bool AbrirArchivo(const std::string& ruta); // lee el .lua al buffer (tabs -> 4 espacios, sin \r)
        bool Guardar();                             // ATOMICO: tmp + rename. false si fallo
        void Refresh();                             // guardar + recargar scripts en vivo (ver arriba)
        void SetTexto(const std::string& texto);    // reemplaza el buffer entero (resetea undo)
        std::string GetTexto() const;               // el buffer unido con '\n'
        void InsertarTexto(const std::string& t);   // en el cursor (pisa la seleccion); acepta '\n'
        void InsertarChar(char c);                  // un imprimible (agrupa el undo del tipeo)
        void Backspace();
        void DelForward();
        void Enter();
        void TabKey();                              // inserta 4 espacios
        void CursorA(int lin, int col, bool shift); // mueve el cursor (clampea); shift extiende la seleccion
        void MoverCursor(int dLin, int dCol, bool shift); // flechas (dCol cruza de linea)
        void Home(bool shift);
        void End(bool shift);
        void PaginaArribaAbajo(int dir, bool shift); // PgUp (-1) / PgDn (+1)
        void SeleccionarTodo();                     // Ctrl+A
        std::string TextoSeleccionado() const;
        void BorrarSeleccion();
        void Copiar();                              // Ctrl+C (clipboard SDL + copia interna)
        void Cortar();                              // Ctrl+X
        void Pegar();                               // Ctrl+V
        void DeshacerTexto();                       // Ctrl+Z
        // una tecla del editor (W3dTecla). true = consumida (el atajo global NO corre).
        bool TeclaEditor(int tecla, bool repeticion);
        // texto tipeado (UTF-8 de SDL_TEXTINPUT): inserta los imprimibles ASCII (32..126);
        // el resto (acentos, multibyte) se ignora (la fuente pixel del editor es ASCII).
        void TextoTipeado(const char* utf8);
        void ClickContenido(int mx, int my, bool shift); // click posiciona el cursor (coords de ventana)

        // ---- LEXER lua por linea. Clasifica CADA caracter de la linea:
        //   0 = normal, 1 = comentario, 2 = string, 3 = keyword, 4 = numero
        // 'enBloque' entra/sale con el estado de comentario de bloque --[[ ]] entre
        // lineas ('tipos' puede ser NULL para solo avanzar el estado). LIMITACIONES
        // (documentadas): no soporta niveles anidados [==[ ]==]; los strings largos
        // [[...]] solo se pintan dentro de su linea (multilinea queda como normal).
        static void LexLinea(const std::string& linea, bool* enBloque,
                             std::vector<unsigned char>* tipos);

        // ================= viewport =================
        void Render() override;
        void Resize(int newW, int newH) override;
        void button_left() override;
        void event_mouse_motion(int mx, int my) override; // drag izq = seleccion; medio = scroll
        void event_key_down(int tecla, bool repeticion) override; // Symbian / ruta normal
#ifndef W3D_SYMBIAN
        void mouse_button_up(int boton) override; // IMPRESCINDIBLE: libera ViewPortClickDown
        void event_mouse_wheel(float dy, int mx, int my) override;
#endif
        bool event_finger_scroll(int px, int py, int dx, int dy) override; // touch: arrastrar = scroll

        // sincroniza la barra (texto del selector = script abierto + '*' si esta sucio).
        // Puro (sin GL): lo llaman Render y el click de barra.
        void SincronizarBarra();
        // abre el PRIMER script del proyecto si hay uno y no hay nada abierto
        void AbrirPrimerScript();

    private:
        void Mutar(int op);              // antes de CADA mutacion: undo + marcar sucio
        void NormalizarSel(int* l0, int* c0, int* l1, int* c1) const;
        void AsegurarCursorVisible();    // ajusta PosX/PosY para que el cursor se vea
        void RecalcularScroll();         // rango del scrollbar con las metricas actuales
        std::vector<unsigned char> bloqueAlInicio; // estado --[[ ]] al INICIO de cada linea
        void RecalcularBloques();        // recalcula bloqueAlInicio (tras cada edicion)
        int GutterW() const;             // ancho del margen de numeros de linea (px)
};

// ---- helpers globales (los usan controles.cpp / LayoutInput.cpp / idetest) ----

// junta los .lua del proyecto: los referenciados por las escenas cargadas (scriptDatos
// de todo el arbol) + los *.lua de la carpeta contenido/ del proyecto. Rutas completas,
// sin repetidos, en orden escena -> contenido (alfabetico).
void IDEColectarScripts(std::vector<std::string>* rutas);

// captura del TECLADO (PC): si el viewport con foco por hover es un IDE, la tecla es
// del editor de texto y NO debe disparar atajos globales. true = consumida.
bool IDETecladoCaptura(int tecla, bool repeticion);
// captura del TEXTO tipeado (PC, SDL_TEXTINPUT). true = lo consumio un IDE enfocado.
bool IDETextoTexto(const char* utf8);

// nombre "lindo" de una ruta de script para la UI (relativo al proyecto si se puede)
std::string IDENombreScript(const std::string& ruta);

#endif // IDE_H
