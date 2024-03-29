/**
 *  \file   xy_sensor.c
 *  \brief  XY: a Column-Row Data Dissemination protocol similar to the XYLS protocol (Das, S.M. and Pucha, H. and Hu, Y.C. Performance comparison of scalable location services for geographic ad hoc routing, INFOCOM 2005)
 *  \author Elyes Ben Hamida
 *  \date   2008
 **/

#include <include/modelutils.h>
#include "../data_d_common.h"


/* these macros turn ON/OFF logging */

#define LOG_APPLICATION_DISSEMINATION
//#define LOG_APPLICATION_DISSEMINATION_ROUTING

#define LOG_APPLICATION_REQUEST
//#define LOG_APPLICATION_REQUEST_ROUTING

#define LOG_APPLICATION_RESPONSE
//#define LOG_APPLICATION_RESPONSE_ROUTING


/* this macro allows to ckeck the node state during the next-hop computation */
//#define CHECK_ACTIVE_NODE


/**************************************************************************/
/**************************************************************************/
model_t model =  {
    "XY: a Column-Row Data Dissemination protocol",
    "Elyes Ben Hamida",
    "0.1",
    MODELTYPE_APPLICATION, 
    {NULL, 0}
};


/* ************************************************** */
/* ************************************************** */

/* Defining global variables */

#define XY_HELLO_TYPE    10
#define XY_DATA_TYPE     11
#define XY_REQUEST_TYPE  12
#define XY_RESPONSE_TYPE 13

#define INLINE_NODE 1
#define SENSOR_NODE 3

#define DIRECTION_NONE   '-'
#define DIRECTION_NORTH  'N'
#define DIRECTION_SOUTH  'S'
#define DIRECTION_EAST   'E'
#define DIRECTION_WEST   'W'


/* ************************************************** */
/* ************************************************** */

/* Defining packet types */

struct xy_hello_p {
  int         type;      /* packet type */
  nodeid_t    src;       /* node id */
  position_t  position;  /* node position */
};

struct xy_data_p {
  int type;              /* packet type */
  int n_hop;             /* nexthop ID */
  int source;            /* source ID */
  int metadata;          /* type of the disseminated data */
  int d_seq;             /* sequence number of the data */ 
  int d_value;           /* value of the disseminated data */
  int hop;               /* for stats: hop number from the source */
  uint64_t time;         /* for stats: data birth time */
  char direction;         /* direction of the query */
};

struct xy_request_p {
  int type;               /* packet type */
  int n_hop;              /* nexthop ID */
  int sink;               /* sink ID */
  position_t position;    /* home node position */
  int r_seq;              /* sequence number of the query */ 
  int metadata;           /* metadata of the requested data */ 
  int d_seq;              /* sequence number of the requested data */
  char direction;         /* direction of the query */
  int padding;
};

struct xy_response_p {
  int type;  
  int n_hop;
  int sink;
  int source;
  position_t position;
  int r_seq;
  int metadata;
  int d_seq;
  int d_value;
  int hop;               /* for stats: hop number from the source node */
  uint64_t time;         /* for stats: data birth time */
};

/* ************************************************** */
/* ************************************************** */

/* Defining data type for neighbors table */
struct xy_neighbor {
    int id;
    position_t position;                         
    uint64_t time;
};

/**************************************************************************/
/**************************************************************************/

/* Node data */
struct nodedata {
    /* Global variables*/
    int overhead;
    int type;
    int group_id;

    /* Hello protocol */
    uint64_t h_start;
    uint64_t h_timeout;	
    uint64_t h_period;	
    int      h_nbr;

    /* storage */
    void *neighbors;
    int s_seq[MAX_SOURCE];
    int r_seq[MAX_SINK];
    int d_source[MAX_METADATA];
    int d_value[MAX_METADATA];
    int d_seq[MAX_METADATA];
    int d_hop[MAX_METADATA];
    uint64_t d_time[MAX_METADATA];        

};

/* Entity data */
struct entitydata {
  /* stats */
  int TX_hello;
  int TX_data;
  int TX_query;
  int TX_response;
  int TX_sensor_adv;
  int TX_sensor_data;
  int TX_home_adv;
};


/**************************************************************************/
/**************************************************************************/
int hello_callback(call_t *c, void *args);
struct xy_neighbor *xy_next_hop(call_t *c, position_t *position);

int xy_is_nearest(call_t *c, position_t *position);

void rx_xy_hello(call_t *c, packet_t *packet);
void rx_xy_data(call_t *c, packet_t *packet);
void rx_xy_request(call_t *c, packet_t *packet);
void rx_xy_response(call_t *c, packet_t *packet);

void rx_source_adv(call_t *c, packet_t *packet);
void rx_source_data(call_t *c, packet_t *packet);
void rx_sink_adv(call_t *c, packet_t *packet);


void xy_stats(call_t *c);

/**************************************************************************/
/**************************************************************************/
int init(call_t *c, void *params) {
  struct entitydata *entitydata = malloc(sizeof(struct entitydata));

  entitydata->TX_hello       = 0;
  entitydata->TX_data        = 0;
  entitydata->TX_query       = 0;
  entitydata->TX_response    = 0;
  entitydata->TX_sensor_adv  = 0;
  entitydata->TX_sensor_data = 0;
  entitydata->TX_home_adv    = 0;

  set_entity_private_data(c, entitydata);

  return 0;    
}

