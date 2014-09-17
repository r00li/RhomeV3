/*
**
**                           Main.cpp
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

#include "stm32f4xx_conf.h"
#include "stm32f4xx_exti.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_usart.h"
#include "misc.h"
#include "stm32f4xx_syscfg.h"
#include "stm32f4xx_rtc.h"
#include "stm32f4xx_pwr.h"
#include "stm32f4xx_flash.h"


#include "tm_stm32f4_ili9341.h"
#include "tm_stm32f4_fonts.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include <stdio.h>
#include <string>
#include <sstream>
#include <stdlib.h>
#include <functional>

#include "Lighting.h"
#include "Menu.h"
#include "Blinds.h"
#include "RemoteTransmitter.h"
#include "RemoteReceiver.h"
#include "TempSensor.h"
#include "Remote.h"

#include "essentials.h"

//
// Redefinition of operators new and delete so that
// dynamic memory allocation can work with Free RTOS
//
void *operator new(size_t size)
{
    void *p;

    if(uxTaskGetNumberOfTasks())
        p=pvPortMalloc(size);
    else
        p=malloc(size);

    return p;
}

void operator delete(void *p)
{
    if(uxTaskGetNumberOfTasks())
        vPortFree( p );
    else
        free( p );

    p = NULL;
}

//
// Functions declarations for some of the functions
//
void Delay(__IO uint32_t nCount);
void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName );
void vApplicationMallocFailedHook(void);
void vTask1Function (void *pvParameters);
void vTask2Function (void *pvParameters);
void menuCheckerTask(void *pvParameters);
void displayInfoScreen();
void remoteEvent(uint32_t event_code);

//
// Settings for maximum number of automation units
// Also definitions for vectors where we store this data
//
#define kMAX_LIGHTS 5
#define kMAX_BLINDS 4
#define kMAX_BUTTONS 50
std::vector<Light *> lights;
std::vector<Blind *> blinds;
std::vector<RemoteButton *> remoteButtons;

//
// Menu variables (menu stack, currently drawn menu options, ...)
//
std::vector<MenuOption *> mainMenuOptions;
std::vector<std::vector<MenuOption *>> menuStack;
std::vector<MenuOption *> pressedMenuOptionsStack;

//
// Temperature sensing variables
//
InternalTempSensor tempSensor;
uint8_t tempAdjust = 0;

//
// Wifi connection 1 buffer
//
const int eth1_buff_size = 800;
volatile char eth1_buff[eth1_buff_size];
volatile int eth1_buff_indicator = 0;
volatile uint8_t eth1_busy = 0;

//
// Wifi connection 2 buffer
//
const int eth2_buff_size = 400;
volatile char eth2_buff[eth2_buff_size];
volatile int eth2_buff_indicator = 0;
volatile uint8_t eth2_busy = 0;

//
// Ethernet response buffer
//
char eth_send_buff[6000];
volatile unsigned int eth_send_pointer = 0;

//
// Some global buffers for use across the code
//
int globalIntBuffer[10];
char text_buffer[70];
char mini_text_buffer1[35];
char mini_text_buffer2[35];
char mini_text_buffer3[35];

//
// Alphabet definition (for on-screen keyboard)
//
const char alphabet[][2] = {"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z", " ","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "_", "-", ".", ",", "+", "(", ")"};
#define kALPHABET_SIZE 70

//
// HLK-RM04 Wifi encryption options and their pretty versions
//
const char wifiEncryptionTypes[][11] = {"None", "WEP open", "WEP", "WPA TKIP", "WPA AES", "WPA2 TKIP", "WPA2 AES", "WPA/2 TKIP", "WPA/2 AES"};
const char wifiEcnryptionCommands[][13] = {"none", "wep_open", "wep", "wpa_tkip", "wpa_aes", "wpa2_tkip", "wpa2_aes", "wpawpa2_tkip", "wpawpa2_aes"};
#define kWifiEncryptionTypes 9

//
// Default web server settings
//
char web_user[15] = "user";
char web_pass[15] = "user";
int webPort = 8080;

//
// Variables for blue status LED
//
int blueLedOnTime = 0;
bool blueLedOn = false;

//
// Delayed actions variables
// Automatic blind open/close, delay light-off
//
int delayedActionTime = 0;
bool delayLightOff = false;
bool delayBlindOpen = false;
bool delayBlindClose = false;

//
// Variables for setting the autoblind timer
// (autoblind is disabled by default)
//
uint8_t automaticBlindOpenHour = 0;
uint8_t automaticBlindOpenMinute = 0;
uint8_t automaticBlindCloseHour = 0;
uint8_t automaticBlindCloseMinute = 0;
uint8_t automaticBlindOpenPosition = 0;
uint8_t automaticBlindClosePosition = 0;
bool autoOpenBlindEnabled = false;
bool autoCloseBlindEnabled = false;

//
// Flash definitions
//
const uint8_t flash_data[] __attribute__ ((section(".eeprom"), used)) = {};
uint8_t flash_status;
uint8_t empty_check;


//
// Interrupt handlers
// Using extern "C" because interrupt handlers can't be inside of C++ code
//
extern "C"
{
    void USART1_IRQHandler(void)
    {
        //
        // Wifi connection 1 handler (server)
        //
        if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
        {
            if (eth1_buff_indicator >= eth1_buff_size || eth1_busy)
            {
                return;
            }

            eth1_buff[eth1_buff_indicator] = USART_ReceiveData(USART1);
            eth1_buff_indicator++;
        }
    }

    void USART2_IRQHandler(void)
    {
        //
        // Wifi connection 2 handler (client)
        //
        if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
        {
            if (eth2_buff_indicator >= eth2_buff_size || eth2_busy)
            {
                return;
            }

            eth2_buff[eth2_buff_indicator] = USART_ReceiveData(USART2);
            eth2_buff_indicator++;
        }
    }

    void EXTI1_IRQHandler(void)
    {
        //
        // Rotary encoder integrated button handler
        //
        if (EXTI_GetITStatus(EXTI_Line1))
        {
            Menu::enterPressed = true;
            Menu::lastInteractionTime = millis();
            delay(10);
        }

        EXTI_ClearITPendingBit(EXTI_Line1);
    }

    void EXTI9_5_IRQHandler(void)
    {
        //
        // RF-receiver handler
        //
        if (EXTI_GetITStatus(EXTI_Line6) != RESET)
        {
            taskENTER_CRITICAL();
            //taskDISABLE_INTERRUPTS();
            RemoteReceiver::interruptHandler();
            EXTI_ClearITPendingBit(EXTI_Line6);
            taskEXIT_CRITICAL();
            //taskENABLE_INTERRUPTS();
        }
    }

    void EXTI15_10_IRQHandler(void)
    {
        //
        // User buttons handler
        //
        if (EXTI_GetITStatus(EXTI_Line13) != RESET)
        {
            remoteEvent(1);
            EXTI_ClearITPendingBit(EXTI_Line13);
        }

        if (EXTI_GetITStatus(EXTI_Line12) != RESET)
        {
            remoteEvent(2);
            EXTI_ClearITPendingBit(EXTI_Line12);
        }

        if (EXTI_GetITStatus(EXTI_Line11) != RESET)
        {
            remoteEvent(3);
            EXTI_ClearITPendingBit(EXTI_Line11);
        }

        if (EXTI_GetITStatus(EXTI_Line10) != RESET)
        {
            remoteEvent(4);
            EXTI_ClearITPendingBit(EXTI_Line10);
        }
    }

    void RTC_Alarm_IRQHandler()
    {
        //
        // RTC alarm handler
        // Used for autoblind
        //
        if(RTC_GetITStatus(RTC_IT_ALRA) != RESET)
        {
            if (autoOpenBlindEnabled)
                delayBlindOpen = true;
            RTC_ClearITPendingBit(RTC_IT_ALRA);
            EXTI_ClearITPendingBit(EXTI_Line17);
        }

        if(RTC_GetITStatus(RTC_IT_ALRB) != RESET)
        {
            if (autoCloseBlindEnabled)
                delayBlindClose = true;
            RTC_ClearITPendingBit(RTC_IT_ALRB);
            EXTI_ClearITPendingBit(EXTI_Line17);
        }
    }
}


// ------------------------------------------------------------------------------------------------------------------------------------------------------
// FLASH FUNCTIONS
// ------------------------------------------------------------------------------------------------------------------------------------------------------

void loadFromFlash()
{
    //
    // Function loads settings from internal flash
    //

    if (flash_data[0])
    {
        empty_check = 1;
        return;
    }
    else
    {
        uint8_t* address = (uint8_t *) &flash_data[0];
        address += 4;

        //restore lights
        for (int i = 0 ; i < kMAX_LIGHTS; i++)
        {
            unsigned char *data1 = address;
            address += 4;

            uint8_t light_present = data1[0];
            uint8_t light_type = data1[1];
            uint8_t device = data1[3];

            char name[12];
            for (int j=0; j < 12; j++)
            {
                name[j] = *address;
                address++;
            }
            name[11] = '\0';

            uint32_t bt_code = *((uint32_t *)address);
            address += 4;

            uint32_t system_code = *((uint32_t *)address);
            address += 4;

            //additional unused bits
            address += 8;

            if (light_present == 0)
            {
                Light *lght = new Light;
                lght->setName(name);
                lght->btCode = bt_code;

                if (light_type == RF_KAKU)
                    lght->setTypeKaku(device, (unsigned short) system_code);

                lght->onOff(false);

                lights.push_back(lght);
            }
        }

        //restore blinds
        for (int i = 0 ; i < kMAX_BLINDS; i++)
        {
            unsigned char *data1 = address;
            address += 4;

            uint8_t blind_present = data1[0];
            uint8_t blind_channel = data1[1];
            uint8_t blind_type = data1[2];
            //uint8_t device = data1[3];

            uint32_t bt_code = *((uint32_t *)address);
            address += 4;

            uint32_t min_pos = *((uint32_t *)address);
            address += 4;

            uint32_t mid_pos = *((uint32_t *)address);
            address += 4;

            uint32_t max_pos = *((uint32_t *)address);
            address += 4;

            uint32_t step = *((uint32_t *)address);
            address += 4;

            char name[12];
            for (int j=0; j < 12; j++)
            {
                name[j] = *address;
                address++;
            }
            name[11] = '\0';

            //additional unused bits
            address += 8;

            if (blind_present == 0)
            {
                Blind *bld = new Blind;
                bld->setName(name);
                bld->btCode = bt_code;
                bld->setBounds(min_pos, mid_pos, max_pos);
                bld->step = step;
                bld->setType((BlindType) blind_type);
                bld->setChannel((BlindChannel) blind_channel);

                bld->enableOutput(true);
                bld->setPosition(mid_pos);
                delay(1000);
                bld->enableOutput(false);

                blinds.push_back(bld);
            }
        }

        //restore remote/buttons
        for (int i = 0 ; i < kMAX_BUTTONS; i++)
        {
            unsigned char *data1 = address;
            address += 4;

            uint8_t button_present = data1[0];
            uint8_t event_type = data1[1];

            uint32_t buttonCode = *((uint32_t *)address);
            address += 4;

            uint32_t eventHash = *((uint32_t *)address);
            address += 4;

            if (button_present == 0)
            {
                RemoteButton* btn = new RemoteButton();
                btn->eventHash = eventHash;
                btn->remoteButton = buttonCode;
                btn->eventType = (RemoteEventType) event_type;

                remoteButtons.push_back(btn);
            }
        }

        //restore web server settings
        {
            unsigned char *data1 = address;
            address += 4;

            uint8_t settingsSaved = data1[0];

            uint32_t port = *((uint32_t *)address);
            address += 4;

            char user[16];
            for (int j=0; j < 16; j++)
            {
                user[j] = *address;
                address++;
            }
            user[14] = '\0';

            char pass[16];
            for (int j=0; j < 16; j++)
            {
                pass[j] = *address;
                address++;
            }
            pass[14] = '\0';

            if (settingsSaved == 0)
            {
                webPort = port;
                strncpy(web_user, user, 15);
                strncpy(web_pass, pass, 15);
            }
        }

        //restore auto blind settings
        {
            unsigned char *data1 = address;
            address += 4;

            autoOpenBlindEnabled = data1[0];
            automaticBlindOpenHour = data1[1];
            automaticBlindOpenMinute = data1[2];
            automaticBlindOpenPosition = data1[3];

            unsigned char *data2 = address;
            address += 4;

            autoCloseBlindEnabled = data2[0];
            automaticBlindCloseHour = data2[1];
            automaticBlindCloseMinute = data2[2];
            automaticBlindClosePosition = data2[3];
        }

        //restore temp adjustment factor
        {
            unsigned char *data1 = address;
            address += 4;

            if (data1[0] == 0)
            {
                tempAdjust = data1[1];
            }
        }
    }
}

void save_data_to_flash()
{
    //
    // Function stores settings to internal flash
    //

    flash_status = FLASH_COMPLETE;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    flash_status = FLASH_EraseSector(FLASH_Sector_11, VoltageRange_3);

    if (flash_status != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return;
    }

    uint8_t* address = (uint8_t *) &flash_data[0];

    //program first run status bit
    flash_status = FLASH_ProgramByte((uint32_t)address, 0x00);
    address += 4;
    if (flash_status != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return;
    }

    //save lights
    int light_counter = 0;
    for (int i = 0 ; i < lights.size(); i++)
    {
        light_counter++;
        Light* lght = lights[i];
        uint8_t data1[4] = {0, lght->getType(), 0, lght->device};

        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)&data1));
        address += 4;

        char* name_pointer = (char *)lght->getName();
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)name_pointer));
        name_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)name_pointer));
        name_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)name_pointer));
        name_pointer += 4;
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)lght->btCode);
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)lght->systemCode);
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, 0);
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, 0);
        address += 4;
    }
    address += 32*(kMAX_LIGHTS - light_counter);

    //save blinds
    int blind_counter = 0;
    for (int i = 0 ; i < blinds.size(); i++)
    {
        blind_counter++;
        Blind* bld = blinds[i];
        uint8_t data1[4] = {0, bld->channel, bld->type, 0};

        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)&data1));
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)bld->btCode);
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)bld->minPosition);
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)bld->midPosition);
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)bld->maxPosition);
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)bld->step);
        address += 4;

        char* name_pointer = (char *)bld->getName();
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)name_pointer));
        name_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)name_pointer));
        name_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)name_pointer));
        name_pointer += 4;
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, 0);
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, 0);
        address += 4;
    }
    address += 44*(kMAX_BLINDS - blind_counter);

    //save remote/local buttons
    int button_counter = 0;
    for (int i = 0; i < remoteButtons.size(); i++)
    {
        button_counter++;
        RemoteButton* btn = remoteButtons[i];

        uint8_t data[4] = {0, (uint8_t) btn->eventType, 0, 0};
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)&data));
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)btn->remoteButton);
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)btn->eventHash);
        address += 4;
    }
    address += 12*(kMAX_BUTTONS - button_counter);

    //save web server settings
    {
        uint8_t data[4] = {0, 0, 0, 0};
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)&data));
        address += 4;

        flash_status = FLASH_ProgramWord((uint32_t)address, (uint32_t)webPort);
        address += 4;

        char* user_pointer = web_user;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)user_pointer));
        user_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)user_pointer));
        user_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)user_pointer));
        user_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)user_pointer));
        user_pointer += 4;
        address += 4;

        char* pass_pointer = web_pass;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)pass_pointer));
        pass_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)pass_pointer));
        pass_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)pass_pointer));
        pass_pointer += 4;
        address += 4;
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)pass_pointer));
        pass_pointer += 4;
        address += 4;
    }

    //save auto blinds settings
    {
        uint8_t data[4] = {autoOpenBlindEnabled, automaticBlindOpenHour, automaticBlindOpenMinute, automaticBlindOpenPosition};
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)&data));
        address += 4;

        uint8_t data2[4] = {autoCloseBlindEnabled, automaticBlindCloseHour, automaticBlindCloseMinute, automaticBlindClosePosition};
        flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)&data2));
        address += 4;
    }

    //save temp adjustment factor
    {
        if (tempAdjust != 0)
        {
            uint8_t data[4] = {0, tempAdjust, 0, 0};
            flash_status = FLASH_ProgramWord((uint32_t)address, *((uint32_t *)&data));
        }
        address += 4;
    }

    FLASH_Lock();

   // NVIC_SystemReset();
}

void deleteFlash()
{
    //
    // Function deletes all settings from flash
    //

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_OPERR |FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    flash_status = FLASH_EraseSector(FLASH_Sector_11, VoltageRange_3);

    NVIC_SystemReset();
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// WIFI (WEB SERVER) FUNCTIONS - PART 1
// ------------------------------------------------------------------------------------------------------------------------------------------------------

void sendWifiUsart1(char* data)
{
    //
    // Sends data to wifi module using connection 1
    // For now PIO only
    //

    int sent;

    for (sent = 0; sent < strlen(data); sent++)
    {
        while(!USART_GetFlagStatus(USART1, USART_FLAG_TXE)) {}
        USART_SendData(USART1, data[sent]);
    }
}

void clearWifiUsart1Buffer()
{
    //
    // Clears entire wifi buffer for connection 1
    //

    eth1_busy = 1;
    eth1_buff_indicator = 0;
    for (int i = 0; i < eth1_buff_size; i++)
    {
        eth1_buff[i] = '\0';
    }
    eth1_busy = 0;
}

void clearEthSendBuffer()
{
    //
    // Clears the entire wifi send buffer
    //

    eth_send_pointer = 0;
    for (int i = 0; i < sizeof(eth_send_buff); i++)
    {
        eth_send_buff[i] = '\0';
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// BASIC GPIO FUNCTIONS
// ------------------------------------------------------------------------------------------------------------------------------------------------------

void GPIO_LED_init()
{
    //
    // Intialize the blue and green LEDs
    //

    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14|GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOE, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOE,GPIO_Pin_14|GPIO_Pin_15);
}

void LCD_backlight_init()
{
    //
    // Initialize LCD backlight LED
    //

    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;

    GPIO_Init(GPIOD, &GPIO_InitStructure);
    GPIO_SetBits(GPIOD,GPIO_Pin_15);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// OTHER HELPER FUNCTIONS
// ------------------------------------------------------------------------------------------------------------------------------------------------------

void clearTextBuffer(int buffer = -1)
{
    //
    // Clears the selected text buffer (or all buffers by default)
    //

    if (buffer == -1 || buffer == 0)
    {
        for (int i = 0; i < 70; i++)
            text_buffer[i] = '\0';
    }

    if (buffer == -1 || buffer == 1)
    {
        for (int i = 0; i < 35; i++)
        {
            mini_text_buffer1[i] = '\0';
        }
    }

    if (buffer == -1 || buffer == 2)
    {
        for (int i = 0; i < 35; i++)
        {
            mini_text_buffer2[i] = '\0';
        }
    }

    if (buffer == -1 || buffer == 1)
    {
        for (int i = 0; i < 35; i++)
        {
            mini_text_buffer3[i] = '\0';
        }
    }
}

long int_map(long x, long in_min, long in_max, long out_min, long out_max)
{
    //
    // Maps variable from one interval to the other
    //

    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// BACKGROUND TASK FUNCTIONS
// ------------------------------------------------------------------------------------------------------------------------------------------------------

void vTask1Function (void *pvParameters)
{
    //
    // Task function for refreshing the info display. Calls displayInfoScreen every few seconds
    // and refreshing the information (clock, temperature, light status)
    // Only do this if the info screen is currently displayed
    //

    while (1)
    {
        if (Menu::onInfoScreen)
        {
            taskENTER_CRITICAL();
            displayInfoScreen();
            taskEXIT_CRITICAL();
        }

        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}

void vTask2Function (void *pvParameters)
{
    //
    // Task function for handling remote input, and handling
    // LED status displays and autoblind feature
    //

    int greenBlinkNum = 0;
    bool greenOn = true;
    bool greenBlink = false;

    while (1)
    {
        if (RemoteButton::shouldRunActions && RemoteButton::remoteCodePressed)
        {
            greenBlink = true;
            greenBlinkNum = 2;
            greenOn = true;

            RemoteButton* butt = NULL;

            for (int i = 0; i < remoteButtons.size(); i++)
            {
                if (RemoteButton::remoteCodePressed == remoteButtons[i]->remoteButton)
                {
                    butt = remoteButtons[i];
                    break;
                }
            }

            if (butt->eventType == TYPE_LIGHT_ON || butt->eventType == TYPE_LIGHT_OFF || butt->eventType == TYPE_LIGHT_TOGGLE)
            {
                for (int j=0; j < lights.size(); j++)
                {
                    if (lights[j]->calculateHash() == butt->eventHash)
                    {
                        taskENTER_CRITICAL();
                        if (butt->eventType == TYPE_LIGHT_TOGGLE)
                        {
                            lights[j]->onOff(!lights[j]->isOn());
                        }
                        else
                        {
                            lights[j]->onOff(butt->eventType == TYPE_LIGHT_ON);
                        }
                        taskEXIT_CRITICAL();
                    }
                }
            }
            else if (butt->eventType == TYPE_BLIND_TOGGLE)
            {
                for (int j=0; j < blinds.size(); j++)
                {
                    if (blinds[j]->calculateHash() == butt->eventHash)
                    {
                        blinds[j]->toggleState();
                    }
                }
            }
            else if (butt->eventType == TYPE_ACTION)
            {
                if (butt->eventHash == 0)
                {
                    blueLedOn = true;
                    blueLedOnTime = 50000;

                    delayedActionTime = 30000;
                    delayLightOff = true;
                }
            }
            RemoteButton::remoteCodePressed = 0;
        }

        if (delayBlindOpen && autoOpenBlindEnabled)
        {
            for (int i=0; i < blinds.size(); i++)
            {
                blinds[i]->toggleState(automaticBlindOpenPosition);
            }
            delayBlindOpen = false;

            blueLedOn = true;
            blueLedOnTime = 500;
        }

        if (delayBlindClose && autoCloseBlindEnabled)
        {
            for (int i=0; i < blinds.size(); i++)
            {
                blinds[i]->toggleState(automaticBlindClosePosition);
            }
            delayBlindClose = false;

            blueLedOn = true;
            blueLedOnTime = 500;
        }

        if (delayLightOff)
        {
            if (delayedActionTime > 0)
            {
                delayedActionTime -= 150;
            }
            else
            {
                delayLightOff = false;
                delayedActionTime = 0;

                for (int i=0; i < lights.size(); i++)
                {
                    lights[i]->onOff(false);
                }

            }
        }

        if (greenBlink == true && greenBlinkNum > 0)
        {
            if (greenOn)
            {
                GPIO_SetBits(GPIOE, GPIO_Pin_15);
                greenOn = false;
            }
            else
            {
                GPIO_ResetBits(GPIOE, GPIO_Pin_15);
                greenOn = true;
                greenBlinkNum--;
            }
        }
        else if (greenBlink == true && greenBlinkNum == 0)
        {
            greenBlink = false;
            greenOn = true;
        }

        if (blueLedOn)
        {
            if (blueLedOnTime > 0)
            {
                blueLedOnTime -= 150;
                GPIO_SetBits(GPIOE, GPIO_Pin_14);
            }
            else
            {
                blueLedOnTime = 0;
                blueLedOn = false;
                GPIO_ResetBits(GPIOE, GPIO_Pin_14);
            }
        }


        vTaskDelay(150 / portTICK_RATE_MS);
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// WEB SERVER FUNCTIONS FUNCTIONS
// ------------------------------------------------------------------------------------------------------------------------------------------------------

//
// HTTP server defines
//
#define kHTTP_OK_HEAD "HTTP/1.1 200 OK\r\nConnection: close\r\nServer: RHome\r\nPragma: no-cache\r\nContent-Length: "
#define kHTTP_AUTH_HEAD "HTTP/1.1 403 Forbidden\r\nConnection: close\r\nServer: RHome\r\nPragma: no-cache\r\nContent-Length: "
#define kHTTP_HEAD_PART2 "\r\nContent-Type: text/html\r\n\r\n"

void printWebPageStart(char *dest)
{
    //
    // Prints (copies to destination) the beginning of the control web page (including stylesheet)
    //

    dest[0] = '\0';
    strcat(dest, "<!DOCTYPE html><html><head><style type='text/css'>html {height: 100%;} body {min-height: 100%; background: repeating-linear-gradient(45deg, #2c3339, #2c3339 7px, #161819 7px, #161819 12px); font-family: Arial, Helvetica, sans-serif; color: #F3F9FE;} h1 {padding:0px; margin:0px; color: #F3F9FE; font-size: 1.6em;} input {background-color: #9BCCF5; border: solid #161819 1px; width: 300px; height: 25px;} button {background-color:#9BCCF5; border: solid #9BCCF5 1px; height: 30px; vertical-align: middle; font-size: 0.65em;} .main {width: 1000px; height: 550px; border-radius: 275px; margin: auto; background-color: #5DB5FF; position: absolute; top: 0; left: 0; bottom: 0; right: 0; overflow: auto;} .left_top {float:left; width: 499px; height: 274px;} .light_off {border: solid #FFC719 2px; width: 22px; height: 22px; border-radius:11px; display: inline-block; position:relative; top:12px;} .light_on {background-color: #FFC719; width: 24px; height: 24px; border-radius:12px; display: inline-block; position:relative; top:12px;} .elements {color: #FFC719; font-size: 1.3em; line-height: 45px;} progress {width: 70px; apperance: none; -webkit-appearance: none; -moz-appearance: none; border: none; background-color: #F3F9FE;} progress::-webkit-progress-value {background-color: #FFC719;} progress::-moz-progress-bar {background-color: #FFC719;}</style><title>Rhome v3</title></head><body><div class='main'>");
}

void printWebPageEnd(char *dest)
{
    //
    // Prints the end of the control web page
    //

    strcat(dest, "</div></body></html>\0");
}

void printWebErrorResponse(char *dest, char *content_buff, int error)
{
    //
    // Prints server errors (Log in and incorrect username/password)
    //

    char temp_buff[6];
    dest[0] = '\0';

    printWebPageStart(content_buff);

    if (error == 1)
    {
        strcat(content_buff, "<div style='width: 300px; height: 250px; margin: auto; padding-top:125px;'><h1>Please log in!</h1><form method='GET' onSubmit='return false;' id='login'><p>User: <input type='text' id='user' /></p><p>Password: <input type='password' id='pass' /></p><p><button id='sub' onclick=\"var user = document.getElementById('user').value; var pass = document.getElementById('pass').value; location.href = user+'/' + pass;\" >Submit</button></p></form></div>");
    }
    else if (error == 2)
    {
        strcat(content_buff, "<div style='width: 300px; height: 250px; margin: auto; padding-top:125px;'><h1>You are not logged in!</h1>Check your username and password!<br /><br /><button id='sub' onclick=\"location.href = '/' \" >Go back</button></div>");
    }

    printWebPageEnd(content_buff);

    sprintf(temp_buff, "%d", strlen(content_buff));
    strcat(dest, (error == 1)?kHTTP_OK_HEAD : kHTTP_AUTH_HEAD);
    strcat(dest, temp_buff);
    strcat(dest, kHTTP_HEAD_PART2);
}

void task3(void *pvParameters)
{
    //
    // The actual web server task
    //

    char tokens[5][20];
    char user_pass[45];
    int token = -1;
    int token_pointer = 0;

    char web_buff[100];
    std::string delim = "/";

    while(1)
    {
        if (eth1_busy || eth1_buff_indicator < 5)
        {
            vTaskDelay(200 / portTICK_RATE_MS);
            continue;
        }

        clearEthSendBuffer();

        for (int i=0; i < 5; i++)
        {
            tokens[i][0] = '\0';
        }

        char buff = '\0';
        token = -1;
        token_pointer = 0;
        for (int i = 4; i < 100; i++)
        {
            buff = eth1_buff[i];
            if (buff == ' ')
            {
                if (token_pointer > 0)
                {
                    token++;
                }
                break;
            }

            if (buff == '/')
            {
                token++;
                token_pointer = 0;
                continue;
            }

            if (token == -1 || token > 4 || token_pointer >= 18)
            {
                break;
            }

            tokens[token][token_pointer] = buff;
            tokens[token][token_pointer + 1] = '\0';
            token_pointer++;
        }

        //taskENTER_CRITICAL();
        eth1_busy = 1;
        USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);


        int error = 0;
        if (!token)
        {
            error = 1;
        }
        else if (!(strcmp(web_user, tokens[0]) == 0 && strcmp(web_pass, tokens[1]) == 0))
        {
            error = 2;
        }

        if (error)
        {
            printWebErrorResponse((char *)eth1_buff, eth_send_buff, error);
            sendWifiUsart1((char *)eth1_buff);
            sendWifiUsart1((char *)eth_send_buff);
        }
        else
        {
            strcpy(user_pass, tokens[0]);
            strcat(user_pass, "/");
            strcat(user_pass, tokens[1]);

            if (strcmp(tokens[2], "lght") == 0)
            {
                int index = atoi(tokens[3]);

                if (index >= 0 && index < lights.size())
                {
                    bool turnOn = (strcmp(tokens[4], "on") == 0)?true:false;
                    lights[index]->onOff(turnOn);
                }
            }

            if (strcmp(tokens[2], "bld") == 0)
            {
                int index = atoi(tokens[3]);

                if (index >= 0 && index < blinds.size())
                {
                    int newPos = atoi(tokens[4]);
                    if (newPos >= 0 && newPos <= 2)
                    {
                        blinds[index]->toggleState(newPos);
                    }
                }
            }

            clearWifiUsart1Buffer();
            clearEthSendBuffer();

            printWebPageStart(eth_send_buff);
            strcat((char *)eth_send_buff, "<div class='left_top' style='border-right: solid #9BCCF5 1px; border-bottom: solid #9BCCF5 1px; '><div style='padding: 10px; float: right;'><div style='text-align:right;' class='elements'>");

            for (int i = 0; i < lights.size(); i++)
            {
                //light_off'>&nbsp;</div>&nbsp;&nbsp;<button>On</button>&nbsp;<button>Off</button><br />
                sprintf(web_buff, "%d", i);
                strcat((char *)eth_send_buff, lights[i]->getName());
                strcat((char *)eth_send_buff, ":&nbsp;&nbsp;<div class='");
                strcat((char *)eth_send_buff, (lights[i]->isOn())?"light_on":"light_off");
                strcat((char *)eth_send_buff, "'>&nbsp;</div>&nbsp;&nbsp;<button onclick=\"location.href = '/");
                strcat((char *)eth_send_buff, user_pass);
                strcat((char *)eth_send_buff, "/lght/");
                strcat((char *)eth_send_buff, web_buff);
                strcat((char *)eth_send_buff, "/on';\" >On</button>");
                strcat((char *)eth_send_buff, "&nbsp;<button onclick=\"location.href = '/");
                strcat((char *)eth_send_buff, user_pass);
                strcat((char *)eth_send_buff, "/lght/");
                strcat((char *)eth_send_buff, web_buff);
                strcat((char *)eth_send_buff, "/off';\" >Off</button>");
                strcat((char *)eth_send_buff, "<br />");
            }
            strcat((char *)eth_send_buff, "</div></div><h1 style='position: relative; bottom:-235px; left: 10px; '>Lights</h1></div><div class='left_top' style='border-left: solid #9BCCF5 1px; border-bottom: solid #9BCCF5 1px;'><h1 style='position: relative; bottom:-235px; float: right; right: 10px;'>Blinds</h1><div style='padding: 10px'><div style='text-align:left;' class='elements'>");
            //Left:&nbsp;&nbsp;<progress value='10' max='100'></progress>&nbsp;&nbsp;<button>Min</button>&nbsp;<button>Mid</button>&nbsp;<button>Max</button><br />
            for (int i = 0; i < blinds.size(); i++)
            {
                sprintf(web_buff, "%d", i);
                strcat((char *)eth_send_buff, "<progress value='");
                strcat((char *)eth_send_buff, (blinds[i]->getState() == 0)? "1": (blinds[i]->getState() == 1)? "50" : "100");
                strcat((char *)eth_send_buff, "' max='100'></progress>&nbsp;&nbsp;<button onclick=\"location.href = '/");
                strcat((char *)eth_send_buff, user_pass);
                strcat((char *)eth_send_buff, "/bld/");
                strcat((char *)eth_send_buff, web_buff);
                strcat((char *)eth_send_buff, "/0';\" >&lt;</button>");
                strcat((char *)eth_send_buff, "&nbsp;<button onclick=\"location.href = '/");
                strcat((char *)eth_send_buff, user_pass);
                strcat((char *)eth_send_buff, "/bld/");
                strcat((char *)eth_send_buff, web_buff);
                strcat((char *)eth_send_buff, "/1';\" >-</button>");
                strcat((char *)eth_send_buff, "&nbsp;<button onclick=\"location.href = '/");
                strcat((char *)eth_send_buff, user_pass);
                strcat((char *)eth_send_buff, "/bld/");
                strcat((char *)eth_send_buff, web_buff);
                strcat((char *)eth_send_buff, "/2';\" >&gt;</button>&nbsp;&nbsp;");
                strcat((char *)eth_send_buff, blinds[i]->getName());
                strcat((char *)eth_send_buff, "<br />");
            }

            strcat((char *)eth_send_buff, "</div></div></div><div class='left_top' style='border-right: solid #9BCCF5 1px; border-top: solid #9BCCF5 1px;'><div style='padding: 10px; float: left;'><h1>Info</h1></div><div style='text-align:right; margin-right:50px;' class='elements'>Temperature: ");
            float temperature = tempSensor.getTemp();
            sprintf((char *)web_buff, "%d", (int)temperature);
            strcat((char *)eth_send_buff, web_buff);
            strcat((char *)eth_send_buff, "°C<br />");
            //strcat((char *)eth_send_buff, "C<br />Outside: 34C<br />");

            RTC_TimeTypeDef RTC_TimeStruct;
            RTC_GetTime(RTC_Format_BIN, &RTC_TimeStruct);
            sprintf(web_buff,"%02d:%02d:%02d", RTC_TimeStruct.RTC_Hours, RTC_TimeStruct.RTC_Minutes, RTC_TimeStruct.RTC_Seconds);
            strcat((char *)eth_send_buff, web_buff);
            strcat((char *)eth_send_buff, "<br /><button onclick=\"location.href = '/");
            strcat((char *)eth_send_buff, user_pass);
            strcat((char *)eth_send_buff, "';\" >Refresh</button>");

            strcat((char *)eth_send_buff, "</div></div><div class='left_top' style='border-left: solid #9BCCF5 1px; border-top: solid #9BCCF5 1px;'><div class='elements' style='margin-left:50px;'>Rhome v3.0<br />http://www.r00li.com</div><div style='padding: 10px;'><h1 style='float:right;'>&nbsp;</h1></div></div><div style='background-color: #0284F0; display:block; width: 100px; border-radius: 50px; height: 100px; position: relative; left:450px; top: 225px;'></div>");
            printWebPageEnd(eth_send_buff);

            sprintf(web_buff, "%d", strlen((char *)eth_send_buff));

            strcat((char *)eth1_buff, kHTTP_OK_HEAD);
            strcat((char *)eth1_buff, web_buff);
            strcat((char *)eth1_buff, kHTTP_HEAD_PART2);
            //strcat((char *)eth1_buff, (char *)eth_send_buff);

            sendWifiUsart1((char *)eth1_buff);
            sendWifiUsart1((char *)eth_send_buff);

        }

        eth1_buff_indicator = 0;

        eth1_busy = 0;
        USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

        clearWifiUsart1Buffer();

        //taskEXIT_CRITICAL();

        vTaskDelay(200 / portTICK_RATE_MS);
    }
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// WIFI (WEB SERVER) FUNCTIONS - PART 2
// ------------------------------------------------------------------------------------------------------------------------------------------------------

void init_USART1()
{
    //
    // Configuration of the first connection with the wifi module
    //

    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

    //inicializacija pinov 6 in 7 naprave GPIOB
    //nastavimo, da naj bo na pin priklopljena alternativna funkcija
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    //za pina 6&7 naprav GPIOB definiramo, da ju prevzame USART1
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_USART1);
    GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_USART1);

    //nastavimo USART1
    //nastavimo baudrate
    USART_InitStruct.USART_BaudRate = 115200;
    //nastavimo dol?ino besede
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    //nastavimo ?tevilo stop bitov
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    //nastavimo pariteto
    USART_InitStruct.USART_Parity = USART_Parity_No;
    //signalov RTS in CTS ne bomo uporabili
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    //napravo USART bomo uporabili tako za sprejemanje in oddajanje
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStruct);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    //inicializacija USART1 prekinitev v NVIC
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);


    USART_Cmd(USART1, ENABLE);

}

void init_USART2()
{
    //
    // Configuration of the first connection with the wifi module
    //

    GPIO_InitTypeDef GPIO_InitStruct;
    USART_InitTypeDef USART_InitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    //inicializacija pinov 6 in 7 naprave GPIOB
    //nastavimo, da naj bo na pin priklopljena alternativna funkcija
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    //za pina 6&7 naprav GPIOB definiramo, da ju prevzame USART1
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_USART2);
    GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_USART2);

    //nastavimo USART1
    //nastavimo baudrate
    USART_InitStruct.USART_BaudRate = 115200;
    //nastavimo dol?ino besede
    USART_InitStruct.USART_WordLength = USART_WordLength_8b;
    //nastavimo ?tevilo stop bitov
    USART_InitStruct.USART_StopBits = USART_StopBits_1;
    //nastavimo pariteto
    USART_InitStruct.USART_Parity = USART_Parity_No;
    //signalov RTS in CTS ne bomo uporabili
    USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    //napravo USART bomo uporabili tako za sprejemanje in oddajanje
    USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStruct);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

    //inicializacija USART1 prekinitev v NVIC
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);


    USART_Cmd(USART2, ENABLE);
}

void initWifiES()
{
    //
    // HLK-RM04 ES Pin configuration
    //

    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_SetBits(GPIOA, GPIO_Pin_8);
}

void setWifiServerPort(int port, bool skipCommit = false)
{
    //
    // sets web server port
    // (connection 1)
    //

    sprintf(text_buffer, "at+remoteport=%d\r\n", port);
    sendWifiUsart1(text_buffer);

    if (!skipCommit)
    {
        sendWifiUsart1("at+net_commit=1\r\n");
        sendWifiUsart1("at+save=1\r\n");
        sendWifiUsart1("at+reconn=1\r\n");
    }
}

void setWifiBasicConfig(bool skipCommit = false)
{
    //
    // Sets basic configuration for connection 1
    //

    sendWifiUsart1("at+mode=Server\r\n");
    sendWifiUsart1("at+remotepro=Tcp\r\n");
    setWifiServerPort(webPort, true);
    sendWifiUsart1("at+timeout=0\r\n");

    sendWifiUsart1("at+uartpacktimeout=0\r\n");
    sendWifiUsart1("at+C2_mode=0\r\n"); //0 = none, 1 = server, 2 = client
    sendWifiUsart1("at+C2_uartpacktimeout=0\r\n");
    sendWifiUsart1("at+C2_uart=115200,8,n,1\r\n");
    sendWifiUsart1("at+C2_protocol=1\r\n");

    if (!skipCommit)
    {
        sendWifiUsart1("at+net_commit=1\r\n");
        sendWifiUsart1("at+save=1\r\n");
        sendWifiUsart1("at+reconn=1\r\n");
    }
}

void wifiConnectTo(char* ssid, char* pass, int encr_type)
{
    //
    // Connects to the specified wifi network
    //

    GPIO_ResetBits(GPIOA, GPIO_Pin_8);
    delay(500);
    GPIO_SetBits(GPIOA, GPIO_Pin_8);

    setWifiBasicConfig(true);

    sendWifiUsart1("at+netmode=2\r\n");
    sprintf(text_buffer, "at+wifi_conf=%s,%s,%s\r\n", ssid, wifiEcnryptionCommands[encr_type], pass);
    sendWifiUsart1(text_buffer);

    sendWifiUsart1("at+net_commit=1\r\n");
    sendWifiUsart1("at+save=1\r\n");
    sendWifiUsart1("at+reconn=1\r\n");

    sendWifiUsart1("at+out_trans=0\r\n");
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// UI MENUS AND OTHER MAIN FUNCTIONS
// ------------------------------------------------------------------------------------------------------------------------------------------------------

void menuCheckerTask(void* pvParameters)
{
    //
    // Main menu checker functions - controls the menu flow throughout the app
    //

    while (1)
    {
        if (millis() > Menu::lastInteractionTime + 30000 && !Menu::screenOff)
        {
            Menu::screenOff = true;
            GPIO_ResetBits(GPIOD,GPIO_Pin_15);
        }
        else if (millis() <= Menu::lastInteractionTime + 30000 && Menu::screenOff)
        {
            Menu::screenOff = false;
            GPIO_SetBits(GPIOD,GPIO_Pin_15);
        }

        if (Menu::resignInputControl && Menu::resignedController)
        {
            Menu::resignInputControl = false;
            Menu::resignedController();
            continue;
        }

        Menu::calculateTurns();
        Menu::calculateNiceTurns();

        if (Menu::enterPressed)
        {
            Menu::onInfoScreen = false;
            taskENTER_CRITICAL();
            pressedMenuOptionsStack.push_back(mainMenuOptions[Menu::positionSelected]);
            Menu::enterPressed = false;
            Menu::actionIndex = (mainMenuOptions[Menu::positionSelected])->selectionId;
            (mainMenuOptions[Menu::positionSelected])->doOnClick();
            taskEXIT_CRITICAL();

            continue;
        }

        if (Menu::turns == 0)
        {
            vTaskDelay(50 / portTICK_RATE_MS);
            continue;
        }

        int oldMenuPosition = Menu::positionSelected;

        Menu::positionSelected = Menu::positionSelected + Menu::turns;
        if (Menu::positionSelected + Menu::turns < 0)
            Menu::positionSelected = 0;
        else if (Menu::positionSelected + Menu::turns >= mainMenuOptions.size())
            Menu::positionSelected = mainMenuOptions.size() - 1;

        Menu::turns = 0;

        if (oldMenuPosition == Menu::positionSelected)
        {
            vTaskDelay(50 / portTICK_RATE_MS);
            continue;
        }

        (mainMenuOptions[oldMenuPosition])->setSelected(false);
        (mainMenuOptions[Menu::positionSelected])->setSelected(true);


        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

void backMenuButtonHandler()
{
    //
    // Handles menu back button pressed
    // Restores previous menu in menu hierarchy
    //

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    for (int i = 0; i < mainMenuOptions.size(); i++)
    {
        delete mainMenuOptions[i];
    }

    mainMenuOptions = menuStack.back();
    if (!menuStack.empty())
    {
        menuStack.pop_back();
    }

    if (!menuStack.empty())
    {
        pressedMenuOptionsStack.pop_back();
        pressedMenuOptionsStack.pop_back();

        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            delete mainMenuOptions[i];
        }
        mainMenuOptions = menuStack.back();
        menuStack.pop_back();

        pressedMenuOptionsStack.back()->doOnClick();
    }
    else
    {
        pressedMenuOptionsStack.back()->setHidden(true);
        pressedMenuOptionsStack.pop_back();
    }


    Menu::positionSelected = 0;
    mainMenuOptions[Menu::positionSelected]->setSelected(true);
}

void drawLights()
{
    //
    // Draws the light menu (shows the on/off menu for the light)
    //

    std::vector<MenuOption *> options;

    int selIndex = Menu::actionIndex;

    Light *lght = lights[selIndex];
    IndicatorMenuOption* option = (IndicatorMenuOption *) mainMenuOptions[selIndex];

    int menuStartx = mainMenuOptions[selIndex]->getX();
    int menuStarty = mainMenuOptions[selIndex]->getY()-3;
    int menuEndx = 320;
    int menuEndy = mainMenuOptions[selIndex]->getY() + 18 + 4;

    TM_ILI9341_DrawFilledRectangle(menuStartx, menuStarty, menuEndx, menuEndy, ILI9341_COLOR_BLUE2);

    auto button_click = [&, lght, option, menuStartx, menuStarty, menuEndx, menuEndy] ()
    {
        if (Menu::positionSelected == 0)
            lght->onOff(true);
        else if (Menu::positionSelected == 1)
            lght->onOff(false);

        option->setOn(lght->isOn());
        TM_ILI9341_DrawFilledRectangle(menuStartx, menuStarty, menuEndx, menuEndy, ILI9341_COLOR_BLACK);
        option->setNeedsUpdate();
        option->draw();

        option->setSelected(false);

        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            delete mainMenuOptions[i];
        }
        mainMenuOptions = menuStack.back();
        menuStack.pop_back();

        pressedMenuOptionsStack.pop_back();
        pressedMenuOptionsStack.pop_back();

        Menu::positionSelected = 0;
        mainMenuOptions.front()->setSelected(true);
    };

    MenuOption* onsw = new MenuOption(menuStartx + 5, menuStarty+3, "On", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    onsw->selectionColor = ILI9341_COLOR_BLACK;
    onsw->setSelected(true);
    onsw->setOnClickListener(button_click);
    options.push_back(onsw);

    MenuOption* offsw = new MenuOption(menuStartx + 5 + 2*18 + 3 + 5, menuStarty+3, "Off", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    offsw->selectionColor = ILI9341_COLOR_BLACK;
    offsw->setOnClickListener(button_click);
    options.push_back(offsw);

    MenuOption* bck = new MenuOption(320-18, menuStarty+3, "X", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    bck->selectionColor = ILI9341_COLOR_BLACK;
    bck->setOnClickListener(button_click);
    options.push_back(bck);

    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void displayLightMenu()
{
    //
    // Draws the menu with light selection
    // (you select the light you wish to control here)
    //

    Menu::displayLoading();
    Menu::clearRightMenu();

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    for (int i = 0; i < lights.size(); i++)
    {
        Light *lgh = lights[i];
        IndicatorMenuOption* op = new IndicatorMenuOption(165, 38+35*i, lgh->getName(), ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

        op->setOnClickListener(drawLights);
        op->selectionId = i;
        op->setOn(lgh->isOn());

        options.push_back(op);
    }

    MenuOption* bck = new MenuOption(135, 240-20, "< Back", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener(backMenuButtonHandler);
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;

    Menu::clearTitle();
    std::string title = "Lights";
    Label lbl(320-title.length()*11, 0, title.c_str(), ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
}

void displayInfoScreen()
{
    //
    // Draws the main star screen of the system
    // it also the only screen that periodically updates
    //

    if (!Menu::onInfoScreen)
    {
        Menu::onInfoScreen = true;
        Menu::clearTitle();
        sprintf(text_buffer, "Info");
        TM_ILI9341_Puts(320-11*strlen(text_buffer), 0, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

        Menu::clearRightMenu();
    }

    RTC_TimeTypeDef RTC_TimeStruct;
    RTC_GetTime(RTC_Format_BIN, &RTC_TimeStruct);
    sprintf(text_buffer,"   %02d:%02d", RTC_TimeStruct.RTC_Hours, RTC_TimeStruct.RTC_Minutes);
    TM_ILI9341_Puts(165, 38, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    RTC_DateTypeDef RTC_DateStruct;
    RTC_GetDate(RTC_Format_BIN, &RTC_DateStruct);
    sprintf(text_buffer," %02d/%02d/%04d", RTC_DateStruct.RTC_Date, RTC_DateStruct.RTC_Month, RTC_DateStruct.RTC_Year+2000);
    TM_ILI9341_Puts(165, 60, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    float temperature = tempSensor.getTemp();
    sprintf(text_buffer, "Temp: %02dC", (int)temperature);
    TM_ILI9341_Puts(165, 102, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    //draw light quick info
    {
        TM_ILI9341_DrawRectangle(165, 132, 315, 158, ILI9341_COLOR_GRAY);
        int spacing = (lights.size() != kMAX_LIGHTS)? (kMAX_LIGHTS - lights.size())*30 : 30;
        int startx_offset = (lights.size() != kMAX_LIGHTS)? spacing/2 : 0;
        spacing = (lights.size() != kMAX_LIGHTS)? spacing / (kMAX_LIGHTS - lights.size()) : spacing;
        for (int i=0; i < lights.size(); i++)
        {
            //sprintf(mini_text_buffer1, "%d", i);

            if (lights[i]->isOn())
            {
                TM_ILI9341_DrawFilledCircle(179+i*spacing + startx_offset, 145, 8, ILI9341_COLOR_YELLOW);
                //TM_ILI9341_Puts(176+i*spacing + startx_offset, 141, mini_text_buffer1, &TM_Font_7x10, ILI9341_COLOR_BLACK, ILI9341_COLOR_YELLOW);
            }
            else
            {
                TM_ILI9341_DrawFilledCircle(179+i*spacing + startx_offset, 145, 8, ILI9341_COLOR_BLACK);
                TM_ILI9341_DrawCircle(179+i*spacing + startx_offset, 145, 8, ILI9341_COLOR_WHITE);
                //TM_ILI9341_Puts(176+i*spacing + startx_offset, 141, mini_text_buffer1, &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            }
        }

    }

    //Draw blind quick info
    {
        TM_ILI9341_DrawRectangle(165, 165, 315, 191, ILI9341_COLOR_GRAY);
        int spacing = (blinds.size() != kMAX_BLINDS)? (kMAX_BLINDS - blinds.size())*36 : 36;
        int startx_offset = (blinds.size() != kMAX_BLINDS)? spacing/2 : 0;
        spacing = (blinds.size() != kMAX_BLINDS)? spacing / (kMAX_BLINDS - blinds.size()) : spacing;
        for (int i=0; i < blinds.size(); i++)
        {
            sprintf(mini_text_buffer1, "%d", i);

            if (blinds[i]->getState() == kBlindStateMin)
            {
                TM_ILI9341_DrawFilledRectangle(179+i*spacing + startx_offset, 171, 179+i*spacing + startx_offset + 13, 185, ILI9341_COLOR_BLACK);
                TM_ILI9341_DrawRectangle(179+i*spacing + startx_offset, 171, 179+i*spacing + startx_offset + 13, 185, ILI9341_COLOR_WHITE);
            }
            else if (blinds[i]->getState() == kBlindStateMid)
            {
                TM_ILI9341_DrawFilledRectangle(179+i*spacing + startx_offset, 171, 179+i*spacing + startx_offset + 13, 185, ILI9341_COLOR_BLACK);
                TM_ILI9341_DrawRectangle(179+i*spacing + startx_offset, 171, 179+i*spacing + startx_offset + 13, 185, ILI9341_COLOR_WHITE);
                TM_ILI9341_DrawFilledRectangle(179+i*spacing + startx_offset, 179, 179+i*spacing + startx_offset + 13, 185, ILI9341_COLOR_WHITE);
            }
            else
            {
                TM_ILI9341_DrawRectangle(179+i*spacing + startx_offset, 171, 179+i*spacing + startx_offset + 13, 185, ILI9341_COLOR_WHITE);
                TM_ILI9341_DrawFilledRectangle(179+i*spacing + startx_offset, 171, 179+i*spacing + startx_offset + 13, 185, ILI9341_COLOR_WHITE);
            }
        }
    }
}

void displayBlindAdjustmentMenu()
{
    //
    // Displays the menu for blind adjustment
    // Shows the current blind position and enables you to control the blind
    //

    std::vector<MenuOption *> options;

    int selIndex = Menu::actionIndex;

    Blind *bld = blinds[selIndex];
    ProgressBarMenuOption* option = (ProgressBarMenuOption *) mainMenuOptions[selIndex];
    option->setProgressColor(ILI9341_COLOR_YELLOW);

    Menu::resignInputControl = true;
    Menu::resignedController = [&,bld,option] ()
    {
        bld->enableOutput(true);
        int oldPos = bld->getPosition();
        while (!Menu::enterPressed)
        {
            int newPos = bld->getPosition();

            Menu::calculateTurns();

            newPos = bld->getPosition() + Menu::realTurns*bld->step;
            Menu::realTurns = 0;

            if (newPos == oldPos)
            {
                continue;
            }

            bld->setPosition(newPos);
            oldPos = newPos;
            option->setProgress(bld->getPosition());
        }
        bld->enableOutput(false);
        option->setProgressColor(ILI9341_COLOR_BLUE2);
        Menu::enterPressed = false;
    };
}

void displayBlindMenu()
{
    //
    // Shows the list of all blinds in the room
    //

    Menu::displayLoading();
    Menu::clearRightMenu();

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    for (int i = 0; i < blinds.size(); i++)
    {
        Blind *bld = blinds[i];
        ProgressBarMenuOption* op = new ProgressBarMenuOption(165, 38+45*i, bld->getName(), ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

        op->setOnClickListener(displayBlindAdjustmentMenu);
        op->selectionId = i;
        op->setMinMax(bld->minPosition, bld->maxPosition);
        op->setProgress(bld->getPosition());

//MenuPosition p1(13, 15, 0, 0, 0);
//op1->setMenuPosition(p1);
        options.push_back(op);
    }

    MenuOption* bck = new MenuOption(135, 240-20, "< Back", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener(backMenuButtonHandler);
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;

    Menu::clearTitle();
    std::string title = "Blinds";
    Label lbl(320-title.length()*11, 0, title.c_str(), ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
}

void initRTC(bool restart = false)
{
    //
    // Initializes the internal RTC on the micro
    //

    /* Enable the PWR clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    /* Allow access to RTC */
    PWR_BackupAccessCmd(ENABLE);

    if (restart)
    {
        //Do this part only if the user chose to set the date/time

        RCC_BackupResetCmd(ENABLE);
        RCC_BackupResetCmd(DISABLE);

        //RCC_LSICmd(ENABLE);
        RCC_LSEConfig(RCC_LSE_ON);

        /* Wait until LSE is ready */
        while (RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);

        /* RTC Clock Source Selection */
        RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);

        /* Enable the RTC */
        RCC_RTCCLKCmd(ENABLE);

        RTC_WaitForSynchro();

        RTC_InitTypeDef RTC_InitStruct;
        RTC_StructInit(&RTC_InitStruct);
        RTC_InitStruct.RTC_HourFormat=RTC_HourFormat_24;
        RTC_Init(&RTC_InitStruct);
    }
    else
    {
        RTC_WaitForSynchro();
    }
}

