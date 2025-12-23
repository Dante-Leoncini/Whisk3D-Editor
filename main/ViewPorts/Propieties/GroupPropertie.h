#include <vector>
#include <string>

#include "UI/card.h"
#include "variables.h"
#include "PropertieBase.h"
#include "UI/colores.h"

class GroupPropertie {
    public:
        std::string name = "Propertie";
        Card* card;
        Card* propertiBox;
        bool open = true;
        int width = 300;
        int height = 300;
        int maxPixelsTitle = 1920;
        std::vector<PropertieBase*> properties;

        //seleccion
        int selectIndex = -2;
        bool NextSelect();

        void Render();
        void RenderPropertiBox();
        void RenderPropertiValue();
        void RenderPropertiLabel();
        void Resize(int Width, int Height);

        GroupPropertie(const std::string& Name);
        ~GroupPropertie();
};

extern std::vector<GroupPropertie*> GroupProperties;