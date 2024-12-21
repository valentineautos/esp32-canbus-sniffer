
// requires https://github.com/collin80/esp32_can and https://github.com/collin80/can_common

#include <esp32_can.h>
#include <esp_now.h>
#include <WiFi.h>
#include <cmath>
#include <iostream>

uint8_t receiver_mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // Your screen MAC Address

TaskHandle_t send_speed_handle = NULL;

int send_speed_interval = 100; // 10 sends per second

struct can_struct {
  uint8_t speed_mph;
  uint16_t speed_rpm;
};

can_struct CANMessage;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Send Success" : "Send Failed");
}

void send_speed(void *parameter) {
  while(true) {
    esp_err_t result = esp_now_send(receiver_mac, (uint8_t *)&CANMessage, sizeof(CANMessage));

      if (result == ESP_OK) {
        Serial.println("Sent via ESP-NOW");
      } else {
        Serial.print("Error sending message: ");
        Serial.println(result);
      }
  
    vTaskDelay(send_speed_interval / portTICK_PERIOD_MS);
  }
}

void setup() {
    Serial.begin(115200);

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        return;
    }
    esp_now_register_send_cb(OnDataSent);

    // Add peer
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, receiver_mac, 6);
    peerInfo.channel = 0; // Default channel
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    Serial.println("ESP-NOW Initialized");

    CAN0.setCANPins(GPIO_NUM_4, GPIO_NUM_5); // see important note above
    CAN0.begin(500000); // 500Kbps
    CAN0.watchFor();

    xTaskCreate(
      send_speed,
      "SendSpeed",
      4096,
      NULL,
      1,
      &send_speed_handle
    );
}

void loop() {
    CAN_FRAME can_message;

    if (CAN0.read(can_message)) {

      if (can_message.id == 721) {
        uint8_t byte1 = can_message.data.byte[1]; // Second byte (LSB)
        uint8_t byte2 = can_message.data.byte[2]; // Third byte (MSB)

        uint16_t big_endian_value = (byte2 << 8) | byte1;
        
        float mph = (big_endian_value * 0.621371) / 10; // Convert kph to mph
        int mph_int = std::round(mph);   // Convert to an integer

        CANMessage.speed_mph = mph_int;

        // Print the result
        Serial.print("MPH: ");
        Serial.println(mph_int);
      }

      if (can_message.id == 573) {
        uint8_t byte1 = can_message.data.byte[3]; // Second byte (LSB)
        uint8_t byte2 = can_message.data.byte[4]; // Third byte (MSB)

        uint16_t big_endian_value = (byte2 << 8) | byte1;
        int rpm_int = (int)(big_endian_value * 3.463);

        CANMessage.speed_rpm = rpm_int;

        // Print the result
        Serial.print("RPM: ");
        Serial.println(rpm_int);
      }
    }
}