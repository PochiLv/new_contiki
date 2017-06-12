/******************************************************************

		我才不管这个是什么，反正我就是邋邋ICMP6最熟，所以
		我从这个文件开始注释，这个文件，其实是非常重要的
		控制消息的发送，几乎都是依靠这个文件完成的。

*******************************************************************/
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
 *         ICMP6 I/O for RPL control messages.
 *
 * \author Joakim Eriksson <joakime@sics.se>, Nicolas Tsiftes <nvt@sics.se>
 * Contributors: Niclas Finne <nfi@sics.se>, Joel Hoglund <joel@sics.se>,
 *               Mathieu Pouillot <m.pouillot@watteco.com>
 *               George Oikonomou <oikonomou@users.sourceforge.net> (multicast)
 */

/**
 * \addtogroup uip6
 * @{
 */

/********************************************************

			前面这个部分就是引入头文件

*********************************************************/

#include "net/ip/tcpip.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-nd6.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/rpl/rpl-private.h"
#include "net/packetbuf.h"
#include "net/ipv6/multicast/uip-mcast6.h"

#include <limits.h>
#include <string.h>

 /*************************************************************************
	这个部分是我添加的，用来打印debug信息的，因为有些下面打印的信息和
	DEBUG定义成什么是有关系的。

	下面这部分内容定义在uip-debug.h
	
    DEBUG_NONE      0
	DEBUG_PRINT     1
	DEBUG_ANNOTATE  2
	DEBUG_FULL      DEBUG_ANNOTATE | DEBUG_PRINT


	modified date:	2016/10/9

	version:	v1.0.2

	modified line:  following 2 line
**************************************************************************/



//#define DEBUG DEBUG_NONE
//#define DEBUG DEBUG_FULL
#define DEBUG DEBUG_PRINT
//#define DEBUG DEBUG_ANNOTATE

 /*************************************************************************
	当这个东西RPL_CONF_DAO_ACK设置成1的时候，才有ACK，
	不然CONTIKI默认不发


	modified date:		2016/10/10

	version:			v1.1.2
	
	modified line:		following 1 line

	before:				none

	after:				#define RPL_CONF_DAO_ACK 1
**************************************************************************/


#define RPL_CONF_DAO_ACK 1


#include "net/ip/uip-debug.h"

/*---------------------------------------------------------------------------*/

/*****************************************************************************

		这里面定义的是一些用以判断GROUND 或者MOP的一些常量

*****************************************************************************/
#define RPL_DIO_GROUNDED                 0x80
#define RPL_DIO_MOP_SHIFT                3
#define RPL_DIO_MOP_MASK                 0x38
#define RPL_DIO_PREFERENCE_MASK          0x07

/****************************************************************************

		这部分和UIP有关系，很多相关的信息，其实是从UIP那里拿到的

*****************************************************************************/
#define UIP_IP_BUF       ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_ICMP_BUF     ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_ICMP_PAYLOAD ((unsigned char *)&uip_buf[uip_l2_l3_icmp_hdr_len])
/*---------------------------------------------------------------------------*/
static void dis_input(void);
static void dio_input(void);
static void dao_input(void);
static void dao_ack_input(void);

/* some debug callbacks useful when debugging RPL networks */
/* 看起来这个部分是用来做DIO DAO debug用的，但是我一次都没用过 */
#ifdef RPL_DEBUG_DIO_INPUT
void RPL_DEBUG_DIO_INPUT(uip_ipaddr_t *, rpl_dio_t *);
#endif

#ifdef RPL_DEBUG_DAO_OUTPUT
void RPL_DEBUG_DAO_OUTPUT(rpl_parent_t *);
#endif

/*************************************************************************

	因为查找RFC6550显示，rpl的counter被细分为lollipop，所以这个东西，
	应该就是用来用做测试序列的。

**************************************************************************/
static uint8_t dao_sequence = RPL_LOLLIPOP_INIT;

/*************************************************************************
	这一系列定义的东西，我用来计算各种种类的控制消息数目的。
	至于为什么用uint16_t，因为我一开始用的uint8_t结果最大只能计数到255，
	差评，最后改成这个的。


	modified date:	2016/11/20

	version:	v2.1.1 

	modified line:  following 8 line

	description:	patch a bug of msg num can not up to 255
**************************************************************************/
static uint16_t dis_received_num=0;
static uint16_t dio_received_num=0;
static uint16_t dao_received_num=0;
static uint16_t dao_ack_received_num=0;
static uint16_t dis_sended_num=0;
static uint16_t dio_sended_num=0;
static uint16_t dao_sended_num=0;
static uint16_t dao_ack_sended_num=0;

 /**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：空
	修改后：int energy_est=1280;
	修改目的：
	定义剩余能量的初始值，为后面的剩余能量相关的操作奠定基础
    **************************************************************************/
int energy_est=1000;

/* 这个应该是RPL目标函数的变量 */
extern rpl_of_t RPL_OF;

/* 如果有配置这个RPL_CONF_MULTICAST则定义一个变量，就是下面这个变量 */
#if RPL_CONF_MULTICAST
static uip_mcast6_route_t *mcast_group;
#endif
/*---------------------------------------------------------------------------*/
/* Initialise RPL ICMPv6 message handlers */
/* 初始化ICMP6的消息处理器 */
UIP_ICMP6_HANDLER(dis_handler, ICMP6_RPL, RPL_CODE_DIS, dis_input);
UIP_ICMP6_HANDLER(dio_handler, ICMP6_RPL, RPL_CODE_DIO, dio_input);
UIP_ICMP6_HANDLER(dao_handler, ICMP6_RPL, RPL_CODE_DAO, dao_input);
UIP_ICMP6_HANDLER(dao_ack_handler, ICMP6_RPL, RPL_CODE_DAO_ACK, dao_ack_input);
/*---------------------------------------------------------------------------*/

