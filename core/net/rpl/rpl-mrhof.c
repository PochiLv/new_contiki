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
 */

/**
 * \file
 *         这个文件应该就是比较重要的关于OF的，如果以后想要修改OF的话，
 *         这个文件是必须要会的
 *
 *         The Minimum Rank with Hysteresis Objective Function (MRHOF)
 *
 *         This implementation uses the estimated number of
 *         transmissions (ETX) as the additive routing metric,
 *         and also provides stubs for the energy metric.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 */

/**
 * \addtogroup uip6
 * @{
 */

#include "net/rpl/rpl-private.h"
/**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：空
	修改后：#include "net/rpl/rpl-icmp6.c"
	修改目的：
	为了extern的成功实行
    **************************************************************************/
#include "net/nbr-table.h"
/**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：空
	修改后：extern uint16_t energy_est;
	修改目的：
	引用我在incmp6文件中定义的那个能量
    **************************************************************************/
extern int energy_est;

// 同样，打开debug
#define DEBUG DEBUG_FULL
#include "net/ip/uip-debug.h"

/*
		有这么几种方法
		重置dag
		邻居链路的callback
		选择最佳的父节点
		选择最佳的dag
		计算rank值
		update metric container
		
*/
static void reset(rpl_dag_t *);
static void neighbor_link_callback(rpl_parent_t *, int, int);
static rpl_parent_t *best_parent(rpl_parent_t *, rpl_parent_t *);
static rpl_dag_t *best_dag(rpl_dag_t *, rpl_dag_t *);
static rpl_rank_t calculate_rank(rpl_parent_t *, rpl_rank_t);
static void update_metric_container(rpl_instance_t *);

//这个意思是不是先都调用一遍
rpl_of_t rpl_mrhof = {
  reset,
  neighbor_link_callback,
  best_parent,
  best_dag,
  calculate_rank,
  update_metric_container,
  1
};

/* Constants for the ETX moving average */
/* 关于ETX的初始化 */
#define ETX_SCALE   100
#define ETX_ALPHA   90

/* Reject parents that have a higher link metric than the following. */
/* 如果有的父节点的link metric 比下述的值还高，那么就抛弃这个父节点 */
#define MAX_LINK_METRIC			10

/* Reject parents that have a higher path cost than the following. */
/* 抛弃比下述path cost还高的父节点 */
#define MAX_PATH_COST			100

/*
 * The rank must differ more than 1/PARENT_SWITCH_THRESHOLD_DIV in order
 * to switch preferred parent.
 * 选择一个新的父节点也不是乱选的，一定要高过一个门槛，这里定义的是2
 */
#define PARENT_SWITCH_THRESHOLD_DIV	2

typedef uint16_t rpl_path_metric_t;

static rpl_path_metric_t
calculate_path_metric(rpl_parent_t *p)
{
  uip_ds6_nbr_t *nbr;
  if(p == NULL) {
    return MAX_PATH_COST * RPL_DAG_MC_ETX_DIVISOR;
  }
  nbr = rpl_get_nbr(p);
  if(nbr == NULL) {
    return MAX_PATH_COST * RPL_DAG_MC_ETX_DIVISOR;
  }
// 如果metric container 是 none，那么rank 值就是 parent的rank+link_metric
#if RPL_DAG_MC == RPL_DAG_MC_NONE
  {
    return p->rank + (uint16_t)nbr->link_metric;
  }
// 如果container是 ETX 就是parent的etx+link_metric
#elif RPL_DAG_MC == RPL_DAG_MC_ETX
  return p->mc.obj.etx + (uint16_t)nbr->link_metric;
// 能量也是依次类推
#elif RPL_DAG_MC == RPL_DAG_MC_ENERGY
   /**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：return p->mc.obj.energy.energy_est + (uint16_t)nbr->link_metric;
	修改后：return energy_est;
	修改目的：
	修改path_metric的计算方法
    **************************************************************************/
  //return p->mc.obj.energy.energy_est + (uint16_t)nbr->link_metric;
  return energy_est;
#else
#error "Unsupported RPL_DAG_MC configured. See rpl.h."
#endif /* RPL_DAG_MC */
}

// 这个是重置
static void
reset(rpl_dag_t *dag)
{
  PRINTF("RPL: Reset MRHOF\n");
}

