#ifndef W3D_AVISO_H
#define W3D_AVISO_H

#include <string>
#include <cstdio>
#include <cstdarg>

// Notificar vive en LayoutInput.cpp (toasts). Se declara aca para no arrastrar
// Notificaciones.h a los .cpp del modelo (ObjectMode/MeshEdit ya lo forward-declaraban).
void Notificar(const std::string& msg, bool error);

// ============================================================================
//  AVISOS (toasts) QUE LLEVAN NOMBRES - LA UNICA PUERTA.
//
//  Los nombres los tipea el usuario (TextField SIN limite de largo) o vienen del
//  .w3d (tampoco tiene cota), asi que NO hay ningun largo maximo garantizado.
//  Armar el aviso con sprintf() sobre un char[] fijo desbordaba la PILA: con un
//  .w3d de 4 objetos "BotonConfiguracionDeSonidoMenu" (30 chars, de lo mas
//  normal) abrir el archivo terminaba en "*** buffer overflow detected ***".
//  Habia 12 sitios asi (renames de objeto/grupo/hueso, reparent, reparacion de
//  carga). Ahora TODOS pasan por aca y el desborde es imposible por construccion:
//
//   - W3dNombreCorto(): recorta el nombre a un largo legible (y no parte un
//     caracter UTF-8 al medio), asi el aviso sigue siendo util y ademas corto.
//   - W3dAvisof(): formatea con vsnprintf ACOTADO. Aunque alguien se olvide de
//     recortar un nombre, el peor caso es un mensaje truncado, NUNCA un desborde.
//
//  Precedente: ConfirmarPopup.cpp ya habia arreglado su propio caso con snprintf.
//  Al agregar un aviso nuevo con nombres: usar W3dAvisof (o los dos atajos de
//  abajo), NUNCA sprintf sobre un buffer propio.
// ============================================================================

// recorta 'nombre' a 'max' bytes agregando "..." (por defecto 32: entra comodo en
// dos nombres + el texto fijo). Respeta UTF-8: no corta a la mitad de un caracter.
inline std::string W3dNombreCorto(const std::string& nombre, unsigned max = 32) {
    if (nombre.size() <= (size_t)max) return nombre;
    size_t corte = (size_t)max;
    // los bytes 10xxxxxx son CONTINUACION de un caracter multibyte: retroceder hasta el arranque
    while (corte > 0 && ((unsigned char)nombre[corte] & 0xC0) == 0x80) corte--;
    return nombre.substr(0, corte) + "...";
}

// formatea ACOTADO y notifica. Los %s de nombres deberian venir de W3dNombreCorto().
#if defined(__GNUC__)
__attribute__((format(printf, 2, 3)))
#endif
inline void W3dAvisof(bool error, const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt ? fmt : "", ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = '\0';   // algunas libc viejas no terminan al truncar
    Notificar(std::string(buf), error);
}

// ---- los DOS avisos de nombres que se repiten en todo el editor -------------
// "'Pedido' ya existe -> 'Pedido.001'" (informativo, no es un error)
inline void W3dAvisoYaExiste(const std::string& pedido, const std::string& quedo) {
    W3dAvisof(false, "'%s' ya existe -> '%s'",
              W3dNombreCorto(pedido).c_str(), W3dNombreCorto(quedo).c_str());
}
// "N hueso(s) 'Viejo' -> 'Nuevo' (binding por nombre)": el arrastre de la OTRA punta
// de un vinculo por nombre. 'que' = "hueso(s)" / "hueso(s) 2D" / "vertex group(s)" / "UV group(s)".
inline void W3dAvisoArrastre(int n, const char* que, const std::string& viejo, const std::string& nuevo) {
    W3dAvisof(false, "%d %s '%s' -> '%s' (binding por nombre)",
              n, que ? que : "?", W3dNombreCorto(viejo).c_str(), W3dNombreCorto(nuevo).c_str());
}

#endif // W3D_AVISO_H