/********************************************************

	通过底层的uip地址得到全局global地址放在addr里面
	如果获得了global_addr那么，就返回1，不然就返回0

*********************************************************/
static int
get_global_addr(uip_ipaddr_t *addr)
{
  int i;
  int state;

  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      if(!uip_is_addr_link_local(&uip_ds6_if.addr_list[i].ipaddr)) {
        memcpy(addr, &uip_ds6_if.addr_list[i].ipaddr, sizeof(uip_ipaddr_t));
        return 1;
      }
    }
  }
  return 0;
}
/******************************************************************************

		这部分我看不懂，什么又get16，又get32的，而且又没有注释

*******************************************************************************/
/*---------------------------------------------------------------------------*/
static uint32_t
get32(uint8_t *buffer, int pos)
{
  return (uint32_t)buffer[pos] << 24 | (uint32_t)buffer[pos + 1] << 16 |
         (uint32_t)buffer[pos + 2] << 8 | buffer[pos + 3];
}
/*---------------------------------------------------------------------------*/
static void
set32(uint8_t *buffer, int pos, uint32_t value)
{
  buffer[pos++] = value >> 24;
  buffer[pos++] = (value >> 16) & 0xff;
  buffer[pos++] = (value >> 8) & 0xff;
  buffer[pos++] = value & 0xff;
}
/*---------------------------------------------------------------------------*/
static uint16_t
get16(uint8_t *buffer, int pos)
{
  return (uint16_t)buffer[pos] << 8 | buffer[pos + 1];
}
/*---------------------------------------------------------------------------*/
static void
set16(uint8_t *buffer, int pos, uint16_t value)
{
  buffer[pos++] = value >> 8;
  buffer[pos++] = value & 0xff;
}
/*---------------------------------------------------------------------------*/
/* 接收dis消息 */
static void
dis_input(void)
{
  //局部变量定义了instance和end,都是rpl_instance类型的
  rpl_instance_t *instance;
  rpl_instance_t *end;

  /* DAG Information Solicitation */
  /* 这个就是DIS的全称 */
  PRINTF("RPL: Received a DIS from ");
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF("\n");

  //遍历instance列表，但是其实我们一般只有一个instance就对了
  for(instance = &instance_table[0], end = instance + RPL_MAX_INSTANCES;
      instance < end; ++instance) {
	//遍历到的这个instance还是要有人用的，没有人用就没意义了。
	if(instance->used == 1) {
//如果是叶子节点
#if RPL_LEAF_ONLY
	  //如果接收的DIS是单播，那么
      if(!uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) {
	  //打印，DIS只有多播才会reset DIO 定时器
	    PRINTF("RPL: LEAF ONLY Multicast DIS will NOT reset DIO timer\n");
 //如果不是叶子节点
#else /* !RPL_LEAF_ONLY */
	  //如果接收到的DIS是多播
	  if(uip_is_addr_mcast(&UIP_IP_BUF->destipaddr)) {
		//打印"接收多播DIS,reset DIO 定时器"
		PRINTF("RPL: Multicast DIS => reset DIO timer\n");
		//同时reset DIO定时器
		rpl_reset_dio_timer(instance);
      } else {
#endif /* !RPL_LEAF_ONLY */
		//打印"RPL: 单播DIS，回复"单播DIS，回复sender
        PRINTF("RPL: Unicast DIS, reply to sender\n");
        dio_output(instance, &UIP_IP_BUF->srcipaddr);
      }
    }
  }
  //设置uip长度为0
  uip_len = 0;
/**************************************************************************
	Date:20170608
	Version:2.0
	修改行数：2
	修改前：空
	修改后：
		energy_est=energy_est-1;
		PRINTF("enery_est:%d",energy_est);
	修改目的：
	接收dis、dio、dao、dao-ack的时候都-2
    **************************************************************************/
  energy_est=energy_est-2;
  PRINTF("enery_est:%d",energy_est);
/*************************************************************************
	增加dis_r的数量，并且打印出各控制消息的数量

	modified date:	2016/10/10

	version:	v1.1.0

	modified line:  following 2 line
**************************************************************************/
  dis_received_num++;
  PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);
}
/*---------------------------------------------------------------------------*/
/* 发送dis的函数 */
void
dis_output(uip_ipaddr_t *addr)
{
  //定义字符数组
  unsigned char *buffer;
  //定义uip地址
  uip_ipaddr_t tmpaddr;

  /*
   * DAG Information Solicitation  - 2 bytes reserved
   *      0                   1                   2
   *      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
   *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   *     |     Flags     |   Reserved    |   Option(s)...
   *     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   */

  // uip_icmp负载赋到buffer上
  buffer = UIP_ICMP_PAYLOAD;
  buffer[0] = buffer[1] = 0;

  //如果dis的目标地址为空
  if(addr == NULL) {
  	//tmpaddr的意思应该就是tempaddr，暂存addr，把本地多播地址放在tmpaddr里
    uip_create_linklocal_rplnodes_mcast(&tmpaddr);
	//再把tmpaddr附于addr
    addr = &tmpaddr;
  }

  //打印"发送DIS给xxx(地址)"
  PRINTF("RPL: Sending a DIS to ");
  PRINT6ADDR(addr);
  PRINTF("\n");
  //发送icmp消息，类型RPL，dis控制消息,负载消息长度2
  uip_icmp6_send(addr, ICMP6_RPL, RPL_CODE_DIS, 2);
/**************************************************************************
	Date:20170608
	Version:2.0
	修改行数：1
	修改前：空
	修改后：
		energy_est=energy_est-2;
		PRINTF("enery_est:%d",energy_est);
		
	修改目的：
	发送dis、dio、dao、dao-ack的时候都-2
    **************************************************************************/
   energy_est=energy_est-2;
   PRINTF("enery_est:%d",energy_est);
  /*************************************************************************
    发送dis数量++

	modified date:	2016/10/9

	version:	v1.0.1

	modified line:  following 2 line
**************************************************************************/
  dis_sended_num++;
  PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);
}
/*---------------------------------------------------------------------------*/
/* 接收dio消息 */
static void
dio_input(void)
{
  //定义buffer
  unsigned char *buffer;
  //定义buffer长度
  uint8_t buffer_length;
  //定义dio
  rpl_dio_t dio;
  //定义option的类型
  uint8_t subopt_type;
  int i;
  int len;
  //定义dio消息从哪里发过来的
  uip_ipaddr_t from;
  uip_ds6_nbr_t *nbr;
  //将dio那部分内存空间清空，用0去代替
  memset(&dio, 0, sizeof(dio));

  /* Set default values in case the DIO configuration option is missing. */
  /* 设置默认值，以防DIO option丢失 */
  //设置dag的间隔
  dio.dag_intdoubl = RPL_DIO_INTERVAL_DOUBLINGS;
  //设置dio最小间隔
  dio.dag_intmin = RPL_DIO_INTERVAL_MIN;
  //设置dio的冗余，默认值是10
  dio.dag_redund = RPL_DIO_REDUNDANCY;
  //rpl最小增长跳数
  dio.dag_min_hoprankinc = RPL_MIN_HOPRANKINC;
  //rpl最大增长跳数
  dio.dag_max_rankinc = RPL_MAX_RANKINC;
  //ocp的全称是object code point意思是选哪一个of
  //如果定义成0，调用的就是OF0目标函数
  //如果定义成1，调用的就是MRHOF目标函数
  //默认是mrhof
  dio.ocp = RPL_OF.ocp;
  //设置dio默认生存时间，默认0xff
  dio.default_lifetime = RPL_DEFAULT_LIFETIME;
  //设置dio的生存时间模块，默认为0xffff
  dio.lifetime_unit = RPL_DEFAULT_LIFETIME_UNIT;
  //将发送者的地址赋给from
  uip_ipaddr_copy(&from, &UIP_IP_BUF->srcipaddr);

  /* DAG Information Object */
  PRINTF("RPL: Received a DIO from ");
  PRINT6ADDR(&from);
  PRINTF("\n");
  //如果在本节点的邻居列表中找不到发送节点的地址
  if((nbr = uip_ds6_nbr_lookup(&from)) == NULL) {
  	//如果存储邻居节点地址的列表没有满
    if((nbr = uip_ds6_nbr_add(&from, (uip_lladdr_t *)
                              packetbuf_addr(PACKETBUF_ADDR_SENDER),
                              0, NBR_REACHABLE)) != NULL) {
      /* set reachable timer */
	  //设置邻居节点可达计时器
	  stimer_set(&nbr->reachable, UIP_ND6_REACHABLE_TIME / 1000);
	  //将邻居节点添加到邻居节点缓存里面
      PRINTF("RPL: Neighbor added to neighbor cache ");
      PRINT6ADDR(&from);
      PRINTF(", ");
      PRINTLLADDR((uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER));
      PRINTF("\n");
    } else {
      //如果存邻居节点的缓存已经满了，就要打印提示
      PRINTF("RPL: Out of memory, dropping DIO from ");
      PRINT6ADDR(&from);
      PRINTF(", ");
      PRINTLLADDR((uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER));
      PRINTF("\n");
      return;
    }
  } else {
    //如果是查找结果是节点已经在邻居节点缓存里面
    PRINTF("RPL: Neighbor already in neighbor cache\n");
  }

  //计算出buff的长度
  buffer_length = uip_len - uip_l3_icmp_hdr_len;

  /* Process the DIO base option. */
  /* 处理DIO基础选项 */
  i = 0;
  //buffer就是uip弄上来的消息
  buffer = UIP_ICMP_PAYLOAD;

  //根据rfc6550定义的dio的格式，将报文的信息提取出来
  //赋给dio的成员变量。
  dio.instance_id = buffer[i++];
  dio.version = buffer[i++];
  dio.rank = get16(buffer, i);
  //因为这里是有保留位置的，所以i+=2
  i += 2;
  //打印出接收到的DIO消息的id,version,rank
  PRINTF("RPL: Incoming DIO (id, ver, rank) = (%u,%u,%u)\n",
         (unsigned)dio.instance_id,
         (unsigned)dio.version,
         (unsigned)dio.rank);

  /* define dio other property */
  dio.grounded = buffer[i] & RPL_DIO_GROUNDED;
  /* mop have 4 values, the default value is 2 means unicast dao up and down route */
  dio.mop = (buffer[i]& RPL_DIO_MOP_MASK) >> RPL_DIO_MOP_SHIFT;
  /* preference present the how preferable the root is compared to other node  */
  /* the least preferred is 0x00, most preferred is 0x07 */
  dio.preference = buffer[i++] & RPL_DIO_PREFERENCE_MASK;
  /* maintain downward route */
  dio.dtsn = buffer[i++];
  /* two reserved bytes */
  i += 2;

  /* a type of function use to set a block data */
  memcpy(&dio.dag_id, buffer + i, sizeof(dio.dag_id));
  i += sizeof(dio.dag_id);

  /* print the id and preference */
  PRINTF("RPL: Incoming DIO (dag_id, pref) = (");
  PRINT6ADDR(&dio.dag_id);
  PRINTF(", %u)\n", dio.preference);

  /* Check if there are any DIO suboptions. */
  for(; i < buffer_length; i += len) {
    /* get suboption type ,total types */
    subopt_type = buffer[i];
    if(subopt_type == RPL_OPTION_PAD1) {
      len = 1;
    } else {
      /* Suboption with a two-byte header + payload */
      len = 2 + buffer[i + 1];
    }

    /* if the length is out of range represent that it's an invalid dio */
    if(len + i > buffer_length) {
      PRINTF("RPL: Invalid DIO packet\n");
      RPL_STAT(rpl_stats.malformed_msgs++);
      return;
    }

    /* print the sub opt type */
    PRINTF("RPL: DIO option %u, length: %u\n", subopt_type, len - 2);

    switch(subopt_type) {
    /* subopt=0x02 */
    case RPL_OPTION_DAG_METRIC_CONTAINER:
      if(len < 6) {
        PRINTF("RPL: Invalid DAG MC, len = %d\n", len);
	RPL_STAT(rpl_stats.malformed_msgs++);
        return;
      }
      dio.mc.type = buffer[i + 2];
      dio.mc.flags = buffer[i + 3] << 1;
      dio.mc.flags |= buffer[i + 4] >> 7;
      dio.mc.aggr = (buffer[i + 4] >> 4) & 0x3;
      dio.mc.prec = buffer[i + 4] & 0xf;
      dio.mc.length = buffer[i + 5];

      if(dio.mc.type == RPL_DAG_MC_NONE) {
        /* No metric container: do nothing */
      } else if(dio.mc.type == RPL_DAG_MC_ETX) {
        /* get parent's or neighbor's node etx as metric*/
        dio.mc.obj.etx = get16(buffer, i + 6);

        PRINTF("RPL: DAG MC: type %u, flags %u, aggr %u, prec %u, length %u, ETX %u\n",
	       (unsigned)dio.mc.type,
	       (unsigned)dio.mc.flags,
	       (unsigned)dio.mc.aggr,
	       (unsigned)dio.mc.prec,
	       (unsigned)dio.mc.length,
	       (unsigned)dio.mc.obj.etx);
      } else if(dio.mc.type == RPL_DAG_MC_ENERGY) {
        /* use energy as metric and get the energy metric */
        dio.mc.obj.energy.flags = buffer[i + 6];
        dio.mc.obj.energy.energy_est = buffer[i + 7];
      } else {
       /* other metric is not allowed in Contiki RPL */
       PRINTF("RPL: Unhandled DAG MC type: %u\n", (unsigned)dio.mc.type);
       return;
      }
      break;
    /* value=0x03;this option is about route information */
    case RPL_OPTION_ROUTE_INFO:
      if(len < 9) {
        PRINTF("RPL: Invalid destination prefix option, len = %d\n", len);
	RPL_STAT(rpl_stats.malformed_msgs++);
        return;
      }

      /* The flags field includes the preference value. */
      dio.destination_prefix.length = buffer[i + 2];
      dio.destination_prefix.flags = buffer[i + 3];
      dio.destination_prefix.lifetime = get32(buffer, i + 4);

      /* 
   	  I can not figure it out, why should copy destination?
	  destiantion is this node? or sender node?
      */
      if(((dio.destination_prefix.length + 7) / 8) + 8 <= len &&
         dio.destination_prefix.length <= 128) {
        PRINTF("RPL: Copying destination prefix\n");
        memcpy(&dio.destination_prefix.prefix, &buffer[i + 8],
               (dio.destination_prefix.length + 7) / 8);
      } else {
        PRINTF("RPL: Invalid route info option, len = %d\n", len);
	RPL_STAT(rpl_stats.malformed_msgs++);
	return;
      }

      break;
    /*
	value:0x04
 	represent dodag configuration
    */
    case RPL_OPTION_DAG_CONF:
      if(len != 16) {
        PRINTF("RPL: Invalid DAG configuration option, len = %d\n", len);
	RPL_STAT(rpl_stats.malformed_msgs++);
        return;
      }

      /* Path control field not yet implemented - at i + 2 */
      // about dag interval 
      dio.dag_intdoubl = buffer[i + 3];
      dio.dag_intmin = buffer[i + 4];
      // about dag redundency
      dio.dag_redund = buffer[i + 5];
      // about rank increase
      dio.dag_max_rankinc = get16(buffer, i + 6);
      dio.dag_min_hoprankinc = get16(buffer, i + 8);
      // about which of should select
      dio.ocp = get16(buffer, i + 10);
      /* buffer + 12 is reserved */
      dio.default_lifetime = buffer[i + 13];
      dio.lifetime_unit = get16(buffer, i + 14);
      /* it seems like I have written */
      PRINTF("RPL: DAG conf:dbl=%d, min=%d red=%d maxinc=%d mininc=%d ocp=%d d_l=%u l_u=%u\n",
             dio.dag_intdoubl, dio.dag_intmin, dio.dag_redund,
             dio.dag_max_rankinc, dio.dag_min_hoprankinc, dio.ocp,
             dio.default_lifetime, dio.lifetime_unit);
      break;
    /* value: 0x08 about prefix infomation */
    case RPL_OPTION_PREFIX_INFO:
      if(len != 32) {
        PRINTF("RPL: Invalid DAG prefix info, len != 32\n");
	RPL_STAT(rpl_stats.malformed_msgs++);
        return;
      }
      dio.prefix_info.length = buffer[i + 2];
      dio.prefix_info.flags = buffer[i + 3];
      /* valid lifetime is ingnored for now - at i + 4 */
      /* preferred lifetime stored in lifetime */
      dio.prefix_info.lifetime = get32(buffer, i + 8);
      /* 32-bit reserved at i + 12 */
      PRINTF("RPL: Copying prefix information\n");
      memcpy(&dio.prefix_info.prefix, &buffer[i + 16], 16);
      PRINTF("Prefix Information:");
      PRINT6ADDR(&dio.prefix_info.prefix);
      PRINTF("\n");
      break;
    default:
      PRINTF("RPL: Unsupported suboption type in DIO: %u\n",
	(unsigned)subopt_type);
    }
  }

#ifdef RPL_DEBUG_DIO_INPUT
  RPL_DEBUG_DIO_INPUT(&from, &dio);
#endif

  /* don't know what will do in this function */
  rpl_process_dio(&from, &dio);

  uip_len = 0;
/**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：2
	修改前：空
	修改后：
		energy_est=energy_est-1;
		PRINTF("enery_est:%d",energy_est);
	修改目的：
	接收dis、dio、dao、dao-ack的时候都-1
    **************************************************************************/
  energy_est=energy_est-2;
  PRINTF("enery_est:%d",energy_est);


  /*************************************************************************

	modified date:	2016/10/9

	version:	v1.0.1

	modified line:  following 2 line
**************************************************************************/
  dio_received_num++;
  PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);
}
/*---------------------------------------------------------------------------*/
/***************************************************************************

	It's DIO Send Function

****************************************************************************/
void
dio_output(rpl_instance_t *instance, uip_ipaddr_t *uc_addr)
{
  /* define some variables */
  unsigned char *buffer;
  int pos;
  rpl_dag_t *dag = instance->current_dag;
/* if this node is not leaf node */
#if !RPL_LEAF_ONLY
  // define addr
  uip_ipaddr_t addr;
#endif /* !RPL_LEAF_ONLY */

/* if this node is a leaf node */
#if RPL_LEAF_ONLY
  /* In leaf mode, we only send DIO messages as unicasts in response to
     unicast DIS messages. */
  if(uc_addr == NULL) {
    PRINTF("RPL: LEAF ONLY have multicast addr: skip dio_output\n");
    return;
  }
#endif /* RPL_LEAF_ONLY */

  /* DAG Information Object */
  pos = 0;

  /* get some sys info and write into buf */
  buffer = UIP_ICMP_PAYLOAD;
  buffer[pos++] = instance->instance_id;
  buffer[pos++] = dag->version;

/* if this node is a leaf node */
#if RPL_LEAF_ONLY
  // we should set leaf node's rank as 65535
  PRINTF("RPL: LEAF ONLY DIO rank set to INFINITE_RANK\n");
  set16(buffer, pos, INFINITE_RANK);
#else /* RPL_LEAF_ONLY */
  set16(buffer, pos, dag->rank);
#endif /* RPL_LEAF_ONLY */
  pos += 2;

  buffer[pos] = 0;
  if(dag->grounded) {
    buffer[pos] |= RPL_DIO_GROUNDED;
  }
  // do sth to make buffer standard
  buffer[pos] |= instance->mop << RPL_DIO_MOP_SHIFT;
  buffer[pos] |= dag->preference & RPL_DIO_PREFERENCE_MASK;
  pos++;

  buffer[pos++] = instance->dtsn_out;
  
  //when dio is multicast dio,we request new DAO
  if(uc_addr == NULL) {
    /* Request new DAO to refresh route. We do not do this for unicast DIO
     * in order to avoid DAO messages after a DIS-DIO update,
     * or upon unicast DIO probing. */
    RPL_LOLLIPOP_INCREMENT(instance->dtsn_out);
  }

  /* reserved 2 bytes */
  buffer[pos++] = 0; /* flags */
  buffer[pos++] = 0; /* reserved */

  memcpy(buffer + pos, &dag->dag_id, sizeof(dag->dag_id));
  pos += 16;

/* if this node is not a leaf node */
#if !RPL_LEAF_ONLY
  // we should send dio with some metric container
  if(instance->mc.type != RPL_DAG_MC_NONE) {
    //update OF of this instance
    instance->of->update_metric_container(instance);

    buffer[pos++] = RPL_OPTION_DAG_METRIC_CONTAINER;
    buffer[pos++] = 6;
    buffer[pos++] = instance->mc.type;
    buffer[pos++] = instance->mc.flags >> 1;
    buffer[pos] = (instance->mc.flags & 1) << 7;
    buffer[pos++] |= (instance->mc.aggr << 4) | instance->mc.prec;
    // check weather mc is etx or energy
    if(instance->mc.type == RPL_DAG_MC_ETX) {
      buffer[pos++] = 2;
      set16(buffer, pos, instance->mc.obj.etx);
      pos += 2;
    } else if(instance->mc.type == RPL_DAG_MC_ENERGY) {
      buffer[pos++] = 2;
      buffer[pos++] = instance->mc.obj.energy.flags;
      buffer[pos++] = instance->mc.obj.energy.energy_est;
    } else {
      PRINTF("RPL: Unable to send DIO because of unhandled DAG MC type %u\n",
	(unsigned)instance->mc.type);
      return;
    }
  }
#endif /* !RPL_LEAF_ONLY */

  //Both leaf and no-leaf node should include this option
  /* Always add a DAG configuration option. */
  buffer[pos++] = RPL_OPTION_DAG_CONF;
  buffer[pos++] = 14;
  buffer[pos++] = 0; /* No Auth, PCS = 0 */
  buffer[pos++] = instance->dio_intdoubl;
  buffer[pos++] = instance->dio_intmin;
  buffer[pos++] = instance->dio_redundancy;
  set16(buffer, pos, instance->max_rankinc);
  pos += 2;
  set16(buffer, pos, instance->min_hoprankinc);
  pos += 2;
  /* OCP is in the DAG_CONF option */
  set16(buffer, pos, instance->of->ocp);
  pos += 2;
  buffer[pos++] = 0; /* reserved */
  buffer[pos++] = instance->default_lifetime;
  set16(buffer, pos, instance->lifetime_unit);
  pos += 2;

  /* Check if we have a prefix to send also. */
  if(dag->prefix_info.length > 0) {
    buffer[pos++] = RPL_OPTION_PREFIX_INFO;
    buffer[pos++] = 30; /* always 30 bytes + 2 long */
    buffer[pos++] = dag->prefix_info.length;
    buffer[pos++] = dag->prefix_info.flags;
    set32(buffer, pos, dag->prefix_info.lifetime);
    pos += 4;
    set32(buffer, pos, dag->prefix_info.lifetime);
    pos += 4;
    memset(&buffer[pos], 0, 4);
    pos += 4;
    memcpy(&buffer[pos], &dag->prefix_info.prefix, 16);
    pos += 16;
    PRINTF("RPL: Sending prefix info in DIO for ");
    PRINT6ADDR(&dag->prefix_info.prefix);
    PRINTF("\n");
  } else {
    PRINTF("RPL: No prefix to announce (len %d)\n",
           dag->prefix_info.length);
  }

/* if this node a leaf node */
#if RPL_LEAF_ONLY
// if DEBUG = DEBUG_PRINT OR DEBUG_FULL
#if (DEBUG) & DEBUG_PRINT
  if(uc_addr == NULL) {
    PRINTF("RPL: LEAF ONLY sending unicast-DIO from multicast-DIO\n");
  }
#endif /* DEBUG_PRINT */
  PRINTF("RPL: Sending unicast-DIO with rank %u to ",
      (unsigned)dag->rank);
  PRINT6ADDR(uc_addr);
  PRINTF("\n");
  uip_icmp6_send(uc_addr, ICMP6_RPL, RPL_CODE_DIO, pos);
#else /* RPL_LEAF_ONLY */
  /* this node is no-leaf node here */
  /* Unicast requests get unicast replies! */
  if(uc_addr == NULL) {
    PRINTF("RPL: Sending a multicast-DIO with rank %u\n",
        (unsigned)instance->current_dag->rank);
    uip_create_linklocal_rplnodes_mcast(&addr);
    uip_icmp6_send(&addr, ICMP6_RPL, RPL_CODE_DIO, pos);
  } else {
    PRINTF("RPL: Sending unicast-DIO with rank %u to ",
        (unsigned)instance->current_dag->rank);
    PRINT6ADDR(uc_addr);
    PRINTF("\n");
    uip_icmp6_send(uc_addr, ICMP6_RPL, RPL_CODE_DIO, pos);
  }
#endif /* RPL_LEAF_ONLY */
  /**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：空
	修改后：
		energy_est=energy_est-4;
		PRINTF("enery_est:%d",energy_est);
		
	修改目的：
	发送dis、dio、dao、dao-ack的时候都-4
    **************************************************************************/
   energy_est=energy_est-2;
   PRINTF("enery_est:%d",energy_est);
  /*************************************************************************

	modified date:	2016/10/9

	version:	v1.0.1

	modified line:  following 2 line
  **************************************************************************/
  dio_sended_num++;
  PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);
}
/*---------------------------------------------------------------------------*/
/* receive dao */
static void
dao_input(void)
{
  // define some variables
  uip_ipaddr_t dao_sender_addr;
  rpl_dag_t *dag;
  rpl_instance_t *instance;
  unsigned char *buffer;
  uint16_t sequence;
  uint8_t instance_id;
  uint8_t lifetime;
  uint8_t prefixlen;
  uint8_t flags;
  uint8_t subopt_type;
  /*
  uint8_t pathcontrol;
  uint8_t pathsequence;
  */
  uip_ipaddr_t prefix;
  uip_ds6_route_t *rep;
  uint8_t buffer_length;
  int pos;
  int len;
  int i;
  int learned_from;
  // here have two variables
  rpl_parent_t *parent;
  uip_ds6_nbr_t *nbr;

  prefixlen = 0;
  parent = NULL;

  // get dao_sender_address
  uip_ipaddr_copy(&dao_sender_addr, &UIP_IP_BUF->srcipaddr);

  /* Destination Advertisement Object */
  PRINTF("RPL: Received a DAO from ");
  PRINT6ADDR(&dao_sender_addr);
  PRINTF("\n");

  // get icmp packet from uip
  buffer = UIP_ICMP_PAYLOAD;
  buffer_length = uip_len - uip_l3_icmp_hdr_len;
  // initialization 
  pos = 0;
  // get instance_id
  instance_id = buffer[pos++];
  
  // depend on instance_id to get instance
  instance = rpl_get_instance(instance_id);
  // if the instance is null
  if(instance == NULL) {
    // ignore this dao with doing nothing
    PRINTF("RPL: Ignoring a DAO for an unknown RPL instance(%u)\n",
           instance_id);
    return;
  }

  // get instance_lifetime
  lifetime = instance->default_lifetime;

  // get flags
  flags = buffer[pos++];
  /* reserved */
  pos++;
  sequence = buffer[pos++];

  // get current dag
  /* so,since then, we have both dag and instance */
  dag = instance->current_dag;
  /* Is the DAG ID present? */
  if(flags & RPL_DAO_D_FLAG) {
    if(memcmp(&dag->dag_id, &buffer[pos], sizeof(dag->dag_id))) {
      // ignore the different dao from other dag
      PRINTF("RPL: Ignoring a DAO for a DAG different from ours\n");
      return;
    }
    pos += 16;
  }
  

  // to figure out if this dao is mc or uc 
  learned_from = uip_is_addr_mcast(&dao_sender_addr) ?
                 RPL_ROUTE_FROM_MULTICAST_DAO : RPL_ROUTE_FROM_UNICAST_DAO;

  PRINTF("RPL: DAO from %s\n",
         learned_from == RPL_ROUTE_FROM_UNICAST_DAO? "unicast": "multicast");
  if(learned_from == RPL_ROUTE_FROM_UNICAST_DAO) {
    /* Check whether this is a DAO forwarding loop. */
    //use sender_addr to find 
    parent = rpl_find_parent(dag, &dao_sender_addr);
    /* check if this is a new DAO registration with an "illegal" rank */
    /* if we already route to this node it is likely */
    if(parent != NULL &&
       // DAG_RANK=rank-/nstance->min_hoprankinc
       DAG_RANK(parent->rank, instance) < DAG_RANK(dag->rank, instance)) {
      PRINTF("RPL: Loop detected when receiving a unicast DAO from a node with a lower rank! (%u < %u)\n",
          DAG_RANK(parent->rank, instance), DAG_RANK(dag->rank, instance));
      //0xffff 
      parent->rank = INFINITE_RANK;
      // define new parent flag
      parent->flags |= RPL_PARENT_FLAG_UPDATED;
      return;
    }

    /* If we get the DAO from our parent, we also have a loop. */
    if(parent != NULL && parent == dag->preferred_parent) {
      PRINTF("RPL: Loop detected when receiving a unicast DAO from our parent\n");
      parent->rank = INFINITE_RANK;
      parent->flags |= RPL_PARENT_FLAG_UPDATED;
      return;
    }
  }

  /* Check if there are any RPL options present. */
  for(i = pos; i < buffer_length; i += len) {
    subopt_type = buffer[i];
    if(subopt_type == RPL_OPTION_PAD1) {
      len = 1;
    } else {
      /* The option consists of a two-byte header and a payload. */
      len = 2 + buffer[i + 1];
    }

    switch(subopt_type) {
    //0x05
    case RPL_OPTION_TARGET:
      /* Handle the target option. */
      prefixlen = buffer[i + 3];
      memset(&prefix, 0, sizeof(prefix));
      memcpy(&prefix, buffer + i + 4, (prefixlen + 7) / CHAR_BIT);
      break;
    //0x06
    case RPL_OPTION_TRANSIT:
      /* The path sequence and control are ignored. */
      /*      pathcontrol = buffer[i + 3];
              pathsequence = buffer[i + 4];*/
      lifetime = buffer[i + 5];
      /* The parent address is also ignored. */
      break;
    }
  }
 /*************************************************************************
        This change print option info.


	modified date:	2016/10/10

	version:	v1.0.1

	modified line:  following 2 line

	before code:

		PRINTF("RPL: DAO lifetime: %u, prefix length: %u prefix: ",
          (unsigned)lifetime, (unsigned)prefixlen);
		PRINT6ADDR(&prefix);

	modified code:
		 PRINTF("RPL: DAO flag: %u, option type: %u, lifetime: %u, prefix length: %u prefix: ",
          (unsigned)flags,(unsigned)subopt_type,(unsigned)lifetime, (unsigned)prefixlen);
		 PRINT6ADDR(&prefix);

**************************************************************************/

 PRINTF("RPL: DAO flag: %u, option type: %u, lifetime: %u, prefix length: %u prefix: ",
          (unsigned)flags,(unsigned)subopt_type,(unsigned)lifetime, (unsigned)prefixlen);
 PRINT6ADDR(&prefix);
 PRINTF("\n");


/* if dao multicast */
#if RPL_CONF_MULTICAST
  if(uip_is_addr_mcast_global(&prefix)) {
    mcast_group = uip_mcast6_route_add(&prefix);
    if(mcast_group) {
      mcast_group->dag = dag;
      mcast_group->lifetime = RPL_LIFETIME(instance, lifetime);
    }
    goto fwd_dao;
  }
#endif

  rep = uip_ds6_route_lookup(&prefix);

  // if dao's life ==0
  if(lifetime == RPL_ZERO_LIFETIME) {
    PRINTF("RPL: No-Path DAO received\n");
    /* No-Path DAO received; invoke the route purging routine. */
    /* purge is like to clean */
    if(rep != NULL &&
       rep->state.nopath_received == 0 &&
       rep->length == prefixlen &&
       uip_ds6_route_nexthop(rep) != NULL &&
       uip_ipaddr_cmp(uip_ds6_route_nexthop(rep), &dao_sender_addr)) {
      /* expiration is like to no-use */
      PRINTF("RPL: Setting expiration timer for prefix ");
      PRINT6ADDR(&prefix);
      PRINTF("\n");
      rep->state.nopath_received = 1;
      rep->state.lifetime = DAO_EXPIRATION_TIMEOUT;

      /* We forward the incoming no-path DAO to our parent, if we have
         one. */
      if(dag->preferred_parent != NULL &&
         rpl_get_parent_ipaddr(dag->preferred_parent) != NULL) {
        PRINTF("RPL: Forwarding no-path DAO to parent ");
        PRINT6ADDR(rpl_get_parent_ipaddr(dag->preferred_parent));
        PRINTF("\n");
        uip_icmp6_send(rpl_get_parent_ipaddr(dag->preferred_parent),
                       ICMP6_RPL, RPL_CODE_DAO, buffer_length);
      }
      // if define the ack flag then output ack
      if(flags & RPL_DAO_K_FLAG) {
        dao_ack_output(instance, &dao_sender_addr, sequence);
      }
    }
    return;
  }

  PRINTF("RPL: adding DAO route\n");
  
  /* it just add neighbor cache */
  if((nbr = uip_ds6_nbr_lookup(&dao_sender_addr)) == NULL) {
     // if we have space to store the neighbor info
     if((nbr = uip_ds6_nbr_add(&dao_sender_addr,
                              (uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER),
                              0, NBR_REACHABLE)) != NULL) {
      /* set reachable timer */
      stimer_set(&nbr->reachable, UIP_ND6_REACHABLE_TIME / 1000);
      PRINTF("RPL: Neighbor added to neighbor cache ");
      PRINT6ADDR(&dao_sender_addr);
      PRINTF(", ");
      PRINTLLADDR((uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER));
      PRINTF("\n");
    } else {
      PRINTF("RPL: Out of Memory, dropping DAO from ");
      PRINT6ADDR(&dao_sender_addr);
      PRINTF(", ");
      PRINTLLADDR((uip_lladdr_t *)packetbuf_addr(PACKETBUF_ADDR_SENDER));
      PRINTF("\n");
      return;
    }
  } else {
    // if the sender had already in our cache, then
    PRINTF("RPL: Neighbor already in neighbor cache\n");
  }

  rpl_lock_parent(parent);

  // it's uip_root type
  rep = rpl_add_route(dag, &prefix, prefixlen, &dao_sender_addr);
  if(rep == NULL) {
    RPL_STAT(rpl_stats.mem_overflows++);
    PRINTF("RPL: Could not add a route after receiving a DAO\n");
    return;
  }

  rep->state.lifetime = RPL_LIFETIME(instance, lifetime);
  rep->state.learned_from = learned_from;
  rep->state.nopath_received = 0;

// if dao is multicaset
#if RPL_CONF_MULTICAST
fwd_dao:
#endif
  // if dao is unicast
  if(learned_from == RPL_ROUTE_FROM_UNICAST_DAO) {
    if(dag->preferred_parent != NULL &&
       rpl_get_parent_ipaddr(dag->preferred_parent) != NULL) {
      PRINTF("RPL: Forwarding DAO to parent ");
      PRINT6ADDR(rpl_get_parent_ipaddr(dag->preferred_parent));
      PRINTF("\n");
      uip_icmp6_send(rpl_get_parent_ipaddr(dag->preferred_parent),
                     ICMP6_RPL, RPL_CODE_DAO, buffer_length);
     /**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：2
	修改前：空
	修改后：
		energy_est=energy_est-1;
		PRINTF("enery_est:%d",energy_est);
	修改目的：
	接收dis、dio、dao、dao-ack的时候都-1
    **************************************************************************/
     energy_est=energy_est-2;

     PRINTF("enery_est:%d",energy_est); 

     /*********************************************************

	modified date:	2016/11/19

	version:	v2.0.1

	modified line:  following 2 line

        result: patch the forwarding dao num


      ***********************************************************/
       dao_sended_num++;
       PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d        \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);
    }
    if(flags & RPL_DAO_K_FLAG) {
      dao_ack_output(instance, &dao_sender_addr, sequence);
    }
  }
  uip_len = 0;
  /**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：2
	修改前：空
	修改后：
		energy_est=energy_est-1;
		PRINTF("enery_est:%d",energy_est);
	修改目的：
	接收dis、dio、dao、dao-ack的时候都-1
    **************************************************************************/
  energy_est=energy_est-2;

  PRINTF("enery_est:%d",energy_est);
  /*************************************************************************

	modified date:	2016/10/9

	version:	v1.0.1

	modified line:  following 2 line
**************************************************************************/
  dao_received_num++;
  PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);
}
/*---------------------------------------------------------------------------*/
/* send dao */
void
dao_output(rpl_parent_t *parent, uint8_t lifetime)
{
  /* Destination Advertisement Object */
  uip_ipaddr_t prefix;

  if(get_global_addr(&prefix) == 0) {
    PRINTF("RPL: No global address set for this node - suppressing DAO\n");
    return;
  }

  /* Sending a DAO with own prefix as target */
  dao_output_target(parent, &prefix, lifetime);

  /*************************************************************************

	modified date:		2016/10/10

	version:			v1.1.2

	modified line:		following 2 line

	before:				dao_sended_num++;
						PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);

	now:				(deleted)
**************************************************************************/
  
}
/*---------------------------------------------------------------------------*/
/* send dao*/
void
dao_output_target(rpl_parent_t *parent, uip_ipaddr_t *prefix, uint8_t lifetime)
{
  // define some variables
  rpl_dag_t *dag;
  rpl_instance_t *instance;
  unsigned char *buffer;
  uint8_t prefixlen;
  int pos;

  /* Destination Advertisement Object */

  /* If we are in feather mode, we should not send any DAOs */
  if(rpl_get_mode() == RPL_MODE_FEATHER) {
    return;
  }

  if(parent == NULL) {
    PRINTF("RPL dao_output_target error parent NULL\n");
    return;
  }

  dag = parent->dag;
  if(dag == NULL) {
    PRINTF("RPL dao_output_target error dag NULL\n");
    return;
  }

  instance = dag->instance;

  if(instance == NULL) {
    PRINTF("RPL dao_output_target error instance NULL\n");
    return;
  }
  if(prefix == NULL) {
    PRINTF("RPL dao_output_target error prefix NULL\n");
    return;
  }
#ifdef RPL_DEBUG_DAO_OUTPUT
  RPL_DEBUG_DAO_OUTPUT(parent);
#endif

  // get icmp news from uip
  buffer = UIP_ICMP_PAYLOAD;
  
  // RPL use lollipop algorithm
  RPL_LOLLIPOP_INCREMENT(dao_sequence);
  pos = 0;

  buffer[pos++] = instance->instance_id;
  buffer[pos] = 0;
/* if we more than one instance set RPL_DAO_SPECIFY_DAG=1 otherwise set it to 0 */
#if RPL_DAO_SPECIFY_DAG
  buffer[pos] |= RPL_DAO_D_FLAG;
#endif /* RPL_DAO_SPECIFY_DAG */

/* if we define need ack, in this file, I set it to 1 */
#if RPL_CONF_DAO_ACK
  buffer[pos] |= RPL_DAO_K_FLAG;
#endif /* RPL_CONF_DAO_ACK */
  ++pos;
  buffer[pos++] = 0; /* reserved */
  buffer[pos++] = dao_sequence;


#if RPL_DAO_SPECIFY_DAG
  memcpy(buffer + pos, &dag->dag_id, sizeof(dag->dag_id));
  pos+=sizeof(dag->dag_id);
#endif /* RPL_DAO_SPECIFY_DAG */

  /* create target subopt */
  prefixlen = sizeof(*prefix) * CHAR_BIT;
  buffer[pos++] = RPL_OPTION_TARGET;
  buffer[pos++] = 2 + ((prefixlen + 7) / CHAR_BIT);
  buffer[pos++] = 0; /* reserved */
  buffer[pos++] = prefixlen;
  memcpy(buffer + pos, prefix, (prefixlen + 7) / CHAR_BIT);
  pos += ((prefixlen + 7) / CHAR_BIT);

  /* Create a transit information sub-option. */
  buffer[pos++] = RPL_OPTION_TRANSIT;
  buffer[pos++] = 4;
  buffer[pos++] = 0; /* flags - ignored */
  buffer[pos++] = 0; /* path control - ignored */
  buffer[pos++] = 0; /* path seq - ignored */
  buffer[pos++] = lifetime;

  PRINTF("RPL: Sending DAO with prefix ");
  PRINT6ADDR(prefix);
  PRINTF(" to ");
  PRINT6ADDR(rpl_get_parent_ipaddr(parent));
  PRINTF("\n");

  if(rpl_get_parent_ipaddr(parent) != NULL) {
    uip_icmp6_send(rpl_get_parent_ipaddr(parent), ICMP6_RPL, RPL_CODE_DAO, pos);
  }
  /**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：空
	修改后：
		energy_est=energy_est-4;
		PRINTF("enery_est:%d",energy_est);
		
	修改目的：
	发送dis、dio、dao、dao-ack的时候都-4
    **************************************************************************/
   energy_est=energy_est-2;
   PRINTF("enery_est:%d",energy_est);
  /*************************************************************************

	modified date:	2016/10/9

	version:	v1.0.1

	modified line:  following 2 line
**************************************************************************/
  dao_sended_num++;
  PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);
}
/*---------------------------------------------------------------------------*/
/* receive dao-ack */
static void
dao_ack_input(void)
{
/* if we don't set DEBUG will not run the following code */
#if DEBUG
  unsigned char *buffer;
  uint8_t buffer_length;
  uint8_t instance_id;
  uint8_t sequence;
  uint8_t status;

  buffer = UIP_ICMP_PAYLOAD;
  buffer_length = uip_len - uip_l3_icmp_hdr_len;

  instance_id = buffer[0];
  sequence = buffer[2];
  status = buffer[3];

  PRINTF("RPL: Received a DAO ACK with sequence number %d and status %d from ",
    sequence, status);
  PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
  PRINTF("\n");
/**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：2
	修改前：空
	修改后：
		energy_est=energy_est-1;
		PRINTF("enery_est:%d",energy_est);
	修改目的：
	接收dis、dio、dao、dao-ack的时候都-1
    **************************************************************************/
  energy_est=energy_est-2;
  PRINTF("enery_est:%d",energy_est);
/*************************************************************************

	modified date:	2016/10/9

	version:	v1.0.1

	modified line:  following 2 line
**************************************************************************/
  dao_ack_received_num++;
  PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);

