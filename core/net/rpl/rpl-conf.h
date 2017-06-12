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
 *	Public configuration and API declarations for ContikiRPL.
 * \author
 *	Joakim Eriksson <joakime@sics.se> & Nicolas Tsiftes <nvt@sics.se>
 *
 */

#ifndef RPL_CONF_H
#define RPL_CONF_H

// include contiki-conf head file
#include "contiki-conf.h"


/* Set to 1 to enable RPL statistics */
#ifndef RPL_CONF_STATS
#define RPL_CONF_STATS 0
#endif /* RPL_CONF_STATS */

/* 
 * 选择当前环境下的路由metric，但是必须是一个有效的metric类型。现在只支持
 * ETX和Energy。当使用MRFOH函数的时候，就不用使用其他的metric container了
 *
 * Select routing metric supported at runtime. This must be a valid
 * DAG Metric Container Object Type (see below). Currently, we only 
 * support RPL_DAG_MC_ETX and RPL_DAG_MC_ENERGY.
 * When MRHOF (RFC6719) is used with ETX, no metric container must
 * be used; instead the rank carries ETX directly.
 */
#ifdef RPL_CONF_DAG_MC
#define RPL_DAG_MC RPL_CONF_DAG_MC
#else
/*
	date :20170531
	version: 1.0 
	修改内容：下一行
	修改前：#define RPL_DAG_MC RPL_DAG_MC_NONE
	修改后：#define RPL_DAG_MC RPL_DAG_MC_ENERGY
	修改期望效果：
	这个部分是修改Metric的，把原来的没有Metric改为采用能量的Metric
*/
//#define RPL_DAG_MC RPL_DAG_MC_NONE
#define RPL_DAG_MC RPL_DAG_MC_ENERGY
#endif /* RPL_CONF_DAG_MC */

/*
 * 目标函数是通过RPL_CONF_OF这个参数来配置
 * The objective function used by RPL is configurable through the 
 * RPL_CONF_OF parameter. This should be defined to be the name of an 
 * rpl_of object linked into the system image, e.g., rpl_of0.
 */
#ifdef RPL_CONF_OF
#define RPL_OF RPL_CONF_OF
#else
/* ETX is the default objective function. */
/* 默认的就是mrhof */
#define RPL_OF rpl_mrhof
#endif /* RPL_CONF_OF */

/* This value decides which DAG instance we should participate in by default. */
/* 这个参数决定了我们应该默认加入哪个instance和dag */
#ifdef RPL_CONF_DEFAULT_INSTANCE
#define RPL_DEFAULT_INSTANCE RPL_CONF_DEFAULT_INSTANCE
#else
#define RPL_DEFAULT_INSTANCE	       0x1e
#endif /* RPL_CONF_DEFAULT_INSTANCE */

/*
 * 这个值决定了这个值是不是必须是叶子节点还是默认非叶子节点
 * This value decides if this node must stay as a leaf or not
 * as allowed by draft-ietf-roll-rpl-19#section-8.5
 */
#ifdef RPL_CONF_LEAF_ONLY
#define RPL_LEAF_ONLY RPL_CONF_LEAF_ONLY
#else
#define RPL_LEAF_ONLY 0
#endif

/*
 * 定义了最大允许的instance数目，如果没有定义默认就是1
 * Maximum of concurent RPL instances.
 */
#ifdef RPL_CONF_MAX_INSTANCES
#define RPL_MAX_INSTANCES     RPL_CONF_MAX_INSTANCES
#else
#define RPL_MAX_INSTANCES     1
#endif /* RPL_CONF_MAX_INSTANCES */

/*
 * 定义了一个instance里面最大的dag数，默认2
 * Maximum number of DAGs within an instance.
 */
#ifdef RPL_CONF_MAX_DAG_PER_INSTANCE
#define RPL_MAX_DAG_PER_INSTANCE     RPL_CONF_MAX_DAG_PER_INSTANCE
#else
#define RPL_MAX_DAG_PER_INSTANCE     2
#endif /* RPL_CONF_MAX_DAG_PER_INSTANCE */

