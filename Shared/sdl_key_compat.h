// Si estás en SDL2 y no existen las versiones mayúsculas, definilas en base a las minúsculas.

#ifndef SDLK_A
#define SDLK_A SDLK_a
#endif
#ifndef SDLK_B
#define SDLK_B SDLK_b
#endif
#ifndef SDLK_C
#define SDLK_C SDLK_c
#endif
#ifndef SDLK_D
#define SDLK_D SDLK_d
#endif
#ifndef SDLK_E
#define SDLK_E SDLK_e
#endif
#ifndef SDLK_F
#define SDLK_F SDLK_f
#endif
#ifndef SDLK_G
#define SDLK_G SDLK_g
#endif
#ifndef SDLK_H
#define SDLK_H SDLK_h
#endif
#ifndef SDLK_I
#define SDLK_I SDLK_i
#endif
#ifndef SDLK_J
#define SDLK_J SDLK_j
#endif
#ifndef SDLK_K
#define SDLK_K SDLK_k
#endif
#ifndef SDLK_L
#define SDLK_L SDLK_l
#endif
#ifndef SDLK_M
#define SDLK_M SDLK_m
#endif
#ifndef SDLK_N
#define SDLK_N SDLK_n
#endif
#ifndef SDLK_O
#define SDLK_O SDLK_o
#endif
#ifndef SDLK_P
#define SDLK_P SDLK_p
#endif
#ifndef SDLK_Q
#define SDLK_Q SDLK_q
#endif
#ifndef SDLK_R
#define SDLK_R SDLK_r
#endif
#ifndef SDLK_S
#define SDLK_S SDLK_s
#endif
#ifndef SDLK_T
#define SDLK_T SDLK_t
#endif
#ifndef SDLK_U
#define SDLK_U SDLK_u
#endif
#ifndef SDLK_V
#define SDLK_V SDLK_v
#endif
#ifndef SDLK_W
#define SDLK_W SDLK_w
#endif
#ifndef SDLK_X
#define SDLK_X SDLK_x
#endif
#ifndef SDLK_Y
#define SDLK_Y SDLK_y
#endif
#ifndef SDLK_Z
#define SDLK_Z SDLK_z
#endif

// ===============================
// Eventos Mouse SDL3 → SDL2
// ===============================

#ifndef SDL_EVENT_MOUSE_MOTION
#define SDL_EVENT_MOUSE_MOTION SDL_MOUSEMOTION
#endif

#ifndef SDL_EVENT_MOUSE_WHEEL
#define SDL_EVENT_MOUSE_WHEEL SDL_MOUSEWHEEL
#endif

#ifndef SDL_EVENT_MOUSE_BUTTON_DOWN
#define SDL_EVENT_MOUSE_BUTTON_DOWN SDL_MOUSEBUTTONDOWN
#endif

#ifndef SDL_EVENT_MOUSE_BUTTON_UP
#define SDL_EVENT_MOUSE_BUTTON_UP SDL_MOUSEBUTTONUP
#endif

// =====================================================
//  SDL3 → SDL2  — EVENTOS DE TECLAS
// =====================================================
#ifndef SDL_EVENT_KEY_DOWN
#define SDL_EVENT_KEY_DOWN SDL_KEYDOWN
#endif
#ifndef SDL_EVENT_KEY_UP
#define SDL_EVENT_KEY_UP SDL_KEYUP
#endif

// =====================================================
//  SDL3 → SDL2  — EVENTOS MOUSE
// =====================================================
#ifndef SDL_EVENT_MOUSE_MOTION
#define SDL_EVENT_MOUSE_MOTION SDL_MOUSEMOTION
#endif
#ifndef SDL_EVENT_MOUSE_WHEEL
#define SDL_EVENT_MOUSE_WHEEL SDL_MOUSEWHEEL
#endif
#ifndef SDL_EVENT_MOUSE_BUTTON_DOWN
#define SDL_EVENT_MOUSE_BUTTON_DOWN SDL_MOUSEBUTTONDOWN
#endif
#ifndef SDL_EVENT_MOUSE_BUTTON_UP
#define SDL_EVENT_MOUSE_BUTTON_UP SDL_MOUSEBUTTONUP
#endif

// =====================================================
//  SDL3 → SDL2  — EVENTOS DE VENTANA
// =====================================================
#ifndef SDL_EVENT_QUIT
#define SDL_EVENT_QUIT SDL_QUIT
#endif

#ifndef SDL_EVENT_WINDOW_RESIZED
#define SDL_EVENT_WINDOW_RESIZED SDL_WINDOWEVENT_RESIZED
#endif

#ifndef SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
#define SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED SDL_WINDOWEVENT_SIZE_CHANGED
#endif

// =====================================================
//  SDL3 → SDL2 — FLAGS DE VENTANA
// =====================================================
#ifndef SDL_WINDOW_HIGH_PIXEL_DENSITY
#define SDL_WINDOW_HIGH_PIXEL_DENSITY 0 // SDL2 no lo tiene → ignorar
#endif

#ifndef SDL_WINDOW_OPENGL
#define SDL_WINDOW_OPENGL SDL_WINDOW_OPENGL
#endif

// =====================================================
//  SDL3 → SDL2 — GL Compatibility
// =====================================================
#ifndef SDL_GL_DestroyContext
#define SDL_GL_DestroyContext SDL_GL_DeleteContext
#endif