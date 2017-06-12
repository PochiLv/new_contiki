/***********************************************************
		
			这个文件主要是用来定义一些宏定义之类的

************************************************************/
/*
 * Copyright (c) 2010, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * \file
 *	Public API declarations for ContikiRPL.
 * \author
 *	Joakim Eriksson <joakime@sics.se> & Nicolas Tsiftes <nvt@sics.se>
 *
 */

#ifndef RPL_H
#define RPL_H

#include "rpl-conf.h"

#include "lib/list.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "sys/ctimer.h"

/*---------------------------------------------------------------------------*/
typedef uint16_t rpl_rank_t;
typedef uint16_t rpl_ocp_t;
/*---------------------------------------------------------------------------*/
/* DAG Metric Container Object Types, to be confirmed by IANA. */
/* Metric Container 里面的Object类型表 */

#define RPL_DAG_MC_NONE			0 /* Local identifier for empty MC */
#define RPL_DAG_MC_NSA                  1 /* Node State and Attributes */
#define RPL_DAG_MC_ENERGY               2 /* Node Energy */
#define RPL_DAG_MC_HOPCOUNT             3 /* Hop Count */
#define RPL_DAG_MC_THROUGHPUT           4 /* Throughput */
#define RPL_DAG_MC_LATENCY              5 /* Latency */
#define RPL_DAG_MC_LQL                  6 /* Link Quality Level */
#define RPL_DAG_MC_ETX                  7 /* Expected Transmission Count */
#define RPL_DAG_MC_LC                   8 /* Link Color */

/* DAG Metric Container flags. */
/* Metric Container 标志 */
#define RPL_DAG_MC_FLAG_P               0x8
#define RPL_DAG_MC_FLAG_C               0x4
#define RPL_DAG_MC_FLAG_O               0x2
#define RPL_DAG_MC_FLAG_R               0x1

/* DAG Metric Container aggregation mode. */
/* Metric Contain 聚合模式 */
#define RPL_DAG_MC_AGGR_ADDITIVE        0
#define RPL_DAG_MC_AGGR_MAXIMUM         1
#define RPL_DAG_MC_AGGR_MINIMUM         2
#define RPL_DAG_MC_AGGR_MULTIPLICATIVE  3

/* The bit index within the flags field of
   the rpl_metric_object_energy structure. */
#define RPL_DAG_MC_ENERGY_INCLUDED	3
#define RPL_DAG_MC_ENERGY_TYPE		1
#define RPL_DAG_MC_ENERGY_ESTIMATION	0

#define RPL_DAG_MC_ENERGY_TYPE_MAINS		0
#define RPL_DAG_MC_ENERGY_TYPE_BATTERY		1
#define RPL_DAG_MC_ENERGY_TYPE_SCAVENGING	2

struct rpl_metric_object_energy {
  uint8_t flags;
  uint8_t energy_est;
};

/* Logical representation of a DAG Metric Container. */
struct rpl_metric_container {
  uint8_t type;
  uint8_t flags;
  uint8_t aggr;
  uint8_t prec;
  uint8_t length;
  union metric_object {
    struct rpl_metric_object_energy energy;
    uint16_t etx;
  } obj;
};
typedef struct rpl_metric_container rpl_metric_container_t;
/*---------------------------------------------------------------------------*/
struct rpl_instance;
struct rpl_dag;
/*---------------------------------------------------------------------------*/
#define RPL_PARENT_FLAG_UPDATED           0x1
#define RPL_PARENT_FLAG_LINK_METRIC_VALID 0x2

