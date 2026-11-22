// ============================================================================
//  W3dNoNearFar.h - WINDOWS: <windows.h> define 'near' y 'far' como macros VACIAS (restos de los punteros de 16 bits).
//  El codigo usa 'near' / 'far' como nombres (el plano cercano de una camara, una sonda...): despues de windows.h
//  'v.near' queda 'v.' y no compila. Este header las deshace; va DESPUES de los #include del archivo que lo necesita
//  (y de nuevo si algo vuelve a incluir windows.h por primera vez mas abajo). En el resto de las plataformas no hace nada.
// ============================================================================
#ifdef _WIN32
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
// (y rpcndr.h, que llega con windows.h, define 'small' como 'char': tambien se usa como nombre)
#ifdef small
#undef small
#endif
#endif
