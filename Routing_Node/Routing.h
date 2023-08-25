#ifndef ROUTING_H_
#define ROUTING_H_

#include <stdint.h>
#include <stdbool.h>

#include "contiki.h"
#include "net/rime/rime.h"


#define MAX_NEIGHBORS 5

typedef struct {
  linkaddr_t neighbor_addr;
}neighbor_entry_t;

typedef struct {
  neighbor_entry_t entries[MAX_NEIGHBORS];
  uint8_t num_entries;
}neighbor_table_t;

struct neighbor_database{
  neighbor_table_t neighbor_table[MAX_NEIGHBORS];
  uint8_t num_nodes;
};

/// @brief finds closest node with the least number of hops
/// @param table neighboring nodes table
/// @param visited to track the visited node
/// @return closet node entry 
neighbor_entry_t find_closest_node(neighbor_table_t *table, bool *visited);

/// @brief calculates the path
/// @param neighbor_data neighboring nodes table
/// @param source current source (for which route is calculated)
neighbor_table_t calculate_dijkstra(neighbor_table_t *neighbor_data, linkaddr_t *source);


#endif /* ROUTING_H_ */