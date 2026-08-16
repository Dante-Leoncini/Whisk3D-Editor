/*
 * ==============================================================================
 *  w3dt9.cpp -- entrada de texto multi-tap (T9) del keypad del N95.
 *
 *  El keypad escribe en el campo enfocado (g_textFieldActivo) O en el editor de
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
#include "TextField.h"     // TextField, g_textFieldActivo (USERINCLUDE libs\WhiskUI\widgets)

extern bool NumEditActivo();        // WhiskUI/PropFloat.cpp: hay una edicion NUMERICA (un PropFloat)?

// ---- destino del T9: ademas de los TextField (g_textFieldActivo), el editor de scripts (IDE). Estos hooks los
// setea la plataforma (w3dlayout) apuntando al IDE ACTIVO. Con W3dT9IDEEditando()==true el T9 escribe en el IDE. ----
bool (*W3dT9IDEEditando)()     = 0;   // el IDE activo esta en modo edicion de texto (barFocusIndex < 0)
void (*W3dT9IDEInsertar)(int c) = 0;  // insertar un imprimible en el IDE
void (*W3dT9IDEBorrar)()       = 0;   // backspace en el IDE
bool (*W3dT9IDEInicioPalabra)() = 0;  // true si el cursor del IDE esta al inicio de palabra (modo Abc)

static bool IDEEdit() { return (W3dT9IDEEditando && W3dT9IDEEditando()) ? true : false; }
static void SinkInsertar(int c) { if (IDEEdit()) { if (W3dT9IDEInsertar) W3dT9IDEInsertar(c); } else if (g_textFieldActivo) g_textFieldActivo->InsertChar(c); }
static void SinkBorrar()        { if (IDEEdit()) { if (W3dT9IDEBorrar) W3dT9IDEBorrar(); }     else if (g_textFieldActivo) g_textFieldActivo->Backspace(); }

// modo: 0=Abc (1a mayuscula) / 1=abc / 2=ABC / 3=123 (numerico)
static int   gModo = 0;
static TInt  gKey  = -1;    // tecla en multi-tap ('0'..'9'), -1 = ninguna pendiente
static int   gTap  = 0;     // indice dentro del ciclo de gKey
static TUint gTick = 0;     // NTickCount del ultimo tap (timeout)
static TUint gHashDown = 0; // NTickCount del down de '#'
static TUint gKeyFirstDown = 0; // NTickCount del down que ARRANCO la pulsacion actual (para detectar "mantener")
static bool  gHeldLocked = false; // ya se inserto el DIGITO por mantener la tecla -> no seguir ciclando ni pasarse
static bool  gHuboUp = true;      // hubo key-UP desde el ultimo down? distingue MANTENER (auto-repeat, sin release)
                                  // de un re-tap DELIBERADO (release + nuevo tap)

#ifdef __WINS__
static const TUint kHz = 200;   // el emulador corre NTickCount a 200Hz
#else
static const TUint kHz = 1000;  // el device (N95) a 1000Hz = ms
#endif
static const TUint kTimeout  = kHz;        // 1 seg para confirmar el char
static const TUint kHoldHash = kHz / 2;    // 0.5 seg = '#' mantenido -> 123

// ciclo de cada tecla (minusculas + su digito). '1' = puntuacion; '0' = espacio + 0.
static const char* Ciclo(TInt sc) {
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

TBool W3dT9Activo() { return (g_textFieldActivo != 0 || IDEEdit()) ? ETrue : EFalse; }

TBool W3dT9HayPendiente() { return (gKey >= 0 && (g_textFieldActivo || IDEEdit())) ? ETrue : EFalse; }

void W3dT9Reset() { gKey = -1; gTap = 0; }

// caso de una letra segun el modo (+ inicio de palabra para "Abc")
static char ConCaso(char c) {
    if (c < 'a' || c > 'z') return c;          // no es letra
    if (gModo == 1) return c;                  // abc
    if (gModo == 2) return (char)(c - 32);     // ABC
    // Abc: mayuscula si es la 1a letra del campo o va despues de un espacio
    bool mayus;
    if (IDEEdit()) {
        mayus = W3dT9IDEInicioPalabra ? W3dT9IDEInicioPalabra() : true;
    } else {
        TextField* f = g_textFieldActivo;
        mayus = (f->caret <= 0);
        if (!mayus && f->caret - 1 < (int)f->text.size() && f->text[f->caret - 1] == ' ') mayus = true;
    }
    return mayus ? (char)(c - 32) : c;
}

void W3dT9Tick() {
    if (gKey < 0) return;
    if (User::NTickCount() - gTick >= kTimeout) W3dT9Reset();  // confirma: el char ya quedo insertado
}

const char* W3dT9ModoTexto() {
    if (!g_textFieldActivo && !IDEEdit()) return "";
    if (gModo == 3 || NumEditActivo()) return "123";
    if (gModo == 1) return "abc";
    if (gModo == 2) return "ABC";
    return "Abc";
}

TBool W3dT9Tecla(TInt aScan, TBool aDown) {
    if (!g_textFieldActivo && !IDEEdit()) return EFalse;

    // '#': tap = cicla modo (Abc->abc->ABC); mantenido = 123. Se resuelve al SOLTAR.
    if (aScan == EStdKeyHash) {
        if (aDown) { gHashDown = User::NTickCount(); }
        else {
            if (User::NTickCount() - gHashDown >= kHoldHash) gModo = 3;      // 123
            else gModo = (gModo == 3) ? 0 : ((gModo + 1) % 3);              // Abc->abc->ABC->Abc
            W3dT9Reset();
        }
        return ETrue;
    }
    // key-UP de una tecla numerica: marca que HUBO release (distingue MANTENER de re-tap deliberado). Si la tecla
    // venia LOCKEADA por mantener (ya inserto el digito), al soltar se CONFIRMA: el proximo tap arranca fresco (no
    // re-cicla el digito). El resto de teclas solo se procesan en el key-down.
    if (!aDown) {
        if (Ciclo(aScan)) {
            gHuboUp = true;
            if (gHeldLocked && gKey == aScan) { gKey = -1; gTap = 0; gHeldLocked = false; }
        }
        return ETrue;
    }

    // MODO NUMERICO (o campo PropFloat): los digitos van directo, sin multi-tap
    if (gModo == 3 || NumEditActivo()) {
        if (aScan >= '0' && aScan <= '9') { W3dT9Reset(); SinkInsertar((int)aScan); return ETrue; }
        return EFalse;
    }

    // MODO TEXTO: multi-tap
    const char* ciclo = Ciclo(aScan);
    if (!ciclo) return EFalse;
    int len = (int)strlen(ciclo);
    // MANTENER la tecla = auto-repeat del OS (mismo down SIN key-up entre medio). A los 0.5s inserta el DIGITO y lo
    // LOCKEA (Nokia: mantener '1' = '1', sin seguir ciclando la puntuacion ni pasarse). El re-tap DELIBERADO (con
    // release) tiene gHuboUp=true -> NO entra aca, cicla normal.
    bool manteniendo = (gKey == aScan && !gHuboUp);
    gHuboUp = false;
    if (manteniendo) {
        if (!gHeldLocked && (User::NTickCount() - gKeyFirstDown) >= kHoldHash) {
            SinkBorrar(); SinkInsertar((unsigned char)aScan); gHeldLocked = true;  // el DIGITO de la tecla
        }
        gTick = User::NTickCount();
        return ETrue;   // mientras se mantiene: el candidato queda quieto (o el digito lockeado), no cicla
    }
    // pulsacion FRESCA (tecla nueva o re-tap deliberado): arranca el reloj del "mantener" para esta tecla
    gKeyFirstDown = User::NTickCount();
    gHeldLocked = false;
    bool retap = (gKey == aScan && (User::NTickCount() - gTick) < kTimeout);
    if (retap) {
        SinkBorrar();                         // sacar el candidato anterior de ESTA tecla
        gTap = (gTap + 1) % len;
    } else {
        gTap = 0;                             // tecla nueva: la anterior queda confirmada (ya insertada)
    }
    SinkInsertar((unsigned char)ConCaso(ciclo[gTap]));
    gKey = aScan;
    gTick = User::NTickCount();
    return ETrue;
}
