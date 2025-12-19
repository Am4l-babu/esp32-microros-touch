#include <Arduino.h>
#include <WiFi.h>
#include <micro_ros_platformio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/string.h>
#include "secrets.h"

// --- CONFIGURATION ---
#define LED_PIN 2        // Internal LED
#define TOUCH_PIN 4      // GPIO 4 is Touch0
#define THRESHOLD 30     // Values below this = "Touched"

// Formatting IP for the WiFi transport function
IPAddress agent_ip(192, 168, 1, 6); 
size_t agent_port = 8888;

// --- ROS VARIABLES ---
rcl_publisher_t publisher;
std_msgs__msg__String msg;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;
rclc_executor_t executor;

// Error handling macros
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

void error_loop(){
  while(1){
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
}

void timer_callback(rcl_timer_t * timer, int64_t last_call_time) {
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    int touch_val = touchRead(TOUCH_PIN);
    
    // Logic: If touched, send "PRESSED", otherwise send "IDLE"
    if (touch_val < THRESHOLD) {
      msg.data.data = (char*)"PRESSED";
      digitalWrite(LED_PIN, HIGH);
    } else {
      msg.data.data = (char*)"IDLE";
      digitalWrite(LED_PIN, LOW);
    }
    
    msg.data.size = strlen(msg.data.data);
    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  
  // 1. Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  // 2. Set Micro-ROS to WiFi (UDP)
  // We cast the const char* from secrets.h to char* to satisfy the function signature
  set_microros_wifi_transports((char*)WIFI_SSID, (char*)WIFI_PASSWORD, agent_ip, agent_port);
  delay(2000);

  // 3. Initialize Micro-ROS entities
  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "wifi_touch_node", "", &support));

  RCCHECK(rclc_publisher_init_default(
    &publisher, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String),
    "touch_status"));

  RCCHECK(rclc_timer_init_default(&timer, &support, RCUTILS_MS_TO_NS(100), timer_callback));
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));

  msg.data.capacity = 20;
}

void loop() {
  // Use spin_some to handle the timer callback
  RCSOFTCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));
}