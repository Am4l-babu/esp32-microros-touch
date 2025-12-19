#include <Arduino.h>
#include <micro_ros_platformio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/bool.h>
#include <secrets.h>
// DEFINES
#define LED_BUILTIN 2 // Ensure this matches your board
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){error_loop();}}
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if((temp_rc != RCL_RET_OK)){}}

// VARIABLES
rcl_publisher_t publisher;
rcl_subscription_t subscriber;
std_msgs__msg__Int32 msg;
std_msgs__msg__Bool led_msg;
rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;
rcl_timer_t timer;

// Error loop
void error_loop(){
  while(1){
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
  }
}

// CALLBACK: Runs when LED command is received
void subscription_callback(const void * msgin)
{  
  const std_msgs__msg__Bool * msg = (const std_msgs__msg__Bool *)msgin;
  // If true -> HIGH (ON), If false -> LOW (OFF)
  digitalWrite(LED_BUILTIN, (msg->data) ? HIGH : LOW);  
}

// TIMER: Publishes counter
void timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{  
  RCLC_UNUSED(last_call_time);
  if (timer != NULL) {
    RCSOFTCHECK(rcl_publish(&publisher, &msg, NULL));
    msg.data++;
  }
}

void setup() {
  Serial.begin(115200);
  set_microros_serial_transports(Serial); 

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); 

  delay(2000);

  allocator = rcl_get_default_allocator();

  // Create init_options
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));

  // Create node
  RCCHECK(rclc_node_init_default(&node, "esp32_node", "", &support));

  // Create PUBLISHER
  RCCHECK(rclc_publisher_init_default(
    &publisher,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
    "micro_ros_counter"));

  // Create SUBSCRIBER
  RCCHECK(rclc_subscription_init_default(
    &subscriber,
    &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
    "led_cmd"));

  // Create TIMER (Corrected: Removed the extra message type argument)
  RCCHECK(rclc_timer_init_default(
    &timer,
    &support,
    RCUTILS_MS_TO_NS(1000), // 1 second interval
    timer_callback));

  // Create executor (2 handles: timer + subscriber)
  RCCHECK(rclc_executor_init(&executor, &support.context, 2, &allocator));
  RCCHECK(rclc_executor_add_timer(&executor, &timer));
  RCCHECK(rclc_executor_add_subscription(&executor, &subscriber, &led_msg, &subscription_callback, ON_NEW_DATA));
}

void loop() {
  delay(100);
  RCCHECK(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)));
}