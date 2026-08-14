#ifndef NOTIFICACIONES_H
#define NOTIFICACIONES_H

/**
 * @file Notificaciones.h
 * @brief Notificaciones (toasts): tarjetas de exito/error/hint. Extraido de LayoutInput.
 *
 * Notificaciones (toasts) del editor: tarjetas abajo a la izquierda con exito (verde) / error (rojo) / hint (azul).
 * Extraido de LayoutInput. Todo el estado es file-local en Notificaciones.cpp; esto es la interfaz.
 */
#include <string>

void Notificar(const std::string& msg, bool error);   // agrega una (y la loguea)
void NotificarHint(const std::string& msg);            // cartel-tutorial azul, persistente
void NotificarHintClear();                             // cierra los hints
void NotificacionesTick(float dt);                     // 1x por frame: expira las de exito
void NotificacionesRender(int screenW, int screenH);   // dibuja encima de todo
bool NotificacionesClick(int mx, int my);              // 'x' de error: cierra. true si consumio
void NotificacionesMotion(int mx, int my);             // hover de la 'x' (gris->blanca)

#endif
