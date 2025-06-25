#define LGFX_USE_V1
#include "Arduino.h"
#include <LovyanGFX.hpp>
#include <lvgl.h>
#include <string.h>
#include <WiFi.h>
#include <PsychicMqttClient.h>
#include <time.h>
#include <Preferences.h>
#include "main.h"

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Touch_CST816S _touch_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read = 20000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = SCLK;
      cfg.pin_mosi = MOSI;
      cfg.pin_miso = MISO;
      cfg.pin_dc = DC;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs = CS;
      cfg.pin_rst = RST;
      cfg.pin_busy = -1;

      cfg.memory_width = WIDTH;
      cfg.memory_height = HEIGHT;
      cfg.panel_width = WIDTH;
      cfg.panel_height = HEIGHT;
      cfg.offset_x = OFFSET_X;
      cfg.offset_y = OFFSET_Y;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = RGB_ORDER;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;

      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();

      cfg.pin_bl = BL;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 1;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    {
      auto cfg = _touch_instance.config();

      cfg.x_min = 0;
      cfg.x_max = WIDTH;
      cfg.y_min = 0;
      cfg.y_max = HEIGHT;
      cfg.pin_int = TP_INT;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 0;
      cfg.i2c_addr = 0x15;
      cfg.pin_sda = I2C_SDA;
      cfg.pin_scl = I2C_SCL;
      cfg.freq = 400000;

      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};
LGFX tft;
Preferences prefs;

// Color data structure
typedef struct {
  int red;
  int green;
  int blue;
} ColorData;

static const uint32_t screenWidth = WIDTH;
static const uint32_t screenHeight = HEIGHT;
lv_color_t black = lv_color_make(0, 0, 0);


int glow_red;
int glow_green;
int glow_blue;
int power_state;

lv_color_t color_gray = lv_color_make(60, 60, 60);
const int picker_btn_size = 50;

// Screens
lv_obj_t *home_screen;
lv_obj_t *secondary_screen;

// Objects
lv_obj_t *power_arc;
lv_obj_t *timer_btn;
lv_obj_t *back_btn;
lv_obj_t *watch_label;
lv_obj_t *timer_label;
lv_obj_t *outside_label;
lv_obj_t *temp_vpn_label;
lv_obj_t *power_label;
lv_obj_t *kw_label;
lv_obj_t *btn_dab;

// styles
static lv_style_t style_unchecked;
static lv_style_t style_checked;
lv_style_t dab_style_def;
const unsigned int lvBufferSize = screenWidth * 10;
uint8_t lvBuffer[2][lvBufferSize];

// accessory for dimming the screen after 10s of inactivity
const uint32_t SCREEN_DIM_TIMEOUT = 10000;
const uint8_t DIM_BRIGHTNESS = 10;
const uint8_t DEFAULT_BRIGHTNESS = 100;
uint32_t lastActivityTime = 0;
bool isScreenDimmed = false;   

/**
 * Create a PsychicMqttClient object
 */
PsychicMqttClient mqttClient;

const char ssid[] = "...";
const char pass[] = "...";
const char mqtt_server[] = "mqtt://192.168.1.100";
String topic_kw = "han/kw";
String topic_outside_temp = "esp/itroom/temperature";
String topic_temperatures = "RPi/Temp/#";

/*
  * MQTT topics
*/

String topic_timer = "ESP32/round/timer";
String topic_status = "ESP32/round/status";
String topic_dab = "ESP32/round/dab";
boolean timer_status = false; // Timer status
boolean dab_status = false;

// NTP
const char* ntpServer = "europe.pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
time_t timer_start;
long prev_tick = 0L;
long timer_clock = 0L;
time_t time_prev = 0L;


void screenBrightness(uint8_t value)
{
  tft.setBrightness(value);
}

