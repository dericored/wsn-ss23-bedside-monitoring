#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include "dev/sys-ctrl.h"
#include "dev/cc2538-rf.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "project-conf.h"
#include "random.h"

#define MAX_NEIGHBORS 4
#define MAX_NODES 7
#define MAX_HISTORY_SIZE 5
#define MAX_MESSAGE_NUM 5

static bool nei_disc = true;
static bool net_disc = false;
linkaddr_t nextHop;
linkaddr_t empty_addr;
uint8_t minHops = MAX_NODES;
linkaddr_t User_node = {.u8[0] = 0xFF, .u8[1] = 0xFF};

struct neighbor_entry {
  linkaddr_t neighbor_addr;
};

struct neighbor_table {
  struct neighbor_entry entries[MAX_NEIGHBORS];
  uint8_t num_entries;
};

struct neighbor_database{
  struct neighbor_table neighbor_table[MAX_NODES];
  uint8_t num_nodes;
};

static struct broadcast_conn broadcast;

PROCESS(network_discovery, "Network Discovery");
PROCESS(combine_patients,"Combine Patients");
AUTOSTART_PROCESSES(&network_discovery);

static struct neighbor_table neighbor_table;

static struct neighbor_table empty_table;

static struct neighbor_database my_database;

static struct neighbor_database empty_database;

static void broadcast_neighbor_table() {
  leds_on(LEDS_BLUE);
  packetbuf_clear();
  packetbuf_copyfrom("reset",6);
  broadcast_send(&broadcast);
  leds_off(LEDS_BLUE);
}

static void broadcast_neighbor_database() {
  leds_on(LEDS_WHITE);
  packetbuf_clear();
  packetbuf_copyfrom(&my_database, sizeof(struct neighbor_database));
  broadcast_send(&broadcast);
  leds_off(LEDS_WHITE);
}

static void update_own_neighbors(linkaddr_t *from){
 bool new = false;
 for(int i = 0; i < neighbor_table.num_entries;i++){
   if (linkaddr_cmp(&neighbor_table.entries[i].neighbor_addr,from)==0){
       new = true;
     }
   else if (linkaddr_cmp(&neighbor_table.entries[i].neighbor_addr,from)!=0){
       new = false;
       break;
     }
   }
 if (new){
       neighbor_table.entries[neighbor_table.num_entries].neighbor_addr = *from;
       neighbor_table.num_entries++;
   }
  packetbuf_clear();
}

static void update_neighbor_database(struct neighbor_database recv_database){
  bool new = false;
  for(int i = 0; i < recv_database.num_nodes;i++){
    for(int j = 0; j < my_database.num_nodes;j++){
      if(linkaddr_cmp(&my_database.neighbor_table[j].entries[0].neighbor_addr,&recv_database.neighbor_table[i].entries[0].neighbor_addr)==0){
        new = true;
      }
      else if(linkaddr_cmp(&my_database.neighbor_table[j].entries[0].neighbor_addr,&recv_database.neighbor_table[i].entries[0].neighbor_addr)!=0){
        new = false;
        break;
      }
    }
    if(new){
      my_database.neighbor_table[my_database.num_nodes] = recv_database.neighbor_table[i];
      my_database.num_nodes++;
      printf("update %d\n\r",my_database.num_nodes);
    }
  }
  packetbuf_clear();
}

static void print_neighbor_table() {
  int i;
  printf("Neighbor Table:\n\r");
  for (i = 0; i < neighbor_table.num_entries; i++) {
    printf("Neighbor %d: %x:%x      ",i,
           neighbor_table.entries[i].neighbor_addr.u8[0], neighbor_table.entries[i].neighbor_addr.u8[1]);
  }
  printf("\n\r");
}

static void print_neighbor_database() {
  printf("Neighbor Database: number of nodes %d\n\r",my_database.num_nodes);
  for (int i = 0; i < my_database.num_nodes; i++) {
    printf("Node %x%x, with %d neighbors:\n\r",my_database.neighbor_table[i].entries[0].neighbor_addr.u8[0],
                                               my_database.neighbor_table[i].entries[0].neighbor_addr.u8[1],
                                               my_database.neighbor_table[i].num_entries-1);
    for(int j = 1; j < my_database.neighbor_table[i].num_entries;j++){
      printf("%d  %x%x       ",j,my_database.neighbor_table[i].entries[j].neighbor_addr.u8[0],my_database.neighbor_table[i].entries[j].neighbor_addr.u8[1]);
    }
    printf("\n\r");
  }
}


