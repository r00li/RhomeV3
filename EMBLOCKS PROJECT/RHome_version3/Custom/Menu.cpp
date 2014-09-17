/*
**
**                           Menu.cpp
**
**
**********************************************************************/
/*
   Author:                 Andrej Rolih
                           www.r00li.com
   Version:                0.1
   License:                GNU GPL v3
                           See attached LICENSE file for details
                           External library files do not include such header and
                           are released under GPL v3 or their specific license.
                           Check those files for more details.

**********************************************************************/

#include "Menu.h"
#include "essentials.h"


int Menu::turns;
int Menu::turnMultiplier;
bool Menu::enterPressed;
int Menu::positionSelected;
int Menu::realTurns;
int Menu::actionIndex;
//bool Menu::skipredraw;
bool Menu::resignInputControl;
bool Menu::onInfoScreen;
unsigned int Menu::lastInfoRefresh;
bool Menu::screenOff;
unsigned int Menu::lastInteractionTime;
std::function<void(void)> Menu::resignedController;

GraphicObject::GraphicObject()
{
    x = 0;
    y = 0;
    hidden = true;
}

void GraphicObject::draw()
{
}

void GraphicObject::setPosition(uint16_t x1, uint16_t y1)
{
    x = x1;
    y = y1;
}


Label::Label()
{
    bgColor = ILI9341_COLOR_BLACK;
    color = ILI9341_COLOR_WHITE;
    //text = "";
    width = 0;
    font = &TM_Font_11x18;
    hidden = true;
}

Label::Label(uint16_t x1, uint16_t y1, const char* text1, uint16_t color1, uint16_t bgColor1)
{
    hidden = true;

    font = &TM_Font_11x18;
    bgColor = bgColor1;
    color = color1;

    setPosition(x1, y1);
    setText(text1);

    needsUpdate = true;
    hidden = false;

    draw();
}

void Label::setHidden(bool hid)
{
    clearCurrent();
    needsUpdate = true;

    hidden = hid;
    draw();
}

const char* Label::getText()
{
    return text;
}

void Label::setText(const char* text1)
{
    clearCurrent();
    needsUpdate = true;

    strncpy(text, text1, 19);
    width = font->FontWidth*strlen(text);

    draw();
}

void Label::setPosition(uint16_t x1, uint16_t y1)
{
    clearCurrent();
    needsUpdate = true;

    x = x1;
    y = y1;

    draw();
}

void Label::setColor(uint16_t bgColor1, uint16_t color1)
{
    clearCurrent();
    needsUpdate = true;

    bgColor = bgColor;
    color = color;

    draw();
}

void Label::colorSwap()
{
    width = width - 1;
    clearCurrent();
    needsUpdate = true;
    width = width + 1;

    uint16_t tempColor = color;
    color = bgColor;
    bgColor = tempColor;

    draw();
}

int Label::getWidth()
{
    return width;
}

uint16_t Label::getX()
{
    return x;
}

uint16_t Label::getY()
{
    return y;
}

void Label::clearCurrent()
{
    if (hidden)
    {
        return;
    }

    TM_ILI9341_DrawFilledRectangle(x, y, x+width, y+font->FontHeight, bgColor);
}

void Label::setNeedsUpdate()
{
    needsUpdate = true;
}

void Label::draw()
{
    if (hidden || !needsUpdate)
    {
        return;
    }

    TM_ILI9341_Puts(x, y, text, font, color, bgColor);
}



MenuOption::MenuOption()
{
    selected = false;
    selectionColor = color;
    sel_x1 = 0;
    sel_x2 = 0;
    sel_y1 = 0;
    sel_y2 = 0;
}


MenuOption::MenuOption(uint16_t x1, uint16_t y1, const char* text1, uint16_t color1, uint16_t bgColor1) : Label(x1, y1, text1, color1, bgColor1)
{
    selected = false;
    selectionColor = color1;
    sel_x1 = x1 - 3;
    sel_y1 = y1 - 3;
    sel_x2 = width + 3 + x1;
    sel_y2 = font->FontHeight + y1 + 3;

    selectionDraw();
}

void MenuOption::selectionDraw()
{
    if (selected)
    {
        TM_ILI9341_DrawRectangle(sel_x1, sel_y1, sel_x2, sel_y2, selectionColor);
    }
    else
    {
        TM_ILI9341_DrawRectangle(sel_x1, sel_y1, sel_x2, sel_y2, bgColor);
    }
}

