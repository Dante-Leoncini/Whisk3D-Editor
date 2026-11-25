#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

/**
 * @file Notifications.h
 * @brief Notificaciones (toasts): tarjetas de exito/error/hint. Extraido de LayoutInput.
 *
 * Notificaciones (toasts) del editor: tarjetas abajo a la izquierda con exito (verde) / error (rojo) / hint (azul).
 * Extraido de LayoutInput. Todo el estado es file-local en Notifications.cpp; esto es la interfaz.
 */
#include <string>

void Notify(const std::string& msg, bool error);   // agrega una (y la loguea)
void NotifyHint(const std::string& msg);              // cartel-tutorial azul, persistente
void NotifyHintClear();                               // cierra los hints
void NotificationsTick(float dt);                     // 1x por frame: expira las de exito
void NotificationsRender(int screenW, int screenH);   // dibuja encima de todo
bool NotificationsClick(int mx, int my);              // 'x' de error: cierra. true si consumio
void NotificationsMotion(int mx, int my);             // hover de la 'x' (gray->blanca)
// las que se ven ahora (no los hints): alguna contiene 'texto'? Para las pruebas (el harness: 'notif')
bool NotificationVisible(const std::string& text);
void NotificationsClear();                          // las cierra todas (una prueba arranca de cero)
bool NotificationsVisible();                        // hay alguna en pantalla

#endif
