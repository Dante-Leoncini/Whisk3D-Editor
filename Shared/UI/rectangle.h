class Rectangle {
	public:
    	Object2D& parent;
		GLshort width = 10;
		GLshort height = 10;
		GLubyte opacity = 255;
		GLubyte color[3] = { 255, 255, 255 };

		GLshort vertices[8] = { 0,0, 10,0, 0,10, 10,10 };

    	// Constructor completo
		Rectangle(Object2D& p): parent(p){}

		// Calcular los vértices una sola vez
		void SetSize(GLshort x, GLshort y, GLshort w, GLshort h) {
			width = w;
			height = h;
			vertices[0] = (GLshort)x;
			vertices[1] = (GLshort)y;

			vertices[2] = (GLshort)(x + w);
			vertices[3] = (GLshort)y;

			vertices[4] = (GLshort)x;
			vertices[5] = (GLshort)(y + h);

			vertices[6] = (GLshort)(x + w);
			vertices[7] = (GLshort)(y + h);

			// Debug: imprimir todos los vertices
			/*std::cout << "Vertices: ";
			for (int i = 0; i < 8; i += 2) {
				std::cout << "(" << vertices[i] << "," << vertices[i+1] << ") ";
			}
			std::cout << std::endl;*/

		    /*std::cout << "Tamaño: " << width << "x" << height << std::endl;
		    std::cout << "Pos x: " << x << " y: " << y << std::endl;*/
		}

		void SetColor(GLubyte red, GLubyte green, GLubyte blue){
			color[0] = red;
			color[1] = green;
			color[2] = blue;
		}

		void Render() const {
			glColor4ub(color[0], color[1], color[2], opacity);
    		glVertexPointer(2, GL_SHORT, 0, vertices);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
};

// Crear un Text vacío
/*int AddRectangle(int parent){
	Rectangle rec;
	rec.parent = parent;

	// Retorna el índice
	return static_cast<int>(Rectangles.size()) - 1;	
}*/

Rectangle* AddRectangle(Object2D& parent) {
    return new Rectangle(parent);
}