// call back neighbor
// 这个部分的内容，我一直觉得都是比较难得
static void
neighbor_link_callback(rpl_parent_t *p, int status, int numtx)
{
  uint16_t recorded_etx = 0;
  uint16_t packet_etx = numtx * RPL_DAG_MC_ETX_DIVISOR;
  uint16_t new_etx;
  uip_ds6_nbr_t *nbr = NULL;

  nbr = rpl_get_nbr(p);
  if(nbr == NULL) {
    /* No neighbor for this parent - something bad has occurred */
    return;
  }

  recorded_etx = nbr->link_metric;

  /*
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：空
	修改后：PRINTF("Status%d MAC_TX_OK%d MAC_TX_NOAC0K%d\n",status,MAC_TX_OK,MAC_TX_NOACK);
	修改目的：
	打印一下Status MAC_TX_OK MAC_TX_NOACK
	然后判断后面的if到底是怎么走的
	
  */
  //PRINTF("Status%d MAC_TX_OK%d MAC_TX_NOAC0K%d\n",status,MAC_TX_OK,MAC_TX_NOACK);
  /*
	通过打印得到status=0 MAC_TX_OK=0 MAC_TX_NOACK=2；
  */
  /* Do not penalize the ETX when collisions or transmission errors occur. */
  if(status == MAC_TX_OK || status == MAC_TX_NOACK) {
    if(status == MAC_TX_NOACK) {
      packet_etx = MAX_LINK_METRIC * RPL_DAG_MC_ETX_DIVISOR;
    }

    if(p->flags & RPL_PARENT_FLAG_LINK_METRIC_VALID) {
      /*
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：空
	修改后：PRINTF("Valicated!!!!!\n");
	修改目的：
	看看是不是验证过的ETX
    */
    //PRINTF("Valicated!!!!!\n");
      /*
	确实是进入到这个验证中
      */
      /* We already have a valid link metric, use weighted moving average to update it */

      new_etx = ((uint32_t)recorded_etx * ETX_ALPHA +
                 (uint32_t)packet_etx * (ETX_SCALE - ETX_ALPHA)) / ETX_SCALE;
    } else {
      /* We don't have a valid link metric, set it to the current packet's ETX */
      new_etx = packet_etx;
      /* Set link metric as valid */
      p->flags |= RPL_PARENT_FLAG_LINK_METRIC_VALID;
    }

    PRINTF("RPL: ETX changed from %u to %u (packet ETX = %u)\n",
        (unsigned)(recorded_etx / RPL_DAG_MC_ETX_DIVISOR),
        (unsigned)(new_etx  / RPL_DAG_MC_ETX_DIVISOR),
        (unsigned)(packet_etx / RPL_DAG_MC_ETX_DIVISOR));
    /* update the link metric for this nbr */
    nbr->link_metric = new_etx;
  }
}

/* 计算rpl rank值 */
static rpl_rank_t
calculate_rank(rpl_parent_t *p, rpl_rank_t base_rank)
{
  rpl_rank_t new_rank;
  rpl_rank_t rank_increase;
  uip_ds6_nbr_t *nbr;

  if(p == NULL || (nbr = rpl_get_nbr(p)) == NULL) {
    if(base_rank == 0) {
      return INFINITE_RANK;
    }
	// 这个情况，应该是特殊的没有父节点的情况，increase 
    rank_increase = RPL_INIT_LINK_METRIC * RPL_DAG_MC_ETX_DIVISOR;
  } else {
	// 一般情况下的increase其实就是link_metric
    rank_increase = nbr->link_metric;
    if(base_rank == 0) {
      base_rank = p->rank;
    }
  }

  if(INFINITE_RANK - base_rank < rank_increase) {
    /* Reached the maximum rank. */
    new_rank = INFINITE_RANK;
  } else {
   /* Calculate the rank based on the new rank information from DIO or
      stored otherwise. */
    new_rank = base_rank + rank_increase;
  }

  return new_rank;
}