void exploreNeighbors(linkaddr_t currentNode, uint8_t hops)
{
  hops++;
  if(hops>=MAX_NODES-1){
    if(linkaddr_cmp(&currentNode,&User_node)){
      if(hops<minHops){
        minHops = hops;
      }
    }
    hops--;
    return;
  }

    else{
      if(linkaddr_cmp(&currentNode,&User_node)){
        if(hops<minHops){
          minHops = hops;
          hops--;
          return;
        }
       }
      for (uint8_t i = 0; i < MAX_NODES; i++){

      if (linkaddr_cmp(&currentNode, &(my_database.neighbor_table[i].entries[0].neighbor_addr))!=0)
      {
       for (uint8_t j = 0; j < my_database.neighbor_table[i].num_entries; j++)
        {
           exploreNeighbors(my_database.neighbor_table[i].entries[j].neighbor_addr, hops);
        }
     }
    }
  }
}

uint8_t smallestPath;


static void choosePath(){
  smallestPath = 100;
  nextHop = empty_addr;
  printf("num of neighbors %d\n\r",my_database.neighbor_table[0].num_entries);
  for (uint8_t j = 1; j < my_database.neighbor_table[0].num_entries; j++)
        {
          printf("explore neighbor %d\n\r",j);
           exploreNeighbors(my_database.neighbor_table[0].entries[j].neighbor_addr, 1);
           if (smallestPath > minHops){
            smallestPath = minHops;
            nextHop = my_database.neighbor_table[0].entries[j].neighbor_addr;
            printf("bingo\n\r");
           }            
           minHops = 100;
    }
  printf("next hop %x%x\n\r",nextHop.u8[0],nextHop.u8[1]);
}







static void change_process_1(){
  choosePath();
  nei_disc = true;
  net_disc = false;
  broadcast_close(&broadcast);
  process_start(&combine_patients,NULL);
}

static void change_process_2(){
  broadcast_close(&broadcast);
  process_start(&network_discovery,NULL);
}

static void broadcast_recv_1(struct broadcast_conn *c, const linkaddr_t *from){
    if(from->u8[0]>=0x80 && nei_disc){
      update_own_neighbors(from);
    }
    if(from->u8[0]>=0x80 && net_disc){
      struct neighbor_database received_database;
      packetbuf_copyto(&received_database);
      update_neighbor_database(received_database);
    } 
}

static void change_function(){
  my_database.neighbor_table[0] = neighbor_table;
  my_database.num_nodes = 1;
  nei_disc = false;
  net_disc = true;
  broadcast_close(&broadcast);
  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL, 12);
  static struct broadcast_callbacks callbacks = {broadcast_recv_1};
  broadcast_open(&broadcast,129,&callbacks);
  packetbuf_copyfrom(&my_database,sizeof(struct neighbor_database));
  broadcast_send(&broadcast);
  packetbuf_clear();
}






static int count = 0;
static bool printed = false;

struct patient_message {
  uint8_t sensor_value;
  bool Patient_Motion;
  linkaddr_t history[MAX_HISTORY_SIZE];
  int depth;
};

struct routing_message{
  int count;
  struct patient_message patient_messages[MAX_MESSAGE_NUM];
};

struct etimer forward_data_timer;

struct routing_message message;
struct routing_message printer;
struct routing_message routing_empty;

struct patient_message patient;
struct patient_message patient_empty;

static void broadcast_recv_2(struct broadcast_conn *c, const linkaddr_t *from) {
 printf("brodcast from %x\n\r", from->u8[0]);
 if(from->u8[0]<0x80){
  leds_on(LEDS_PURPLE);
  printf("patient detected\n\r");
  packetbuf_clear();
  packetbuf_copyfrom("connect",8);
  broadcast_send(&broadcast);
  leds_off(LEDS_PURPLE);
 }
 else{
  char info[8] ={0};
  printf("br recv\n\r");
  packetbuf_copyto(&info);
  for(int k = 0;k<5;k++) printf("%c",info[k]);
  if (strcmp(info,"Reset")==0){
    printf("reset for\n\r");
    leds_on(LEDS_RED);
    packetbuf_clear();
    packetbuf_copyfrom("Reset",6);
    broadcast_send(&broadcast);
    process_start(&network_discovery,NULL);
    leds_off(LEDS_RED);
  }
 }
}

