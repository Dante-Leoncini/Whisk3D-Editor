/*
 * ==============================================================================
 *  w3dt9.cpp -- entrada de texto multi-tap (T9) del keypad del N95.
 *
 *  El keypad escribe en el campo enfocado (g_activeTextField) O en el editor de
 *  scripts (IDE) cuando esta en modo edicion. Multi-tap clasico: apretar una tecla
 *  cicla sus letras; sin apretar 1 seg el char se confirma; otra tecla confirma la
 *  anterior. '#' cicla el modo (Abc/abc/ABC) y MANTENIDO -> 123. El char "a medio
 *  elegir" ya esta insertado: al re-tap se saca y se pone el siguiente (Backspace +
 *  InsertChar), asi el usuario ve el candidato.
 * ==============================================================================
 */

#include "w3dt9.h"
#include <e32std.h>
#include <e32keys.h>
#include <string.h>
#include "TextField.h"     // TextField, g_activeTextField (USERINCLUDE libs\WhiskUI\widgets)

extern bool NumEditActive();        // WhiskUI/PropFloat.cpp: hay una edicion NUMERICA (un PropFloat)?

// ---- destino del T9: ademas de los TextField (g_activeTextField), el editor de scripts (IDE). Estos hooks los
// setea la plataforma (w3dlayout) apuntando al IDE ACTIVO. Con W3dT9IDEEditing()==true el T9 escribe en el IDE. ----
bool (*W3dT9IDEEditing)()     = 0;   // el IDE activo esta en modo edicion de texto (barFocusIndex < 0)
void (*W3dT9IDEInsert)(int c) = 0;   // insertar un imprimible en el IDE
void (*W3dT9IDEDelete)()       = 0;  // backspace en el IDE
bool (*W3dT9IDEStartWord)() = 0;     // true si el cursor del IDE esta al inicio de palabra (modo Abc)

static bool IDEEdit() { return (W3dT9IDEEditing && W3dT9IDEEditing()) ? true : false; }
static void SinkInsert(int c) { if (IDEEdit()) { if (W3dT9IDEInsert) W3dT9IDEInsert(c); } else if (g_activeTextField) { TextFieldContinueFocus(); g_activeTextField->InsertChar(c); } }
static void SinkDelete()        { if (IDEEdit()) { if (W3dT9IDEDelete) W3dT9IDEDelete(); }     else if (g_activeTextField) { TextFieldContinueFocus(); g_activeTextField->Backspace(); } }

// modo: 0=Abc (1a mayuscula) / 1=abc / 2=ABC / 3=123 (numerico)
static int   gMode = 0;
static TInt  gKey  = -1;    // tecla en multi-tap ('0'..'9'), -1 = ninguna pendiente
static int   gTap  = 0;     // indice dentro del ciclo de gKey
static TUint gTick = 0;     // NTickCount del ultimo tap (timeout)
static TUint gHashDown = 0; // NTickCount del down de '#'
static TUint gKeyFirstDown = 0; // NTickCount del down que ARRANCO la pulsacion actual (para detectar "mantener")
static bool  gHeldLocked = false; // ya se inserto el DIGITO por mantener la tecla -> no seguir ciclando ni pasarse
static bool  gHadUp = true;       // hubo key-UP desde el ultimo down? distingue MANTENER (auto-repeat, sin release)
                                  // de un re-tap DELIBERADO (release + nuevo tap)

#ifdef __WINS__
static const TUint kHz = 200;   // el emulador corre NTickCount a 200Hz
#else
static const TUint kHz = 1000;  // el device (N95) a 1000Hz = ms
#endif
static const TUint kTimeout  = kHz;        // 1 seg para confirmar el char
static const TUint kHoldHash = kHz / 2;    // 0.5 seg = '#' mantenido -> 123

// ciclo de cada tecla (minusculas + su digito). '1' = puntuacion; '0' = espacio + 0.
static const char* Cycle(TInt sc) {
    switch (sc) {
        case '0': return " 0";
        case '1': return ".,?!1@\"-_():;&/%*";
        case '2': return "abc2";
        case '3': return "def3";
        case '4': return "ghi4";
        case '5': return "jkl5";
        case '6': return "mno6";
        case '7': return "pqrs7";
        case '8': return "tuv8";
        case '9': return "wxyz9";
        default:  return 0;
    }
}

TBool W3dT9Active() { return (g_activeTextField != 0 || IDEEdit()) ? ETrue : EFalse; }

// el campo enfocado es un NUMERO (un PropFloat o un TextField marcado): sin letras
static bool NumericField() { return NumEditActive() || (!IDEEdit() && g_activeTextField && g_activeTextField->numeric); }

TBool W3dT9HasPending() { return (gKey >= 0 && (g_activeTextField || IDEEdit())) ? ETrue : EFalse; }

void W3dT9Reset() { gKey = -1; gTap = 0; }