void checkScreenDimming() {
  if (!isScreenDimmed && (millis() - lastActivityTime > SCREEN_DIM_TIMEOUT)) {
    // Time to dim the screen
    screenBrightness(DIM_BRIGHTNESS);
    isScreenDimmed = true;
  }
}

void resetScreenBrightness() {
  if (isScreenDimmed) {
    screenBrightness(DEFAULT_BRIGHTNESS);
    isScreenDimmed = false;
  }
  lastActivityTime = millis();
}

// accessory for touch sensing
void my_disp_flush(lv_display_t *display, const lv_area_t *area, unsigned char *data)
{
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  lv_draw_sw_rgb565_swap(data, w * h);

  if (tft.getStartCount() == 0)
  {
    tft.endWrite();
  }

  tft.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1, (uint16_t *)data);
  lv_disp_flush_ready(display);
}

void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
  uint16_t touchX, touchY;
  bool touched = tft.getTouch(&touchX, &touchY);

  if (!touched)
  {
    data->state = LV_INDEV_STATE_REL;
  }
  else
  {
    char buffer[10];
    data->state = LV_INDEV_STATE_PR;
    data->point.x = touchX;
    data->point.y = touchY;
    resetScreenBrightness();
    /*
    Serial.print("X:"); Serial.println(data->point.x);
    Serial.print("Y:"); Serial.println(data->point.y);
    Serial.println();
    sprintf(buffer, "X:%d\nY:%d", data->point.x, data->point.y);

    lv_label_set_text(watch_label, buffer);
    lv_label_set_text(outside_label, buffer);
    */ 
    lastActivityTime = millis(); // Reset the activity timer on touch
  }
}


// move from home screen to color picker screen
void arc_click_event(lv_event_t *e) {
  lv_scr_load_anim(secondary_screen, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 400, 0, false);
}

// DAB-button engaged
void dab_click_event(lv_event_t *e) {
  // lv_scr_load_anim(home_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 400, 0, false);
  dab_status = !dab_status;
  if (!dab_status)
  {
    Serial.println("DAB on");
    mqttClient.publish(topic_dab.c_str(), 0, 0, "1"); // Publish DAB on message");
    lv_style_set_bg_color(&dab_style_def, lv_color_make(0xff, 0xff, 0));
    lv_style_set_text_color(&dab_style_def, lv_color_make(0, 0, 0));
  }
  else{
    Serial.println("DAB Off");
    mqttClient.publish(topic_dab.c_str(), 0, 0, "0"); // Publish DAB on message");
    lv_style_set_bg_color(&dab_style_def, lv_color_make(0xff, 0, 0));
    lv_style_set_text_color(&dab_style_def, lv_color_make(0xff, 0xff, 0xff));
  }

}

// move from color picker back to home_screen
void back_click_event(lv_event_t *e) {
  lv_scr_load_anim(home_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, 400, 0, false);
}

// toggling the power button
void power_click_event(lv_event_t *e) {
  if (lv_obj_get_state(timer_btn) & LV_STATE_CHECKED) {
    timer_status = true;
    /*
    lv_obj_set_style_arc_color(power_arc, lv_color_make(glow_red, glow_green, glow_blue), LV_PART_MAIN);
    lv_obj_set_style_arc_color(power_arc, lv_color_make(glow_red, glow_green, glow_blue), LV_PART_INDICATOR);
    */
    lv_label_set_text(power_label, "Stop");
    prev_tick = 0L;
    time(&timer_start ); 
    prefs.putInt("powerState", 1);
  } else {
    timer_status = false;
    /*
    lv_obj_set_style_arc_color(power_arc, lv_color_make(40, 40, 40), LV_PART_MAIN); // Set to gray
    lv_obj_set_style_arc_color(power_arc, lv_color_make(40, 40, 40), LV_PART_INDICATOR);
    */
    lv_label_set_text(power_label, "Start"); 
    prefs.putInt("powerState", 0);
  }
}