static struct runicast_conn runicast;

static void runicast_sent(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
  printf("combined msg sent\n\r");
}

static void runicast_timedout(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions) {
  // Transmission timed out, indicating a failure
  printf("combined packet timed out. Failed to send\n\r");
  packetbuf_clear();
  packetbuf_copyfrom("Reset",6);
  broadcast_send(&broadcast);
  process_start(&network_discovery,NULL);
  }

// Runicast connection handler
static void runicast_recv(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
  leds_on(LEDS_YELLOW);
  printf("runicast received\n\r");
  if(from->u8[0]<0x80){
  packetbuf_copyto(&patient);
  printf("patient data received\n\r");

  if (count<MAX_MESSAGE_NUM){
  
  message.patient_messages[count] = patient;


  message.patient_messages[count].history[1].u8[0] = linkaddr_node_addr.u8[0];
  message.patient_messages[count].history[1].u8[1] = linkaddr_node_addr.u8[1];
  message.patient_messages[count].depth++;

  count++;

  message.count = count;

  patient = patient_empty;
  }

  packetbuf_clear();

  }
  else if (from->u8[0]>=0x80 && from->u8[0]!=0xFF && from->u8[1]!=0xFF){
    printf("combined patients received\n\r");
    struct routing_message routing;
    packetbuf_copyto(&routing);
    for(int i = 0; i < routing.count;i++){

      if(count < MAX_MESSAGE_NUM){

      message.patient_messages[count] = routing.patient_messages[i];

      message.patient_messages[count].depth++;

      if (message.patient_messages[count].depth<MAX_HISTORY_SIZE){
      message.patient_messages[count].history[message.patient_messages[count].depth].u8[0] = linkaddr_node_addr.u8[0];
      message.patient_messages[count].history[message.patient_messages[count].depth].u8[1] = linkaddr_node_addr.u8[1];
      }
      count++;

      message.count = count;
      }
    }
    packetbuf_clear();
    routing = routing_empty;
    forward_data();
  }
  leds_off(LEDS_YELLOW);
}


void forward_data(){
  leds_on(LEDS_GREEN);
  printf("initiate print %d\n\r",message.count);
    
  for(int i = 0; i < message.count; i++){
    for(int j = 0; j <= message.patient_messages[i].depth; j++){
        printf("message %d, layer %d  %x%x\n\r",i,j,message.patient_messages[i].history[j].u8[0],message.patient_messages[i].history[j].u8[1]);
    }
  }
  packetbuf_copyfrom(&message,sizeof(struct routing_message));
  runicast_send(&runicast,&nextHop,(uint8_t)5);
  printf("\rrunicast sent to:%x%x\n\r",nextHop.u8[0],nextHop.u8[1]);
  printed = false;
  message = routing_empty;
  count = 0;
  message.count = count;
  leds_off(LEDS_GREEN);
}

PROCESS_THREAD(network_discovery, ev, data) {

  PROCESS_EXITHANDLER(printf("Discovery Process Ended\n\r"));
  PROCESS_BEGIN();

  process_exit(&combine_patients);

  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL, 11);
  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, 7);

  static struct etimer periodic_timer;
  
  static struct ctimer neighbor_discovery_timer;

  static struct ctimer change_process_timer;

  static struct broadcast_callbacks callbacks = {broadcast_recv_1};

  ctimer_set(&neighbor_discovery_timer,3*CLOCK_SECOND,&change_function,NULL);

  ctimer_set(&change_process_timer,6*CLOCK_SECOND,&change_process_1,NULL);

  broadcast_open(&broadcast,129,&callbacks);

  packetbuf_clear();
  packetbuf_copyfrom("Reset",6);
  broadcast_send(&broadcast);

  neighbor_table.num_entries = 1;

  neighbor_table.entries[0].neighbor_addr = linkaddr_node_addr;

  etimer_set(&periodic_timer, CLOCK_SECOND * (0.2 + 0.2 * random_rand()/RANDOM_RAND_MAX));

  while (1) {
    PROCESS_WAIT_EVENT();    
    
    if(nei_disc) broadcast_neighbor_table();

    if(net_disc) broadcast_neighbor_database();
    
    if(nei_disc) print_neighbor_table();

    if(net_disc) print_neighbor_database();

    etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}

void test_broadcast(){
  static const struct broadcast_callbacks broadcast_callbacks = {broadcast_recv_2};
    broadcast_open(&broadcast,129,&broadcast_callbacks);
    my_database = empty_database;
    neighbor_table = empty_table;
}