int destroy(call_t *c) {
  struct entitydata *entitydata = get_entity_private_data(c);

  long int total = entitydata->TX_hello + entitydata->TX_data + entitydata->TX_query + entitydata->TX_response + entitydata->TX_sensor_adv + entitydata->TX_sensor_data + entitydata->TX_home_adv;

  double total_virtual = entitydata->TX_hello*0.5 + entitydata->TX_data + entitydata->TX_query + entitydata->TX_response + entitydata->TX_sensor_adv*0.5 + entitydata->TX_sensor_data + entitydata->TX_home_adv*0.5;

  long int total_octets = entitydata->TX_hello*sizeof(struct xy_hello_p) + entitydata->TX_data*sizeof(struct xy_data_p) + entitydata->TX_query*sizeof(struct xy_request_p) + entitydata->TX_response*sizeof(struct xy_response_p) + entitydata->TX_sensor_adv*sizeof(struct sensor_adv_p) + entitydata->TX_sensor_data*sizeof(struct sensor_data_p) + entitydata->TX_home_adv*sizeof(struct home_adv_p);
  
  printf("[ENERGY] HELLO %d DATA %d QUERY %d RESPONSE %d SENSOR_ADV %d SENSOR_DATA %d HOME_ADV %d TOTAL %ld TOTAL_VIRTUAL %lf TOTAL_OCTETS %ld\n", entitydata->TX_hello, entitydata->TX_data, entitydata->TX_query, entitydata->TX_response, entitydata->TX_sensor_adv, entitydata->TX_sensor_data, entitydata->TX_home_adv, total, total_virtual, total_octets);

  free(entitydata);
  return 0;
}


/**************************************************************************/
/**************************************************************************/
int setnode(call_t *c, void *params) {
    struct nodedata *nodedata     = malloc(sizeof(struct nodedata));
    param_t *param;

    int i = MAX_METADATA;
    int j = MAX_SOURCE;
    int k = MAX_SINK;
    
    /* default values */
    nodedata->h_start   = 0;
    nodedata->h_period  = 1000000000;	
    nodedata->h_timeout = nodedata->h_period * 2.5;
    nodedata->h_nbr     = -1;
    nodedata->neighbors = das_create();

    while (i--) {
        nodedata->d_source[i] = -1;
        nodedata->d_value[i]  = -1;
        nodedata->d_seq[i]    = -1;
        nodedata->d_hop[i]    = -1;
        nodedata->d_time[i]   = -1;
    }
    while (j--) {
        nodedata->s_seq[j] = -1;        
    }
    while (k--) {
        nodedata->r_seq[k] = -1;        
    }

    /* get parameters */
    das_init_traverse(params);
    while ((param = (param_t *) das_traverse(params)) != NULL) {
        if (!strcmp(param->key, "hello-start")) {
	  if (get_param_time(param->value, &(nodedata->h_start))) {
	        fprintf(stderr,"[XY] Error while reading the hello-start parameter ! (%s)\n",param->value);
                goto error;
	  }
        }
        if (!strcmp(param->key, "hello-period")) {
	  if (get_param_time(param->value, &(nodedata->h_period))) {
	        fprintf(stderr,"[XY] Error while reading the hello-period parameter ! (%s)\n",param->value);
                goto error;
	  }
        }
        if (!strcmp(param->key, "hello-timeout")) {
	  if (get_param_time(param->value, &(nodedata->h_timeout))) {
	        fprintf(stderr,"[XY] Error while reading the hello-timeout parameter ! (%s)\n",param->value);
                goto error;
	  }
        }
        if (!strcmp(param->key, "hello-nbr")) {
	  if (get_param_integer(param->value, &(nodedata->h_nbr))) {
	        fprintf(stderr,"[XY] Error while reading the hello-nbr parameter ! (%s)\n",param->value);
                goto error;
	  }
        }
    }

    set_node_private_data(c, nodedata);
    return 0;

 error:
    free(nodedata);
    return -1;

}

int unsetnode(call_t *c) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct xy_neighbor *neighbor;

    //xy_stats(c);

    while ((neighbor = (struct xy_neighbor *) das_pop(nodedata->neighbors)) != NULL) {
        free(neighbor);
    }
    das_destroy(nodedata->neighbors);

    free(nodedata);
    return 0;
}


/**************************************************************************/
/**************************************************************************/
int bootstrap(call_t *c) {
    struct nodedata *nodedata = get_node_private_data(c);
    entityid_t *down = get_entity_links_down(c);
    call_t c0 = {down[0], c->node, c->entity};
    uint64_t schedule = get_time() + nodedata->h_start + get_random_double() * nodedata->h_period;

    /* get overhead */
    if ((get_entity_type(&c0) != MODELTYPE_ROUTING) 
        && (get_entity_type(&c0) != MODELTYPE_MAC)) {
        nodedata->overhead = 0;
    } else {
        nodedata->overhead = GET_HEADER_SIZE(&c0);
    }

    /* scheduler first hello */
    if (nodedata->h_nbr == -1 || nodedata->h_nbr > 0) {
      scheduler_add_callback(schedule, c, hello_callback, NULL);
    }

    return 0;
}


