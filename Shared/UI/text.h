#include "./font.h"

class Text {
	public:
		int parent = -1;
		int interpolacion = closest;
		//colores RGB
		GLfloat r = 1.0f;
		GLfloat g = 1.0f;
		GLfloat b = 1.0f;
		std::string value;
    	std::vector<Sprite> letters;

		void SetColor(GLfloat red, GLfloat green, GLfloat blue){
			r = red;
			g = green;
			b = blue;
		}

		// Calcular posiciones y UVs solo cuando cambia el texto
		void UpdateCache() {
        	if (parent < 0 || parent >= (int)Objects2D.size()) return;
			Object2D& ObjParent = Objects2D[parent];
			letters.clear();

			int offsetX = 0;

			size_t i = 0;
			while (i < value.size()) {
				std::string letra;
				unsigned char c = value[i];

				if (c < 128) { 
					letra = std::string(1, c); 
					i++; 
				} else { 
					letra = value.substr(i, 2); 
					i += 2; 
				}

				Sprite ls;

				// Setear UV directamente desde el atlas
				const GLfloat* uv = font.getUV(letra);
				for (int j = 0; j < 8; ++j) ls.uvs[j] = uv[j];

				// Posición + escala viene del padre
				ls.SetVertices(
					ObjParent.x + offsetX, 
					ObjParent.y, 
					5 * ObjParent.scaleX, 
					11 * ObjParent.scaleY
				);

				letters.push_back(ls);

				offsetX += 6 * ObjParent.scaleX; // ancho letra escalado
			}
		}

		// Setear el texto y recalcular la cache
		void SetValue(const std::string& newValue) {
			value = newValue;
			UpdateCache();
		}

		void SetScaleX(int value){
        	if (parent < 0 || parent >= (int)Objects2D.size()) return;
			Object2D& ObjParent = Objects2D[parent];
			ObjParent.scaleX = value;
			UpdateCache();
		}

		void SetScaleY(int value){
        	if (parent < 0 || parent >= (int)Objects2D.size()) return;
			Object2D& ObjParent = Objects2D[parent];
			ObjParent.scaleY = value;
			UpdateCache();
		}

		void SetX(int input){
			if (parent < 0 || parent >= (int)Objects2D.size()) return;
			Object2D& ObjParent = Objects2D[parent];

			int offsetX = input;

			for (auto& ls : letters) {
				ls.SetX(
					ObjParent.x + offsetX,
					5 * ObjParent.scaleX
				);

				offsetX += 6 * ObjParent.scaleX; // ancho letra escalado
			}
		}

		void SetY(int input) {
			if (parent < 0 || parent >= (int)Objects2D.size()) return;
			Object2D& ObjParent = Objects2D[parent];

			int offsetY = input;

			for (auto& ls : letters) {
				ls.SetY(
					ObjParent.y + offsetY,
					11 * ObjParent.scaleY // altura de letra escalada (ajustá este valor según tu fuente)
				);
			}
		}

		void Render() const {
			glBindTexture(GL_TEXTURE_2D, Textures[0].iID);

			//esto se hace despues del bind de la textura. sino no le afecta
			if (interpolacion == closest){
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			}
			else {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}

			for (const auto& ls : letters) {
				ls.Render();
			}
		}
};

std::vector<Text> Texts;

// Crear un Text vacío
int AddText(int parent, std::string value = "Text"){
	Text txt;
	txt.parent = parent;
	txt.value = value;

	// Guardar en el vector de textos
	Texts.push_back(txt);

	// Retorna el índice
	return static_cast<int>(Texts.size()) - 1;	
}