// 可以看到rpl_parent这个类有两个成员变量
struct rpl_parent {
  // 一个是parent
  struct rpl_parent *next;
  // 一个是dag，所以在parent中是有dag的
  struct rpl_dag *dag;
#if RPL_DAG_MC != RPL_DAG_MC_NONE
  rpl_metric_container_t mc;
#endif /* RPL_DAG_MC != RPL_DAG_MC_NONE */
  // 除了上面的成员变量还有rank
  rpl_rank_t rank;
  // 计时器
  clock_time_t last_tx_time;
  // dtsn
  uint8_t dtsn;
  //标志位
  uint8_t flags;
};
typedef struct rpl_parent rpl_parent_t;
/*---------------------------------------------------------------------------*/
/* RPL DIO prefix suboption */
//看这个意思，似乎这个是sub option的那个类
struct rpl_prefix {
  uip_ipaddr_t prefix;
  uint32_t lifetime;
  uint8_t length;
  uint8_t flags;
};
typedef struct rpl_prefix rpl_prefix_t;
/*---------------------------------------------------------------------------*/
/* Directed Acyclic Graph */
struct rpl_dag {
  uip_ipaddr_t dag_id;
  rpl_rank_t min_rank; /* should be reset per DAG iteration! */
  uint8_t version;
  uint8_t grounded;
  uint8_t preference;
  uint8_t used;
  /* live data for the DAG */
  uint8_t joined;
  rpl_parent_t *preferred_parent;
  rpl_rank_t rank;
  struct rpl_instance *instance;
  rpl_prefix_t prefix_info;
};
typedef struct rpl_dag rpl_dag_t;
typedef struct rpl_instance rpl_instance_t;
/*---------------------------------------------------------------------------*/
/*
 * API for RPL objective functions (OF)
 *
 * reset(dag)

 *  针对某个特定的DAG，重置目标函数的状态，这个功能在全局修复的时候使用。
 *  Resets the objective function state for a specific DAG. This function is
 *  called when doing a global repair on the DAG.
 *
 *  neighbor_link_callback(parent, known, etx)
 *  
 *  接收到数据链路层，know参数只能被设置成0或者1。etx参数指明现在与邻居节点的etx
 *  Receives link-layer neighbor information. The parameter "known" is set
 *  either to 0 or 1. The "etx" parameter specifies the current
 *  ETX(estimated transmissions) for the neighbor.
 *
 * best_parent(parent1, parent2)
 *  
 *  根据OF对比两个父节点，返回the best one
 *  Compares two parents and returns the best one, according to the OF.
 *
 * best_dag(dag1, dag2)
 *
 *  返回好的那个dag
 *  Compares two DAGs and returns the best one, according to the OF.
 *
 * calculate_rank(parent, base_rank)
 *
 *  用父节点和base rank计算这个节点的rank，如果没有父节点，那么of选择一个
 *  默认增长加在base rank上。
 *  Calculates a rank value using the parent rank and a base rank.
 *  If "parent" is NULL, the objective function selects a default increment
 *  that is adds to the "base_rank". Otherwise, the OF uses information known
 *  about "parent" to select an increment to the "base_rank".
 *
 * update_metric_container(dag)
 *
 *  更新metric container，如果of没有使用metric container function
 *  应该设置object 类型weight RPL_DAG_MC_NONE
 *  Updates the metric container for outgoing DIOs in a certain DAG.
 *  If the objective function of the DAG does not use metric containers, 
 *  the function should set the object type to RPL_DAG_MC_NONE.
 */
struct rpl_of {
  void (*reset)(struct rpl_dag *);
  void (*neighbor_link_callback)(rpl_parent_t *, int, int);
  rpl_parent_t *(*best_parent)(rpl_parent_t *, rpl_parent_t *);
  rpl_dag_t *(*best_dag)(rpl_dag_t *, rpl_dag_t *);
  rpl_rank_t (*calculate_rank)(rpl_parent_t *, rpl_rank_t);
  void (*update_metric_container)( rpl_instance_t *);
  rpl_ocp_t ocp;
};
typedef struct rpl_of rpl_of_t;

/* 声明OF */
/* Declare the selected objective function. */
extern rpl_of_t RPL_OF;
/*---------------------------------------------------------------------------*/
/* Instance */
/* 实例 */
struct rpl_instance {
  /* DAG configuration */
  /* DAG 设置声明 */
  rpl_metric_container_t mc;
  rpl_of_t *of;
  rpl_dag_t *current_dag;
  rpl_dag_t dag_table[RPL_MAX_DAG_PER_INSTANCE];

