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
    BRIDE_Guardar,    // menu ARCHIVO: Guardar (Ctrl+S) / Guardar como... / Guardar en el w3d o fuera del w3d / Cargar .lua
    BRIDE_Refresh,    // TRAE el archivo (lo editado afuera) + recarga los scripts EN VIVO si el juego corre
    BRIDE_Editar,     // menu EDITAR: deshacer / rehacer / cortar / copiar / pegar / buscar / reemplazar
    BRIDE_Seleccionar, // menu SELECCIONAR: seleccionar todo
    BRIDE_Compilar    // COMPILAR (solo con un shader .glsl abierto): guarda y compila los materiales que lo usan
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
//  Barra: [selector de script] [Archivo] [Refresh]. Guardar es ATOMICO
//  (escribe a un .w3dtmp y renombra: el .lua nunca queda a medias).
//  Refresh TRAE el archivo (lo que se edito con otro editor); si hay cambios
//  locales sin guardar, primero pregunta. Guardar y Refresh, con la sim
//  corriendo, recargan los lua_State de los objetos que usan el script
//  (SimScriptsCambiados: sus variables arrancan de cero y corre inicio() de
//  nuevo; el resto del juego NO se reinicia).
//
//  El estado de edicion es publico a proposito: el comando de test
//  'idetest' (main/test/W3dScript.cpp) maneja el buffer por esta API,
//  headless, sin GL ni teclado.
class IDE : public ViewportBase, public WithBorder, public Scrollable {
    public:
        // 1=3D 2=outliner 3=props 4=UV 5=timeline 6=2D 7=console 8=IDE
        int ViewportKind() const W3D_OVERRIDE { return 8; }
        Scrollable* ComoScrollable() W3D_OVERRIDE { return this; }

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
        std::vector<EstadoTexto> pilaRedo;   // lo deshecho, para rehacer (se vacia con cualquier edicion nueva)
        int undoUltimaOp;                // ultima op mutante (agrupa el tipeo seguido)

        // metricas del ultimo calculo de scroll (recalcular SOLO si cambia algo,
        // como lastContentRows del Outliner / lastCount de la Console)
        int lastLineas, lastMaxAncho;
        bool selDrag;                    // arrastre de seleccion con boton izquierdo en curso

        IDE();
        virtual ~IDE();

        // ================= API de edicion (headless: la usa idetest) =================
        bool AbrirArchivo(const std::string& ruta); // lee el .lua al buffer (tabs -> 4 espacios, sin \r)
        // ATOMICO: tmp + rename. false si fallo. Un archivo EXTERNO que cambio en el disco desde que se abrio (lo edito
        // otro programa) no se pisa sin preguntar; forzar = ya se pregunto.
        bool Guardar(bool forzar = false);
        // "Guardar" del menu Archivo / Ctrl+S: Guardar() y, si el script vive ADENTRO del proyecto, guarda tambien el
        // proyecto (el .w3d del disco queda con el script nuevo). Sin archivo abre "Guardar como..."
        bool GuardarEnProyecto();
        bool silencioso;                            // Guardar() sin aviso (lo da GuardarEnProyecto al final)
        // TRAE el archivo (lo editado afuera). Con cambios locales sin guardar pregunta primero; forzar = ya se pregunto.
        // Lo de antes queda en el deshacer.
        void Refresh(bool forzar = false);
        void RehacerTexto();
        // BUSCAR (el popup de Buscar/Reemplazar, PopUp/BuscarPopup): coincidencias de 't' en el buffer (por linea; byte
        // de inicio). mayus = distinguir mayusculas; palabra = solo palabras completas.
        void Coincidencias(const std::string& t, bool mayus, bool palabra, std::vector<int>* lin, std::vector<int>* col) const;
        bool IrACoincidencia(const std::string& t, bool mayus, bool palabra, int dir);   // selecciona la siguiente/anterior (da la vuelta)
        bool SeleccionEsCoincidencia(const std::string& t, bool mayus) const;
        int  ReemplazarTodas(const std::string& t, const std::string& por, bool mayus, bool palabra);   // un solo paso de deshacer
        void RecargarEnVivo(bool avisarSiNada);     // con el juego corriendo: recarga los objetos que usan este script
        // COMPILAR un shader (.glsl): si algun material lo usa, se guarda y se compilan esos materiales; si no, se
        // compila lo que hay en el editor de prueba. El resultado sale en un aviso (el error con su renglon)
        void CompilarShader();
        std::string textoDisco;                     // el archivo como estaba la ultima vez que se leyo o escribio
        bool hayTextoDisco;
        void SetTexto(const std::string& texto);    // reemplaza el buffer entero (resetea undo)
        std::string GetTexto() const;               // el buffer unido con '\n'
        void InsertarTexto(const std::string& t);   // en el cursor (pisa la seleccion); acepta '\n'
        void InsertarChar(char c);                  // un imprimible (agrupa el undo del tipeo)
        void InsertarTipeado(const std::string& t); // lo tipeado (letras UTF-8, sin saltos): agrupa el undo del tipeo
        void Backspace();
        void DelForward();
        void Enter();
        void TabKey(bool shift = false);            // espacios hasta el tope de 4; con varias lineas, sangria (Shift: la saca)
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
        // 'hasta': clasifica solo los bytes de antes (el dibujo: hasta lo que se ve en un renglon de
        // cientos de miles de letras). Cortada, 'tipos' queda de ese largo y 'enBloque' no se toca.
        static void LexLinea(const std::string& linea, bool* enBloque,
                             std::vector<unsigned char>* tipos, size_t hasta = (size_t)-1);

