#include <vector>
#include <string>

#include "UI/card.h"
#include "variables.h"
#include "PropertieBase.h"
#include "PropList.h"

#include "UI/colores.h"

class GroupPropertie {
    public:
        std::string name = "Propertie";
        Card* card;
        Card* propertiBox;
        bool open = true;
        bool visible = true;
        bool editando = false;

        int width = 300;
        int height = 300;
        int maxPixelsTitle = 1920;
        std::vector<PropertieBase*> properties;

        //seleccion
        int selectIndex = -2;
        bool NextSelect();
        bool PrevSelect();
        void selectLastIndexProperty();
        bool EnterPropertieSelect();
        bool Cancel();

        void button_up();
        void button_down();
        void button_left();
        void button_right();

        void Render();
        void RenderPropertiBox();
        void RenderPropertiValue();
        void RenderPropertiLabel();
        void Resize(int Width, int Height);

        GroupPropertie(const std::string& Name);
        ~GroupPropertie();
};

extern std::vector<GroupPropertie*> GroupProperties;