/* ************************************************** */
/* ************************************************** */

int xy_is_nearest(call_t *c, position_t *sink_position) {
  struct nodedata *nodedata = get_node_private_data(c);
  struct xy_neighbor *neighbor = NULL;
  double dist = distance(get_node_position(c->node), sink_position);

  das_init_traverse(nodedata->neighbors);    

  while ((neighbor = (struct xy_neighbor *) das_traverse(nodedata->neighbors)) != NULL) {
    if ((distance(&(neighbor->position), sink_position) < dist) && (is_node_alive(neighbor->id))) {
      return 0;
    }
  }
    
  return 1;
}

/* Greedy geographic routing => computing the nexthop */ 
struct xy_neighbor *xy_next_hop(call_t *c, position_t *position) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct xy_neighbor *neighbor = NULL, *n_hop = NULL;
    double dist = distance(get_node_position(c->node), position);
    double d = dist;
    uint64_t clock = get_time();

    /* parse neighbors */
    das_init_traverse(nodedata->neighbors);    

    while ((neighbor = (struct xy_neighbor *) das_traverse(nodedata->neighbors)) != NULL) {

#ifdef CHECK_ACTIVE_NODE
        if ( !is_node_alive(neighbor->id) || ((nodedata->h_timeout > 0) && (clock - neighbor->time) >= nodedata->h_timeout) ) {
            continue;
        }
#else
        if ((nodedata->h_timeout > 0) && (clock - neighbor->time) >= nodedata->h_timeout ) {
            continue;
        }
#endif
        
        /* choose next hop */
        if ((d = distance(&(neighbor->position), position)) < dist) {
            dist = d;
            n_hop = neighbor;
        }
    }    
    
    return n_hop;
}

int neighbor_timeout(void *data, void *arg) {
    call_t *c = (call_t *) arg;
    struct nodedata *nodedata = get_node_private_data(c);
    struct xy_neighbor *neighbor = (struct xy_neighbor *) data;

    if ((get_time() - neighbor->time) > nodedata->h_timeout) {
        return 1;
    }

    return 0;
}

/* Periodic exchange of hello packets */
int hello_callback(call_t *c, void *args) {
    struct entitydata *entitydata = get_entity_private_data(c);
    struct nodedata *nodedata = get_node_private_data(c);

    entityid_t *down = get_entity_links_down(c);
    call_t c0 = {down[0], c->node, c->entity};

    destination_t destination = {BROADCAST_ADDR, {-1, -1, -1}};

    packet_t *packet = packet_create(c, nodedata->overhead + sizeof(struct xy_hello_p), -1);
    struct xy_hello_p *hello = (struct xy_hello_p *) (packet->data + nodedata->overhead);
    position_t *pos = get_node_position(c->node);

    /* set mac header */
    if (SET_HEADER(&c0, packet, &destination) == -1) {
        packet_dealloc(packet);
        return -1;
    }
    
    /* set header */
    hello->type       = XY_HELLO_TYPE;
    hello->src        = c->node;
    hello->position.x = pos->x;
    hello->position.y = pos->y;
    hello->position.z = pos->z;

    TX(&c0, packet);	   
    entitydata->TX_hello++;

    /* check neighbors timeout */
    if (nodedata->h_timeout > 0) {
        das_selective_delete(nodedata->neighbors, neighbor_timeout, (void *) c);
    }
    
    /* schedules hello */
    if (nodedata->h_nbr > 0) {
        nodedata->h_nbr --;
    }

    if (nodedata->h_nbr == -1 || nodedata->h_nbr > 0) {
      scheduler_add_callback(get_time() + nodedata->h_period, c, hello_callback, NULL);
    }

    return 0;
}


/* ************************************************** */
/* ************************************************** */
void rx(call_t *c, packet_t *packet) {
   struct nodedata *nodedata = get_node_private_data(c);
   struct data_d_header *header = (struct data_d_header *) (packet->data + nodedata->overhead);
    
   switch (header->type) {       

   case XY_HELLO_TYPE:
        /* for us */
       rx_xy_hello(c, packet);
       break;       

   case XY_DATA_TYPE:
        /* for us */
       rx_xy_data(c, packet);
       break;       
   case XY_REQUEST_TYPE:
        /* for us */
       rx_xy_request(c, packet);
       break;       
   case XY_RESPONSE_TYPE:
        /* for us */
       rx_xy_response(c, packet);
       break;       

   case SOURCE_ADV_TYPE:
        /* for us */
        rx_source_adv(c, packet);
        break;
   case SOURCE_DATA_TYPE:
        /* for us */
        rx_source_data(c, packet);
        break;
   case SINK_ADV_TYPE:
        /* for us */
       rx_sink_adv(c, packet);
       break;

   default:
       /* not for us */
       packet_dealloc(packet);
       break;
   }
   
    return;
}


