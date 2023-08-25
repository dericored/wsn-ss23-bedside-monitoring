#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include "dev/adc.h"
#include "dev/adc-zoul.h"      // ADC
#include "dev/zoul-sensors.h"  // Sensor functions
#include "dev/sys-ctrl.h"
#include "dev/cc2538-rf.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "project-conf.h"
#include "dev/gpio.h"
#include "dev/nvic.h"
#include "dev/ioc.h"
#include "random.h"

#define MAX_RETRANSMISSIONS 10 
#define THRESHOLD       1050         // Threshold for peak detection
#define PIN_NUM 0
#define MAX_HISTORY_SIZE 5



PROCESS(Patient_Node_SenseSend, "Patient Node SenseSend");
PROCESS(Routing_Node_Discovery, "Patient Node Process");
PROCESS(pulse_sensor_process, "Pulse Sensor Process");
PROCESS(motion_sensor_process,"Motion Sensor Process");


AUTOSTART_PROCESSES(&Routing_Node_Discovery,&pulse_sensor_process,&motion_sensor_process);

static struct broadcast_conn broadcast;
static struct runicast_conn runicast;
static process_event_t gpio_interrupt_event;

// Define a struct for the patient message
struct patient_message {
  uint8_t sensor_value;
  bool Patient_Motion;
 linkaddr_t history[MAX_HISTORY_SIZE]; 
 int depth;
};

// Global variables
static int transmission_power = 0; // Initial transmission power in dBm
static bool routing_node_found = false;
static bool Transmission_Failed;
static bool Discovery_Failed = false;
static linkaddr_t routing_node_id;
static int pulseSensorValue;       // Raw sensor value
static uint8_t bpm;                    // Calculated heart rate (BPM)
static int CurrentPeak = 0;
static float currentTime;
static float CurrentPeak_Time;
static float PreviousPeak_Time;
static float timeElapsed;
static bool Patient_in_Bed = true;

// Broadcast connection handler
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from) {
  // Ignore broadcast messages if a routing node has already been found
  if (routing_node_found) {
    return;
  }
  
  // Check if the received message is from a routing node--------------------------------------------
  printf("0x%x%x\n\r",from->u8[0],from->u8[1]);
  if (from->u8[0] >= 0x80 && from->u8[0]!= 0xFF && from->u8[1] != 0xFF) {
    // Set the flag to indicate a routing node has been discovered
    routing_node_found = true;
    printf("Routing node found!\n\r");
    routing_node_id.u8[0] = from->u8[0];
    routing_node_id.u8[1] = from->u8[1];
  }
}


static const struct broadcast_callbacks broadcast_callbacks = {broadcast_recv};

// Runicast connection handler
static void runicast_recv(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
  // Ignore received messages from other nodes
}

static void runicast_sent(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
  printf("data msg sent\n\r");
}

static void runicast_timedout(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions) {
  // Transmission timed out, indicating a failure
  printf("Runicast packet timed out. Failed to send\n\r");
  Transmission_Failed = true;
  routing_node_found = false;
  process_start(&Routing_Node_Discovery,NULL);
  }

static const struct runicast_callbacks runicast_callbacks = {runicast_recv,runicast_sent,runicast_timedout};

// Function to adjust the transmission power
static void adjust_transmission_power(int power) {
  // Check if the new power level is within the allowable range
  if (power >= 0 && power <= 7) {
    transmission_power = power;
    NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, transmission_power);
  }
}

// Function to send the patient message to the discovered routing node
static void send_patient_message() {
  leds_on(LEDS_GREEN);
  struct patient_message msg;

  msg.history->u8[0] = (linkaddr_node_addr.u8[0] & 0xFF);
  msg.history->u8[1] = (linkaddr_node_addr.u8[1] & 0xFF);

  msg.sensor_value = bpm;
  msg.Patient_Motion = Patient_in_Bed;

  // dummy message
  // msg.sensor_value = 80 + 40 * random_rand()/RANDOM_RAND_MAX;
  // if (random_rand()/RANDOM_RAND_MAX > 0.7) msg.Patient_Motion = false;
  // else msg.Patient_Motion = true;

  msg.depth = 0;

  packetbuf_clear();
  packetbuf_copyfrom(&msg, sizeof(struct patient_message));
  runicast_send(&runicast, &routing_node_id, MAX_RETRANSMISSIONS);
  leds_off(LEDS_GREEN);
}

