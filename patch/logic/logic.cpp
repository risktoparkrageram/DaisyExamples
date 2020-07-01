#include "daisysp.h"
#include "daisy_patch.h"
#include <string>

using namespace daisy;
using namespace daisysp;

DaisyPatch patch;

struct gate{
    int gateType;
    bool Process(bool a, bool b)
    {
        switch(gateType)
        {
            case 0:
                return a && b;  //AND
            case 1:
                return a || b;  //OR
            case 2:
                return (a && !b) || (!a && b);  //XOR
            case 3:
                return !(a && b);  //NAND
            case 4:
                return !(a || b);  //NOR
            case 5:
                return (a && b) || (!a && !b);  //XNOR
        }
        return false;
    }
};

bool inputs[4];
gate gates[2];
std::string gateNames[6];
bool isInverted[6];

//menu logic
int menuPos; //6 positions
bool inSubMenu; //only on gate types


bool CvToBool(float in) { return in > .8f; }
int mod(int x, int m) { return (x % m + m) % m; }

void ProcessControls();
void ProcessOled();
void ProcessOutputs();

void InitGateNames()
{
    gateNames[0] = " AND  ";
    gateNames[1] = " OR   ";
    gateNames[2] = " XOR  ";
    gateNames[3] = " NAND ";
    gateNames[4] = " NOR  ";
    gateNames[5] = " XNOR ";
}

int main(void)
{
    patch.Init(); // Initialize hardware (daisy seed, and patch)

    InitGateNames();

    //both init to AND
    gates[0].gateType = gates[1].gateType = 0; 
    
    //no inputs are inverted
    isInverted[0] = isInverted[1] = false;
    isInverted[2] = isInverted[3] = false;

    patch.StartAdc();
    while(1)
    {
        ProcessControls();
        ProcessOled();
        ProcessOutputs();
    }
}

void ProcessIncrement(int increment)
{
    //change gate type
    if (inSubMenu)
    {
        if (menuPos == 1){
            gates[0].gateType += increment;
            gates[0].gateType = mod(gates[0].gateType, 6);
        }

        else
        {
            gates[1].gateType += increment;
            gates[1].gateType = mod(gates[1].gateType, 6);
        }
    }

    //move through the menu
    else 
    {
        menuPos += increment;
        menuPos = mod(menuPos, 6);
    }
}

void ProcessRisingEdge()
{   
    switch (menuPos) 
    {
        //flip inversions
        case 0:
            isInverted[0] = ! isInverted[0];
            break;
        case 2:
            isInverted[1] = ! isInverted[1];
            break;
        case 3:
            isInverted[2] = ! isInverted[2];
            break;
        case 5:
            isInverted[3] = ! isInverted[3];
            break;
        
        //enter/exit submenu
        case 1:
        case 4:
            inSubMenu = ! inSubMenu;
    }
}

void ProcessEncoder()
{
    ProcessIncrement(patch.encoder.Increment());
    
    if (patch.encoder.RisingEdge())
    { 
        ProcessRisingEdge();
    }
}

void ProcessControls()
{
    patch.UpdateAnalogControls();
    patch.DebounceControls();

    //inputs
    for (int i = 0; i < 4; i++)
    {
        inputs[i] = CvToBool(patch.controls[i].Process());
        
        //invert the input if isInverted says so
        inputs[i] = isInverted[i] ? ! inputs[i] : inputs[i];
    }

    ProcessEncoder();
}

void ProcessOutputs()
{
    dsy_dac_write(DSY_DAC_CHN1, gates[0].Process(inputs[0], inputs[1]) * 4095);
    dsy_dac_write(DSY_DAC_CHN2, gates[1].Process(inputs[2], inputs[3]) * 4095);
}

void ProcessOled()
{
    patch.display.Fill(false);

    std::string str;
    char* cstr = &str[0];

    patch.display.SetCursor(0,0);
    str = "Logic";
    patch.display.WriteString(cstr, Font_7x10, true);

    patch.display.SetCursor(0,35);
    str = "1" + gateNames[gates[0].gateType] + "2";
    patch.display.WriteString(cstr, Font_6x8, true);

    patch.display.SetCursor(65,35);
    str = "3" + gateNames[gates[1].gateType] + "4";
    patch.display.WriteString(cstr, Font_6x8, true);

    patch.display.Update();
}