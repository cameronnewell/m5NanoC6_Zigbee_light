// M5NanoC6_Zigbee_Light.ino
// This file contains setup() and loop() to satisfy the Arduino IDE.

#ifndef ZIGBEE_MODE_ED
#error "Zigbee end device mode is not selected in Tools->Zigbee mode"
#endif

#include "esp_zigbee_core.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include <Adafruit_NeoPixel.h>

// M5NanoC6 Pin Definitions
#define M5NANO_C6_BLUE_LED_PIN      7
#define M5NANO_C6_BTN_PIN           9
#define M5NANO_C6_IR_TX_PIN         3
#define M5NANO_C6_RGB_LED_PWR_PIN   19
#define M5NANO_C6_RGB_LED_DATA_PIN  20
#define LED_PIN M5NANO_C6_RGB_LED_DATA_PIN

// The number of LEDs is still 1
#define NUM_LEDS 1

Adafruit_NeoPixel strip(NUM_LEDS, M5NANO_C6_RGB_LED_DATA_PIN,
                        NEO_GRB + NEO_KHZ800);

/* Default End Device config */
#define ESP_ZB_ZED_CONFIG()                                     \
    {                                                           \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                   \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,       \
        .nwk_cfg = {                                            \
            .zed_cfg = {                                        \
                .ed_timeout = ED_AGING_TIMEOUT,                 \
                .keep_alive = ED_KEEP_ALIVE,                    \
            },                                                  \
        },                                                      \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000
#define HA_ESP_LIGHT_ENDPOINT           10
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK

// Global variables for light state
char modelid[] = {13, 'E', 'S', 'P', '3', '2', 'C', '6', '.', 'L', 'i', 'g', 'h', 't'};
char manufname[] = {9, 'E', 's', 'p', 'r', 'e', 's', 's', 'i', 'f'};

// MODIFIED: Global variables for light state
bool light_state = false;
uint8_t light_level = 254; // Default level to a non-zero value
uint16_t light_hue = 0;
uint16_t light_saturation = 254;


/********************* Zigbee functions (prototypes for functions defined below) **************************/
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask);
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct);
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message);
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message);
static void esp_zb_task(void *pvParameters);

// MODIFIED: New function to update the NeoPixel based on global state
void update_led_strip() {
    if (light_state) {
        uint32_t color = strip.ColorHSV(light_hue * 182, light_saturation * 255, light_level);
        strip.setPixelColor(0, color);
    } else {
        strip.setPixelColor(0, 0); // Turn the single LED off
    }
    strip.show();
}