/* ************************************************** */
/* ************************************************** */
void rx_xy_hello(call_t *c, packet_t *packet) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct xy_hello_p *hello = (struct xy_hello_p *) (packet->data + nodedata->overhead);
    struct xy_neighbor *neighbor;

   /* check for existing neighbors */
    das_init_traverse(nodedata->neighbors);      
    while ((neighbor = (struct xy_neighbor *) das_traverse(nodedata->neighbors)) != NULL) {
        if (neighbor->id == hello->src) {
            neighbor->position.x = hello->position.x;
            neighbor->position.y = hello->position.y;
            neighbor->position.z = hello->position.z;
            neighbor->time       = get_time();
            packet_dealloc(packet);
            return;
        }
    }  

    /* new neighbor */
    neighbor = (struct xy_neighbor *) malloc(sizeof(struct xy_neighbor));
    neighbor->id         = hello->src;
    neighbor->position.x = hello->position.x;
    neighbor->position.y = hello->position.y;
    neighbor->position.z = hello->position.z;
    neighbor->time       = get_time();
    das_insert(nodedata->neighbors, (void *) neighbor);
    packet_dealloc(packet);
    return;
}


/* ************************************************** */
/* ************************************************** */
void rx_source_adv(call_t *c, packet_t *packet) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct entitydata *entitydata = get_entity_private_data(c);
    struct source_adv_p *source = (struct source_adv_p *) (packet->data + nodedata->overhead);
    struct sensor_adv_p *sensor;
    packet_t *packet0;
    destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};
    entityid_t *down = get_entity_links_down(c);
    call_t c0 = {down[0], c->node, c->entity};

    /* check adv sequence */
    if (source->s_seq <= nodedata->s_seq[source->source]) {
        /* old request */
        packet_dealloc(packet);
        return;
    }
    nodedata->s_seq[source->source] = source->s_seq;
    
    /* reply */
    packet0 = packet_create(c, nodedata->overhead + sizeof(struct sensor_adv_p), -1);
    sensor = (struct sensor_adv_p *) (packet0->data + nodedata->overhead);
    if (SET_HEADER(&c0, packet0, &dst) == -1) {
        packet_dealloc(packet);
        packet_dealloc(packet0);
        return;
    }
    sensor->type   = SENSOR_ADV_TYPE;
    sensor->sensor = c->node;
    sensor->source = source->source;
    sensor->s_seq  = source->s_seq;

    TX(&c0, packet0);
    entitydata->TX_sensor_adv++;

    packet_dealloc(packet);
    return;
}


/**************************************************************************/
/**************************************************************************/

/* received a DATA report from a source node => forwarding towards the rendez-vous area */
void rx_source_data(call_t *c, packet_t *packet) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct entitydata *entitydata = get_entity_private_data(c);
    struct source_data_p *data = (struct source_data_p *) (packet->data + nodedata->overhead);
    position_t rdv_position;
    struct xy_neighbor *n_hop;

    /* stores the received data */
    if (data->d_seq > nodedata->d_seq[data->metadata]) {
        nodedata->d_seq[data->metadata]    = data->d_seq;
        nodedata->d_source[data->metadata] = data->source;
	nodedata->d_value[data->metadata]  = data->d_value;
	nodedata->d_hop[data->metadata]    = 1;
	nodedata->d_time[data->metadata]   = get_time();
    }

    /* check sensor */
    if (data->sensor != c->node) {
        /* not for us */
        packet_dealloc(packet);
        return;
    }

#ifdef LOG_APPLICATION_DISSEMINATION
    printf("[XY] Node %d (%lf,%lf) received a new DATA from source %d => replication towards north and south directions\n",c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y,data->source);