PROCESS_THREAD(combine_patients,ev,data){
  PROCESS_EXITHANDLER(printf("Combine Patients Process Ended\n\r");runicast_close(&runicast));
  PROCESS_BEGIN();

  static struct ctimer start_comms;

  static struct ctimer change_process_timer;

  ctimer_set(&change_process_timer,120*CLOCK_SECOND,&change_process_2,NULL);
  ctimer_set(&start_comms,2*CLOCK_SECOND,&test_broadcast,NULL);

  process_exit(&network_discovery);

  static const struct runicast_callbacks runicast_callbacks = {runicast_recv,runicast_sent,runicast_timedout};


  packetbuf_clear();
  printf("start forwarding\n\r");

  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL, 11);
  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, 7);


  runicast_open(&runicast,146,&runicast_callbacks);
  message.count = count;

  etimer_set(&forward_data_timer,CLOCK_SECOND * (1 + 0.5 * random_rand()/RANDOM_RAND_MAX));
  while(1){
  packetbuf_clear();
  PROCESS_WAIT_EVENT();
  if (printed == false ){
    // printer = message;
    printf("copied to printer\n\r");
    printed = true;
  }
  if (count > 0) forward_data();
  
  etimer_reset(&forward_data_timer);
  }
PROCESS_END();
}