void numberInput(MenuOption* option, int min_num, int max_num, int buffer_location, Blind *blind = NULL)
{
    //
    // Display the numeric input menu
    // Enables the user to select one of the values from the desired range)
    //
    // Includes special code for setting Blind MIN/MID/MAX values
    //

    option->selectionColor = ILI9341_COLOR_BLUE2;
    option->setNeedsUpdate();
    option->draw();

    Menu::resignInputControl = true;
    Menu::resignedController = [&,option, min_num, max_num, blind, buffer_location] ()
    {
        int selection = (blind == NULL)?min_num: min_num + (max_num-min_num)/2;
        Menu::turns = selection;
        char num_buff[6];

        sprintf(num_buff, "%d", (blind == NULL)?selection:selection/10);
        option->setText(num_buff);

        if (blind != NULL)
        {
            blind->enableOutput(true);
            Menu::turnMultiplier = 10;
        }

        while (!Menu::enterPressed)
        {
            Menu::calculateTurns();
            Menu::calculateNiceTurns();

            if (Menu::turns < min_num)
                Menu::turns = min_num;
            else if ( Menu::turns > max_num)
                Menu::turns = max_num;

            if (selection ==  Menu::turns)
                continue;

            selection = Menu::turns;

            if (blind != NULL)
            {
                blind->setPosition(selection);
            }

            sprintf(num_buff, "%d", (blind == NULL)?selection:selection/10);

            option->setText(num_buff);

            delay(70);
        }

        if (blind != NULL)
        {
            blind->enableOutput(false);
            Menu::turnMultiplier = 1;
        }


        delay(50);
        Menu::enterPressed = false;
        Menu::turns = 0;

        globalIntBuffer[buffer_location] = selection;

        option->selectionColor = ILI9341_COLOR_WHITE;
        option->setNeedsUpdate();
        option->draw();

        pressedMenuOptionsStack.pop_back();
        Menu::resignInputControl = false;
        Menu::resignedController = NULL;
    };
}

