#ifndef CUSTOM_STRUCT_H
#define CUSTOM_STRUCT_H

#include <cstdint>
#include <QMap>
#include <QString>

#define MAX_HISTORY_SIZE 5
#define MAX_MESSAGE_NUM 5
#define MAX_NEIGHBORS 5
#define LINKADDR_SIZE 2

struct linkaddr_t{
    uint8_t u8[LINKADDR_SIZE];
    uint16_t u16;
};

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

struct neighbor_entry {
   linkaddr_t neighbor_addr;
};

struct neighbor_table {
   struct neighbor_entry entries[MAX_NEIGHBORS];
   uint8_t num_entries;
};

struct neighbor_database{
   struct neighbor_table neighbor_table[MAX_NEIGHBORS];
   uint8_t num_nodes;
};

extern QMap<QString, int> table_dict;
extern QMap<QString, bool> list_dict;
extern QMap<QString, int> node_dict;
extern QMap<QString, QMap<QString, int>> edge_dict;


#endif // CUSTOM_STRUCT_H