// Function to calculate heart rate (BPM)
void calculate_bpm() {
  currentTime = clock_time()/(float)128;
  // Detect a beat by finding the peak of the pulse
  if (pulseSensorValue > THRESHOLD && pulseSensorValue > CurrentPeak) {
    // Calculate the time elapsed since the previous beat
    CurrentPeak = pulseSensorValue;
    CurrentPeak_Time = currentTime;
  }
  else if (pulseSensorValue < THRESHOLD && CurrentPeak){
    timeElapsed = (CurrentPeak_Time - PreviousPeak_Time);
    bpm = 60.0/timeElapsed;
    PreviousPeak_Time = CurrentPeak_Time;
    CurrentPeak = 0;
  }
}

// Define the GPIO callback function
static void gpio_callback(uint8_t port, uint8_t pin){
  // Call the GPIO interrupt handler function
  leds_toggle(LEDS_RED);
  if (Patient_in_Bed) {
    Patient_in_Bed = false;
    printf("Patient Moved Out");
    }
  else {
    Patient_in_Bed = true;
    printf("Patient Moved Back");
  }
  process_post(&motion_sensor_process,gpio_interrupt_event,NULL);
}

 

PROCESS_THREAD(Routing_Node_Discovery, ev, data) {
  static struct etimer routing_node_timer;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)

  PROCESS_BEGIN();
  process_exit(&Patient_Node_SenseSend);


  // Initialize the broadcast connection
  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL, 11);
  broadcast_open(&broadcast, 129, &broadcast_callbacks);
  
  // Start the routing node discovery process
  adjust_transmission_power(transmission_power); // Adjust power to initial value

  etimer_set(&routing_node_timer, CLOCK_SECOND * (0.5 + 0.5 *random_rand()/RANDOM_RAND_MAX)); // Set timeout for routing node discovery


  while (!routing_node_found && transmission_power <= 7) {

    leds_on(LEDS_BLUE);
    // Increase the transmission power
  adjust_transmission_power(transmission_power);

  struct patient_message msg;

  msg.history->u8[0] = (linkaddr_node_addr.u8[0] & 0xFF);
  msg.history->u8[1] = (linkaddr_node_addr.u8[1] & 0xFF);

  msg.sensor_value = bpm;

  //dummy message
  msg.sensor_value = 80 + 40 * random_rand()/RANDOM_RAND_MAX;
  if (random_rand()/RANDOM_RAND_MAX > 0.7) msg.Patient_Motion = false;
  else msg.Patient_Motion = true;

  // msg.Patient_Motion = Patient_in_Bed;

  // msg.depth = 0;

  packetbuf_clear();
  packetbuf_copyfrom(&msg, sizeof(struct patient_message));



    broadcast_send(&broadcast);
    printf("search msg sent\n\r");
    // Wait for a response from the routing node
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER && data == &routing_node_timer);

    transmission_power++;
    etimer_set(&routing_node_timer, CLOCK_SECOND * (0.5 + 0.5 *random_rand()/RANDOM_RAND_MAX)); // Set timeout for routing node discovery
    leds_off(LEDS_BLUE);
  }

  // Check if a routing node was found
  if (!routing_node_found) {
     // Indicate that no routing node was found
    leds_on(LEDS_PURPLE);
    transmission_power = 0;
    Discovery_Failed = true;
    PROCESS_EXIT(); // Exit the process as there is no routing node to communicate with
  }
  else {
    process_start(&Patient_Node_SenseSend,NULL);
  }
  PROCESS_END();
}



