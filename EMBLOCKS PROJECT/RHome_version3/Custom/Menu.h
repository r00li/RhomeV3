/*
**
**                           Menu.h
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

#ifndef MENU_H
#define MENU_H

#include "tm_stm32f4_ili9341.h"
#include "tm_stm32f4_fonts.h"

#include "stm32f4xx_conf.h"
#include "stm32f4xx_exti.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include "misc.h"
#include "stm32f4xx_syscfg.h"
#include "stm32f4xx_tim.h"


#include <string.h>
#include <vector>
#include <functional>
#include <stdlib.h>

class MenuMessage
{
    public:
    int id;
};

class GraphicObject
{
    protected:
    uint16_t x;
    uint16_t y;
    bool hidden;
    bool needsUpdate;

    public:
    GraphicObject();
    void draw();
    void setPosition(uint16_t x1, uint16_t y1);
};

class Label : public GraphicObject
{
    protected:
    char text[20];
    int width;

    public:
    TM_FontDef_t *font;
    Label();
    Label(uint16_t x1, uint16_t y1, const char* text1, uint16_t color1, uint16_t bgColor1);
    uint16_t bgColor;
    uint16_t color;

    void setText(const char* text1);
    const char* getText();
    void setPosition(uint16_t x1, uint16_t y1);
    void setColor(uint16_t bgColor1, uint16_t color1);
    void colorSwap();
    int getWidth();
    uint16_t getX();
    uint16_t getY();
    void setNeedsUpdate();
    void draw();
    void clearCurrent();
    void setHidden(bool hid);
};

class MenuOption : public Label
{
    protected:
    void selectionDraw();
    std::function<void(void)> onClick;

    public:
    int selectionId;
    bool selected;
    uint16_t selectionColor;
    uint16_t sel_x1;
    uint16_t sel_y1;
    uint16_t sel_x2;
    uint16_t sel_y2;
    MenuOption(uint16_t x1, uint16_t y1, const char* text1, uint16_t color1, uint16_t bgColor1);
    MenuOption();
    void draw();
    void setSelected(bool sel);
    void setOnClickListener(std::function<void(void)> clk);
    void doOnClick();

    std::vector<MenuOption> submenu;
};

class ProgressBarMenuOption : public MenuOption
{
    protected:
    int min_progress;
    int max_progress;
    int progress;
    int bar_width;
    uint16_t progressColor;

    public:
    ProgressBarMenuOption(uint16_t xn, uint16_t yn, const char* text1, uint16_t color1, uint16_t bgColor1);
    void setMinMax(int minp, int maxp);
    void setProgress(int prog);
    void progressDraw();
    void draw();
    static long valMap(long x, long in_min, long in_max, long out_min, long out_max);
    void setProgressColor(uint16_t col);
};

class IndicatorMenuOption : public MenuOption
{
    protected:
    bool on;
    uint16_t circle_x;
    uint16_t circle_y;
    uint16_t circle_r;
    uint16_t indicatorColor;

    public:
    IndicatorMenuOption(uint16_t x1, uint16_t y1, const char* text1, uint16_t color1, uint16_t bgColor1);
    void setOn(bool status);
    void indicatorDraw();
    void draw();
};


class Menu
{
    protected:

    public:
    static int positionSelected;
    static int actionIndex;
    //static bool skipredraw;
    static bool onInfoScreen;
    static unsigned int lastInfoRefresh;

    static int turns;
    static int realTurns;
    static int turnMultiplier;
    static bool enterPressed;

    static bool resignInputControl;
    static std::function<void(void)> resignedController;

    static bool screenOff;
    static unsigned int lastInteractionTime;

    static void initInputMethod();
    static int getTurns();
    static void calculateTurns();
    static void calculateNiceTurns();

    static void clearLeftMenu();
    static void clearRightMenu();
    static void clearTitle();
    static void clearMenu();
    static void displayLoading();
    static void clearPopup();
    static void clearNotification();
    static void removeNotification();
};

#endif /* MENU_H */