/* 得到最佳的dag */
static rpl_dag_t *
best_dag(rpl_dag_t *d1, rpl_dag_t *d2)
{
  // 先判断grounded
  if(d1->grounded != d2->grounded) {
    return d1->grounded ? d1 : d2;
  }
  
  //再判断preference
  if(d1->preference != d2->preference) {
    return d1->preference > d2->preference ? d1 : d2;
  }
  
  // 最后判断rank
  return d1->rank < d2->rank ? d1 : d2;
}
// 选择最好的父节点
static rpl_parent_t *
best_parent(rpl_parent_t *p1, rpl_parent_t *p2)
{
  // 定义几个要用到的相关变量
  rpl_dag_t *dag;
  rpl_path_metric_t min_diff;
  rpl_path_metric_t p1_metric;
  rpl_path_metric_t p2_metric;

  // 因为这里有个前提，父节点，都是在同一个dag当中
  // 因此拿到一个dag就够了
  dag = p1->dag; /* Both parents are in the same DAG. */

  //最近diff是 rpl_dag_mc_etx因子/parent门槛
  min_diff = RPL_DAG_MC_ETX_DIVISOR /
             PARENT_SWITCH_THRESHOLD_DIV;

  //计算p1的path_metric
  p1_metric = calculate_path_metric(p1);
  //计算p2的path_metric
  p2_metric = calculate_path_metric(p2);

  /* 如果rank一样，就维持原有的preference */
  /* Maintain stability of the preferred parent in case of similar ranks. */
  if(p1 == dag->preferred_parent || p2 == dag->preferred_parent) {
    if(p1_metric < p2_metric + min_diff &&
       p1_metric > p2_metric - min_diff) {
      PRINTF("RPL: MRHOF hysteresis: %u <= %u <= %u\n",
             p2_metric - min_diff,
             p1_metric,
             p2_metric + min_diff);
      return dag->preferred_parent;
    }
  }
  
 
  /**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：return p1_metric < p2_metric ? p1 : p2;
	修改后：return p1_metric > p2_metric ? p1 : p2;
	修改目的：
	当度量是ETX的时候，当然是谁小选谁，但是如果度量单位是剩余能量的时候，当然是
	谁大选谁。
    **************************************************************************/
 //哪个的metric小，返回哪一个
 //return p1_metric < p2_metric ? p1 : p2;
 return p1_metric > p2_metric ? p1 : p2;
}

//如果没有metric container
#if RPL_DAG_MC == RPL_DAG_MC_NONE
static void
update_metric_container(rpl_instance_t *instance)
{
  // 其实我觉得挺奇怪的，如果没有conf mc，最后赋的值还是none
  instance->mc.type = RPL_DAG_MC;
}
#else
static void
update_metric_container(rpl_instance_t *instance)
{
  rpl_path_metric_t path_metric;
  rpl_dag_t *dag;
#if RPL_DAG_MC == RPL_DAG_MC_ENERGY
  uint8_t type;
#endif
  
  //修改关于instance有关的mc选项
  instance->mc.type = RPL_DAG_MC;
  instance->mc.flags = RPL_DAG_MC_FLAG_P;
  instance->mc.aggr = RPL_DAG_MC_AGGR_ADDITIVE;
  instance->mc.prec = 0;

  dag = instance->current_dag;

  if (!dag->joined) {
    PRINTF("RPL: Cannot update the metric container when not joined\n");
    return;
  }
  
  // 如果是root metric设置为0
  if(dag->rank == ROOT_RANK(instance)) {
    path_metric = 0;
  } else {
    path_metric = calculate_path_metric(dag->preferred_parent);
  }

// 如果container是etx
#if RPL_DAG_MC == RPL_DAG_MC_ETX
  instance->mc.length = sizeof(instance->mc.obj.etx);
  instance->mc.obj.etx = path_metric;

  PRINTF("RPL: My path ETX to the root is %u.%u\n",
	instance->mc.obj.etx / RPL_DAG_MC_ETX_DIVISOR,
	(instance->mc.obj.etx % RPL_DAG_MC_ETX_DIVISOR * 100) /
	 RPL_DAG_MC_ETX_DIVISOR);
#elif RPL_DAG_MC == RPL_DAG_MC_ENERGY
  instance->mc.length = sizeof(instance->mc.obj.energy);

  if(dag->rank == ROOT_RANK(instance)) {
    type = RPL_DAG_MC_ENERGY_TYPE_MAINS;
  } else {
    type = RPL_DAG_MC_ENERGY_TYPE_BATTERY;
  }

  instance->mc.obj.energy.flags = type << RPL_DAG_MC_ENERGY_TYPE;
  instance->mc.obj.energy.energy_est = path_metric;
#endif /* RPL_DAG_MC == RPL_DAG_MC_ETX */
}
#endif /* RPL_DAG_MC == RPL_DAG_MC_NONE */

/** @}*/