/********************* Zigbee function implementations (all in .ino) **************************/
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p      = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = (esp_zb_app_signal_type_t)*p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        log_i("Zigbee stack initialized");
        Serial.println("Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            log_i("Start network steering");
            Serial.println("Start network steering");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            /* commissioning failed */
            log_w("Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
            Serial.print("Failed to initialize Zigbee stack (status: ");
            Serial.println(esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            log_i("Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            Serial.println("Joined network successfully");
            Serial.print("Channel : ");
            Serial.println(esp_zb_get_current_channel());
        } else {
            log_i("Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            Serial.print("Network steering was not successful (status : ");
            Serial.println(esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        log_i("ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                      esp_err_to_name(err_status));
        Serial.print("ZDO signal : ");
        Serial.println(esp_zb_zdo_signal_to_string(sig_type));
        break;
    }
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        log_w("Receive Zigbee action(0x%x) callback", callback_id);
        Serial.print("Receive Zigbee action callback : ");
        Serial.println(callback_id);
        break;
    }
    return ret;
}

static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack with Zigbee end-device config */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);

    /* set the on-off light device config */
    uint8_t test_attr, test_attr2;
 
    test_attr = 0;
    test_attr2 = 4;
    /* basic cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &test_attr);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &test_attr2);
    esp_zb_cluster_update_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &test_attr2);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, &modelid[0]);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, &manufname[0]);
    /* identify cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &test_attr);
    /* group cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_GROUPS);
    esp_zb_groups_cluster_add_attr(esp_zb_groups_cluster, ESP_ZB_ZCL_ATTR_GROUPS_NAME_SUPPORT_ID, &test_attr);
    /* scenes cluster create with standard cluster + customized */
    esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_scenes_cluster_create(NULL);
    esp_zb_cluster_update_attr(esp_zb_scenes_cluster, ESP_ZB_ZCL_ATTR_SCENES_NAME_SUPPORT_ID, &test_attr);
    
    // MODIFIED: Added Level Control and Color Control clusters
    esp_zb_level_control_cluster_cfg_t level_control_cfg;
    level_control_cfg.current_level = light_level; // Set initial level
    esp_zb_attribute_list_t *esp_zb_level_control_cluster = esp_zb_level_control_cluster_create(&level_control_cfg);

    esp_zb_color_control_cluster_cfg_t color_control_cfg;
    color_control_cfg.current_hue = light_hue;
    color_control_cfg.current_saturation = light_saturation;
    color_control_cfg.color_mode = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_MODE_CURRENT_HUE_SATURATION;
    color_control_cfg.enhanced_current_hue = 0;
    color_control_cfg.color_temperature = 0;
    esp_zb_attribute_list_t *esp_zb_color_control_cluster = esp_zb_color_control_cluster_create(&color_control_cfg);

    /* on-off cluster create with standard cluster config*/
    esp_zb_on_off_cluster_cfg_t on_off_cfg;
    on_off_cfg.on_off = light_state; // Set initial state
    esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);

    /* create cluster lists for this endpoint */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_scenes_cluster(esp_zb_cluster_list, esp_zb_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    // MODIFIED: Add the new clusters to the list
    esp_zb_cluster_list_add_level_control_cluster(esp_zb_cluster_list, esp_zb_level_control_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_color_control_cluster(esp_zb_cluster_list, esp_zb_color_control_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);


    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();

    // Corrected: Define the endpoint configuration structure with CORRECT MEMBER NAMES
    esp_zb_endpoint_config_t ep_config = {
        .endpoint = (uint8_t)HA_ESP_LIGHT_ENDPOINT,
        // MODIFIED: Changed device ID to Color Dimmable Light
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID,
        .app_device_version = 0,
    };

    /* add created endpoint (cluster_list) to endpoint list */
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, ep_config);

    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

/* Handle the light attribute */
static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;

    if(!message){
      log_e("Empty message");
      Serial.println("Empty message");
    }
    if(message->info.status != ESP_ZB_ZCL_STATUS_SUCCESS){
      log_e("Received message: error status(%d)", message->info.status);
      Serial.print("Received message: error status : ");
      Serial.println(message->info.status);
    }

    log_i("Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
              message->attribute.id, message->attribute.data.size);
    Serial.println("Received message : ");
    Serial.println(message->info.dst_endpoint);
    Serial.println(message->info.cluster);
    Serial.println(message->attribute.id);
    Serial.println(message->attribute.data.size);

    if (message->info.dst_endpoint == HA_ESP_LIGHT_ENDPOINT) {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                // MODIFIED: Update the global light state variable
                light_state = *(bool *)message->attribute.data.value;
                log_i("Light sets to %s", light_state ? "On" : "Off");
                Serial.print("Light Toggle : ");
                Serial.println(light_state);
            }
        // MODIFIED: Handle Level Control cluster
        } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID) {
                light_level = *(uint8_t *)message->attribute.data.value;
                log_i("Light level sets to %d", light_level);
                Serial.print("Light Level : ");
                Serial.println(light_level);
            }
        // MODIFIED: Handle Color Control cluster
        } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID) {
                light_hue = *(uint16_t *)message->attribute.data.value;
                log_i("Light hue sets to %d", light_hue);
                Serial.print("Light Hue : ");
                Serial.println(light_hue);
            } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID) {
                light_saturation = *(uint16_t *)message->attribute.data.value;
                log_i("Light saturation sets to %d", light_saturation);
                Serial.print("Light Saturation : ");
                Serial.println(light_saturation);
            }
        }
        // MODIFIED: After processing any command, update the physical LED strip
        update_led_strip();
    }
    return ret;
}

/********************* Arduino functions **************************/
void setup() {
    // Init Zigbee
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    // Enable RGB LED Power
    pinMode(M5NANO_C6_RGB_LED_PWR_PIN, OUTPUT);
    digitalWrite(M5NANO_C6_RGB_LED_PWR_PIN, HIGH);

    strip.begin();
    strip.show();

    // MODIFIED: Initial update of the strip based on default values
    update_led_strip();

    Serial.begin(115200);
    Serial.println("M5Stack M5NanoC6 Zigbee bulb startup!");

    // Start Zigbee task
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}

void loop() {
    //empty, zigbee running in task
}