/*
 * RPL默认生存时间
 * RPL Default route lifetime
 * RPL路由生存时间是用于下行路由和默认路由的。在高密度的DIO网络中，很有可能出现以下情况，
 * 一个节点从未发送过任何一个DIO，interval非常的高，它却已经从其他很多节点那儿接收到DIO了。
 * 当收到了dio的时候，就会reset timer，使得默认路由的timer无穷被改变。
 * 
 * The RPL route lifetime is used for the downward routes and for the default
 * route. In a high density network with DIO suppression activated it may happen
 * that a node will never send a DIO once the DIO interval becomes high as it
 * has heard DIO from many neighbors already. As the default route to the
 * preferred parent has a lifetime reset by receiving DIO from the parent, it
 * means that the default route can be destroyed after a while. Setting the
 * default route with infinite lifetime secures the upstream route.
 */
#ifdef RPL_CONF_DEFAULT_ROUTE_INFINITE_LIFETIME
#define RPL_DEFAULT_ROUTE_INFINITE_LIFETIME                    RPL_CONF_DEFAULT_ROUTE_INFINITE_LIFETIME
#else
#define RPL_DEFAULT_ROUTE_INFINITE_LIFETIME                    0
#endif /* RPL_CONF_DEFAULT_ROUTE_INFINITE_LIFETIME */

/*
 * 
 */
#ifndef RPL_CONF_DAO_SPECIFY_DAG
  #if RPL_MAX_DAG_PER_INSTANCE > 1
    #define RPL_DAO_SPECIFY_DAG 1
  #else
    #define RPL_DAO_SPECIFY_DAG 0
  #endif /* RPL_MAX_DAG_PER_INSTANCE > 1 */
#else
  #define RPL_DAO_SPECIFY_DAG RPL_CONF_DAO_SPECIFY_DAG
#endif /* RPL_CONF_DAO_SPECIFY_DAG */

/*
 * The DIO interval (n) represents 2^n ms.
 * DIO的时间间隔的公式是2^n次方
 * 默认值是3，即8ms，contiki认为太小了改成了4s
 * According to the specification, the default value is 3 which
 * means 8 milliseconds. That is far too low when using duty cycling
 * with wake-up intervals that are typically hundreds of milliseconds.
 * ContikiRPL thus sets the default to 2^12 ms = 4.096 s.
 */
#ifdef RPL_CONF_DIO_INTERVAL_MIN
#define RPL_DIO_INTERVAL_MIN        RPL_CONF_DIO_INTERVAL_MIN
#else
#define RPL_DIO_INTERVAL_MIN        12
#endif

/*
 * Maximum amount of timer doublings.
 * timer doubling的那个最大值，也就是那个n。这里设为8，因为在rpc里面是
 * 最大时间间隔为2^(20)
 * 
 * The maximum interval will by default be 2^(12+8) ms = 1048.576 s.
 * RFC 6550 suggests a default value of 20, which of course would be
 * unsuitable when we start with a minimum interval of 2^12.
 */
#ifdef RPL_CONF_DIO_INTERVAL_DOUBLINGS
#define RPL_DIO_INTERVAL_DOUBLINGS  RPL_CONF_DIO_INTERVAL_DOUBLINGS
#else
#define RPL_DIO_INTERVAL_DOUBLINGS  8
#endif

/* 
 * DIO redundancy. To learn more about this, see RFC 6206.
 * DIO冗余，如果要知道详细介绍，看rfc6206
 * 
 * RFC6550 建议默认值为10，但是并没有人明白到底这个值得依据是什么。
 * 
 * RFC 6550 suggests a default value of 10. It is unclear what the basis
 * of this suggestion is. Network operators might attain more efficient
 * operation by tuning this parameter for specific deployments.
 */
#ifdef RPL_CONF_DIO_REDUNDANCY
#define RPL_DIO_REDUNDANCY          RPL_CONF_DIO_REDUNDANCY
#else
#define RPL_DIO_REDUNDANCY          10
#endif

/*
 * Initial metric attributed to a link when the ETX is unknown
 * 初始化metric值 2
 */
#ifndef RPL_CONF_INIT_LINK_METRIC
#define RPL_INIT_LINK_METRIC        2
#else
#define RPL_INIT_LINK_METRIC        RPL_CONF_INIT_LINK_METRIC
#endif

