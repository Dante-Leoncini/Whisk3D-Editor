#ifndef IMPORTSVERTEXANIMATION_H
#define IMPORTSVERTEXANIMATION_H

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vector>
#include <GL/gl.h>
#include <string>
#include <fstream>
#include <unordered_map>

#include "Animation.h"
#include "Objects/Mesh.h"

class ShapeKeyAnimation { 
	public:
		Object* obj = nullptr;
		std::vector<Animation> Animations;
		int ChangeAnimation;
		int LastAnimation;
		int NextAnimation;
		int LastFrame;
		int NextFrame;
		int Mix;
		bool Normals;
		bool Faces;
		bool Interpolacion;
};

// Importa animaciones de vértices desde un archivo .txt
bool ImportVertexAnimation(const std::string& filepath);

#endif