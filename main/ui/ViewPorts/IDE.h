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

// ROLES estables de los botones de la barra del IDE (dispatch por rol via BarRoleBtn,
// NO por indice: agregar/reordenar botones no rompe nada; mismo esquema que 2D/UV).
enum BarRoleIDE {
    BRIDE_Script = 1, // selector desplegable: los .lua del proyecto (marca el abierto)
    BRIDE_Save,       // menu ARCHIVO: Guardar (Ctrl+S) / Guardar como... / Guardar en el w3d o fuera del w3d / Cargar .lua
    BRIDE_Refresh,    // TRAE el archivo (lo editado afuera) + recarga los scripts EN VIVO si el juego corre
    BRIDE_Edit,       // menu EDITAR: deshacer / rehacer / cortar / copiar / pegar / buscar / reemplazar
    BRIDE_Select, // menu SELECCIONAR: seleccionar todo
    BRIDE_Compile    // COMPILAR (solo con un shader .glsl abierto): guarda y compila los materiales que lo usan
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
//  RESALTADO de sintaxis lua por linea (lexer simple, ver LexLine).
//
//  Barra: [selector de script] [Archivo] [Refresh]. Guardar es ATOMICO
//  (escribe a un .w3dtmp y renombra: el .lua nunca queda a medias).
//  Refresh TRAE el archivo (lo que se edito con otro editor); si hay cambios
//  locales sin guardar, primero pregunta. Guardar y Refresh, con la sim
//  corriendo, recargan los lua_State de los objetos que usan el script
//  (SimScriptsChanged: sus variables arrancan de cero y corre start() de
//  nuevo; el resto del juego NO se reinicia).
//
//  El estado de edicion es publico a proposito: el comando de test
//  'idetest' (main/test/W3dScript.cpp) maneja el buffer por esta API,
//  headless, sin GL ni teclado.
class IDE : public ViewportBase, public WithBorder, public Scrollable {
    public:
        // 1=3D 2=outliner 3=props 4=UV 5=timeline 6=2D 7=console 8=IDE
        int ViewportKind() const { return 8; }
        Scrollable* AsScrollable() { return this; }

        // ---- buffer ----
        std::vector<std::string> lines; // el texto (sin '\n'; siempre >= 1 linea)
        std::string file;               // ruta del .lua abierto ("" = ninguno)
        bool dirty;                     // cambios sin guardar (asterisco en el selector)

        // ---- cursor / seleccion ----
        int curLin, curCol;              // cursor (linea 0.., columna 0..len)
        bool hasSel;                     // hay una seleccion activa
        int selLin, selCol;              // ANCLA de la seleccion (el otro extremo es el cursor)

        // ---- undo (snapshots del buffer, agrupados por tipo de operacion) ----
        struct TextState {
            std::vector<std::string> lines;
            int curLin, curCol;
        };
        std::vector<TextState> stackUndo;
        std::vector<TextState> stackRedo;   // lo deshecho, para rehacer (se vacia con cualquier edicion nueva)
        int undoLastOp;                // ultima op mutante (agrupa el tipeo seguido)

        // metricas del ultimo calculo de scroll (recalcular SOLO si cambia algo,
        // como lastContentRows del Outliner / lastCount de la Console)
        int lastLines, lastMaxWidth;
        bool selDrag;                    // arrastre de seleccion con boton izquierdo en curso

        IDE();
        virtual ~IDE();