/*
#include "contiki.h"
#include "net/rime/rime.h"
#include "dev/leds.h"
#include "dev/sys-ctrl.h"
#include "dev/cc2538-rf.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "project-conf.h"
#include "random.h"

#define MAX_NEIGHBORS 4
#define MAX_NODES 7
#define MAX_HISTORY_SIZE 5
#define MAX_MESSAGE_NUM 5

static bool nei_disc = true;
static bool net_disc = false;

struct neighbor_entry {
  linkaddr_t neighbor_addr;
};

struct neighbor_table {
  struct neighbor_entry entries[MAX_NEIGHBORS];
  uint8_t num_entries;
};

struct neighbor_database{
  struct neighbor_table neighbor_table[MAX_NODES];
  uint8_t num_nodes;
};

static struct broadcast_conn broadcast;

PROCESS(network_discovery, "Network Discovery");
PROCESS(receive_msgs,"Receive msgs");
AUTOSTART_PROCESSES(&network_discovery);

static struct neighbor_table neighbor_table;

static struct neighbor_database my_database;

//struct etimer read_data_timer;


static void broadcast_neighbor_table() {
  leds_on(LEDS_BLUE);
  packetbuf_clear();
  packetbuf_copyfrom(&neighbor_table, sizeof(struct neighbor_table));
  broadcast_send(&broadcast);
  leds_off(LEDS_BLUE);
}

static void broadcast_neighbor_database() {
  leds_on(LEDS_WHITE);
  packetbuf_clear();
  packetbuf_copyfrom(&my_database, sizeof(struct neighbor_database));
  broadcast_send(&broadcast);
  leds_off(LEDS_WHITE);
}

static void update_own_neighbors(linkaddr_t *from){
 bool new = false;
 for(int i = 0; i < neighbor_table.num_entries;i++){
   if (!linkaddr_cmp(&neighbor_table.entries[i].neighbor_addr,from)){
       new = true;
     }
   else if (linkaddr_cmp(&neighbor_table.entries[i].neighbor_addr,from)){
       new = false;
       break;
     }
   }
 if (new){
       neighbor_table.entries[neighbor_table.num_entries].neighbor_addr = *from;
       neighbor_table.num_entries++;
   }
}

static void update_neighbor_database(struct neighbor_database recv_database){
  bool new = false;
  for(int i = 0; i < recv_database.num_nodes;i++){
    for(int j = 0; j < my_database.num_nodes;j++){
      if(!linkaddr_cmp(&my_database.neighbor_table[j].entries[0].neighbor_addr,&recv_database.neighbor_table[i].entries[0].neighbor_addr)){
        new = true;
      }
      else if(linkaddr_cmp(&my_database.neighbor_table[j].entries[0].neighbor_addr,&recv_database.neighbor_table[i].entries[0].neighbor_addr)){
        new = false;
        break;
      }
    }
    if(new){
      my_database.neighbor_table[my_database.num_nodes] = recv_database.neighbor_table[i];
      my_database.num_nodes++;
      printf("update %d\n\r",my_database.num_nodes);
    }
  }

}

static void print_neighbor_table() {
  int i;
  printf("Neighbor Table:\n\r");
  for (i = 0; i < neighbor_table.num_entries; i++) {
    printf("Neighbor %d: %x:%x      ",i,
           neighbor_table.entries[i].neighbor_addr.u8[0], neighbor_table.entries[i].neighbor_addr.u8[1]);
  }
  printf("\n\r");
}

static void print_neighbor_database() {
  printf("Neighbor Database: number of nodes %d\n\r",my_database.num_nodes);
  for (int i = 0; i < my_database.num_nodes; i++) {
    printf("Node %x%x, with %d neighbors:\n\r",my_database.neighbor_table[i].entries[0].neighbor_addr.u8[0],
                                               my_database.neighbor_table[i].entries[0].neighbor_addr.u8[1],
                                               my_database.neighbor_table[i].num_entries-1);
    for(int j = 1; j < my_database.neighbor_table[i].num_entries;j++){
      printf("%d  %x%x       ",j,my_database.neighbor_table[i].entries[j].neighbor_addr.u8[0],my_database.neighbor_table[i].entries[j].neighbor_addr.u8[1]);
    }
    printf("\n\r");
  }
}

static void change_function(){
  my_database.neighbor_table[0] = neighbor_table;
  my_database.num_nodes = 1;
  nei_disc = false;
  net_disc = true;
  packetbuf_copyfrom(&my_database,sizeof(struct neighbor_database));
  broadcast_send(&broadcast);
  packetbuf_clear();
}

static void change_process_1(){
  next_hop = next_hop_function;
  process_start(&receive_msgs,NULL);
}

static void change_process_2(){
  process_start(&network_discovery,NULL);
}

static void broadcast_recv_1(struct broadcast_conn *c, const linkaddr_t *from){
    if(from->u8[0]>=0x80 && nei_disc){
      update_own_neighbors(from);
      packetbuf_clear();
    }
    if(from->u8[0]>=0x80 && net_disc){
      struct neighbor_database received_database;
      packetbuf_copyto(&received_database);
      update_neighbor_database(received_database);
      packetbuf_clear();
    } 
}



static int count = 0;
static bool printed = false;

struct patient_message {
  uint16_t sensor_value;
  bool Patient_Motion;
  linkaddr_t history[MAX_HISTORY_SIZE];
  int depth;
};

struct routing_message{
  int count;
  struct patient_message patient_messages[MAX_MESSAGE_NUM];
};

struct routing_message message;
struct routing_message printer;
struct routing_message routing_empty;

struct patient_message patient;
struct patient_message patient_empty;

static void broadcast_recv_2(struct broadcast_conn *c, const linkaddr_t *from) {
 if(from->u8[0]>0x80){
  char info[8];
  packetbuf_copyto(&info);
  if (!strcmp(info,"Reset")){
    leds_on(LEDS_RED);
    packetbuf_clear();
    packetbuf_copyfrom("Reset",6);
    broadcast_send(&broadcast);
    process_start(&network_discovery,NULL);
    leds_off(LEDS_RED);
  }
 }
}

static struct runicast_conn runicast;

static void runicast_sent(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){
//  printf("combined msg sent\n\r");
}

static void runicast_timedout(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions) {
  // // Transmission timed out, indicating a failure
  //printf("combined packet timed out. Failed to send\n\r");
  //packetbuf_clear();
  //packetbuf_copyfrom("Reset",6);
  //broadcast_send(&broadcast);
  //process_start(&network_discovery,NULL);
  }

// Runicast connection handler
static void runicast_recv(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) {
  leds_on(LEDS_YELLOW);
  printf("runicast received\n\r");
  if (from->u8[0]>=0x80){
    printf("combined patients received\n\r");
    struct routing_message printer;
    printf("initiate print %d\n\r",printer.count);
    
    for(int i = 0; i < printer.count; i++){
      for(int j = 0; j <= printer.patient_messages[i].depth; j++){
          printf("message %d, layer %d  %x%x\n\r",i,j,printer.patient_messages[i].history[j].u8[0],printer.patient_messages[i].history[j].u8[1]);
      }
    }

    printer = routing_empty;
    
    /*packetbuf_copyto(&routing);
    for(int i = 0; i < routing.count;i++){

      if(count < MAX_MESSAGE_NUM){

      message.patient_messages[count] = routing.patient_messages[i];

      message.patient_messages[count].depth++;

      if (message.patient_messages[count].depth<MAX_HISTORY_SIZE){
      message.patient_messages[count].history[message.patient_messages[count].depth].u8[0] = linkaddr_node_addr.u8[0];
      message.patient_messages[count].history[message.patient_messages[count].depth].u8[1] = linkaddr_node_addr.u8[1];
      }
      count++;

      message.count = count;
      }
    }
    packetbuf_clear();
    routing = routing_empty;
    forward_data();
    *//*
  }
  leds_off(LEDS_YELLOW);
}





PROCESS_THREAD(network_discovery, ev, data) {

  PROCESS_EXITHANDLER(printf("Discovery Process Ended\n\r"));
  PROCESS_BEGIN();

  process_exit(&receive_msgs);

  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL, 11);
  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, 7);

  static struct etimer periodic_timer;
  
  static struct ctimer neighbor_discovery_timer;

  static struct ctimer change_process_timer;

  static struct broadcast_callbacks callbacks = {broadcast_recv_1};

  ctimer_set(&neighbor_discovery_timer,CLOCK_SECOND,&change_function,NULL);

  ctimer_set(&change_process_timer,2*CLOCK_SECOND,&change_process_1,NULL);

  broadcast_open(&broadcast,129,&callbacks);

  packetbuf_clear();
  packetbuf_copyfrom("Reset",6);
  broadcast_send(&broadcast);

  neighbor_table.num_entries = 1;

  neighbor_table.entries[0].neighbor_addr = linkaddr_node_addr;

  etimer_set(&periodic_timer, CLOCK_SECOND * 0.1 + random_rand()/RANDOM_RAND_MAX);

  while (1) {
    PROCESS_WAIT_EVENT();    
    
    if(nei_disc) broadcast_neighbor_table();

    if(net_disc) broadcast_neighbor_database();
    
    if(nei_disc) print_neighbor_table();

    if(net_disc) print_neighbor_database();

    etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}



PROCESS_THREAD(receive_msgs,ev,data){
  PROCESS_EXITHANDLER(printf("Combine Patients Process Ended\n\r");runicast_close(&runicast));
  PROCESS_BEGIN();

  static struct ctimer change_process_timer;

  ctimer_set(&change_process_timer, 120*CLOCK_SECOND,&change_process_2,NULL);

  process_exit(&network_discovery);

  static const struct broadcast_callbacks broadcast_callbacks = {broadcast_recv_2};

  static const struct runicast_callbacks runicast_callbacks = {runicast_recv,runicast_sent,runicast_timedout};


  packetbuf_clear();
  printf("\n\r");

  NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_CHANNEL, 11);
  //NETSTACK_CONF_RADIO.set_value(RADIO_PARAM_TXPOWER, 7);

  broadcast_open(&broadcast,129,&broadcast_callbacks);

  runicast_open(&runicast,146,&runicast_callbacks);
  message.count = count;

  //etimer_set(&read_data_timer, 0.5 * CLOCK_SECOND);
  while(1){
  packetbuf_clear();
  PROCESS_WAIT_EVENT();
  //if (printed == false ){
  //  printer = message;
  //  printf("copied to printer\n\r");
  //  printed = true;
  //}
  //if (count > 0) forward_data();
  
  //etimer_set(&read_data_timer,0.5 *CLOCK_SECOND);
  }
PROCESS_END();
}
*/
































/*
static void forward_data(){
  leds_on(LEDS_GREEN);
  printer = message;
  message = routing_empty;
  count = 0;
  message.count = count;
  printf("initiate print %d\n\r",printer.count);
    
  for(int i = 0; i < printer.count; i++){
    for(int j = 0; j <= printer.patient_messages[i].depth; j++){
        printf("message %d, layer %d  %x%x\n\r",i,j,printer.patient_messages[i].history[j].u8[0],printer.patient_messages[i].history[j].u8[1]);
    }
  }
  //static linkaddr_t dest;
  //dest.u8[0] = (uint8_t) 0x80;
  //dest.u8[1] = (uint8_t) 0x15;
  //packetbuf_copyfrom(&message,sizeof(struct routing_message));
  //runicast_send(&runicast,&dest,(uint8_t)5);
  //printf("\rrunicast sent to:%x%x\n\r",dest.u8[0],dest.u8[1]);
  //printed = false;
  leds_off(LEDS_GREEN);
}
*/