// caso de una letra segun el modo (+ inicio de palabra para "Abc")
static char WithCase(char c) {
    if (c < 'a' || c > 'z') return c;          // no es letra
    if (gMode == 1) return c;                  // abc
    if (gMode == 2) return (char)(c - 32);     // ABC
    // Abc: mayuscula si es la 1a letra del campo o va despues de un espacio
    bool caps;
    if (IDEEdit()) {
        caps = W3dT9IDEStartWord ? W3dT9IDEStartWord() : true;
    } else {
        TextField* f = g_activeTextField;
        caps = (f->caret <= 0);
        if (!caps && f->caret - 1 < (int)f->text.size() && f->text[f->caret - 1] == ' ') caps = true;
    }
    return caps ? (char)(c - 32) : c;
}

void W3dT9Tick() {
    if (gKey < 0) return;
    if (User::NTickCount() - gTick >= kTimeout) W3dT9Reset();  // confirma: el char ya quedo insertado
}

const char* W3dT9TextMode() {
    if (!g_activeTextField && !IDEEdit()) return "";
    if (gMode == 3 || NumericField()) return "123";
    if (gMode == 1) return "abc";
    if (gMode == 2) return "ABC";
    return "Abc";
}

TBool W3dT9Key(TInt aScan, TBool aDown) {
    if (!g_activeTextField && !IDEEdit()) return EFalse;

    // CAMPO NUMERICO: el '#' es la MATEMATICA, multi-tap como una letra: - + * / ( ) (el punto es el '*' del keypad)
    if (aScan == EStdKeyHash && NumericField()) {
        if (aDown) {
            static const char kOps[] = "-+*/()";
            const TUint now = User::NTickCount();
            if (gKey == EStdKeyHash && now - gTick < kTimeout) { SinkDelete(); gTap = (gTap + 1) % 6; }
            else gTap = 0;
            gKey = EStdKeyHash; gTick = now;
            SinkInsert((int)kOps[gTap]);
        }
        return ETrue;
    }

    // '#': tap = cicla modo (Abc->abc->ABC); mantenido = 123. Se resuelve al SOLTAR.
    if (aScan == EStdKeyHash) {
        if (aDown) { gHashDown = User::NTickCount(); }
        else {
            if (User::NTickCount() - gHashDown >= kHoldHash) gMode = 3;      // 123
            else gMode = (gMode == 3) ? 0 : ((gMode + 1) % 3);              // Abc->abc->ABC->Abc
            W3dT9Reset();
        }
        return ETrue;
    }
    // key-UP de una tecla numerica: marca que HUBO release (distingue MANTENER de re-tap deliberado). Si la tecla
    // venia LOCKEADA por mantener (ya inserto el digito), al soltar se CONFIRMA: el proximo tap arranca fresco (no
    // re-cicla el digito). El resto de teclas solo se procesan en el key-down.
    if (!aDown) {
        if (Cycle(aScan)) {
            gHadUp = true;
            if (gHeldLocked && gKey == aScan) { gKey = -1; gTap = 0; gHeldLocked = false; }
        }
        return ETrue;
    }

    // MODO NUMERICO (o un campo numerico: PropFloat, el valor de un mapa del material): los digitos van directo, sin
    // multi-tap. Antes el metalico era un campo de TEXTO: mantener el "1" daba el punto
    if (gMode == 3 || NumericField()) {
        if (aScan >= '0' && aScan <= '9') { W3dT9Reset(); SinkInsert((int)aScan); return ETrue; }
        return EFalse;
    }

    // MODO TEXTO: multi-tap
    const char* cycle = Cycle(aScan);
    if (!cycle) return EFalse;
    int len = (int)strlen(cycle);
    // MANTENER la tecla = auto-repeat del OS (mismo down SIN key-up entre medio). A los 0.5s inserta el DIGITO y lo
    // LOCKEA (Nokia: mantener '1' = '1', sin seguir ciclando la puntuacion ni pasarse). El re-tap DELIBERADO (con
    // release) tiene gHadUp=true -> NO entra aca, cicla normal.
    bool keeping = (gKey == aScan && !gHadUp);
    gHadUp = false;
    if (keeping) {
        if (!gHeldLocked && (User::NTickCount() - gKeyFirstDown) >= kHoldHash) {
            SinkDelete(); SinkInsert((unsigned char)aScan); gHeldLocked = true;  // el DIGITO de la tecla
        }
        gTick = User::NTickCount();
        return ETrue;   // mientras se mantiene: el candidato queda quieto (o el digito lockeado), no cicla
    }
    // pulsacion FRESCA (tecla nueva o re-tap deliberado): arranca el reloj del "mantener" para esta tecla
    gKeyFirstDown = User::NTickCount();
    gHeldLocked = false;
    bool retap = (gKey == aScan && (User::NTickCount() - gTick) < kTimeout);
    if (retap) {
        SinkDelete();                         // sacar el candidato anterior de ESTA tecla
        gTap = (gTap + 1) % len;
    } else {
        gTap = 0;                             // tecla nueva: la anterior queda confirmada (ya insertada)
    }
    SinkInsert((unsigned char)WithCase(cycle[gTap]));
    gKey = aScan;
    gTick = User::NTickCount();
    return ETrue;
}