#endif

    /* replicates data towards north direction */
    rdv_position.x = (get_node_position(c->node))->x;
    rdv_position.y = 0.0;
    rdv_position.z = 0.0;
    
    /* get next hop */
    n_hop = xy_next_hop(c, &rdv_position);

    if (n_hop != NULL) {
        destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};
	packet_t *packet0 = packet_create(c, nodedata->overhead + sizeof(struct xy_data_p), -1);
        struct xy_data_p *disseminate =  (struct xy_data_p *) (packet0->data + nodedata->overhead);
        entityid_t *down = get_entity_links_down(c);
        call_t c0 = {down[0], c->node, c->entity};
        /* forward data */
        if (SET_HEADER(&c0, packet0, &dst) == -1) {
            packet_dealloc(packet0);
            packet_dealloc(packet);
            return;
        }
        disseminate->type      = XY_DATA_TYPE;
        disseminate->n_hop     = n_hop->id;
        disseminate->source    = data->source;
        disseminate->metadata  = data->metadata;
        disseminate->d_seq     = data->d_seq;
        disseminate->d_value   = data->d_value;
        disseminate->hop       = 2;
        disseminate->time      = get_time();
	disseminate->direction = DIRECTION_NORTH;

        TX(&c0, packet0);
	entitydata->TX_data++;

    } else {
#ifdef LOG_APPLICATION_DISSEMINATION_ROUTING
      printf("[XY] node %d (%lf,%lf) : no path towards NORTH direction\n", c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y);
#endif
    }

    /* replicates data towards south direction */
    rdv_position.x = (get_node_position(c->node))->x;
    rdv_position.y = (get_topology_area())->y;
    rdv_position.z = 0.0;
    
    /* get next hop */
    n_hop = xy_next_hop(c, &rdv_position);

    if (n_hop != NULL) {
        destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};
	packet_t *packet0 = packet_create(c, nodedata->overhead + sizeof(struct xy_data_p), -1);
        struct xy_data_p *disseminate =  (struct xy_data_p *) (packet0->data + nodedata->overhead);
        entityid_t *down = get_entity_links_down(c);
        call_t c0 = {down[0], c->node, c->entity};
        /* forward data */
        if (SET_HEADER(&c0, packet0, &dst) == -1) {
            packet_dealloc(packet0);
            packet_dealloc(packet);
            return;
        }
        disseminate->type      = XY_DATA_TYPE;
        disseminate->n_hop     = n_hop->id;
        disseminate->source    = data->source;
        disseminate->metadata  = data->metadata;
        disseminate->d_seq     = data->d_seq;
        disseminate->d_value   = data->d_value;
        disseminate->hop       = 2;
        disseminate->time      = get_time();
	disseminate->direction = DIRECTION_SOUTH;

        TX(&c0, packet0);
	entitydata->TX_data++;

    } else {
#ifdef LOG_APPLICATION_DISSEMINATION_ROUTING
      printf("[XY] node %d (%lf,%lf) : no path towards SOUTH direction\n", c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y);
#endif
    }

    packet_dealloc(packet);
    return;
}


/**************************************************************************/
/**************************************************************************/

/* Received a DATA report from a sensor */
void rx_xy_data(call_t *c, packet_t *packet) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct entitydata *entitydata = get_entity_private_data(c);
    struct xy_data_p *data = (struct xy_data_p *) (packet->data + nodedata->overhead);
    position_t rdv_position;
    struct xy_neighbor *n_hop;

    /* stores the received data */
    if (data->d_seq > nodedata->d_seq[data->metadata]) {
        nodedata->d_seq[data->metadata]    = data->d_seq;
	nodedata->d_source[data->metadata] = data->source;
	nodedata->d_value[data->metadata]  = data->d_value;
	nodedata->d_hop[data->metadata]    = data->hop;
	nodedata->d_time[data->metadata]   = data->time;
    }

   /* check sensor */
    if (data->n_hop != c->node) {
        /* not for us */
        packet_dealloc(packet);
        return;
    }
    
#ifdef LOG_APPLICATION_DISSEMINATION_ROUTING
    printf("[XY] Node %d (%lf,%lf) received a DATA from sensor %d => replication towards %c direction\n",c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y,data->source,data->direction);
#endif

    /* replicates the DATA towards the choosed direction */

    switch(data->direction){
       case DIRECTION_NORTH: 
	 rdv_position.x = (get_node_position(c->node))->x;
	 rdv_position.y = 0.0; 
	 rdv_position.z = 0.0;
	 break;
       case DIRECTION_SOUTH: 
	 rdv_position.x = (get_node_position(c->node))->x;
	 rdv_position.y = (get_topology_area())->y; 
	 rdv_position.z = 0.0;
	 break;
       default:
#ifdef LOG_APPLICATION_DISSEMINATION_ROUTING
         printf("[XY] Node %d (%lf,%lf) received a DATA from source %d with incorrect forwarding direction (%d) !\n",c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y,data->source,data->direction);
#endif
	 packet_dealloc(packet);
	 return;
    }

    /* get next hop */
    n_hop = xy_next_hop(c, &rdv_position);

    if (n_hop != NULL) {

#ifdef LOG_APPLICATION_DISSEMINATION_ROUTING
        printf("[XY] node %d (%lf,%lf) : forwardind data towards direction %c via node %d \n", c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y,data->direction,n_hop->id);
#endif
        destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};
	entityid_t *down = get_entity_links_down(c);
        call_t c0 = {down[0], c->node, c->entity};
        /* forward data */
        if (SET_HEADER(&c0, packet, &dst) == -1) {
            packet_dealloc(packet);
            return;
        }
        data->n_hop = n_hop->id;
	data->hop++;
        TX(&c0, packet);
	entitydata->TX_data++;

    } else {
#ifdef LOG_APPLICATION_DISSEMINATION_ROUTING
      printf("[XY] node %d (%lf,%lf) : no path towards %c direction\n", c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y,data->direction);
#endif
    }

    return;
}


/* ************************************************** */
/* ************************************************** */