  /* The current default router - used for routing "upwards" */
  /* 现存默认路由器，是在上行路由的时候用的 */
  uip_ds6_defrt_t *def_route;
  uint8_t instance_id;
  uint8_t used;
  uint8_t dtsn_out;
  uint8_t mop;
  uint8_t dio_intdoubl;
  uint8_t dio_intmin;
  uint8_t dio_redundancy;
  uint8_t default_lifetime;
  uint8_t dio_intcurrent;
  uint8_t dio_send; /* for keeping track of which mode the timer is in */
  uint8_t dio_counter;
  rpl_rank_t max_rankinc;
  rpl_rank_t min_hoprankinc;
  uint16_t lifetime_unit; /* lifetime in seconds = l_u * d_l */
#if RPL_CONF_STATS
  uint16_t dio_totint;
  uint16_t dio_totsend;
  uint16_t dio_totrecv;
#endif /* RPL_CONF_STATS */
  clock_time_t dio_next_delay; /* delay for completion of dio interval */
#if RPL_WITH_PROBING
  struct ctimer probing_timer;
#endif /* RPL_WITH_PROBING */
  struct ctimer dio_timer;
  struct ctimer dao_timer;
  struct ctimer dao_lifetime_timer;
};

/*---------------------------------------------------------------------------*/
/* Public RPL functions. */
/* 这里定义的应该是公共使用的函数 */

void rpl_init(void);
void uip_rpl_input(void);
rpl_dag_t *rpl_set_root(uint8_t instance_id, uip_ipaddr_t *dag_id);
int rpl_set_prefix(rpl_dag_t *dag, uip_ipaddr_t *prefix, unsigned len);
int rpl_repair_root(uint8_t instance_id);
int rpl_set_default_route(rpl_instance_t *instance, uip_ipaddr_t *from);
rpl_dag_t *rpl_get_any_dag(void);
rpl_instance_t *rpl_get_instance(uint8_t instance_id);
int rpl_update_header_empty(void);
int rpl_update_header_final(uip_ipaddr_t *addr);
int rpl_verify_header(int);
void rpl_insert_header(void);
void rpl_remove_header(void);
uint8_t rpl_invert_header(void);
uip_ipaddr_t *rpl_get_parent_ipaddr(rpl_parent_t *nbr);
rpl_parent_t *rpl_get_parent(uip_lladdr_t *addr);
rpl_rank_t rpl_get_parent_rank(uip_lladdr_t *addr);
uint16_t rpl_get_parent_link_metric(const uip_lladdr_t *addr);
void rpl_dag_init(void);
uip_ds6_nbr_t *rpl_get_nbr(rpl_parent_t *parent);
void rpl_print_neighbor_list();

/* Per-parent RPL information */
/* 光这名字应该是传入一个rpl_parents 返回一个邻居表 */
NBR_TABLE_DECLARE(rpl_parents);

/**
 * RPL modes
 *
 * 这个RPL mode主要有三种mesh feather和leaf
 * mesh是可以传送消息，也可达
 * feather不可以传消息，但是可达
 * leaf可以传送消息，但是不可达
 *
 * The RPL module can be in either of three modes: mesh mode
 * (RPL_MODE_MESH), feater mode (RPL_MODE_FEATHER), and leaf mode
 * (RPL_MODE_LEAF). In mesh mode, nodes forward data for other nodes,
 * and are reachable by others. In feather mode, nodes can forward
 * data for other nodes, but are not reachable themselves. In leaf
 * mode, nodes do not forward data for others, but are reachable by
 * others. */
enum rpl_mode {
  RPL_MODE_MESH = 0,
  RPL_MODE_FEATHER = 1,
  RPL_MODE_LEAF = 2,
};

/**
 * Set the RPL mode
 * 设置RPL模式
 * 
 * \param mode The new RPL mode
 * \retval The previous RPL mode
 */
enum rpl_mode rpl_set_mode(enum rpl_mode mode);

/**
 * Get the RPL mode
 * 获取RPL模式
 * 
 * \retval The RPL mode
 */
enum rpl_mode rpl_get_mode(void);

/*---------------------------------------------------------------------------*/
#endif /* RPL_H */