        // ================= API de edicion (headless: la usa idetest) =================
        bool OpenFile(const std::string& path); // lee el .lua al buffer (tabs -> 4 espacios, sin \r)
        // ATOMICO: tmp + rename. false si fallo. Un archivo EXTERNO que cambio en el disco desde que se abrio (lo edito
        // otro programa) no se pisa sin preguntar; forzar = ya se pregunto.
        bool Save(bool force = false);
        // "Guardar" del menu Archivo / Ctrl+S: Save() y, si el script vive ADENTRO del proyecto, guarda tambien el
        // proyecto (el .w3d del disco queda con el script nuevo). Sin archivo abre "Guardar como..."
        bool SaveInProject();
        bool silent;                            // Save() sin aviso (lo da SaveInProject al final)
        // TRAE el archivo (lo editado afuera). Con cambios locales sin guardar pregunta primero; forzar = ya se pregunto.
        // Lo de antes queda en el deshacer.
        void Refresh(bool force = false);
        void RedoText();
        // BUSCAR (el popup de Buscar/Reemplazar, PopUp/FindPopup): coincidencias de 't' en el buffer (por linea; byte
        // de inicio). mayus = distinguir mayusculas; palabra = solo palabras completas.
        void Matches(const std::string& t, bool caps, bool word, std::vector<int>* lin, std::vector<int>* col) const;
        bool GoToMatch(const std::string& t, bool caps, bool word, int dir);   // selecciona la siguiente/anterior (da la vuelta)
        bool SelectionIsMatch(const std::string& t, bool caps) const;
        int  ReplaceAll(const std::string& t, const std::string& by, bool caps, bool word);   // un solo paso de deshacer
        void ReloadLive(bool notifyIfNothing);     // con el juego corriendo: recarga los objetos que usan este script
        // COMPILAR un shader (.glsl): si algun material lo usa, se guarda y se compilan esos materiales; si no, se
        // compila lo que hay en el editor de prueba. El resultado sale en un aviso (el error con su renglon)
        void CompileShader();
        std::string textDisk;                     // el archivo como estaba la ultima vez que se leyo o escribio
        bool hasTextDisk;
        void SetText(const std::string& text);  // reemplaza el buffer entero (resetea undo)
        std::string GetText() const;            // el buffer unido con '\n'
        void InsertText(const std::string& t);  // en el cursor (pisa la seleccion); acepta '\n'
        void InsertChar(char c);                // un imprimible (agrupa el undo del tipeo)
        void InsertTyped(const std::string& t); // lo tipeado (letras UTF-8, sin saltos): agrupa el undo del tipeo
        void Backspace();
        void DelForward();
        void Enter();
        void TabKey(bool shift = false);            // espacios hasta el tope de 4; con varias lineas, sangria (Shift: la saca)
        void CursorA(int lin, int col, bool shift); // mueve el cursor (clampea); shift extiende la seleccion
        void MoveCursor(int dLin, int dCol, bool shift); // flechas (dCol cruza de linea)
        void Home(bool shift);
        void End(bool shift);
        void PageUpDown(int dir, bool shift); // PgUp (-1) / PgDn (+1)
        void SelectAll();                     // Ctrl+A
        std::string TextSelected() const;
        void DeleteSelection();
        void Copy();                           // Ctrl+C (clipboard SDL + copia interna)
        void Cut();                            // Ctrl+X
        void Paste();                          // Ctrl+V
        void UndoText();                       // Ctrl+Z
        // una tecla del editor (W3dKey). true = consumida (el atajo global NO corre).
        bool KeyEditor(int key, bool repeat);
        // texto tipeado (UTF-8 de SDL_TEXTINPUT): inserta los imprimibles ASCII (32..126);
        // el resto (acentos, multibyte) se ignora (la fuente pixel del editor es ASCII).
        void TextTyped(const char* utf8);
        void ClickContent(int mx, int my, bool shift); // click posiciona el cursor (coords de ventana)
        // lo escrito pasa al proyecto: un script nuevo se crea adentro del .w3d (en memoria) y uno de adentro se
        // actualiza (cada cuadro y al destruirse el IDE). Ver IDE.cpp
        void SyncMemory();

        // ---- LEXER lua por linea. Clasifica CADA caracter de la linea:
        //   0 = normal, 1 = comentario, 2 = string, 3 = keyword, 4 = numero
        // 'inBlock' entra/sale con el estado de comentario de bloque --[[ ]] entre
        // lineas ('tipos' puede ser NULL para solo avanzar el estado). LIMITACIONES
        // (documentadas): no soporta niveles anidados [==[ ]==]; los strings largos
        // [[...]] solo se pintan dentro de su linea (multilinea queda como normal).
        // 'hasta': clasifica solo los bytes de antes (el dibujo: hasta lo que se ve en un renglon de
        // cientos de miles de letras). Cortada, 'tipos' queda de ese largo y 'inBlock' no se toca.
        static void LexLine(const std::string& line, bool* inBlock,
                             std::vector<unsigned char>* types, size_t until = (size_t)-1);

        // ================= viewport =================
        void Render() W3D_OVERRIDE;
        void Resize(int newW, int newH) W3D_OVERRIDE;
        void button_left() W3D_OVERRIDE;
        void event_mouse_motion(int mx, int my) W3D_OVERRIDE; // drag izq = seleccion; medio = scroll
        void event_key_down(int keyCode, bool repeat) W3D_OVERRIDE; // Symbian / ruta normal
        void mouse_button_up(int button) W3D_OVERRIDE; // IMPRESCINDIBLE: libera ViewPortClickDown (Symbian: con el dedo)
        void event_mouse_wheel(float dy, int mx, int my) W3D_OVERRIDE;
        bool event_finger_scroll(int px, int py, int dx, int dy) W3D_OVERRIDE; // touch: arrastrar = scroll

        // sincroniza la barra (texto del selector = script abierto + '*' si esta sucio).
        // Puro (sin GL): lo llaman Render y el click de barra.
        void SyncBar();
        // abre el PRIMER script del proyecto si hay uno y no hay nada abierto
        void OpenFirstScript();

        void NormalizeSel(int* l0, int* c0, int* l1, int* c1) const;   // la seleccion de principio a fin (Buscar la usa)