PROCESS_THREAD(Patient_Node_SenseSend, ev, data) {

  PROCESS_EXITHANDLER(broadcast_close(&broadcast); runicast_close(&runicast);)

  PROCESS_BEGIN();
  static struct etimer sensor_timer;

  Transmission_Failed = false;

  leds_off(LEDS_BLUE);
  // Start the sensor reading and transmission process
  etimer_set(&sensor_timer, CLOCK_SECOND * (0.5 + 0.5 *random_rand()/RANDOM_RAND_MAX)); // Set the sensor reading interval
  runicast_open(&runicast, 146, &runicast_callbacks);
  while (1) {
    PROCESS_WAIT_EVENT();

    if (ev == PROCESS_EVENT_TIMER && data == &sensor_timer) {
      // Send the patient message to the routing node
      send_patient_message();
      if (Transmission_Failed == true){PROCESS_EXIT();}

      // Restart the timer
      etimer_set(&sensor_timer,  CLOCK_SECOND * (0.5 + 0.5 *random_rand()/RANDOM_RAND_MAX)); // Set the sensor reading interval
    }
  }

  PROCESS_END();
}


PROCESS_THREAD(pulse_sensor_process, ev, data) {
  
  PROCESS_BEGIN();

  static struct etimer sensor_timer;
  static struct etimer rediscover_timer;

  // Initialize the ADC
	adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC1);

  // Set initial values
  PreviousPeak_Time = clock_time();

  // Main loop
  while (1) {
    // Read the raw sensor value from the Pulse Sensor
    pulseSensorValue = adc_zoul.value(ZOUL_SENSORS_ADC1) >> 4;

    // Process the sensor value and calculate the heart rate
    calculate_bpm();

    // Print the heart rate
    // printf("Heart Rate: %d BPM    %d\n\r", bpm,pulseSensorValue);

    // Set the timer for the next sensor reading
    etimer_set(&sensor_timer, 0.2 * CLOCK_SECOND); // Adjust the interval as needed
    if (Discovery_Failed){
       etimer_set(&rediscover_timer,4.9 * CLOCK_SECOND);
       Discovery_Failed = false;
    }

    // Wait for the timer to expire
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER && (data == &sensor_timer||data == &rediscover_timer));
    if (data == &rediscover_timer){ 
      printf("restart discovery\n\r");
      leds_off(LEDS_PURPLE);
      process_start(&Routing_Node_Discovery,NULL);
  }

  PROCESS_END();
}
}

PROCESS_THREAD(motion_sensor_process, ev, data)
{
  PROCESS_BEGIN();

//Configure the PD0 pin
  GPIO_SOFTWARE_CONTROL(GPIO_PORT_TO_BASE(GPIO_D_NUM), GPIO_PIN_MASK(PIN_NUM));
  GPIO_SET_INPUT(GPIO_PORT_TO_BASE(GPIO_D_NUM), GPIO_PIN_MASK(PIN_NUM));
  GPIO_DETECT_RISING(GPIO_PORT_TO_BASE(GPIO_D_NUM),GPIO_PIN_MASK(PIN_NUM));
  GPIO_TRIGGER_SINGLE_EDGE(GPIO_PORT_TO_BASE(GPIO_D_NUM),GPIO_PIN_MASK(PIN_NUM));
  // GPIO_DETECT_EDGE(GPIO_PORT_TO_BASE(GPIO_D_NUM),GPIO_PIN_MASK(PIN_NUM));
  GPIO_ENABLE_INTERRUPT(GPIO_PORT_TO_BASE(GPIO_D_NUM),GPIO_PIN_MASK(PIN_NUM));
  ioc_set_over(GPIO_D_NUM, PIN_NUM, IOC_OVERRIDE_PUE);
  NVIC_EnableIRQ(GPIO_D_IRQn);

  gpio_interrupt_event = process_alloc_event();

    // Register the GPIO interrupt handler function
  gpio_register_callback(gpio_callback, GPIO_D_NUM, PIN_NUM);


  while (1) {
    // Wait for an event to occur
    PROCESS_YIELD_UNTIL(ev==gpio_interrupt_event);
  }

  PROCESS_END();
}