void MenuOption::setSelected(bool sel)
{
    selected = sel;
    selectionDraw();
}

void MenuOption::draw()
{
    Label::draw();
    selectionDraw();
}

void MenuOption::setOnClickListener(std::function<void(void)> clk)
{
    onClick = clk;
}

void MenuOption::doOnClick()
{
    if (onClick)
    {
        onClick();
    }
}


IndicatorMenuOption::IndicatorMenuOption(uint16_t x1, uint16_t y1, const char* text1, uint16_t color1, uint16_t bgColor1) : MenuOption(x1+30, y1, text1, color1, bgColor1)
{
    indicatorColor = ILI9341_COLOR_YELLOW;
    circle_r = 8;
    circle_x = x1 + circle_r/2 + 3 + 3;
    circle_y = y1 + circle_r/2 + 3 + 2;

    sel_x1 = x1 - 3;
    sel_y1 = y1 - 3;
    sel_x2 = width + 3 + x1 + 30;
    //sel_y2 = font->FontHeight + y1 + 3;

    indicatorDraw();
}

void IndicatorMenuOption::indicatorDraw()
{
    if (on)
    {
        TM_ILI9341_DrawFilledCircle(circle_x, circle_y, circle_r, indicatorColor);
    }
    else
    {
        TM_ILI9341_DrawFilledCircle(circle_x, circle_y, circle_r, bgColor);
        TM_ILI9341_DrawCircle(circle_x, circle_y, circle_r, color);
    }
}

void IndicatorMenuOption::draw()
{
    indicatorDraw();
    MenuOption::draw();
}

void IndicatorMenuOption::setOn(bool status)
{
    on = status;
    indicatorDraw();
}



ProgressBarMenuOption::ProgressBarMenuOption(uint16_t xn, uint16_t yn, const char* text1, uint16_t color1, uint16_t bgColor1) : MenuOption(xn, yn, text1, color1, bgColor1)
{
    progressColor = ILI9341_COLOR_BLUE2;
    min_progress = 0;
    max_progress = 100;
    progress = 0;
    bar_width = 139;

    sel_y2 = font->FontHeight + yn + 12 + 3;
    sel_x2 = xn + bar_width + 3 + 3;

    progressDraw();
}

long ProgressBarMenuOption::valMap(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void ProgressBarMenuOption::progressDraw()
{
    int valueToDraw = ProgressBarMenuOption::valMap(progress, min_progress, max_progress, x+3, x+bar_width);
    TM_ILI9341_DrawFilledRectangle(x+3, y+23, x+bar_width, y+23+5, ILI9341_COLOR_BLACK);

    TM_ILI9341_DrawRectangle(x, y+20, x+bar_width+3, y+20+10, color);
    TM_ILI9341_DrawFilledRectangle(x+3, y+23, valueToDraw, y+23+5, progressColor);
}

void ProgressBarMenuOption::draw()
{
    progressDraw();
    MenuOption::draw();
}

void ProgressBarMenuOption::setProgress(int prog)
{
    progress = prog;
    progressDraw();
}

void ProgressBarMenuOption::setMinMax(int minp, int maxp)
{
    min_progress = minp;
    max_progress = maxp;
}

void ProgressBarMenuOption::setProgressColor(uint16_t col)
{
    progressColor = col;
    progressDraw();
}




void Menu::initInputMethod()
{
    //
    // Init rotary encoder
    //
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    GPIO_InitTypeDef GPIO_InitStructure2;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    GPIO_InitStructure2.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure2.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure2.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStructure2);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM8);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_TIM8);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
    TIM_TimeBaseStructure.TIM_Period = 500;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_EncoderInterfaceConfig(TIM8,TIM_EncoderMode_TI1,TIM_ICPolarity_Falling,TIM_ICPolarity_Falling);
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseStructure);
    TIM_Cmd(TIM8, ENABLE);

    TIM_SetCounter(TIM8, 250);

    //
    // Init button
    //
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;

    GPIO_Init(GPIOA, &GPIO_InitStructure);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource1);

    EXTI_InitTypeDef EXTI_struktura;
    EXTI_struktura.EXTI_Line = EXTI_Line1;
    EXTI_struktura.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_struktura.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_struktura.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_struktura);

    //Enable NVIC
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    //
    // Init other buttons
    //
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    GPIO_InitTypeDef GPIO_InitStructure3;
    GPIO_InitStructure3.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure3.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStructure3.GPIO_Pin = GPIO_Pin_13|GPIO_Pin_12|GPIO_Pin_11|GPIO_Pin_10;

    GPIO_Init(GPIOE, &GPIO_InitStructure3);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource13);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource12);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource11);
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource10);

    EXTI_InitTypeDef EXTI_struktura2;
    EXTI_struktura2.EXTI_Line = EXTI_Line13|EXTI_Line12|EXTI_Line11|EXTI_Line10;
    EXTI_struktura2.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_struktura2.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_struktura2.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_struktura2);

    //Enable NVIC
    NVIC_InitTypeDef NVIC_InitStructure2;
    NVIC_InitStructure2.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStructure2.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure2.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure2.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure2);

    Menu::turnMultiplier = 1;
}