/* Received a request from a sink */
void rx_sink_adv(call_t *c, packet_t *packet) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct entitydata *entitydata = get_entity_private_data(c);
    struct sink_adv_p *sink = (struct sink_adv_p *) (packet->data + nodedata->overhead);
    entityid_t *down = get_entity_links_down(c);
    call_t c0 = {down[0], c->node, c->entity};
            
    /* starts home node election */
    if (sink->home == -1) {

        packet_t *packet0;
        struct home_adv_p *adv;
        destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};
        
        /* check request sequence */
        if (sink->r_seq <= nodedata->r_seq[sink->sink]) {
            /* old request */
            packet_dealloc(packet);
            return;
        }
        nodedata->r_seq[sink->sink] = sink->r_seq;

	packet0 = packet_create(c, nodedata->overhead + sizeof(struct home_adv_p), -1);
	adv = (struct home_adv_p *) (packet0->data + nodedata->overhead);
	if (SET_HEADER(&c0, packet0, &dst) == -1) {
	  packet_dealloc(packet);
	  packet_dealloc(packet0);
	  return;
	}

	adv->type   = HOME_ADV_TYPE;
	adv->sensor = c->node;
	adv->sink   = sink->sink;
	adv->r_seq  = sink->r_seq;
	
	TX(&c0, packet0);	
	entitydata->TX_home_adv++;

        packet_dealloc(packet);
        return;
    } else if (sink->home == c->node) {
      
        /* do we have the requested data (or some more recent data) ? */
        if (sink->d_seq <= nodedata->d_seq[sink->metadata]) {
	    packet_t *packet0;
	    struct sensor_data_p *data;
	    destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};

	    packet0 = packet_create(c, nodedata->overhead + sizeof(struct sensor_data_p), -1);
	    data = (struct sensor_data_p *) (packet0->data + nodedata->overhead);
	    if (SET_HEADER(&c0, packet0, &dst) == -1) {
	      packet_dealloc(packet);
	      packet_dealloc(packet0);
	      return;
	    }
	    data->type     = SENSOR_DATA_TYPE;
	    data->metadata = sink->metadata;
	    data->sink     = sink->sink;
	    data->r_seq    = sink->r_seq;
	    data->source   = nodedata->d_source[sink->metadata];
	    data->d_seq    = nodedata->d_seq[sink->metadata];
	    data->d_value  = nodedata->d_value[sink->metadata];
	    data->delay    = (get_time()-nodedata->d_time[sink->metadata])*0.000001;
	    data->hop      = nodedata->d_hop[sink->metadata];

#ifdef LOG_APPLICATION_REQUEST
	    printf("[XY] Home-Node %d (%lf,%lf) : broadcasting DATA REPLY (%d,%d,%d) to sink %d (%lf,%lf)\n", c->node, (get_node_position(c->node))->x, (get_node_position(c->node))->y, nodedata->d_source[sink->metadata], sink->metadata, nodedata->d_seq[sink->metadata], sink->sink,(get_node_position(sink->sink))->x, (get_node_position(sink->sink))->y);
#endif
	    TX(&c0, packet0);
	    entitydata->TX_sensor_data++;

	    packet_dealloc(packet);
	    return;
	
	} else {
	    position_t rdv_position;
	    struct xy_neighbor *n_hop = NULL;
	    destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};
      
	    /* forwards the query to EAST direction */
	    rdv_position.x = (get_topology_area())->x; 
	    rdv_position.y = (get_node_position(c->node))->y;
	    rdv_position.z = -1;
	    
	    n_hop = xy_next_hop(c, &rdv_position);
	    
	    if (n_hop != NULL) {

	      packet_t *packet0 = packet_create(c, nodedata->overhead + sizeof(struct xy_request_p), -1);
	      struct xy_request_p *request =  (struct xy_request_p *) (packet0->data + nodedata->overhead);
	      position_t *pos = get_node_position(c->node);
	      /* create request */
	      if (SET_HEADER(&c0, packet0, &dst) == -1) {
		packet_dealloc(packet);
		packet_dealloc(packet0);
		return;
	      }
	      request->type       = XY_REQUEST_TYPE;
	      request->n_hop      = n_hop->id;
	      request->sink       = sink->sink;
	      request->r_seq      = sink->r_seq;
	      request->metadata   = sink->metadata;
	      request->d_seq      = sink->d_seq;
	      request->position.x = pos->x;
	      request->position.y = pos->y;
	      request->position.z = pos->z;
	      request->direction  = DIRECTION_EAST;

#ifdef LOG_APPLICATION_REQUEST_ROUTING
	      printf("[XY] Node %d (%lf,%lf) : forwarding REQUEST(sink=%d,%d,%d) to EAST direction via node %d (%lf,%lf)\n", c->node, get_node_position(c->node)->x, get_node_position(c->node)->y, sink->sink, sink->metadata, sink->r_seq, n_hop->id, get_node_position(n_hop->id)->x, get_node_position(n_hop->id)->y);
#endif
	      TX(&c0, packet0);    
	      entitydata->TX_query++;

	    } else {
#ifdef LOG_APPLICATION_REQUEST
	      printf("[XY] Node %d : no path towards EAST direction\n", c->node);
#endif
	    }


	    /* forwards the query to WEST direction */
	    rdv_position.x = 0.0; 
	    rdv_position.y = (get_node_position(c->node))->y;
	    rdv_position.z = -1;
	    
	    n_hop = xy_next_hop(c, &rdv_position);
	    
	    if (n_hop != NULL) {

	      packet_t *packet0 = packet_create(c, nodedata->overhead + sizeof(struct xy_request_p), -1);
	      struct xy_request_p *request =  (struct xy_request_p *) (packet0->data + nodedata->overhead);
	      position_t *pos = get_node_position(c->node);
	      /* create request */
	      if (SET_HEADER(&c0, packet0, &dst) == -1) {
		packet_dealloc(packet);
		packet_dealloc(packet0);
		return;
	      }
	      request->type       = XY_REQUEST_TYPE;
	      request->n_hop      = n_hop->id;
	      request->sink       = sink->sink;
	      request->r_seq      = sink->r_seq;
	      request->metadata   = sink->metadata;
	      request->d_seq      = sink->d_seq;
	      request->position.x = pos->x;
	      request->position.y = pos->y;
	      request->position.z = pos->z;
	      request->direction  = DIRECTION_WEST;

#ifdef LOG_APPLICATION_REQUEST_ROUTING
	      printf("[XY] Node %d (%lf,%lf) : forwarding REQUEST(sink=%d,%d,%d) to WEST direction via node %d (%lf,%lf)\n", c->node, get_node_position(c->node)->x, get_node_position(c->node)->y, sink->sink, sink->metadata, sink->r_seq, n_hop->id, get_node_position(n_hop->id)->x, get_node_position(n_hop->id)->y);
#endif
	      TX(&c0, packet0);    
	      entitydata->TX_query++;

	    } else {
#ifdef LOG_APPLICATION_REQUEST
	      printf("[XY] Node %d : no path towards WEST direction\n", c->node);
#endif
	    }

	    packet_dealloc(packet);
	    return;
	}

    } else {
        packet_dealloc(packet);
        return;
    }
}


