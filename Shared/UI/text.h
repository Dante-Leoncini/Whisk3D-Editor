#include "./font.h"

class Text {
	public:
		//Object2D* parent = nullptr;
		//Object2D& ObjParent = *parent;
    	Object2D& parent;
		int interpolacion = closest;
		//colores RGB
		GLubyte color[3] = { 255, 255, 255 };
		std::string value;
    	std::vector<Sprite> letters;

		Text(Object2D& p)
			: parent(p), value("Text") {}

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
					parent.x + offsetX, 
					parent.y, 
					5 * parent.scaleX, 
					11 * parent.scaleY
				);

				letters.push_back(ls);

				offsetX += 6 * parent.scaleX; // ancho letra escalado
			}
		}

		// Setear el texto y recalcular la cache
		void SetValue(const std::string& newValue) {
			value = newValue;
			UpdateCache();
		}

		void SetScaleX(int value){
			parent.scaleX = value;
			UpdateCache();
		}

		void SetScaleY(int value){
			parent.scaleY = value;
			UpdateCache();
		}

		void SetX(int input){
			int offsetX = input;

			for (auto& ls : letters) {
				ls.SetX(
					parent.x + offsetX,
					5 * parent.scaleX
				);

				offsetX += 6 * parent.scaleX; // ancho letra escalado
			}
		}

		void SetY(int input) {
			int offsetY = input;

			for (auto& ls : letters) {
				ls.SetY(
					parent.y + offsetY,
					11 * parent.scaleY // altura de letra escalada (ajustá este valor según tu fuente)
				);
			}
		}

		void Render() const {
			glColor4ub(color[0], color[1], color[2], parent.opacity);
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

Text* AddText(Object2D& parent) {
    return new Text(parent);
}