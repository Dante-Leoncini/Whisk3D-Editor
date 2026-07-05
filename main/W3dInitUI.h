#pragma once
#include <string>

// ============================================================================
//  Init de la UI COMPARTIDO (los 4 OS).
//
//  Carga las texturas de la interfaz (atlas font/iconos, origen, cursor 3d,
//  linea de relacion, lampara) al vector global 'Textures' y arma la fuente,
//  los iconos, el 9-patch del borde, las tarjetas y la scrollbar a partir del
//  atlas. Es lo que antes hacia el shell de cada plataforma por separado.
//
//  'skinDir' = carpeta del skin CON el separador final ya incluido (cada
//  plataforma pone el suyo: PC res/Skins/<skin>/ con '/', Symbian su carpeta
//  privada con '\'). Los archivos se cargan como skinDir + nombre.
//
//  El cursor del mouse NO se carga aca: PC usa el cursor del sistema y Symbian
//  arma un point-sprite propio (lo unico distinto entre plataformas).
// ============================================================================
void W3dInitUI(const std::string& skinDir);

// ============================================================================
//  Estado de graficos inicial COMUN a los 4 OS (via las abstracciones del
//  motor, w3dEngine). Es el baseline del pipeline fijo que TODOS los sistemas
//  arman al arrancar; el render por-frame ajusta depth/textura/luz segun haga
//  falta. Antes estaba disperso (PC entre main.cpp y el constructor; Symbian
//  todo en AppInit). NO toca el color de fondo (depende de la paleta del skin).
// ============================================================================
void W3dInitGraphics();