/**************************************************************************/
/**************************************************************************/

/* received a request from a sensor */
void rx_xy_request(call_t *c, packet_t *packet) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct entitydata *entitydata = get_entity_private_data(c);

    struct xy_request_p *request = (struct xy_request_p *) (packet->data + nodedata->overhead);
    position_t rdv_position;
    struct xy_neighbor *n_hop;
    entityid_t *down = get_entity_links_down(c);
    call_t c0 = {down[0], c->node, c->entity};

    /* are we the next hop */
    if (request->n_hop != c->node) {
        packet_dealloc(packet);
        return;
    }

    /* do we have the requested data (or some more recent data) ? */
    if (request->d_seq <= nodedata->d_seq[request->metadata]) {

	packet_t *packet0;
	destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};
	
	n_hop = xy_next_hop(c, &(request->position));   
	
	if (n_hop == NULL) {
	    struct sensor_data_p *data;
	    
	    packet0 = packet_create(c, nodedata->overhead + sizeof(struct sensor_data_p), -1);
	    data = (struct sensor_data_p *) (packet0->data + nodedata->overhead);
	    if (SET_HEADER(&c0, packet0, &dst) == -1) {
	      packet_dealloc(packet);
	      packet_dealloc(packet0);
	      return;
	    }
	    data->type     = SENSOR_DATA_TYPE;
	    data->metadata = request->metadata;
	    data->sink     = request->sink;
	    data->r_seq    = request->r_seq;
	    data->source   = nodedata->d_source[request->metadata];
	    data->d_seq    = nodedata->d_seq[request->metadata];
	    data->d_value  = nodedata->d_value[request->metadata];
	    data->delay    = nodedata->d_time[request->metadata];
	    data->hop      = nodedata->d_hop[request->metadata];
	    
#ifdef LOG_APPLICATION_REQUEST
	    printf("[XY] Node %d (%lf,%lf) : broadcasting DATA REPLY (%d,%d,%d) to sink %d (%lf,%lf)\n", c->node, (get_node_position(c->node))->x, (get_node_position(c->node))->y, nodedata->d_source[request->metadata], request->metadata, nodedata->d_seq[request->metadata], request->sink, (get_node_position(request->sink))->x, (get_node_position(request->sink))->y);
#endif
	    TX(&c0, packet0);
	    entitydata->TX_sensor_data++;

	    packet_dealloc(packet);
	    return;

	} else {

	  struct xy_response_p *response;
            
	  /* reply */
	  packet0 = packet_create(c, nodedata->overhead + sizeof(struct xy_response_p), -1);
	  response = (struct xy_response_p *) (packet0->data + nodedata->overhead);
	  if (SET_HEADER(&c0, packet0, &dst) == -1) {
	    packet_dealloc(packet);
	    packet_dealloc(packet0);
	    return;
	  }
	  response->type       = XY_RESPONSE_TYPE;
	  response->n_hop      = n_hop->id;
	  response->metadata   = request->metadata;
	  response->sink       = request->sink;
	  response->r_seq      = request->r_seq;
	  response->source     = nodedata->d_source[request->metadata];
	  response->d_seq      = nodedata->d_seq[request->metadata];
	  response->d_value    = nodedata->d_value[request->metadata];
	  response->time       = nodedata->d_time[request->metadata];
	  response->hop        = nodedata->d_hop[request->metadata];
	  response->position.x = request->position.x;
	  response->position.y = request->position.y;
	  response->position.z = request->position.z;

	  TX(&c0, packet0);
	  entitydata->TX_response++;

	  packet_dealloc(packet);
	  return;

	}
	
    } 
    /* forwards the query towards the specified direction */
    else {

	switch(request->direction){
	    case DIRECTION_EAST: 
	      rdv_position.x = (get_topology_area())->x;
	      rdv_position.y = (get_node_position(c->node))->y; 
	      rdv_position.z = 0.0;
	      break;
	    case DIRECTION_WEST: 
	      rdv_position.x = 0.0;
	      rdv_position.y = (get_node_position(c->node))->y; 
	      rdv_position.z = 0.0;
	      break;
	    default:
#ifdef LOG_APPLICATION_REQUEST_ROUTING
	      printf("[XY] Node %d (%lf,%lf) received a DATA with incorrect forwarding direction (%c) !\n",c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y,request->direction);
#endif
	      packet_dealloc(packet);
	      return;
	}

	/* get next hop */
	n_hop = xy_next_hop(c, &rdv_position);

	if (n_hop != NULL) {

#ifdef LOG_APPLICATION_REQUEST_ROUTING
	    printf("[XY] node %d (%lf,%lf) : forwarding request towards direction %c via node %d \n", c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y,request->direction,n_hop->id);
#endif
	    destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};

	    /* forward data */
	    if (SET_HEADER(&c0, packet, &dst) == -1) {
	      packet_dealloc(packet);
	      return;
	    }
	    request->n_hop = n_hop->id;

	    TX(&c0, packet);
	    entitydata->TX_query++;
	    
	} else {
#ifdef LOG_APPLICATION_REQUEST_ROUTING
	  printf("[XY] node %d (%lf,%lf) : no path towards %c direction\n", c->node,(get_node_position(c->node))->x,(get_node_position(c->node))->y,request->direction);
#endif
	}
		
    }
    
    return;
}