        // ---- AUTOCOMPLETADO (como VS Code): al escribir un nombre sale la lista de lo que puede ir ahi -las funciones
        //      de Whisk3D y de Lua, las palabras clave y los nombres del propio script (sus propiedades, funciones y
        //      variables)- con la firma y la descripcion del elegido. Flechas para elegir, Enter o Tab para escribirlo,
        //      Esc para cerrarla; Ctrl+Espacio la abre a mano. Con el dedo, se toca el que se quiere ----
        struct Completion {
            std::string name;        // lo que se escribe
            std::string signature;   // la firma (las funciones de Whisk3D, del manual)
            std::string desc;        // para que sirve
            unsigned char kind;      // 0 palabra del script, 1 funcion del motor, 2 palabra clave, 3 propiedad, 4 funcion del script
        };
        bool acOpen;
        std::vector<Completion> acItems;
        int acSel, acTop;            // el elegido y el primero que se ve
        int acLin, acStartCol;       // donde empieza la palabra que se completa (renglon y byte)
        int acBox[4];                // donde se dibujo la lista (x0, y0, x1, y1, local al viewport): el toque la usa
        void CompletionUpdate(bool force);   // arma o filtra la lista con la palabra del cursor (force: Ctrl+Espacio)
        void CompletionClose();
        bool CompletionAccept();             // escribe el elegido (false = no habia lista)
        bool CompletionKey(int key);         // una tecla con la lista abierta (true = consumida)
        // ---- IR A LA DEFINICION (click derecho, F12 o Ctrl+click): el nombre de esa posicion. Si esta en el script
        //      (una funcion, un local, un parametro, una propiedad) el cursor va ahi; si es del motor, se muestra su
        //      firma y para que sirve al lado de la palabra ----
        bool GoToDefinition(int lin, int col);
        bool WordAt(int lin, int col, int* b0, int* b1) const;   // la palabra en esa posicion (con sus puntos: math.floor)
        std::string defHelp;         // la ayuda de una funcion del motor (vacia = no se muestra)
        int defHelpLin, defHelpCol;

        // ---- TACTIL: doble toque = palabra, triple = renglon; PULSACION LARGA = lupa para poner el cursor justo; con
        //      una seleccion, AGARRADERAS en sus dos puntas para ajustarla (las maneja controls.cpp) ----
        int consecutiveTaps; unsigned int tLastTap; int lastTapX, lastTapY;
        int touchGesture;                          // 0 = nada, 1 = lupa, 2 = agarradera del principio, 3 = del final
        int magnifierX, magnifierY;                // donde esta el dedo (pantalla)
        void PointToText(int mx, int my, int* lin, int* col) const;   // pantalla -> renglon y byte (al principio de una letra)
        void TextToPoint(int lin, int col, int* mx, int* my) const;   // renglon y byte -> donde iria el cursor (pantalla)
        void SelectWord();       // la palabra donde esta el cursor
        void SelectLine();       // el renglon del cursor entero
        bool PressLong(int mx, int my);        // arranca la lupa (el cursor va al dedo)
        int  HandleIn(int mx, int my) const;   // 0 = ninguna, 1 = la del principio, 2 = la del final
        bool HandleGrab(int mx, int my);       // true = el dedo agarro una agarradera
        void TouchGestureMove(int mx, int my);
        void TouchGestureEnd();

    private:
        void Mutate(int op);           // antes de CADA mutacion: undo + marcar sucio
        void SpawnInProject();         // un script nuevo (sin archivo) pasa a ser una entrada del proyecto
        void EnsureCursorVisible();    // ajusta PosX/PosY para que el cursor se vea
        void RecalculateScroll();      // rango del scrollbar con las metricas actuales
        std::vector<unsigned char> blockToStart; // estado --[[ ]] al INICIO de cada linea
        int widthMaxCols;                // el renglon mas largo, en letras (el Render no recorre el texto entero)
        void RecalculateBlocks();        // recalcula blockToStart y widthMaxCols (tras cada edicion)
        int GutterW() const;             // ancho del margen de numeros de linea (px)
};

// ---- helpers globales (los usan controls.cpp / LayoutInput.cpp / idetest) ----

// sube cada vez que el texto de un script cambia en el proyecto o en el disco (lo escrito en un IDE, Guardar, Refresh): el
// panel de Propiedades rearma las tarjetas de los scripts con sus propiedades nuevas
extern unsigned g_ideScriptVersion;

// TELEFONO: el lapiz (shift) apretado mientras se edita: abajo aparece la barra "Copiar | Pegar" (las dos softkeys)
void IDEHeldPencil(bool pressed);

// junta los .lua del proyecto: los referenciados por las escenas cargadas (scriptData
// de todo el arbol) + los *.lua de la carpeta content/ del proyecto. Rutas completas,
// sin repetidos, en orden escena -> contenido (alfabetico).
void IDECollectScripts(std::vector<std::string>* paths);

// captura del TECLADO (PC): si el viewport con foco por hover es un IDE, la tecla es
// del editor de texto y NO debe disparar atajos globales. true = consumida.
bool IDEKeyboardCapture(int key, bool repeat);
// captura del TEXTO tipeado (PC, SDL_TEXTINPUT). true = lo consumio un IDE enfocado.
bool IDETextText(const char* utf8);

// nombre "lindo" de una ruta de script para la UI (relativo al proyecto si se puede)
std::string IDEScriptName(const std::string& path);

#endif // IDE_H
