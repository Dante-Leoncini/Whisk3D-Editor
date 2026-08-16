/*
 * ==============================================================================
 *  w3dt9.cpp -- entrada de texto multi-tap (T9) del keypad del N95.
 *
 *  El keypad escribe en el campo enfocado (g_textFieldActivo). Multi-tap clasico:
 *  apretar una tecla cicla sus letras; sin apretar 1 seg el char se confirma; otra
 *  tecla confirma la anterior. '#' cicla el modo (Abc/abc/ABC) y MANTENIDO -> 123.
 *  El char "a medio elegir" ya esta insertado en el campo: al re-tap se saca y se
 *  pone el siguiente (Backspace + InsertChar), asi el usuario ve el candidato.
 * ==============================================================================
 */

#include "w3dt9.h"
#include <e32std.h>
#include <e32keys.h>
#include <string.h>
#include "TextField.h"     // TextField, g_textFieldActivo (USERINCLUDE libs\WhiskUI\widgets)

extern bool NumEditActivo();        // WhiskUI/PropFloat.cpp: hay una edicion NUMERICA (un PropFloat)?

// modo: 0=Abc (1a mayuscula) / 1=abc / 2=ABC / 3=123 (numerico)
static int   gModo = 0;
static TInt  gKey  = -1;    // tecla en multi-tap ('0'..'9'), -1 = ninguna pendiente
static int   gTap  = 0;     // indice dentro del ciclo de gKey
static TUint gTick = 0;     // NTickCount del ultimo tap (timeout)
static TUint gHashDown = 0; // NTickCount del down de '#'

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

TBool W3dT9Activo() { return g_textFieldActivo != 0 ? ETrue : EFalse; }

TBool W3dT9HayPendiente() { return (gKey >= 0 && g_textFieldActivo) ? ETrue : EFalse; }

void W3dT9Reset() { gKey = -1; gTap = 0; }

// caso de una letra segun el modo (+ inicio de palabra para "Abc")
static char ConCaso(char c) {
    if (c < 'a' || c > 'z') return c;          // no es letra
    if (gModo == 1) return c;                  // abc
    if (gModo == 2) return (char)(c - 32);     // ABC
    // Abc: mayuscula si es la 1a letra del campo o va despues de un espacio
    TextField* f = g_textFieldActivo;
    bool mayus = (f->caret <= 0);
    if (!mayus && f->caret - 1 < (int)f->text.size() && f->text[f->caret - 1] == ' ') mayus = true;
    return mayus ? (char)(c - 32) : c;
}

void W3dT9Tick() {
    if (gKey < 0) return;
    if (User::NTickCount() - gTick >= kTimeout) W3dT9Reset();  // confirma: el char ya quedo insertado
}

const char* W3dT9ModoTexto() {
    if (!g_textFieldActivo) return "";
    if (gModo == 3 || NumEditActivo()) return "123";
    if (gModo == 1) return "abc";
    if (gModo == 2) return "ABC";
    return "Abc";
}

TBool W3dT9Tecla(TInt aScan, TBool aDown) {
    if (!g_textFieldActivo) return EFalse;

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
    if (!aDown) return ETrue;  // el resto solo en el key-down

    // MODO NUMERICO (o campo PropFloat): los digitos van directo, sin multi-tap
    if (gModo == 3 || NumEditActivo()) {
        if (aScan >= '0' && aScan <= '9') { W3dT9Reset(); g_textFieldActivo->InsertChar((int)aScan); return ETrue; }
        return EFalse;
    }

    // MODO TEXTO: multi-tap
    const char* ciclo = Ciclo(aScan);
    if (!ciclo) return EFalse;
    int len = (int)strlen(ciclo);
    bool retap = (gKey == aScan && (User::NTickCount() - gTick) < kTimeout);
    if (retap) {
        g_textFieldActivo->Backspace();       // sacar el candidato anterior de ESTA tecla
        gTap = (gTap + 1) % len;
    } else {
        gTap = 0;                             // tecla nueva: la anterior queda confirmada (ya insertada)
    }
    g_textFieldActivo->InsertChar((unsigned char)ConCaso(ciclo[gTap]));
    gKey = aScan;
    gTick = User::NTickCount();
    return ETrue;
}
