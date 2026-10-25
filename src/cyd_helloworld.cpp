/*
    Rudi Zengers
    15 September 2025
    
    ESP32 oefening met LVGL. Draait op 2,8" Cheap Yellow Display
    Dit is ESP met ST7789 display, spi mode 3, rotation 3, landscape orientation, color rgb, inversion off
    Doel: Pizza recept bepalen
*/

// Touchscreen pins
//#define XPT2046_IRQ  36   // T_IRQ
//#define XPT2046_MOSI 32   // T_DIN
//#define XPT2046_MISO 39   // T_OUT_HEIGHT
//#define XPT2046_CLK  25   // T_CLK
//#define XPT2046_CS   33   // T_CS
#define TOUCH_MISO     39
#define TOUCH_MOSI     32
#define TOUCH_CLK      25
#define TOUCH_CS       33
//#define TFT_WIDTH    320
//#define TFT_HEIGHT   240
//#define SCREEN_ROTATION 3 // 1 = horizontal, 2 = vertical, 3 = horinzontal flipped, 4 vertical flipped

#define DRAW_BUF_SIZE (SCREEN_HEIGHT * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))

#include <Arduino.h>
#include <SPI.h>
#include <bb_spi_lcd.h>
#include <XPT2046_Touchscreen.h>    // 1.4
#include <lvgl.h>                   // 8.3.11

#include "ui.h"                     // 1.5.3 Created by export from Squareline Studio
#include "ui_helpers.h"             // 1.5.3 Created by export from Squareline Studio

TOUCHINFO ti;

int x, y, z; // Touchscreen coordinates: (x, y) and pressure (z)

/* Global storage for recept parameters */
double aantalbollen       = 2.0;     // GUI parameter: Aantal bollen wat we gaan maken. Standaard 2
double bolgrootte         = 220.0;   // GUI parameter: Grootte van 1 deegbol
double hydratiepercentage = 58.0;    // GUI parameter: Verhouding water vs meel
double gistverhouding     = 1.0;     // GUI parameter: Verhouding verse gist vs droge gist. Standaard droge gist

double zoutgehalte        = 0.025;   // Statische parameter: percentage zout in recept
double oliegehalte        = 0.03;    // Statische parameter: percentage olie in recept

double meel               = 1.0;     // Hoeveelheid berekend meel in recept
double water              = 1.0;     // Hoeveelheid berekend water in recept
double gist               = 1.0;     // Hoeveelheid berekend gist in recept
double olie               = 1.0;     // Hoeveelheid berekend olie in recept
double zout               = 1.0;     // Hoeveelheid berekend zout in recept

char gistsoortc[15]       = "Droge gist";  // Buffer to hold the selected string

int receptstatus          = 1;       // where are we on the resultscreen
int klokseconden_te_gaan  = 0;

// lvgl definitions
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ TFT_WIDTH * TFT_HEIGHT / 10 ];

BB_SPI_LCD tft;

// If logging is enabled, it will inform the user about what is happening in the library
#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf) {
    Serial.printf(buf);
    Serial.flush();
}
#endif

#define SPEAKER            26 // pin
#define RGB_BLUE           17 // pin