/**************************************************************************/
/**************************************************************************/
void rx_xy_response(call_t *c, packet_t *packet) {
    struct nodedata *nodedata = get_node_private_data(c);
    struct entitydata *entitydata = get_entity_private_data(c);
    struct xy_response_p *response = (struct xy_response_p *) (packet->data + nodedata->overhead);
    struct xy_neighbor *n_hop;
    entityid_t *down = get_entity_links_down(c);
    call_t c0 = {down[0], c->node, c->entity};

    /* are we the next hop */
    if (response->n_hop != c->node) {
        packet_dealloc(packet);
        return;
    }

    /* get next hop */
    n_hop = xy_next_hop(c, &(response->position));
    
    /* if I am the home node */
    if (n_hop == NULL) {

#ifdef LOG_APPLICATION_RESPONSE
      printf("[HOME] node %d : broadcasting received DATA to (sink=%d,%d,%d)\n", c->node, response->sink, response->metadata, response->r_seq);
#endif
        packet_t *packet0;
        struct sensor_data_p *data;
	destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};
        
        /* reply */
        packet0 = packet_create(c, nodedata->overhead + sizeof(struct sensor_data_p), -1);
        data = (struct sensor_data_p *) (packet0->data + nodedata->overhead);
        if (SET_HEADER(&c0, packet0, &dst) == -1) {
            packet_dealloc(packet);
            packet_dealloc(packet0);
            return;
        }
        data->type     = SENSOR_DATA_TYPE;
        data->metadata = response->metadata;
        data->sink     = response->sink;
        data->r_seq    = response->r_seq;
        data->source   = response->source;
        data->d_seq    = response->d_seq;
        data->d_value  = response->d_value;
	data->delay    = (get_time()-response->time)*0.000001;
	data->hop      = response->hop;

        TX(&c0, packet0);
	entitydata->TX_sensor_data++;

        packet_dealloc(packet);
        return;
    } else {

#ifdef LOG_APPLICATION_RESPONSE_ROUTING
	printf("Node %d : forwarding REPLAY to (sink=%d,%d,%d) via node %d \n", c->node, response->sink, response->metadata, response->r_seq, n_hop->id);
#endif
        /* forward packet */

#ifdef USE_BROADCAST_COMMUNICATIONS
	destination_t dst = {BROADCAST_ADDR, {-1, -1, -1}};
#else
	destination_t dst = {n_hop->id, {-1, -1, -1}};
#endif

        /* forward data */
        if (SET_HEADER(&c0, packet, &dst) == -1) {
            packet_dealloc(packet);
            return;
        }
        response->n_hop = n_hop->id;
	response->hop++;

        TX(&c0, packet);    
	entitydata->TX_response++;
    }

    return;
}

/* ************************************************** */
/* ************************************************** */
void xy_stats(call_t *c) {
   struct nodedata *nodedata = get_node_private_data(c);
   position_t *position = get_node_position(c->node);

   if (nodedata->type == INLINE_NODE) {
     printf("INLINE node %d (%lf,%lf,%lf)  Group_id=%d   Neighbors=%d\n", c->node, position->x, position->y, position->z, nodedata->group_id, das_getsize(nodedata->neighbors));
   } else {
     printf("SENSOR node %d (%lf,%lf,%lf)  Neighbors=%d\n", c->node, position->x, position->y, position->z, das_getsize(nodedata->neighbors));

   }
}


/* ************************************************** */
/* ************************************************** */
application_methods_t methods = {rx};