/*
 * 默认路由生存时间模块，这是rpl 生存时间值中使用的时间间隔
 * Default route lifetime unit. This is the granularity of time
 * used in RPL lifetime values, in seconds.
 */
#ifndef RPL_CONF_DEFAULT_LIFETIME_UNIT
#define RPL_DEFAULT_LIFETIME_UNIT       0xffff
#else
#define RPL_DEFAULT_LIFETIME_UNIT       RPL_CONF_DEFAULT_LIFETIME_UNIT
#endif

/* 
 * 默认路由生存时间作为多播时候的生存时间模块
 * Default route lifetime as a multiple of the lifetime unit.
 */
#ifndef RPL_CONF_DEFAULT_LIFETIME
#define RPL_DEFAULT_LIFETIME            0xff
#else
#define RPL_DEFAULT_LIFETIME            RPL_CONF_DEFAULT_LIFETIME
#endif

/*
 * dag的preference 默认为0
 * DAG preference field
 */
#ifdef RPL_CONF_PREFERENCE
#define RPL_PREFERENCE              RPL_CONF_PREFERENCE
#else
#define RPL_PREFERENCE              0
#endif

/*
 * Hop-by-hop option
 * This option control the insertion of the RPL Hop-by-Hop extension header
 * into packets originating from this node. Incoming Hop-by-hop extension
 * header are still processed and forwarded.
 */
#ifdef RPL_CONF_INSERT_HBH_OPTION
#define RPL_INSERT_HBH_OPTION       RPL_CONF_INSERT_HBH_OPTION
#else
#define RPL_INSERT_HBH_OPTION       1
#endif


/*************************************************************************

		以下内容都是关于probing rpl的，但是我并不清楚 probing rpl是啥

 *************************************************************************/
/*
 * RPL probing. When enabled, probes will be sent periodically to keep
 * parent link estimates up to date.
 * */
#ifdef RPL_CONF_WITH_PROBING
#define RPL_WITH_PROBING RPL_CONF_WITH_PROBING
#else
#define RPL_WITH_PROBING 1
#endif

/*
 * RPL probing interval.
 * */
#ifdef RPL_CONF_PROBING_INTERVAL
#define RPL_PROBING_INTERVAL RPL_CONF_PROBING_INTERVAL
#else
#define RPL_PROBING_INTERVAL (120 * CLOCK_SECOND)
#endif

/*
 * RPL probing expiration time.
 * */
#ifdef RPL_CONF_PROBING_EXPIRATION_TIME
#define RPL_PROBING_EXPIRATION_TIME RPL_CONF_PROBING_EXPIRATION_TIME
#else
#define RPL_PROBING_EXPIRATION_TIME (10 * 60 * CLOCK_SECOND)
#endif

/*
 * Function used to select the next parent to be probed.
 * */
#ifdef RPL_CONF_PROBING_SELECT_FUNC
#define RPL_PROBING_SELECT_FUNC RPL_CONF_PROBING_SELECT_FUNC
#else
#define RPL_PROBING_SELECT_FUNC(dag) get_probing_target((dag))
#endif

/*
 * Function used to send RPL probes.
 * To probe with DIO, use:
 * #define RPL_CONF_PROBING_SEND_FUNC(instance, addr) dio_output((instance), (addr))
 * To probe with DIS, use:
 * #define RPL_CONF_PROBING_SEND_FUNC(instance, addr) dis_output((addr))
 * Any other custom probing function is also acceptable.
 * */
#ifdef RPL_CONF_PROBING_SEND_FUNC
#define RPL_PROBING_SEND_FUNC RPL_CONF_PROBING_SEND_FUNC
#else
#define RPL_PROBING_SEND_FUNC(instance, addr) dio_output((instance), (addr))
#endif

/*
 * Function used to calculate next RPL probing interval
 * */
#ifdef RPL_CONF_PROBING_DELAY_FUNC
#define RPL_PROBING_DELAY_FUNC RPL_CONF_PROBING_DELAY_FUNC
#else
#define RPL_PROBING_DELAY_FUNC() ((RPL_PROBING_INTERVAL / 2) \
    + random_rand() % (RPL_PROBING_INTERVAL))
#endif

#endif /* RPL_CONF_H */