        // ================= viewport =================
        void Render() W3D_OVERRIDE;
        void Resize(int newW, int newH) W3D_OVERRIDE;
        void button_left() W3D_OVERRIDE;
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE; // drag izq = seleccion; medio = scroll
        void event_key_down(int tecla, bool repeticion) W3D_OVERRIDE; // Symbian / ruta normal
        void mouse_button_up(int boton) W3D_OVERRIDE; // IMPRESCINDIBLE: libera ViewPortClickDown (Symbian: con el dedo)
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE; // touch: arrastrar = scroll

        // sincroniza la barra (texto del selector = script abierto + '*' si esta sucio).
        // Puro (sin GL): lo llaman Render y el click de barra.
        void SincronizarBarra();
        // abre el PRIMER script del proyecto si hay uno y no hay nada abierto
        void AbrirPrimerScript();

        void NormalizarSel(int* l0, int* c0, int* l1, int* c1) const;   // la seleccion de principio a fin (Buscar la usa)

        // ---- TACTIL: doble toque = palabra, triple = renglon; PULSACION LARGA = lupa para poner el cursor justo; con
        //      una seleccion, AGARRADERAS en sus dos puntas para ajustarla (las maneja controles.cpp) ----
        int tapsSeguidos; unsigned int tUltimoTap; int ultTapX, ultTapY;
        int gestoTactil;                 // 0 = nada, 1 = lupa, 2 = agarradera del principio, 3 = del final
        int lupaX, lupaY;                // donde esta el dedo (pantalla)
        void PuntoATexto(int mx, int my, int* lin, int* col) const;   // pantalla -> renglon y byte (al principio de una letra)
        void TextoAPunto(int lin, int col, int* mx, int* my) const;   // renglon y byte -> donde iria el cursor (pantalla)
        void SeleccionarPalabra();       // la palabra donde esta el cursor
        void SeleccionarLinea();         // el renglon del cursor entero
        bool PulsacionLarga(int mx, int my);   // arranca la lupa (el cursor va al dedo)
        int  ManijaEn(int mx, int my) const;   // 0 = ninguna, 1 = la del principio, 2 = la del final
        bool ManijaAgarrar(int mx, int my);    // true = el dedo agarro una agarradera
        void GestoTactilMover(int mx, int my);
        void GestoTactilFin();

    private:
        void Mutar(int op);              // antes de CADA mutacion: undo + marcar sucio
        void AsegurarCursorVisible();    // ajusta PosX/PosY para que el cursor se vea
        void RecalcularScroll();         // rango del scrollbar con las metricas actuales
        std::vector<unsigned char> bloqueAlInicio; // estado --[[ ]] al INICIO de cada linea
        int anchoMaxCols;                // el renglon mas largo, en letras (el Render no recorre el texto entero)
        void RecalcularBloques();        // recalcula bloqueAlInicio y anchoMaxCols (tras cada edicion)
        int GutterW() const;             // ancho del margen de numeros de linea (px)
};

// ---- helpers globales (los usan controles.cpp / LayoutInput.cpp / idetest) ----

// TELEFONO: el lapiz (shift) apretado mientras se edita: abajo aparece la barra "Copiar | Pegar" (las dos softkeys)
void IDELapizSostenido(bool apretado);

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
