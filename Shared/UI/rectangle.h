class Rectangle : public Object2D {
	public:
		GLshort width = 10;
		GLshort height = 10;

		GLshort vertices[8] = { 0,0, 10,0, 0,10, 10,10 };

    	// Constructor completo
        Rectangle(Object2D* parent = nullptr): Object2D(parent){}

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
		}

		void SetColor(GLubyte red, GLubyte green, GLubyte blue){
			color[0] = red;
			color[1] = green;
			color[2] = blue;
		}

		void RenderObject(bool usarColorPropio = true) override {
			if (usarColorPropio) glColor4ub(color[0], color[1], color[2], opacity);
    		glVertexPointer(2, GL_SHORT, 0, vertices);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
		}
};