// Get the Touchscreen data
void my_touchpad_read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data ) {
    //if(touchscreen.tirqTouched() && touchscreen.touched()) {
    if (tft.rtReadTouch(&ti)) {
    // rtReadTouch returns calibrated pixel coordinates directly (0..TFT_WIDTH-1 / 0..TFT_HEIGHT-1)
    x = ti.x[0];
    y = ti.y[0];


    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;  // Set the coordinates
    data->point.y = y;

#if LV_USE_LOG != 0
    Serial.print("Touch interface has seen X = "); // Print Touchscreen info about X, Y and Pressure (Z) on the Serial Monitor
    Serial.print(x);
    Serial.print(" | Y = ");
    Serial.print(y);
#endif

  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

/* Display flushing */
void my_disp_flush( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p ) {
    uint32_t w = ( area->x2 - area->x1 + 1 );
    uint32_t h = ( area->y2 - area->y1 + 1 );

    /* bb_spi_lcd's pushPixels sends the buffer bytes as-is (no byte swap) and LVGL is
       pinned to LV_COLOR_16_SWAP 0 by the SquareLine export -> swap bytes here instead,
       so the ST7789 receives every 16-bit pixel MSB first. */
    uint32_t count = w * h;
    uint16_t *pix = ( uint16_t * )color_p;
    for (uint32_t i = 0; i < count; i++) pix[i] = __builtin_bswap16(pix[i]);

    tft.setAddrWindow( area->x1, area->y1, w, h );
    tft.pushPixels( ( uint16_t * )color_p, w * h, DRAW_TO_LCD );
    lv_disp_flush_ready(disp_drv);
}

void setup() {
    Serial.begin( 115200 ); /* prepare for possible serial debug */
 
    String LVGL_Arduino = "Pizza recept program. Rudi Zengers, september 2025 ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.println( LVGL_Arduino );

    lv_init();

#if LV_USE_LOG != 0
    lv_log_register_print_cb(my_print); /* register print function for debugging */
#endif

    /* TFT init: bb_spi_lcd's tested preset for this board (ST7789,
       FLAGS_INVERT=inversion OFF, 40MHz, pins 15/2/-/21/12/13/14, orientation 270) */
    tft.begin(DISPLAY_CYD_2USB);

    /* Fine-tune colors for this ST7789 clone: use the standard Adafruit/datasheet
       gamma curve instead of bb_spi_lcd's defaults */
    extern void spilcdWriteCmdParams(SPILCD *pLCD, uint8_t ucCMD, uint8_t *pParams, int iLen);
    uint8_t gamma1[14] = {0xD0,0x08,0x11,0x08,0x0C,0x15,0x39,0x33,0x50,0x36,0x13,0x14,0x29,0x2D};
    uint8_t gamma2[14] = {0xD0,0x08,0x10,0x08,0x06,0x06,0x39,0x44,0x51,0x0B,0x16,0x14,0x2F,0x31};
    spilcdWriteCmdParams(tft.getLCDStruct(), 0xE0, gamma1, 14);
    spilcdWriteCmdParams(tft.getLCDStruct(), 0xE1, gamma2, 14);

    tft.rtInit(TOUCH_MOSI, TOUCH_MISO, TOUCH_CLK, TOUCH_CS); // initialize touch AFTER initializing the LCD
    tft.getLCDStruct()->iRTThreshold = 6300; // pressure threshold (plain begin() doesn't set the CYD default)
    tft.getLCDStruct()->iRTOrientation = 0;  // touch sensor matches display orientation


    /* Set the touchscreen calibration data, the actual data for your display can be acquired using
     the Generic -> Touch_calibrate example from the TFT_eSPI library */
    uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };
    //tft.setTouch( calData );

    lv_disp_draw_buf_init( &draw_buf, buf, NULL, TFT_WIDTH * TFT_HEIGHT / 10 );

    // Initialize the display
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init( &disp_drv );
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Initialize the input device driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init( &indev_drv );
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ledcAttach(SPEAKER, 1000, 8);  // 1 kHz PWM, 8-bit resolution
    ledcAttach(RGB_BLUE, 1000, 8);  // 1 kHz PWM, 8-bit resolution
    ledcWrite(RGB_BLUE,255); // led off


    /* Start using the UI as defined via Square line */
    ui_init();
    const char compile_date[] = "Rudi Zengers, " __DATE__ " " __TIME__;
    lv_label_set_text(ui_SplashScreen_Label_Label17,compile_date);
}

/* 
   Event functions as defined for GUI elements. 
   Function headers in ui_events.h. Function protype in ui_events.c are commented and replaced with below ones */

// Dropdown change event fired
void ValueBolGrootteWijziging(lv_event_t * e) {
    lv_obj_t *dropdown = lv_event_get_target(e); // Get the dropdown widget
    char selected_text[64]; // Buffer to hold the selected string
    lv_dropdown_get_selected_str(dropdown, selected_text, sizeof(selected_text));
    // bolgroote
	Serial.print("Bol grootte gewijzigd in ");
    Serial.print(selected_text);
    Serial.print("\n");
    if (selected_text == "Normaal") {
        bolgrootte = 220.0;
    }   else { 
            if (selected_text == "Kleiner") {
                bolgrootte = 200.0;
        } else {  
            bolgrootte = 240.0;
            }
    }
}

// Dropdown change event fired
void ValueGistsoortWijziging(lv_event_t * e) {
    //gistverhouding;
    lv_obj_t *dropdown = lv_event_get_target(e); // Get the dropdown widget
 
    lv_dropdown_get_selected_str(dropdown, gistsoortc, sizeof(gistsoortc));
    // gistverhouding
	Serial.print("Gist type gewijzigd in ");
    Serial.println(gistsoortc);

    if (gistsoortc == "Droge gist") {
        gistverhouding = 1.0;
    }   else { 
        gistverhouding = 3.0;
    }
}

// Show where we are in preparing
void ShowPointer(int receptstatus) {
    int yaxis[5] = {-100,-65,-30,5,40};
    lv_obj_set_y( ui_ResultScreen_Label_LabelNext, yaxis[receptstatus]);  
    lv_obj_set_y( ui_ResultScreen_Button_ButtonNext, yaxis[receptstatus]); 
}

// Slider change event fired
void ValueAantalGewijzigd(lv_event_t * e) {
	aantalbollen = 1.0 * lv_slider_get_value(ui_ParamScreen_Slider_Slider1);
    char aantalbollenc[4]; dtostrf(aantalbollen, 2, 0, aantalbollenc);
    
    Serial.print("Aantal bollen gewijzigd in ");
    Serial.println(aantalbollenc);
    lv_label_set_text(ui_ParamScreen_Label_AantalBollen,aantalbollenc);
} 

// Slider change event fired
void ValueHydratatieGewijzigd(lv_event_t * e) {
	hydratiepercentage = 1.0 * lv_slider_get_value(ui_ParamScreen_Slider_Slider2);
    char hydratiepercentagec[4]; dtostrf(hydratiepercentage, 2, 0, hydratiepercentagec);
    strcat(hydratiepercentagec,"%");
    Serial.print("Hydratie gewijzigd in ");
    Serial.println(hydratiepercentagec);
    lv_label_set_text(ui_ParamScreen_Label_Hydratatie,hydratiepercentagec);
}

void klokInterval(lv_timer_t * timer) {
    klokseconden_te_gaan--;
    lv_arc_set_value(ui_ResultScreen_Arc_Klok, klokseconden_te_gaan);

    if (klokseconden_te_gaan > 0) {

        // klok did one sec. Adjust it
        char klokstring[6] = "00:00";
        int min = klokseconden_te_gaan / 60;
        int sec = klokseconden_te_gaan - (min * 60);
        sprintf(klokstring, "%d:%02d", min,sec);
        lv_label_set_text(ui_ResultScreen_Label_Kloklabel,klokstring);

        if (klokseconden_te_gaan == 1) {
            ledcWrite(SPEAKER, 128); // warn time is up
            ledcWrite(RGB_BLUE,0);
        }
    } else {
        // klok is ready
        lv_timer_del(timer);
        ledcWrite(SPEAKER, 0); // speaker quiet
        ledcWrite(RGB_BLUE,255); // led off

        lv_obj_add_flag(ui_ResultScreen_Label_Kloklabel, LV_OBJ_FLAG_HIDDEN); // Hide klok
        lv_obj_add_flag(ui_ResultScreen_Arc_Klok, LV_OBJ_FLAG_HIDDEN); // Hide klok
        lv_obj_add_flag(ui_ResultScreen_Image_mixer, LV_OBJ_FLAG_HIDDEN); // Hide mixer symbol
        if ((receptstatus < 5)) {
            lv_obj_clear_flag(ui_ResultScreen_Button_ButtonNext, LV_OBJ_FLAG_HIDDEN); // Show button
            lv_obj_clear_flag( ui_ResultScreen_Label_LabelNext,  LV_OBJ_FLAG_HIDDEN);  // Show text on it
        } else {
            lv_obj_clear_flag(ui_ResultScreen_Image_finishvlag, LV_OBJ_FLAG_HIDDEN); // Show finish
       }

    }
    // beep for two seconds
}



void StartKlok(int seconds) {
    if (seconds > 0) {

        klokseconden_te_gaan = seconds;
        char klokstring[6] = "00:00";
        int min = klokseconden_te_gaan / 60;
        int sec = klokseconden_te_gaan - (min * 60);
        sprintf(klokstring, "%d:%02d", min,sec);
        lv_arc_set_value(ui_ResultScreen_Arc_Klok, klokseconden_te_gaan);
        
        lv_obj_add_flag(ui_ResultScreen_Button_ButtonNext, LV_OBJ_FLAG_HIDDEN); // hide button
        lv_obj_add_flag( ui_ResultScreen_Label_LabelNext,  LV_OBJ_FLAG_HIDDEN);  // hide text on it

        lv_arc_set_range(ui_ResultScreen_Arc_Klok, 0, seconds);  // Set arc max
        lv_label_set_text(ui_ResultScreen_Label_Kloklabel,klokstring);

        lv_obj_clear_flag(ui_ResultScreen_Label_Kloklabel, LV_OBJ_FLAG_HIDDEN); // Show klok
        lv_obj_clear_flag(ui_ResultScreen_Arc_Klok, LV_OBJ_FLAG_HIDDEN); // Show klok
        lv_obj_clear_flag(ui_ResultScreen_Image_mixer, LV_OBJ_FLAG_HIDDEN); // Show mixer symbol
    
        lv_timer_t * timer = lv_timer_create(klokInterval, 1000,NULL);  // Start timer

        Serial.print("Klok gestart; seconden: ");
        Serial.println(seconds);
    }
}


// Button Next is clicked, move it down
void ButtonNextClicked(lv_event_t * e) {
    int klokoptions[6] = {0,0,0,120,120,420};
    receptstatus++;
    ShowPointer(receptstatus);
    StartKlok(klokoptions[receptstatus]);
    Serial.println("Moving to position " + String(receptstatus));
}
   

// Bereken de hoeveelheid ingredienten
void RecalcRecept(lv_event_t * e) {
    meel = (aantalbollen * bolgrootte) / ((100.0 + hydratiepercentage) / 100.0);
    water = (aantalbollen * bolgrootte) / (100.0 + hydratiepercentage) * hydratiepercentage;
    zout = meel * zoutgehalte;
    olie = meel * oliegehalte;
    gist = water / 100.0 * 0.4 * gistverhouding;

    char meelc[6];  dtostrf(meel,  5, 0, meelc);
    char waterc[6]; dtostrf(water, 5, 0, waterc);
    char zoutc[6];  dtostrf(zout,  5, 1, zoutc);
    char oliec[6];  dtostrf(olie,  5, 0, oliec);
    char gistc[6];  dtostrf(gist,  5, 1, gistc);
    
    String r = "Recept voor ";
    r += String(aantalbollen) + " bollen van " + String(bolgrootte) + " gram: Meel = " + String(meelc);
    r += ", Water = " + String(waterc) + ", Zout = " + String(zoutc) + ", Olie = " + String(oliec) + ", Gist = " + String(gistc);
    Serial.println(r);

    // write stuff on screen
    lv_label_set_text(ui_ResultScreen_Label_ResultMeel,  meelc);
    lv_label_set_text(ui_ResultScreen_Label_ResultWater, waterc);
    lv_label_set_text(ui_ResultScreen_Label_ResultZout,  zoutc);
    lv_label_set_text(ui_ResultScreen_Label_ResultOlie,  oliec);
    lv_label_set_text(ui_ResultScreen_Label_ResultGist,  gistc);
    lv_label_set_text(ui_ResultScreen_Label_Label3,      gistsoortc);
    
    lv_obj_add_flag(ui_ResultScreen_Label_Kloklabel, LV_OBJ_FLAG_HIDDEN); // Hide klok
    lv_obj_add_flag(ui_ResultScreen_Arc_Klok, LV_OBJ_FLAG_HIDDEN); // Hide klok
    lv_obj_add_flag(ui_ResultScreen_Image_mixer, LV_OBJ_FLAG_HIDDEN); // Hide mixer symbol
    lv_obj_add_flag(ui_ResultScreen_Image_finishvlag, LV_OBJ_FLAG_HIDDEN); // Hide finish flag symbol
     lv_obj_add_flag(ui_ResultScreen_Image_wacht, LV_OBJ_FLAG_HIDDEN); // Hide wait symbol

    // Set recept index
    receptstatus = 0;
    lv_obj_clear_flag(ui_ResultScreen_Button_ButtonNext, LV_OBJ_FLAG_HIDDEN); // show button
    lv_obj_clear_flag( ui_ResultScreen_Label_LabelNext,  LV_OBJ_FLAG_HIDDEN);  // show text on it

    ShowPointer(receptstatus);
}

// What to do the rest of our power-on journey
void loop() {
    lv_timer_handler(); /* let the GUI do its work */
    delay(5);
}