void RTC_config(void)
{
    //
    // Displays the RTC config menu
    //

    pressedMenuOptionsStack.pop_back();

    Menu::clearPopup();
    TM_ILI9341_Puts(20, 38, "Enter time (24h format):", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    TM_ILI9341_Puts(20, 110, "Enter date (DD/MM/YYY):", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);


    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    globalIntBuffer[0] = 0;
    MenuOption* oph = new MenuOption(40, 73, "00", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto oph_onclick = [&,oph] ()
    {
        numberInput(oph, 0, 23, 0);
    };
    oph->setOnClickListener(oph_onclick);
    oph->selectionId = 0;
    options.push_back(oph);

    TM_ILI9341_Puts(67, 73, ":", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    globalIntBuffer[1] = 0;
    MenuOption* opm = new MenuOption(83, 73, "00", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto opm_onclick = [&,opm] ()
    {
        numberInput(opm, 0, 59, 1);
    };
    opm->setOnClickListener(opm_onclick);
    opm->selectionId = 1;
    options.push_back(opm);

    TM_ILI9341_Puts(110, 73, ":", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    globalIntBuffer[2] = 0;
    MenuOption* ops = new MenuOption(126, 73, "00", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto ops_onclick = [&,ops] ()
    {
        numberInput(ops, 0, 59, 2);
    };
    ops->setOnClickListener(ops_onclick);
    ops->selectionId = 2;
    options.push_back(ops);


    globalIntBuffer[3] = 1;
    MenuOption* opd = new MenuOption(40, 145, "01", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto opd_onclick = [&,opd] ()
    {
        numberInput(opd, 1, 31, 3);
    };
    opd->setOnClickListener(opd_onclick);
    opd->selectionId = 3;
    options.push_back(opd);

    TM_ILI9341_Puts(67, 145, "/", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    globalIntBuffer[4] = 1;
    MenuOption* opmm = new MenuOption(83, 145, "01", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto opmm_onclick = [&,opmm] ()
    {
        numberInput(opmm, 1, 12, 4);
    };
    opmm->setOnClickListener(opmm_onclick);
    opmm->selectionId = 4;
    options.push_back(opmm);

    TM_ILI9341_Puts(110, 145, "/", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    globalIntBuffer[5] = 2010;
    MenuOption* opy = new MenuOption(126, 145, "2010", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto opy_onclick = [&,opy] ()
    {
        numberInput(opy, 2010, 2100, 5);
    };
    opy->setOnClickListener(opy_onclick);
    opy->selectionId = 5;
    options.push_back(opy);



    MenuOption* bck = new MenuOption(20, 240-35, "Done", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener([] ()
    {
        pressedMenuOptionsStack.pop_back();

        initRTC(true);

        RTC_TimeTypeDef RTC_TimeStruct;
        RTC_TimeStruct.RTC_Hours = globalIntBuffer[0];
        RTC_TimeStruct.RTC_Minutes = globalIntBuffer[1];
        RTC_TimeStruct.RTC_Seconds = globalIntBuffer[2];
        RTC_SetTime(RTC_Format_BIN,&RTC_TimeStruct);

        RTC_DateTypeDef RTC_DateStruct;
        RTC_DateStruct.RTC_WeekDay = 1;
        RTC_DateStruct.RTC_Date = globalIntBuffer[3];
        RTC_DateStruct.RTC_Month = globalIntBuffer[4];
        RTC_DateStruct.RTC_Year = globalIntBuffer[5]-2000;
        RTC_SetDate(RTC_Format_BIN,&RTC_DateStruct);

        Menu::clearMenu();
        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            delete mainMenuOptions[i];
        }
        mainMenuOptions = menuStack.back();
        menuStack.pop_back();
        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            mainMenuOptions[i]->setNeedsUpdate();
            mainMenuOptions[i]->draw();
        }
        for (int i = 0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        mainMenuOptions.front()->setSelected(true);
        Menu::positionSelected = 0;

    });
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void keyboardPopup(bool skipLeftMenuRedraw = true, bool clearPopup = true, int maxChars = 10)
{
    //
    // Shows and handles the on-screen keyboard
    //

    pressedMenuOptionsStack.pop_back();
    std::vector<MenuOption *> options;

    Menu::clearPopup();
    TM_ILI9341_Puts(30, 25, ">", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    TM_ILI9341_DrawLine(10, 52, 310, 52, ILI9341_COLOR_WHITE);

    for (int i=0; i < kALPHABET_SIZE; i++)
    {
        int starty = 60 + 30*(i/14);
        int startx = i - 14*(i/14);

        TM_ILI9341_Puts(20+(startx*20), starty, (char *)alphabet[i], &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    }

    TM_ILI9341_DrawLine(10, 203, 310, 203, ILI9341_COLOR_WHITE);

    TM_ILI9341_Puts(138, 208, "Done", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    TM_ILI9341_Puts(30, 208, "Del", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    Menu::turns = 0;
    int oldTurns = 0;
    bool donePressed = false;
    Label *lbl = new Label(53,25,"",ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    text_buffer[0] = '\0';
    int text_index = 0;

    TM_ILI9341_Puts(20, 60, (char *)alphabet[0], &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);

    while (!donePressed)
    {
        Menu::calculateTurns();
        Menu::calculateNiceTurns();

        if (Menu::turns < 0)
            Menu::turns = 0;
        else if (Menu::turns > kALPHABET_SIZE + 2)
            Menu::turns = kALPHABET_SIZE + 2;

        if (Menu::turns != oldTurns)
        {
            int starty1 = 60 + 30*(oldTurns/14);
            int startx1 = oldTurns - 14*(oldTurns/14);

            if (oldTurns == kALPHABET_SIZE)
            {
                TM_ILI9341_Puts(30, 208, "Del", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            }
            else if (oldTurns > kALPHABET_SIZE)
            {
                TM_ILI9341_Puts(138, 208, "Done", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            }
            else
            {
                TM_ILI9341_Puts(20+(startx1*20), starty1, (char *)alphabet[oldTurns], &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            }

            int starty2 = 60 + 30*(Menu::turns/14);
            int startx2 = Menu::turns - 14*(Menu::turns/14);

            if (Menu::turns == kALPHABET_SIZE)
            {
                TM_ILI9341_Puts(30, 208, "Del", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
            }
            else if (Menu::turns > kALPHABET_SIZE)
            {
                TM_ILI9341_Puts(138, 208, "Done", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
            }
            else
            {
                TM_ILI9341_Puts(20+(startx2*20), starty2, (char *)alphabet[Menu::turns], &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
            }

            oldTurns = Menu::turns;
        }

        if (Menu::enterPressed)
        {
            Menu::enterPressed = false;

            if (oldTurns > kALPHABET_SIZE)
            {
                donePressed = true;
            }
            else if (oldTurns == kALPHABET_SIZE)
            {
                text_buffer[text_index] = '\0';

                if (text_index-1 >= 0)
                {
                    text_buffer[text_index-1] = '\0';
                    text_index--;
                }
            }
            else
            {
                if (text_index < maxChars)
                {
                    text_buffer[text_index] = alphabet[oldTurns][0];
                    text_buffer[text_index+1] = '\0';
                    text_index++;
                }
            }

            lbl->setText(text_buffer);
        }

        delay(70);
    }

    delay(50);
    Menu::enterPressed = false;
    Menu::turns = 0;

    if (clearPopup)
        Menu::clearPopup();
    else
        Menu::clearMenu();

    if (!skipLeftMenuRedraw)
    {
            for (int i = 0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
    }
    for (int i = 0; i < mainMenuOptions.size(); i++)
    {
            mainMenuOptions[i]->setNeedsUpdate();
            mainMenuOptions[i]->draw();
    }

    //mainMenuOptions.front()->setSelected(true);
    //Menu::positionSelected = 0;

}

void displayDropDown(const char opt[][11], int numOpt, MenuOption* selectedOpt, char* menuText, int saveToIndex)
{
    //
    // Displays a drop-down menu
    //

    pressedMenuOptionsStack.pop_back();

    Menu::resignInputControl = true;
    Menu::resignedController = [&, selectedOpt, opt, numOpt, menuText, saveToIndex]
    {
        TM_ILI9341_DrawFilledRectangle(selectedOpt->getX()-3, selectedOpt->getY()-3, 300, selectedOpt->getY() + 22, ILI9341_COLOR_BLUE2);

        int option = -1;
        Menu::turns = 0;

        while (!Menu::enterPressed)
        {
            Menu::calculateTurns();
            delay(70);
            Menu::calculateNiceTurns();

            if (Menu::turns > numOpt-1)
                Menu::turns = numOpt-1;
            else if (Menu::turns < 0)
                Menu::turns = 0;

            if (Menu::turns == option)
                continue;

            option = Menu::turns;

            TM_ILI9341_DrawFilledRectangle(selectedOpt->getX()-3, selectedOpt->getY()-3, 300, selectedOpt->getY() + 22, ILI9341_COLOR_BLUE2);

            if (option > 0)
            {
                TM_ILI9341_Puts(selectedOpt->getX()+5, selectedOpt->getY(), "<", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
            }

            if (option < numOpt-1)
            {
                TM_ILI9341_Puts(300-11-5, selectedOpt->getY(), ">", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
            }

            TM_ILI9341_Puts(160-(strlen(opt[option])/2)*11, selectedOpt->getY(), (char *) opt[option], &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);

        }

        delay(50);

        Menu::enterPressed = false;
        Menu::turns = 0;

        globalIntBuffer[saveToIndex] = option;

        TM_ILI9341_DrawFilledRectangle(selectedOpt->getX()-3, selectedOpt->getY()-3, 300, selectedOpt->getY() + 22, ILI9341_COLOR_BLACK);
        sprintf(text_buffer, "%s %s", menuText, opt[option]);
        selectedOpt->setText(text_buffer);
        selectedOpt->setSelected(true);

        Menu::resignInputControl = false;
        Menu::resignedController = NULL;

    };
}

void drawRemoteButtons()
{
    //
    // Draws remote control settings menu
    //

    pressedMenuOptionsStack.pop_back();
    Menu::displayLoading();
    Menu::clearRightMenu();
    std::string title = "Button settings";
    TM_ILI9341_Puts(320-title.length()*11, 0, (char *)title.c_str(), &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    Menu::resignInputControl = true;
    Menu::resignedController = [&]
    {
        int numOpt = remoteButtons.size() + 1;

        int option = -1;
        Menu::turns = 0;

        bool shouldDelete = false;

        while (true)
        {
            Menu::calculateTurns();
            delay(70);
            Menu::calculateNiceTurns();

            if (Menu::enterPressed)
            {
                shouldDelete = true;
                if (option == remoteButtons.size())
                    shouldDelete = false;

                break;
            }

            if (Menu::turns > numOpt-1)
                Menu::turns = numOpt-1;
            else if (Menu::turns < 0)
                Menu::turns = 0;

            if (Menu::turns == option)
                continue;

            option = Menu::turns;


            TM_ILI9341_DrawFilledRectangle(165, 40, 320, 75, ILI9341_COLOR_BLACK);
            TM_ILI9341_DrawFilledRectangle(165, 100, 320, 160, ILI9341_COLOR_BLACK);
            if (option == remoteButtons.size())
            {
                TM_ILI9341_Puts(237-(2)*11, 100, "EXIT", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

                TM_ILI9341_Puts(153, 190, "               ", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            }
            else
            {

                sprintf(text_buffer, "Button", option);
                TM_ILI9341_Puts(237-(strlen(text_buffer)/2)*11, 40, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

                sprintf(text_buffer, "Assigned to:", option);
                TM_ILI9341_Puts(237-(strlen(text_buffer)/2)*11, 120, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

                TM_ILI9341_Puts(237-(strlen(remoteButtons[option]->getEventTypeName())/2)*11, 145, (char *)remoteButtons[option]->getEventTypeName(), &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

                sprintf(text_buffer, "%#X", remoteButtons[option]->remoteButton);
                TM_ILI9341_Puts(237-(strlen(text_buffer)/2)*11, 60, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

                TM_ILI9341_Puts(153, 190, "Press to delete", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            }

            TM_ILI9341_Puts(165, 100, " ", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            TM_ILI9341_Puts(320-11, 100, " ", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            if (option > 0)
            {
                TM_ILI9341_Puts(165, 100, "<", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            }

            if (option < numOpt-1)
            {
                TM_ILI9341_Puts(320-11, 100, ">", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            }

        }

        delay(50);

        if (shouldDelete)
        {
            remoteButtons.erase(remoteButtons.begin() + option);
            save_data_to_flash();
        }

        Menu::enterPressed = false;
        Menu::turns = 0;

        Menu::resignInputControl = false;
        Menu::resignedController = NULL;

        Menu::clearRightMenu();

        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            mainMenuOptions[i]->setNeedsUpdate();
            mainMenuOptions[i]->draw();
        }
    };
}


void drawAddNewBlind()
{
    //
    // Draws a settings menu for adding new blinds
    //

    Menu::clearPopup();

    globalIntBuffer[0] = 0;
    globalIntBuffer[1] = 0;
    globalIntBuffer[2] = 1500;
    globalIntBuffer[3] = 1500;
    globalIntBuffer[4] = 1500;
    mini_text_buffer1[0] = '\0';
    mini_text_buffer2[0] = '\0';

    Blind *bld = new Blind();
    bld->setType((BlindType) globalIntBuffer[0]);
    bld->setChannel((BlindChannel) globalIntBuffer[1]);
    bld->setBounds(1000, 1500, 2000);

    std::vector<MenuOption *> options;

    MenuOption *type = new MenuOption(20, 30, "Type: Local    ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    type->setOnClickListener([&, type, bld]
    {
        bld->setType((BlindType) globalIntBuffer[0]);
        bld->setChannel((BlindChannel) globalIntBuffer[1]);
        bld->btCode = atoi(mini_text_buffer1);

        displayDropDown(allBlindTypes, kAllBlindTypes, type, "Type:", 0);
    });
    options.push_back(type);

    MenuOption *devaddr1 = new MenuOption(20, 60, "Local channel: 1", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devaddr1->setOnClickListener([&, devaddr1, bld]
    {
        bld->setType((BlindType) globalIntBuffer[0]);
        bld->setChannel((BlindChannel) globalIntBuffer[1]);
        bld->btCode = atoi(mini_text_buffer1);

        displayDropDown(allBlindChannels, kAllBlindChannels, devaddr1, "Local channel:", 1);
    });
    options.push_back(devaddr1);
/*
    MenuOption *devaddrbt = new MenuOption(20, 90, "Bt. code:          ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devaddrbt->setOnClickListener([&, devaddrbt]
    {
        mini_text_buffer1[0] = '\0';
        keyboardPopup();
        strncpy(mini_text_buffer1, text_buffer, 11);
        mini_text_buffer1[10] = '\0';

        sprintf(text_buffer, "Bt. code: %s", mini_text_buffer1);
        devaddrbt->setText(text_buffer);
    });
    options.push_back(devaddrbt);
*/
    MenuOption *devname = new MenuOption(20, 120, "Name:          ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devname->setOnClickListener([&, devname]
    {
        mini_text_buffer2[0] = '\0';
        keyboardPopup();
        strncpy(mini_text_buffer2, text_buffer, 11);
        mini_text_buffer2[10] = '\0';

        sprintf(text_buffer, "Name: %s", mini_text_buffer2);
        devname->setText(text_buffer);
    });
    options.push_back(devname);

    TM_ILI9341_Puts(20, 150, "Min:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    MenuOption *bound1 = new MenuOption(66, 150, "150", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bound1->setOnClickListener([&, bound1, bld]
    {
        bld->setType((BlindType) globalIntBuffer[0]);
        bld->setChannel((BlindChannel) globalIntBuffer[1]);
        bld->btCode = atoi(mini_text_buffer1);

        numberInput(bound1, 1000, 2000, 2, bld);
    });
    options.push_back(bound1);

    TM_ILI9341_Puts(122, 150, "Mid:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    MenuOption *bound2 = new MenuOption(168, 150, "150", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bound2->setOnClickListener([&, bound2, bld]
    {
        bld->setType((BlindType) globalIntBuffer[0]);
        bld->setChannel((BlindChannel) globalIntBuffer[1]);
        bld->btCode = atoi(mini_text_buffer1);

        numberInput(bound2, 1000, 2000, 3, bld);
    });
    options.push_back(bound2);

    TM_ILI9341_Puts(224, 150, "Max:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    MenuOption *bound3 = new MenuOption(270, 150, "150", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bound3->setOnClickListener([&, bound3, bld]
    {
        bld->setType((BlindType) globalIntBuffer[0]);
        bld->setChannel((BlindChannel) globalIntBuffer[1]);
        bld->btCode = atoi(mini_text_buffer1);

        numberInput(bound3, 1000, 2000, 4, bld);
    });
    options.push_back(bound3);

    MenuOption* done = new MenuOption(20, 240-35, "Done", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    done->setOnClickListener([&, bld]
    {
        delete bld;

        Blind *saveBlind = new Blind();
        saveBlind->setType((BlindType) globalIntBuffer[0]);
        saveBlind->setChannel((BlindChannel) globalIntBuffer[1]);
        saveBlind->btCode = 0;
        saveBlind->setName(mini_text_buffer2);

        saveBlind->setBounds(globalIntBuffer[2], globalIntBuffer[3], globalIntBuffer[4]);
        saveBlind->enableOutput(true);
        saveBlind->setPosition(globalIntBuffer[3]);
        delay(2000);
        saveBlind->enableOutput(false);

        blinds.push_back(saveBlind);

        save_data_to_flash();

        Menu::clearMenu();
        for (int i=0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        backMenuButtonHandler();
    });
    options.push_back(done);

    MenuOption* bck = new MenuOption(100, 240-35, "Cancel", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener([&, bld]
    {
        delete bld;

        Menu::clearMenu();
        for (int i=0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        backMenuButtonHandler();
    });
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void drawBlindSettingsPopup()
{
    //
    // Draws a blind setting pop-up menu
    // Used to delete a blind or assign a remote button
    //

    std::vector<MenuOption *> options;

    int selIndex = Menu::actionIndex;

    Blind *bld = blinds[selIndex];
    MenuOption* option = mainMenuOptions[selIndex];

    int menuStartx = mainMenuOptions[selIndex]->getX()-3;
    int menuStarty = 35;
    int menuEndx = 320;
    int menuEndy = 35 + 25*3;

    TM_ILI9341_DrawFilledRectangle(menuStartx, menuStarty, menuEndx, menuEndy, ILI9341_COLOR_BLUE2);

    auto button_click = [&, bld, option, menuStartx, menuStarty, menuEndx, menuEndy, selIndex] ()
    {
        if (Menu::positionSelected == 0)
        {
            blinds.erase(blinds.begin() + selIndex);
            //delete lght;
            delete menuStack.back()[selIndex];
            menuStack.back().erase(menuStack.back().begin() + selIndex);

            save_data_to_flash();
        }
        else if (Menu::positionSelected == 1)
        {
            RemoteButton *bt = new RemoteButton();
            bt->eventType = TYPE_BLIND_TOGGLE;
            bt->eventHash = bld->calculateHash();

            RemoteButton::shouldRunActions = false;

            TM_ILI9341_DrawFilledRectangle(menuStartx, menuStarty, menuEndx, menuEndy + 25, ILI9341_COLOR_BLUE2);
            TM_ILI9341_Puts(menuStartx + 5, menuStarty+3+25, "Press button", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
            TM_ILI9341_Puts(menuStartx + 5, menuStarty+3+25+25, "to assign...", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);

            while (!RemoteButton::remoteCodePressed) {};
            bt->remoteButton = RemoteButton::remoteCodePressed;

            RemoteButton::remoteCodePressed = 0;
            RemoteButton::shouldRunActions = true;

            remoteButtons.push_back(bt);
            save_data_to_flash();
        }


        backMenuButtonHandler();
    };

    MenuOption* delOp = new MenuOption(menuStartx + 5, menuStarty+3, "Delete", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    delOp->selectionColor = ILI9341_COLOR_BLACK;
    delOp->setSelected(true);
    delOp->setOnClickListener(button_click);
    options.push_back(delOp);

    MenuOption* assOp = new MenuOption(menuStartx + 5, menuStarty+3+25, "Assign Toggle", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    assOp->selectionColor = ILI9341_COLOR_BLACK;
    assOp->setOnClickListener(button_click);
    options.push_back(assOp);

    MenuOption* bck = new MenuOption(menuStartx + 5, menuStarty+3+25+25, "Close", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    bck->selectionColor = ILI9341_COLOR_BLACK;
    bck->setOnClickListener(button_click);
    options.push_back(bck);

    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void drawBlindSettings()
{
    //
    // Draws the main blind settings menu
    //

    Menu::displayLoading();
    Menu::clearRightMenu();

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    for (int i = 0; i < blinds.size(); i++)
    {
        Blind *bld = blinds[i];
        MenuOption* op = new MenuOption(165, 38+29*i, bld->getName(), ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

        op->setOnClickListener(drawBlindSettingsPopup);
        op->selectionId = i;

        options.push_back(op);
    }

    if (blinds.size() < kMAX_BLINDS)
    {
        TM_ILI9341_DrawLine(160, 185, 305, 185, ILI9341_COLOR_WHITE);
        MenuOption* addNew = new MenuOption(190, 195, "Add new", ILI9341_COLOR_BLUE2, ILI9341_COLOR_BLACK);
        addNew->setOnClickListener(drawAddNewBlind);
        options.push_back(addNew);
    }

    MenuOption* bck = new MenuOption(135, 240-20, "< Back", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener(backMenuButtonHandler);
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;

    Menu::clearTitle();
    std::string title = "Blind settings";
    TM_ILI9341_Puts(320-title.length()*11, 0, (char *)title.c_str(), &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
}

void drawAddNewLight()
{
    //
    // Draws a menu for adding new lights
    //

    Menu::clearPopup();
    //TM_ILI9341_Puts(20, 25, "Enter light details:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    std::vector<MenuOption *> options;

    MenuOption *type = new MenuOption(20, 30, "Type: KakuSwitch     ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    type->setOnClickListener([&, type]
    {
        globalIntBuffer[0] = 0;
        displayDropDown(allLightTypes, kAllLightTypes, type, "Type:", 0);
    });
    options.push_back(type);

    MenuOption *devaddr1 = new MenuOption(20, 60, "Address 1: A ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devaddr1->setOnClickListener([&, devaddr1]
    {
        globalIntBuffer[1] = 0;
        displayDropDown(lightAddresses1, kLightAddresses1, devaddr1, "Address 1:", 1);
    });
    options.push_back(devaddr1);


    MenuOption *devaddr2 = new MenuOption(20, 90, "Address 2: 1 ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devaddr2->setOnClickListener([&, devaddr2]
    {
        globalIntBuffer[2] = 0;
        displayDropDown(lightAddresses2, kLightAddresses2, devaddr2, "Address 2:", 2);
    });
    options.push_back(devaddr2);
/*
    MenuOption *devaddrbt = new MenuOption(20, 120, "Bt. code:            ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devaddrbt->setOnClickListener([&, devaddrbt]
    {
        mini_text_buffer1[0] = '\0';
        keyboardPopup();
        strncpy(mini_text_buffer1, text_buffer, 11);
        mini_text_buffer1[10] = '\0';

        sprintf(text_buffer, "Bt. code: %s", mini_text_buffer1);
        devaddrbt->setText(text_buffer);
    });
    options.push_back(devaddrbt);
*/
    MenuOption *devname = new MenuOption(20, 150, "Name:            ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devname->setOnClickListener([&, devname]
    {
        mini_text_buffer2[0] = '\0';
        keyboardPopup();
        strncpy(mini_text_buffer2, text_buffer, 11);
        mini_text_buffer2[10] = '\0';

        sprintf(text_buffer, "Name: %s", mini_text_buffer2);
        devname->setText(text_buffer);
    });
    options.push_back(devname);

    MenuOption* done = new MenuOption(20, 240-35, "Done", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    done->setOnClickListener([&]
    {
        Light* newLight = new Light();
        newLight->setName(mini_text_buffer2);

        if (globalIntBuffer[0] == RF_KAKU)
        {
            newLight->setTypeKaku(lightAddresses1[globalIntBuffer[1]][0], atoi(lightAddresses2[globalIntBuffer[2]]));
        }
        else if (globalIntBuffer[0] == BLUETOOTH)
        {
            newLight->btCode = atoi(mini_text_buffer1);
        }

        lights.push_back(newLight);

        save_data_to_flash();

        Menu::clearMenu();
        for (int i=0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        backMenuButtonHandler();
    });
    options.push_back(done);

    MenuOption* bck = new MenuOption(100, 240-35, "Cancel", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener([]
    {
        Menu::clearMenu();
        for (int i=0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        backMenuButtonHandler();
    });
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void drawLightSettingsPopup()
{
    //
    // Draws a light pop-up menu
    // Enables the user to delete lights or set remote buttons
    //

    std::vector<MenuOption *> options;

    int selIndex = Menu::actionIndex;

    Light *lght = lights[selIndex];
    MenuOption* option = mainMenuOptions[selIndex];

    int menuStartx = mainMenuOptions[selIndex]->getX()-3;
    int menuStarty = 35;
    int menuEndx = 320;
    int menuEndy = 35 + 25*5;

    TM_ILI9341_DrawFilledRectangle(menuStartx, menuStarty, menuEndx, menuEndy, ILI9341_COLOR_BLUE2);

    auto button_click = [&, lght, option, menuStartx, menuStarty, menuEndx, menuEndy, selIndex] ()
    {
        if (Menu::positionSelected == 0)
        {
            lights.erase(lights.begin() + selIndex);
            //delete lght;
            delete menuStack.back()[selIndex];
            menuStack.back().erase(menuStack.back().begin() + selIndex);

            save_data_to_flash();
        }
        else if (Menu::positionSelected == 1 || Menu::positionSelected == 2 || Menu::positionSelected == 3)
        {
            RemoteButton *bt = new RemoteButton();
            switch (Menu::positionSelected)
            {
                case 1: bt->eventType = TYPE_LIGHT_ON; break;
                case 2: bt->eventType = TYPE_LIGHT_OFF; break;
                case 3: bt->eventType = TYPE_LIGHT_TOGGLE; break;
            }
            bt->eventHash = lght->calculateHash();

            RemoteButton::shouldRunActions = false;

            TM_ILI9341_DrawFilledRectangle(menuStartx, menuStarty, menuEndx, menuEndy, ILI9341_COLOR_BLUE2);
            TM_ILI9341_Puts(menuStartx + 5, menuStarty+3+25, "Press button", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
            TM_ILI9341_Puts(menuStartx + 5, menuStarty+3+25+25, "to assign...", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);

            while (!RemoteButton::remoteCodePressed) {};
            bt->remoteButton = RemoteButton::remoteCodePressed;

            RemoteButton::remoteCodePressed = 0;
            RemoteButton::shouldRunActions = true;

            remoteButtons.push_back(bt);
            save_data_to_flash();
        }

        backMenuButtonHandler();
    };

    MenuOption* onsw = new MenuOption(menuStartx + 5, menuStarty+3, "Delete", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    onsw->selectionColor = ILI9341_COLOR_BLACK;
    onsw->setSelected(true);
    onsw->setOnClickListener(button_click);
    options.push_back(onsw);

    MenuOption* offsw = new MenuOption(menuStartx + 5, menuStarty+3+25, "Assign On", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    offsw->selectionColor = ILI9341_COLOR_BLACK;
    offsw->setOnClickListener(button_click);
    options.push_back(offsw);

    MenuOption* assOff = new MenuOption(menuStartx + 5, menuStarty+3+25+25, "Assign Off", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    assOff->selectionColor = ILI9341_COLOR_BLACK;
    assOff->setOnClickListener(button_click);
    options.push_back(assOff);

    MenuOption* assTog = new MenuOption(menuStartx + 5, menuStarty+3+25+25+25, "Assign Toggle", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    assTog->selectionColor = ILI9341_COLOR_BLACK;
    assTog->setOnClickListener(button_click);
    options.push_back(assTog);

    MenuOption* bck = new MenuOption(menuStartx + 5, menuStarty+3+25+25+25+25, "Close", ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);
    bck->selectionColor = ILI9341_COLOR_BLACK;
    bck->setOnClickListener(button_click);
    options.push_back(bck);

    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void drawLightSettings()
{
    //
    // Draws the main light settings menu
    //

    Menu::displayLoading();
    Menu::clearRightMenu();

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    for (int i = 0; i < lights.size(); i++)
    {
        Light *lgh = lights[i];
        MenuOption* op = new MenuOption(165, 38+29*i, lgh->getName(), ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

        op->setOnClickListener(drawLightSettingsPopup);
        op->selectionId = i;

        options.push_back(op);
    }

    if (lights.size() < kMAX_LIGHTS)
    {
        TM_ILI9341_DrawLine(160, 185, 305, 185, ILI9341_COLOR_WHITE);
        MenuOption* addNew = new MenuOption(190, 195, "Add new", ILI9341_COLOR_BLUE2, ILI9341_COLOR_BLACK);
        addNew->setOnClickListener(drawAddNewLight);
        options.push_back(addNew);
    }

    MenuOption* bck = new MenuOption(135, 240-20, "< Back", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener(backMenuButtonHandler);
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;

    Menu::clearTitle();
    std::string title = "Light settings";
    TM_ILI9341_Puts(320-title.length()*11, 0, (char *)title.c_str(), &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
}

void drawConnectToWifiPopup()
{
    //
    // Draws a wifi connection pop-up menu
    //

    Menu::clearPopup();

    std::vector<MenuOption *> options;

    MenuOption *ssid = new MenuOption(20, 30, "SSID:               ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    ssid->setOnClickListener([&, ssid]
    {
        mini_text_buffer1[0] = '\0';
        keyboardPopup(true, true, 30);
        strncpy(mini_text_buffer1, text_buffer, 30);
        mini_text_buffer1[30] = '\0';

        sprintf(text_buffer, "SSID: %s", mini_text_buffer1);
        ssid->setText(text_buffer);
    });
    options.push_back(ssid);

    MenuOption *devaddr1 = new MenuOption(20, 60, "Security: None           ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devaddr1->setOnClickListener([&, devaddr1]
    {
        globalIntBuffer[0] = 0;
        displayDropDown(wifiEncryptionTypes, kWifiEncryptionTypes, devaddr1, "Security:", 0);
    });
    options.push_back(devaddr1);


    MenuOption *devaddrbt = new MenuOption(20, 90, "Pass:               ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devaddrbt->setOnClickListener([&, devaddrbt]
    {
        mini_text_buffer2[0] = '\0';
        keyboardPopup(true, true, 30);
        strncpy(mini_text_buffer2, text_buffer, 30);
        mini_text_buffer2[30] = '\0';

        sprintf(text_buffer, "Pass: %s", mini_text_buffer2);
        devaddrbt->setText(text_buffer);
    });
    options.push_back(devaddrbt);


    MenuOption* done = new MenuOption(20, 240-35, "Done", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    done->setOnClickListener([&]
    {
        wifiConnectTo(mini_text_buffer1, mini_text_buffer2, globalIntBuffer[0]);

        Menu::clearMenu();
        for (int i=0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        backMenuButtonHandler();
    });
    options.push_back(done);

    MenuOption* bck = new MenuOption(100, 240-35, "Cancel", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener([]
    {
        Menu::clearMenu();
        for (int i=0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        backMenuButtonHandler();
    });
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void drawWifiStatusMenu()
{
    //
    // Draws a wifi connection status menu
    // (displays current IP and server port)
    //

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    Menu::clearRightMenu();
    Menu::displayLoading();

    MenuOption* bck = new MenuOption(135, 240-20, "< Back", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener(backMenuButtonHandler);
    options.push_back(bck);

    GPIO_ResetBits(GPIOA, GPIO_Pin_8);
    delay(500);
    GPIO_SetBits(GPIOA, GPIO_Pin_8);

    clearTextBuffer();
    clearWifiUsart1Buffer();

    sendWifiUsart1("at+net_wanip=?\r\n");
    delay(2000);
    eth1_busy = 1;
    sscanf((char *)eth1_buff, "%s\r\n%[^,],%[^,],%[^,]", text_buffer, mini_text_buffer1, mini_text_buffer2, mini_text_buffer3);

    TM_ILI9341_Puts(160, 38+25*0, "IP:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    sprintf(text_buffer, "%s", mini_text_buffer1);
    TM_ILI9341_Puts(160, 38+25*1, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    TM_ILI9341_Puts(160, 38+25*2, "Gateway:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    sprintf(text_buffer, "%s", mini_text_buffer3);
    TM_ILI9341_Puts(160, 38+25*3, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    eth1_busy = 0;

    clearTextBuffer();
    clearWifiUsart1Buffer();

    sendWifiUsart1("at+remoteport=?\r\n");
    delay(2000);
    eth1_busy = 1;

    sscanf((char *)eth1_buff, "%s\r\n%[^,]", text_buffer, mini_text_buffer1);

    TM_ILI9341_Puts(160, 38+25*5, "Server port:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    sprintf(text_buffer, "%s", mini_text_buffer1);
    TM_ILI9341_Puts(160, 38+25*6, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    eth1_busy = 0;

    sendWifiUsart1("at+out_trans=0\r\n");

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;

    Menu::clearTitle();
    std::string title = "Wifi Status";
    TM_ILI9341_Puts(320-title.length()*11, 0, (char *)title.c_str(), &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

}

void drawServerSettingsMenu()
{
    //
    // Draws server settings menu
    // (username, password, port)
    //

    Menu::clearPopup();

    std::vector<MenuOption *> options;

    sprintf(text_buffer, "Username: %s          ", web_user);
    MenuOption *user = new MenuOption(20, 30, text_buffer, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    user->setOnClickListener([&, user]
    {
        mini_text_buffer1[0] = '\0';
        keyboardPopup(true, true, 14);
        strncpy(mini_text_buffer1, text_buffer, 14);
        mini_text_buffer1[14] = '\0';

        sprintf(text_buffer, "Username: %s", mini_text_buffer1);
        user->setText(text_buffer);
    });
    options.push_back(user);

    sprintf(text_buffer, "Password: %s          ", web_pass);
    MenuOption *pass = new MenuOption(20, 60, text_buffer, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    pass->setOnClickListener([&, pass]
    {
        mini_text_buffer2[0] = '\0';
        keyboardPopup(true, true, 14);
        strncpy(mini_text_buffer2, text_buffer, 14);
        mini_text_buffer2[14] = '\0';

        sprintf(text_buffer, "Password: %s", mini_text_buffer2);
        pass->setText(text_buffer);
    });
    options.push_back(pass);


    sprintf(text_buffer, "Port: %d          ", webPort);
    MenuOption *port = new MenuOption(20, 90, text_buffer, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    port->setOnClickListener([&, port]
    {
        mini_text_buffer3[0] = '\0';
        keyboardPopup(true, true, 14);
        strncpy(mini_text_buffer3, text_buffer, 14);
        mini_text_buffer3[14] = '\0';

        sprintf(text_buffer, "Port: %s", mini_text_buffer3);
        port->setText(text_buffer);
    });
    options.push_back(port);


    MenuOption* done = new MenuOption(20, 240-35, "Done", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    done->setOnClickListener([&]
    {
        strncpy(web_user, mini_text_buffer1, 15);
        strncpy(web_pass, mini_text_buffer2, 15);
        int newport = atoi(mini_text_buffer3);
        webPort = (newport > 0 && newport < 65535)? newport : 8080;
        setWifiServerPort(webPort);

        save_data_to_flash();

        Menu::clearMenu();
        for (int i=0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        backMenuButtonHandler();
    });
    options.push_back(done);

    MenuOption* bck = new MenuOption(100, 240-35, "Cancel", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener([]
    {
        Menu::clearMenu();
        for (int i=0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        backMenuButtonHandler();
    });
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void displayWifiSettingsMenu()
{
    //
    // Displays the main Wifi settings menu
    //

    Menu::displayLoading();
    Menu::clearRightMenu();

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    MenuOption* op1 = new MenuOption(165, 38+25*0, "Connect", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op1->setOnClickListener(drawConnectToWifiPopup);
    options.push_back(op1);

    MenuOption* op2 = new MenuOption(165, 38+25*1, "Status", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op2->setOnClickListener(drawWifiStatusMenu);
    options.push_back(op2);

    MenuOption* op4 = new MenuOption(165, 38+25*2, "Server setup", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op4->setOnClickListener(drawServerSettingsMenu);
    options.push_back(op4);

    MenuOption* op3 = new MenuOption(165, 38+25*3, "Wifi reset", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op3->setOnClickListener([] {
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        delay(500);
        GPIO_SetBits(GPIOA, GPIO_Pin_8);
        sendWifiUsart1("at+default=1\r\n");
        sendWifiUsart1("at+reboot=1\r\n");
        Menu::clearRightMenu();
        pressedMenuOptionsStack.pop_back();
        TM_ILI9341_Puts(160, 38+25*0, "Resetting wifi", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
        TM_ILI9341_Puts(160, 38+20*2, "Wait 40s...", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

        delay(40000);

        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        delay(500);
        GPIO_SetBits(GPIOA, GPIO_Pin_8);

        setWifiBasicConfig();
        sendWifiUsart1("at+out_trans=0\r\n");

        Menu::clearRightMenu();
        TM_ILI9341_Puts(160, 38+25*0, "Reset complete", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
        TM_ILI9341_Puts(160, 38+20*2, "Setup your wifi", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
        TM_ILI9341_Puts(160, 38+20*3, "details (ssid, pass)", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
        TM_ILI9341_Puts(160, 38+20*5, "Or connect to", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
        TM_ILI9341_Puts(160, 38+20*6, "HI-LINK_X access point", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
        TM_ILI9341_Puts(160, 38+20*7, "& open 192.168.16.254", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
        TM_ILI9341_Puts(160, 38+20*8, "on your computer", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
        TM_ILI9341_Puts(160, 38+20*9, "for manual setup.", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

        delay(15000);

        Menu::clearRightMenu();
        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            mainMenuOptions[i]->setNeedsUpdate();
            mainMenuOptions[i]->draw();
        }
    });
    options.push_back(op3);

    MenuOption* bck = new MenuOption(135, 240-20, "< Back", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener(backMenuButtonHandler);
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;

    Menu::clearTitle();
    std::string title = "Wifi Settings";
    TM_ILI9341_Puts(320-title.length()*11, 0, (char *)title.c_str(), &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
}

void setAutoBlinds()
{
    //
    // Sets autoblind alarms at specified time
    //

    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_ClearITPendingBit(EXTI_Line17);
    EXTI_InitStructure.EXTI_Line = EXTI_Line17;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* Enable the RTC Alarm Interrupt */
    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = RTC_Alarm_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    PWR_BackupAccessCmd(ENABLE);
    RTC_WriteProtectionCmd(DISABLE);

    RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
    RTC_AlarmCmd(RTC_Alarm_B, DISABLE);

    //Auto Open Blind
    {
        RTC_AlarmTypeDef alrm;
        alrm.RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_WeekDay;
        alrm.RTC_AlarmMask = RTC_AlarmMask_DateWeekDay;
        alrm.RTC_AlarmTime.RTC_Hours = automaticBlindOpenHour;
        alrm.RTC_AlarmTime.RTC_Minutes = automaticBlindOpenMinute;
        alrm.RTC_AlarmTime.RTC_Seconds = 0;

        RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_A, &alrm);
        RTC_AlarmCmd(RTC_Alarm_A, ENABLE);
    }

    //Auto Close Blind
    {
        RTC_AlarmTypeDef alrm2;
        alrm2.RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_WeekDay;
        alrm2.RTC_AlarmMask = RTC_AlarmMask_DateWeekDay;
        alrm2.RTC_AlarmTime.RTC_Hours = automaticBlindCloseHour;
        alrm2.RTC_AlarmTime.RTC_Minutes = automaticBlindCloseMinute;
        alrm2.RTC_AlarmTime.RTC_Seconds = 0;

        RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_B, &alrm2);
        RTC_AlarmCmd(RTC_Alarm_B, ENABLE);
    }

    /* Enable Alarm interrupt */
    RTC_ITConfig(RTC_IT_ALRA, ENABLE);
    RTC_ITConfig(RTC_IT_ALRB, ENABLE);

    RTC_WriteProtectionCmd(ENABLE);

}

void drawActionSettingsPopup()
{
    //
    // Shows the main action settings menu
    //

    pressedMenuOptionsStack.pop_back();

    Menu::clearPopup();
    TM_ILI9341_Puts(20, 30, "Blind auto open:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    TM_ILI9341_Puts(20, 90, "Blind auto close:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    //TM_ILI9341_Puts(20, 150, "Setting auto blinds requires restart", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    globalIntBuffer[0] = 0;
    MenuOption* oph = new MenuOption(40, 60, "00", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto oph_onclick = [&,oph] ()
    {
        numberInput(oph, 0, 23, 0);
    };
    oph->setOnClickListener(oph_onclick);
    oph->selectionId = 0;
    options.push_back(oph);

    TM_ILI9341_Puts(67, 60, ":", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    globalIntBuffer[1] = 0;
    MenuOption* opm = new MenuOption(83, 60, "00", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto opm_onclick = [&,opm] ()
    {
        numberInput(opm, 0, 59, 1);
    };
    opm->setOnClickListener(opm_onclick);
    opm->selectionId = 1;
    options.push_back(opm);

    MenuOption *devaddr1 = new MenuOption(150, 60, " Min ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devaddr1->setOnClickListener([&, devaddr1]
    {
        globalIntBuffer[4] = 0;
        displayDropDown(autoBlindPositions, kAutoBLindPositions, devaddr1, "", 4);
    });
    options.push_back(devaddr1);


    globalIntBuffer[2] = 0;
    MenuOption* opd = new MenuOption(40, 120, "00", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto opd_onclick = [&,opd] ()
    {
        numberInput(opd, 0, 23, 2);
    };
    opd->setOnClickListener(opd_onclick);
    opd->selectionId = 2;
    options.push_back(opd);

    TM_ILI9341_Puts(67, 120, ":", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    globalIntBuffer[3] = 0;
    MenuOption* opmm = new MenuOption(83, 120, "00", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto opmm_onclick = [&,opmm] ()
    {
        numberInput(opmm, 0, 59, 3);
    };
    opmm->setOnClickListener(opmm_onclick);
    opmm->selectionId = 3;
    options.push_back(opmm);

    MenuOption *devaddr2 = new MenuOption(150, 120, " Min ", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    devaddr2->setOnClickListener([&, devaddr2]
    {
        globalIntBuffer[5] = 0;
        displayDropDown(autoBlindPositions, kAutoBLindPositions, devaddr2, "", 5);
    });
    options.push_back(devaddr2);

    MenuOption *sleepAssign = new MenuOption(20, 170, "Assign sleep button", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    sleepAssign->setOnClickListener([&, sleepAssign]
    {
        RemoteButton *bt = new RemoteButton();
        bt->eventType = TYPE_ACTION;
        bt->eventHash = 0;

        RemoteButton::shouldRunActions = false;

        TM_ILI9341_DrawFilledRectangle(17, 167, 300, 191, ILI9341_COLOR_BLUE2);
        TM_ILI9341_Puts(20, 170, "Press button to assign...", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_BLUE2);

        while (!RemoteButton::remoteCodePressed) {};
        bt->remoteButton = RemoteButton::remoteCodePressed;

        RemoteButton::remoteCodePressed = 0;
        RemoteButton::shouldRunActions = true;

        remoteButtons.push_back(bt);
        save_data_to_flash();

        TM_ILI9341_DrawFilledRectangle(17, 167, 300, 191, ILI9341_COLOR_BLACK);
        sleepAssign->setNeedsUpdate();
        sleepAssign->draw();
    });
    options.push_back(sleepAssign);


    MenuOption* bck = new MenuOption(20, 240-35, "Done", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener([] ()
    {
        autoCloseBlindEnabled = true;
        autoOpenBlindEnabled = true;
        automaticBlindOpenHour = globalIntBuffer[0];
        automaticBlindOpenMinute = globalIntBuffer[1];
        automaticBlindCloseHour = globalIntBuffer[2];
        automaticBlindCloseMinute = globalIntBuffer[3];
        automaticBlindOpenPosition = globalIntBuffer[4];
        automaticBlindClosePosition = globalIntBuffer[5];

        save_data_to_flash();
        setAutoBlinds();

        //NVIC_SystemReset();

        pressedMenuOptionsStack.pop_back();

        Menu::clearMenu();
        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            delete mainMenuOptions[i];
        }
        mainMenuOptions = menuStack.back();
        menuStack.pop_back();
        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            mainMenuOptions[i]->setNeedsUpdate();
            mainMenuOptions[i]->draw();
        }
        for (int i = 0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        mainMenuOptions.front()->setSelected(true);
        Menu::positionSelected = 0;

    });
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void drawAdjustTempPopup()
{
    //
    // Shows the menu where you can adjust the temperature sensor
    //

    pressedMenuOptionsStack.pop_back();

    Menu::clearPopup();

    int temp = tempSensor.getTemp(false);
    globalIntBuffer[0] = 0;
    globalIntBuffer[1] = temp;

    sprintf(mini_text_buffer1, "Current temperature: %d", temp);
    TM_ILI9341_Puts(20, 30, mini_text_buffer1, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    TM_ILI9341_Puts(20, 90, "Real temperature:", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

    globalIntBuffer[0] = 0;
    MenuOption* oph = new MenuOption(229, 90, "00", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    auto oph_onclick = [&,oph] ()
    {
        numberInput(oph, 0, 35, 0);
    };
    oph->setOnClickListener(oph_onclick);
    oph->selectionId = 0;
    options.push_back(oph);

    MenuOption* bck = new MenuOption(20, 240-35, "Done", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener([] ()
    {
        tempAdjust = globalIntBuffer[1] - globalIntBuffer[0];
        tempSensor.calibration = tempAdjust;

        save_data_to_flash();

        pressedMenuOptionsStack.pop_back();

        Menu::clearMenu();
        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            delete mainMenuOptions[i];
        }
        mainMenuOptions = menuStack.back();
        menuStack.pop_back();
        for (int i = 0; i < mainMenuOptions.size(); i++)
        {
            mainMenuOptions[i]->setNeedsUpdate();
            mainMenuOptions[i]->draw();
        }
        for (int i = 0; i < menuStack.front().size(); i++)
        {
            menuStack.front()[i]->setNeedsUpdate();
            menuStack.front()[i]->draw();
        }
        mainMenuOptions.front()->setSelected(true);
        Menu::positionSelected = 0;

    });
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;
}

void displaySettingsMenu()
{
    //
    // Displays the main settings screen
    //

    Menu::displayLoading();
    Menu::clearRightMenu();

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    MenuOption* op = new MenuOption(165, 38+25*0, "Date & Time", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op->setOnClickListener(RTC_config);
    options.push_back(op);

    MenuOption* op2 = new MenuOption(165, 38+25*1, "Wifi", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op2->setOnClickListener(displayWifiSettingsMenu);
    options.push_back(op2);

    MenuOption* op3 = new MenuOption(165, 38+25*2, "Lights", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op3->setOnClickListener(drawLightSettings);
    options.push_back(op3);

    MenuOption* op4 = new MenuOption(165, 38+25*3, "Blinds", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op4->setOnClickListener(drawBlindSettings);
    options.push_back(op4);

    MenuOption* op6 = new MenuOption(165, 38+25*4, "Buttons", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op6->setOnClickListener(drawRemoteButtons);
    options.push_back(op6);

    MenuOption* op7 = new MenuOption(165, 38+25*5, "Actions", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op7->setOnClickListener(drawActionSettingsPopup);
    options.push_back(op7);

    MenuOption* op5 = new MenuOption(165, 38+25*6, "Temp adjust", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op5->setOnClickListener(drawAdjustTempPopup);
    options.push_back(op5);

    MenuOption* bck = new MenuOption(135, 240-20, "< Back", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener(backMenuButtonHandler);
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;

    Menu::clearTitle();
    std::string title = "Settings";
    TM_ILI9341_Puts(320-title.length()*11, 0, (char *)title.c_str(), &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
}

void displayActionMenu()
{
    //
    // Displays the quick actions menu
    //

    Menu::displayLoading();
    Menu::clearRightMenu();

    mainMenuOptions[Menu::positionSelected]->setSelected(false);

    std::vector<MenuOption *> options;

    MenuOption *op1 = new MenuOption(165, 38+35*0, "Sleep", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op1->setOnClickListener([&]
    {
        blueLedOn = true;
        blueLedOnTime = 50000;

        delayedActionTime = 30000;
        delayLightOff = true;

        backMenuButtonHandler();
        mainMenuOptions[0]->doOnClick();
    });
    options.push_back(op1);

    TM_ILI9341_Puts(165, 80, "AutoBlind:", &TM_Font_7x10, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    IndicatorMenuOption* op2 = new IndicatorMenuOption(165, 65+35*1, "Blind Open", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op2->setOn(autoOpenBlindEnabled);
    op2->setOnClickListener([&, op2]
    {
        if (autoOpenBlindEnabled)
            autoOpenBlindEnabled = false;
        else
            autoOpenBlindEnabled = true;

        save_data_to_flash();
        op2->setOn(autoOpenBlindEnabled);
    });
    options.push_back(op2);

    IndicatorMenuOption* op3 = new IndicatorMenuOption(165, 65+35*2, "Blind Close", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op3->setOn(autoCloseBlindEnabled);
    op3->setOnClickListener([&, op3]
    {
        if (autoCloseBlindEnabled)
            autoCloseBlindEnabled = false;
        else
            autoCloseBlindEnabled = true;

        save_data_to_flash();
        op3->setOn(autoCloseBlindEnabled);
    });
    options.push_back(op3);

    MenuOption* bck = new MenuOption(135, 240-20, "< Back", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    bck->setOnClickListener(backMenuButtonHandler);
    options.push_back(bck);

    options.front()->setSelected(true);
    Menu::positionSelected = 0;

    menuStack.push_back(mainMenuOptions);
    mainMenuOptions = options;

    Menu::clearTitle();
    std::string title = "Actions";
    Label lbl(320-title.length()*11, 0, title.c_str(), ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
}

void remoteEvent(uint32_t event_code)
{
    //
    // Handles remote button presses
    // (for all remotes)
    //

    if (RemoteButton::remoteCodePressed == 0)
    {
        Menu::lastInteractionTime = millis();
        RemoteButton::remoteCodePressed = event_code;
/*
        if (Menu::onInfoScreen)
        {
            Menu::clearNotification();
            TM_ILI9341_Puts(160, 203, "Remote", &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
            sprintf(text_buffer, "%#X", event_code);
            TM_ILI9341_Puts(160, 220, text_buffer, &TM_Font_11x18, ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);

            delay(2000);

            Menu::removeNotification();
        }*/
    }
}

void remoteEventRF(unsigned long code, unsigned int repeats)
{
    //
    // Handles remote button presses from the RF remote
    //

    remoteEvent(code);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------
// MAIN FUNCTION
// ------------------------------------------------------------------------------------------------------------------------------------------------------

int main(void)
{
    //
    // Main function
    // Handles basic setup and initialization
    //

    init_essentials_time();
    GPIO_LED_init();

    TM_ILI9341_Init();
    TM_ILI9341_Rotate(TM_ILI9341_Orientation_Landscape_1);

    TM_ILI9341_Puts(10, 10, "Starting...", &TM_Font_11x18, ILI9341_COLOR_BLACK, ILI9341_COLOR_WHITE);

    LCD_backlight_init();

    initRTC();

    //menuStack.reserve(3);
    //mainMenuOptions.reserve(12);
    //pressedMenuOptionsStack.reserve(6);

    blinds.reserve(4);
    lights.reserve(5);
    remoteButtons.reserve(50);

    loadFromFlash();

    setAutoBlinds();

    RemoteButton::shouldRunActions = true;

    RemoteReceiver::init(0,remoteEventRF);

    Blind::initLocalBlinds();

    Menu::initInputMethod();

    tempSensor.initTempSensor();
    tempSensor.calibration = tempAdjust;

    Menu::clearMenu();
    Menu::clearTitle();
    Menu::clearRightMenu();

    MenuOption* op1 = new MenuOption(13, 15, "Info", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op1->setOnClickListener(displayInfoScreen);
    op1->setSelected(true);
    op1->doOnClick();

    MenuOption* op2 = new MenuOption(30, 45, "Lights", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op2->setOnClickListener(displayLightMenu);

    MenuOption* op3 = new MenuOption(38, 75, "Blinds", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op3->setOnClickListener(displayBlindMenu);

    MenuOption* op4 = new MenuOption(42, 105, "Actions", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op4->setOnClickListener(displayActionMenu);

    MenuOption* op5 = new MenuOption(38, 135, "Settings", ILI9341_COLOR_WHITE, ILI9341_COLOR_BLACK);
    op5->setOnClickListener(displaySettingsMenu);

    mainMenuOptions.push_back(op1);
    mainMenuOptions.push_back(op2);
    mainMenuOptions.push_back(op3);
    mainMenuOptions.push_back(op4);
    mainMenuOptions.push_back(op5);

    initWifiES();
    init_USART1();
    init_USART2();


    //
    // Create all Free RTOS tasks
    //
    xTaskCreate(
        vTask1Function,                   /*Pinter to the function that implements task*/
        ( const signed char * ) "Task1",  /* Task name - for debugging only*/
        200,         /* Stack depth in words */
        ( void* ) NULL,                   /* Pointer to tasks arguments (parameter) */
        tskIDLE_PRIORITY + 1UL,           /* Task priority*/
        NULL                              /* Task handle */
    );

    xTaskCreate(
        vTask2Function,                   /* Pointer to the function that implements task*/
        ( const signed char * ) "Task2",  /* Task name - for debugging only*/
        200,         /* Stack depth in words */
        ( void* ) NULL,                   /* Pointer to tasks arguments (parameter) */
        tskIDLE_PRIORITY + 1UL,           /* Task priority*/
        NULL                              /* Task handle */
    );

    xTaskCreate(
        task3,                   /* Pointer to the function that implements task*/
        ( const signed char * ) "Task3",  /* Task name - for debugging only*/
        200,         /* Stack depth in words */
        ( void* ) NULL,                   /* Pointer to tasks arguments (parameter) */
        tskIDLE_PRIORITY + 1UL,           /* Task priority*/
        NULL                              /* Task handle */
    );

    xTaskCreate(
        menuCheckerTask,                   /* Pointer to the function that implements task*/
        ( const signed char * ) "Task4",  /* Task name - for debugging only*/
        600,         /* Stack depth in words */
        ( void* ) NULL,                   /* Pointer to tasks arguments (parameter) */
        tskIDLE_PRIORITY + 1UL,           /* Task priority*/
        NULL                              /* Task handle */
    );

    // Start task scheduler
    vTaskStartScheduler();

    while(1)
    {
    }
}

/**
  * @brief  Delay Function.
  * @param  nCount:specifies the Delay time length.
  * @retval None
  */
void Delay(__IO uint32_t nCount)
{
    while(nCount--)
    {
    }
}

