enum { trackto, copyrotation, copylocation };

class Constraint { 
	public:
		size_t type = 0;
		Object* Id = nullptr;
		Object* Target = nullptr;
		bool opcion = false;
};

std::vector<Constraint> Constraints;