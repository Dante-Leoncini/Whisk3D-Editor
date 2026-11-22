// ============================================================================
//  W3dRedPermisos — el PERMISO de la red de cada proyecto y el NOMBRE de este dispositivo (lo del editor; la red
//  misma es del Core: libs/Whisk3DCore/net/W3dNet.h).
//
//  Un proyecto USA LA RED si alguno de sus scripts hostea, busca o se une a una partida (redHostear / redBuscar /
//  redUnirse). Esos proyectos no salen a la red hasta que el usuario los deja: el editor pregunta UNA vez por archivo
//  (al abrirlo, o al darle Play si empezo a usarla despues) y recuerda la respuesta en network.txt, en la carpeta del
//  usuario (nunca en el proyecto: un archivo no se puede dar permiso a si mismo). La casilla de la tarjeta File
//  la cambia despues. Sin permiso, el juego corre igual: redDisponible() le dice que no hay red.
//
//  El nombre del dispositivo (Ajustes) es el que ven los demas al buscar partidas; por defecto, el del equipo.
//  En el arnes de pruebas y en un juego empaquetado (Symbian) no se pregunta: se permite.
// ============================================================================
#ifndef W3D_NET_PERMISSIONS_H
#define W3D_NET_PERMISSIONS_H

#include <string>

// el proyecto abierto: aplica lo decidido o pregunta (lo llama OpenW3D al terminar de abrir)
void NetOnOpenProject(const std::string& path);
// antes del Play: true si puede arrancar; false si hace falta preguntar (abre el cartel y arranca al responder)
bool NetCanPlay();
// la casilla de la tarjeta File: el permiso del proyecto abierto
bool NetPermissionProject();
void NetSetPermissionProject(bool yes);
// el nombre de este dispositivo (Ajustes)
std::string NetDevice();
void NetSetDevice(const std::string& name);
// (el arnes de pruebas: preguntar aunque sea el arnes, y olvidar lo decidido para un archivo)
extern bool g_netAskInTests;
void NetForget(const std::string& path);

#endif // W3D_NET_PERMISSIONS_H
