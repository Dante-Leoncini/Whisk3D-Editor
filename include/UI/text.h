#include "./font.h"

class Text : public Object2D {
	public:
		int interpolacion = closest;
		std::string value;
    	std::vector<Sprite> letters;

        Text(const std::string& nombre = "Text", Object2D* parent = nullptr): Object2D(parent) {
            SetValue(nombre);
        }

		void SetColor(GLubyte red, GLubyte green, GLubyte blue){
			color[0] = red;
			color[1] = green;
			color[2] = blue;
		}

		// Calcular posiciones y UVs solo cuando cambia el texto
		void UpdateCache() {
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
					x + offsetX, 
					y, 
					5 * scaleX * GlobalScale, 
					11 * scaleY * GlobalScale
				);

				letters.push_back(ls);

				offsetX += 6 * scaleX * GlobalScale; // ancho letra escalado
			}
		}

		// Setear el texto y recalcular la cache
		void SetValue(const std::string& newValue) {
			value = newValue;
			UpdateCache();
		}

		void SetScaleX(int value){
			scaleX = value;
			UpdateCache();
		}

		void SetScaleY(int value){
			scaleY = value;
			UpdateCache();
		}

		void SetX(int input){
			int offsetX = input;

			for (auto& ls : letters) {
				ls.SetX(
					x + offsetX,
					5 * scaleX
				);

				offsetX += 6 * scaleX; // ancho letra escalado
			}
		}

		void SetY(int input) {
			int offsetY = input;

			for (auto& ls : letters) {
				ls.SetY(
					y + offsetY,
					11 * scaleY // altura de letra escalada (ajustá este valor según tu fuente)
				);
			}
		}

		void RenderObject(bool UsarColor = true) override {
			if (UsarColor){ 
				glColor4ub(color[0], color[1], color[2], opacity);				
			}
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