// Elements for Home Screen
void make_arc() {
  lv_color_t stored_color = lv_color_make(glow_red, glow_green, glow_blue);

  // Create the arc
  power_arc = lv_arc_create(home_screen);
  lv_obj_set_size(power_arc, 200, 200);
  lv_obj_set_style_arc_width(power_arc, 10, LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(power_arc, 10, LV_PART_MAIN);
  lv_arc_set_bg_angles(power_arc, 130, 50); // 120, 60
  lv_obj_remove_style(power_arc, NULL, LV_PART_KNOB); 
  lv_obj_set_style_arc_color(power_arc, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
  lv_obj_set_style_arc_color(power_arc, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR);
  lv_arc_set_range(power_arc, 0, 60);
  lv_arc_set_value(power_arc, 0);
  lv_obj_align(power_arc, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_event_cb(power_arc, arc_click_event, LV_EVENT_CLICKED, NULL);
  Serial.printf("ARC created");
}

void make_timer_btn() {
  timer_btn = lv_btn_create(home_screen);
  lv_obj_align(timer_btn, LV_ALIGN_CENTER, 0, 65);
  lv_obj_add_flag(timer_btn, LV_OBJ_FLAG_CHECKABLE);
  //- lv_obj_set_size(timer_btn, 120, 120);
  lv_obj_set_size(timer_btn, 80, 35); 
  lv_obj_set_style_radius(timer_btn, 60, 0);
  lv_obj_set_style_border_width(timer_btn, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(timer_btn, 0, LV_PART_MAIN);
  
  static lv_style_t style_large_text;
  lv_style_init(&style_large_text);
  lv_style_set_text_font(&style_large_text, &lv_font_montserrat_18);

  // Create and set the label
  power_label = lv_label_create(timer_btn);
  //= lv_label_set_text(label, LV_SYMBOL_POWER);
  lv_label_set_text(power_label, "Timer"); 
  lv_obj_center(power_label);

  lv_obj_add_style(power_label, &style_large_text, 0);

  // Initialize the styles for unchecked and checked states
  static lv_style_t style_unchecked;
  lv_style_init(&style_unchecked);
  //= lv_style_set_bg_color(&style_unchecked, lv_color_make(0, 0, 0));
  lv_style_set_text_color(&style_unchecked, lv_color_make(40, 40, 40)); // White text for OFF
  
  static lv_style_t style_checked;
  lv_style_init(&style_checked);
  //= lv_style_set_bg_color(&style_checked, lv_color_make(0, 0, 0));
  lv_style_set_text_color(&style_checked, lv_color_white()); // White text for ON

  // Apply the styles to the button
  lv_obj_add_style(timer_btn, &style_unchecked, 0); // Default to unchecked style
  lv_obj_add_style(timer_btn, &style_checked, LV_STATE_CHECKED); // Apply checked style for checked state

  lv_obj_add_event_cb(timer_btn, power_click_event, LV_EVENT_CLICKED, NULL); // Add event callback for button click
  lv_obj_add_state(timer_btn, prefs.getInt("powerState") == 1 ? LV_STATE_CHECKED : LV_STATE_DEFAULT); // Set the initial state to checked
}


void lv_dab_button(void)
{
    /*Properties to transition*/
    static lv_style_prop_t props[] = {
        LV_STYLE_TRANSFORM_WIDTH, LV_STYLE_TRANSFORM_HEIGHT, LV_STYLE_TEXT_LETTER_SPACE, 0
    };

    /*Transition descriptor when going back to the default state.
     *Add some delay to be sure the press transition is visible even if the press was very short*/
    static lv_style_transition_dsc_t transition_dsc_def;
    lv_style_transition_dsc_init(&transition_dsc_def, props, lv_anim_path_overshoot, 250, 100, NULL);

    /*Transition descriptor when going to pressed state.
     *No delay, go to presses state immediately*/
    static lv_style_transition_dsc_t transition_dsc_pr;
    lv_style_transition_dsc_init(&transition_dsc_pr, props, lv_anim_path_ease_in_out, 250, 0, NULL);

    /*Add only the new transition to he default state*/
    // lv_style_t dab_style_def;
    lv_style_init(&dab_style_def);
    lv_style_set_transition(&dab_style_def, &transition_dsc_def);
    //-> lv_style_set_bg_color(&dab_style_def, lv_color_make(0, 0, 0)); // Black for OFF

    /*Add the transition and some transformation to the presses state.*/
    static lv_style_t style_pr;
    lv_style_init(&style_pr);
    lv_style_set_transform_width(&style_pr, 10);
    lv_style_set_transform_height(&style_pr, -10);
    lv_style_set_text_letter_space(&style_pr, 10);
    lv_style_set_transition(&style_pr, &transition_dsc_pr);

    btn_dab = lv_button_create(secondary_screen);
    lv_obj_align(btn_dab, LV_ALIGN_CENTER, 0, -80);
    lv_obj_add_style(btn_dab, &style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn_dab, &dab_style_def, 0);

    lv_obj_t * label = lv_label_create(btn_dab);
    lv_label_set_text(label, "DAB");

    lv_obj_add_event_cb(btn_dab, dab_click_event, LV_EVENT_CLICKED, NULL);
}




void make_back_btn() {
  back_btn = lv_btn_create(secondary_screen);
  lv_obj_align(back_btn, LV_ALIGN_CENTER, 0, 100);
  lv_obj_set_size(back_btn, picker_btn_size, picker_btn_size);
  lv_obj_set_style_radius(back_btn, picker_btn_size / 2, 0);
  lv_obj_set_style_bg_color(back_btn, lv_color_make(0, 0, 0), LV_PART_MAIN);
  lv_obj_set_style_text_color(back_btn, lv_color_make(glow_red, glow_green, glow_blue), LV_PART_MAIN);
  lv_obj_set_style_border_width(back_btn, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(back_btn, 0, LV_PART_MAIN);

  static lv_style_t style_back_text;
  lv_style_init(&style_back_text);
  lv_style_set_text_font(&style_back_text, &lv_font_montserrat_40);

  // Create and set the label
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, LV_SYMBOL_DOWN);
  lv_obj_center(back_label);
  lv_obj_add_style(back_label, &style_back_text, 0);

  lv_obj_add_event_cb(back_btn, back_click_event, LV_EVENT_CLICKED, NULL);
}

/* =================================================
  *
  * Make Screens
  * 
*/
void make_home_screen() {
  home_screen = lv_obj_create(NULL);  
  lv_obj_set_style_bg_color(home_screen, black, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(home_screen, LV_OPA_COVER, LV_PART_MAIN);

  static lv_style_t style_text_40;
  lv_style_init(&style_text_40);
  lv_style_set_text_font(&style_text_40, &lv_font_montserrat_40);

  static lv_style_t style_text_32;
  lv_style_init(&style_text_32);
  lv_style_set_text_font(&style_text_32, &lv_font_montserrat_32);
  lv_obj_set_style_text_color(home_screen, lv_color_hex(0x00ff00), LV_PART_MAIN);

  make_arc();
  make_timer_btn();

  kw_label = lv_label_create(home_screen); // lv_screen_active());
  lv_label_set_text(kw_label, "");
  lv_obj_align(kw_label, LV_ALIGN_CENTER, 0, -50);
  lv_obj_add_style(kw_label, &style_text_32, 0);

  watch_label = lv_label_create(home_screen); // lv_screen_active());
  lv_label_set_text(watch_label, "Starting...");
  lv_obj_align(watch_label, LV_ALIGN_CENTER, 0, -5);
  lv_obj_add_style(watch_label, &style_text_40, 0);

  /* */
  timer_label = lv_label_create(home_screen); // lv_screen_active());
  lv_label_set_text(timer_label, "00:00");
  lv_obj_align(timer_label, LV_ALIGN_CENTER, 0, 30);
  lv_obj_add_style(timer_label, &style_text_32, 0);
  lv_obj_set_style_text_color(timer_label, lv_color_hex(0xffffff), LV_PART_MAIN);
  /* */

}

void make_secondary_screen() {
  secondary_screen = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(secondary_screen, black, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(secondary_screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_color(secondary_screen, lv_color_hex(0x00ff00), LV_PART_MAIN);

  make_back_btn();
  lv_dab_button();
  static lv_style_t style;
  lv_style_init(&style);
  lv_style_set_text_font(&style, &lv_font_montserrat_28);

  outside_label = lv_label_create(secondary_screen); // lv_screen_active());
  lv_label_set_text(outside_label, "--");
  lv_obj_align(outside_label, LV_ALIGN_CENTER, 0, -30);
  lv_obj_add_style(outside_label, &style, 0);  // <--- obj is the label}

  temp_vpn_label = lv_label_create(secondary_screen); // lv_screen_active());
  lv_label_set_text(temp_vpn_label, "--");
  lv_obj_align(temp_vpn_label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_style(temp_vpn_label, &style, 0);  // <--- obj is the label}

}

/* ================================================
  *
  * MQTT routines
  * 
  * ================================================
*/
void onMqttConnect(bool sessionPresent)
{
    Serial.println("Connected to MQTT.");
    Serial.printf("Session present: %d\r\n", sessionPresent);

    int packetIdSub = mqttClient.subscribe(topic_kw.c_str(), 2);
    Serial.printf("Subscribing at QoS 2, packetId: %d [%s]\r\n", packetIdSub, topic_kw);

    packetIdSub = mqttClient.subscribe(topic_outside_temp.c_str(), 2);
    Serial.printf("Subscribing at QoS 2, packetId: %d [%s]\r\n", packetIdSub, topic_outside_temp);

    
    packetIdSub = mqttClient.subscribe(topic_temperatures.c_str(), 2);
    Serial.printf("Subscribing at QoS 2, packetId: %d [%s]\r\n", packetIdSub, topic_temperatures);

  }

void onMqttDisconnect(bool sessionPresent)
{
    Serial.println("Disconnected from MQTT.");
}

void onMqttSubscribe(uint16_t packetId)
{
    Serial.println("Subscribe acknowledged.");
    Serial.printf("  packetId: %d\r\n", packetId);
}

void onMqttUnsubscribe(uint16_t packetId)
{
    Serial.println("Unsubscribe acknowledged.");
    Serial.printf("  packetId: %d\r\n", packetId);
}

void onMqttMessage(char *topic, char *payload, int retain, int qos, bool dup)
{
    char buffer[40];

    Serial.printf("Message received: [%s] - %s\r\n", topic, payload);
  
    if (strcmp(topic, topic_kw.c_str()) == 0)
    {
      sprintf(buffer, "%s", payload);
      lv_label_set_text(kw_label, buffer);
      lv_arc_set_value(power_arc, atof(payload)); // /100.0);
    }
    else if (strcmp(topic, topic_outside_temp.c_str()) == 0)
    {
      sprintf(buffer, "IT: %.1f \u00B0C", atof(payload));
      lv_label_set_text(outside_label, buffer);
    }
    else if (strcmp(topic, "RPi/Temp/rpivpn") == 0)
    {
      // RPi/Temp/rpivpn
      sprintf(buffer, "VPN: %.1f \u00B0C", atof(payload));
      lv_label_set_text(temp_vpn_label, buffer);
    }
    else
    {
        Serial.println("Unknown topic received.");
    }
}

void onMqttPublish(uint16_t packetId)
{
    Serial.println("Publish acknowledged.");
    Serial.printf("  packetId: %d\r\n", packetId);
}

void onLongTopic(const char *topic, const char *payload, int retain, int qos, bool dup)
{
    Serial.printf("Received Long Topic: %s\r\n", topic);
    Serial.printf("Received Payload: %s\r\n", payload);
}

void onTopic(const char *topic, const char *payload, int retain, int qos, bool dup)
{
  Serial.printf("Received Topic: %s\r\n", topic);
  Serial.printf("Received Payload: %s\r\n", payload); 
}


void setup()
{
  Serial.begin(115200);
  
  prefs.begin("glow-app", false);

  glow_red = prefs.getInt("glowRed", 140);
  glow_green = prefs.getInt("glowGreen", 0);
  glow_blue = prefs.getInt("glowBlue", 255);
  power_state = prefs.getInt("powerState", 1);

  int rt = prefs.getInt("rotate", 0);
  int br = prefs.getInt("brightness", 100);

  // inititalise screen
  tft.init();
  tft.initDMA();
  tft.startWrite();
  tft.fillScreen(TFT_BLACK);
  tft.setRotation(rt);

  lv_init(); // initialise LVGL

  // setup screen
  static auto *lvDisplay = lv_display_create(screenWidth, screenHeight);
  lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(lvDisplay, my_disp_flush);
  lv_display_set_buffers(lvDisplay, lvBuffer[0], lvBuffer[1], lvBufferSize, LV_DISPLAY_RENDER_MODE_PARTIAL);

  // setup touch
  static auto *lvInput = lv_indev_create();
  lv_indev_set_type(lvInput, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lvInput, my_touchpad_read);

  screenBrightness(br); // startup brightness

  make_home_screen();
  make_secondary_screen();

  lv_scr_load(home_screen); 

  Serial.println("Delay");
  time_t time_now;
  time(&time_now);
  time_prev = time_now;
  while ((time_now - time_prev) < 5)
  { 
    time(&time_now);
    lv_timer_handler();
    yield();
  }
  Serial.println("Connect to WLAN");

  // sleep(10);
  // Then the WiFi
  WiFi.begin(ssid, pass);

  Serial.printf("Connecting to WiFi %s .", ssid);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.printf("\r\nConnected, IP address: %s \r\n", WiFi.localIP().toString().c_str());

  // Set up NTP client
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);


 /**
    * Connect to the MQTT broker
  */
  mqttClient.setServer(mqtt_server);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.onTopic("ESP32/status", 0, onTopic);

  mqttClient.connect();

  /**
    * Wait blocking until the connection is established
    */
  while (!mqttClient.connected())
  {
    delay(500);
  }

  /**
    * Publish a message 
    */
  mqttClient.publish(topic_status.c_str(), 0, 0, "ESP32 running");

  Serial.print("Setup finished");
}

void loop()
{
  char buffer[50];
  time_t time_now;


  static uint32_t lastTick = millis();
  uint32_t current = millis();
  lv_tick_inc(current - lastTick);
  lastTick = current;
  lv_timer_handler();

  // timeout for screen dimming
  checkScreenDimming();
  delay(5);

  time(&time_now);
  if ((time_now - time_prev) > 0)
  { 
    time_prev = time_now; // Update timer clock
    // Update realtime clock
    struct tm *local_time = localtime(&time_now);
    sprintf(buffer, "%02d:%02d:%02d", local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    lv_label_set_text(watch_label, buffer);
  }

  if (timer_status)
  {
    long timer_tick2 = difftime(time_now, timer_start);
    if ((timer_tick2 - prev_tick) > 0)
    {
      prev_tick = timer_tick2; // Update previous tick
      int minutes = timer_tick2 / 60;
      int seconds = timer_tick2 % 60;
      sprintf(buffer, "%d:%02d", minutes, seconds);
      lv_label_set_text(timer_label, buffer);
      mqttClient.publish(topic_timer.c_str(), 0, 0, buffer);
    }
  }


  // lv_label_set_text(watch_label, "OK");
}