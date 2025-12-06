#ifndef TEXTURES_H
#define TEXTURES_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <string>
#include <vector>
#include <iostream>

#include <GL/gl.h>
#ifdef __ANDROID__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
    #include <GL/glu.h>
#endif

class Texture {
    public:
        GLuint iID = 0;
        std::string path;

        Texture(const std::string& p = "") : iID(0), path(p) {} // ahora correcto
};

extern std::vector<Texture*> Textures;

// Funciones de carga de texturas
#if SDL_MAJOR_VERSION == 2
    #ifdef __ANDROID__
    bool LoadTextureFromSurface(SDL_Surface* surf, GLuint &textureID);
    #endif
    bool LoadTexture(const char* filename, GLuint &textureID);
#elif SDL_MAJOR_VERSION == 3
    bool LoadTexture(const char* path, GLuint &textureID);
#endif

#endif