#endif /* DEBUG */
  uip_len = 0;
}
/*---------------------------------------------------------------------------*/
/* send dao-ack */

void
dao_ack_output(rpl_instance_t *instance, uip_ipaddr_t *dest, uint8_t sequence)
{
  unsigned char *buffer;

  PRINTF("RPL: Sending a DAO ACK with sequence number %d to ", sequence);
  PRINT6ADDR(dest);
  PRINTF("\n");

  buffer = UIP_ICMP_PAYLOAD;

  buffer[0] = instance->instance_id;
  buffer[1] = 0;
  buffer[2] = sequence;
  buffer[3] = 0;

  uip_icmp6_send(dest, ICMP6_RPL, RPL_CODE_DAO_ACK, 4);
  /**************************************************************************
	Date:20170601
	Version:1.0
	修改行数：1
	修改前：空
	修改后：
		energy_est=energy_est-4;
		PRINTF("enery_est:%d",energy_est);
		
	修改目的：
	发送dis、dio、dao、dao-ack的时候都-4
    **************************************************************************/
   energy_est=energy_est-2;
   PRINTF("enery_est:%d",energy_est);
 /*************************************************************************

	modified date:	2016/10/9

	version:	v1.0.1

	modified line:  following 2 line
**************************************************************************/
  dao_ack_sended_num++;
  PRINTF("dis_s:%d dis_r:%d dio_s:%d dio_r:%d dao_s:%d dao_r:%d dao_a_s:%d dao_a_r:%d \n",dis_sended_num,dis_received_num,dio_sended_num,dio_received_num,dao_sended_num,dao_received_num,dao_ack_sended_num,dao_ack_received_num);

}
/*---------------------------------------------------------------------------*/
void
rpl_icmp6_register_handlers()
{
  uip_icmp6_register_input_handler(&dis_handler);
  uip_icmp6_register_input_handler(&dio_handler);
  uip_icmp6_register_input_handler(&dao_handler);
  uip_icmp6_register_input_handler(&dao_ack_handler);
}
/*---------------------------------------------------------------------------*/

/** @}*/