int Menu::getTurns()
{
    return Menu::turns;
}

void Menu::calculateTurns()
{
    if (TIM_GetCounter(TIM8) > 250)
    {
        Menu::lastInteractionTime = millis();
        Menu::realTurns += TIM_GetCounter(TIM8) - 250;
    }
    else if (TIM_GetCounter(TIM8) < 250)
    {
        Menu::lastInteractionTime = millis();
        Menu::realTurns -= 250 - TIM_GetCounter(TIM8);
    }
    TIM_SetCounter(TIM8, 250);
}

void Menu::calculateNiceTurns()
{
    bool repeat = false;
    do
    {
        if (Menu::realTurns > 1)
        {
            Menu::turns += Menu::turnMultiplier;
            Menu::realTurns -= 2;
        }
        else if (Menu::realTurns < -1)
        {
            Menu::turns -= Menu::turnMultiplier;
            Menu::realTurns += 2;
        }

        if (abs(Menu::realTurns) > 1)
            repeat = true;
        else
            repeat = false;
    }
    while (repeat);
}

void Menu::clearLeftMenu()
{
    TM_ILI9341_DrawFilledBrokenCircle(-102, 120, 250, ILI9341_COLOR_BLACK);
	TM_ILI9341_DrawFilledBrokenCircle(-120, 120, 150, ILI9341_COLOR_BLUE2);
}

void Menu::clearRightMenu()
{
    TM_ILI9341_DrawFilledRectangle(155, 25, 320, 240, ILI9341_COLOR_BLACK);
    TM_ILI9341_DrawFilledRectangle(135, 220, 135+11, 220+18, ILI9341_COLOR_BLACK);
}

void Menu::clearTitle()
{
    TM_ILI9341_DrawFilledRectangle(135, 0, 320, 23, ILI9341_COLOR_BLACK);
}

void Menu::clearMenu()
{
    TM_ILI9341_Fill(ILI9341_COLOR_BLACK);
    TM_ILI9341_DrawFilledBrokenCircle(-100, 120, 250, ILI9341_COLOR_WHITE);

	TM_ILI9341_DrawLine(0, 23, 320, 23, ILI9341_COLOR_WHITE);
	TM_ILI9341_DrawLine(0, 24, 320, 24, ILI9341_COLOR_WHITE);

	TM_ILI9341_DrawFilledBrokenCircle(-102, 120, 250, ILI9341_COLOR_BLACK);
	TM_ILI9341_DrawFilledBrokenCircle(-120, 120, 150, ILI9341_COLOR_BLUE2);
}

void Menu::clearNotification()
{
    TM_ILI9341_DrawFilledRectangle(157, 201, 317, 237, ILI9341_COLOR_BLACK);
    TM_ILI9341_DrawRectangle(155, 199, 318, 238, ILI9341_COLOR_BLUE2);
    TM_ILI9341_DrawRectangle(156, 200, 319, 239, ILI9341_COLOR_BLUE2);
}

void Menu::removeNotification()
{
    TM_ILI9341_DrawFilledRectangle(155, 199, 320, 240, ILI9341_COLOR_BLACK);
}

void Menu::clearPopup()
{
    TM_ILI9341_DrawFilledRectangle(0, 0, 320, 240, ILI9341_COLOR_BLACK);
    TM_ILI9341_DrawRectangle(8, 8, 312, 232, ILI9341_COLOR_BLUE2);
    TM_ILI9341_DrawRectangle(9, 9, 311, 231, ILI9341_COLOR_BLUE2);
}

void Menu::displayLoading()
{
    TM_ILI9341_DrawFilledRectangle(135, 0, 320, 23, ILI9341_COLOR_BLACK);
    TM_ILI9341_Puts(170, 0, "LOADING...", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
}
