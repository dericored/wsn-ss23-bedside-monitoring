/*
   Wireless Sensor Networks Laboratory

   Technische Universität München
   Lehrstuhl für Kommunikationsnetze
   http://www.lkn.ei.tum.de

   copyright (c) 2018 Chair of Communication Networks, TUM
*/

// implementation of dijkstra algorithm.

// C includes
#include <stdio.h>
#include <stdbool.h>

// Contiki includes
#include "contiki.h"
#include "dev/leds.h"
#include "net/netstack.h"

#define MAX_NEIGHBORS 5

struct neighbor_entry
{
   linkaddr_t neighbor_addr;
};

struct neighbor_table
{
   struct neighbor_entry entries[MAX_NEIGHBORS];
   uint8_t num_entries;
};

struct neighbor_database
{
   struct neighbor_table neighbor_table[MAX_NEIGHBORS];
   uint8_t num_nodes;
};


void exploreNeighbors(struct neighbor_database *my_database, linkaddr_t currentNode, uint8_t hops)
{
   if (linkaddr_cmp(&currentNode, &User_node))
   {
      // Destination found
      if (hops < minHops)
      {
         nextHop = currentNode;
         minHops = hops;
      }
      return;
   }

   for (uint8_t i = 0; i < MAX_NEIGHBORS; i++)
   {
      if (linkaddr_cmp(&currentNode, &(my_database->neighbor_table[i].entries[0].neighbor_addr)))
      {
         for (uint8_t j = 0; j < my_database->neighbor_table[i].num_entries; j++)
         {
            exploreNeighbors(my_database, my_database->neighbor_table[i].entries[j].neighbor_addr, hops + 1);
         }
      }
   }
}

// Function to calculate the next hop
linkaddr_t calculateNextHop(struct neighbor_database *my_database)
{
   exploreNeighbors(&my_database, my_database->neighbor_table[0].entries[0].neighbor_addr, 0);
   return nextHop;
}

linkaddr_t nextHop;
uint8_t minHops = MAX_NEIGHBORS + 1;
linkaddr_t User_node = {.u8[0] = 0xFF, .u8[1] = 0xFF};