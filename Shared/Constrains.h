enum { trackto, copyrotation, copylocation };

class Constraint { 
	public:
		int type = 0;
		int Id = -1;
		int Target = -1;
		bool opcion = false;
};

std::vector<Constraint> Constraints;