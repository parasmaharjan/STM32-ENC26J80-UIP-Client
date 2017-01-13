/**
  ******************************************************************************
  * @file    SysTick/main.c 
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    19-September-2011
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************  
  */ 

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"
#include "uip.h"
#include "uip_arp.h"
#include "tapdev.h"
#include "resolv.h"
#include "uiplib.h"

#include "timer.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#define BUF ((struct uip_eth_hdr *)&uip_buf[0])

#define HOST_OPENWEATHER "api.openweathermap.org"
#define HOST_BINGMAP		 "dev.virtualearth.net"
#define HOST_PORT        80

uint8_t host_openweather_found =0;
uint8_t host_bingmap_found = 0;
extern uint8_t server_openweather_connected, server_bingmap_connected;

struct __FILE {int handle;};
FILE __stdout;

int fputc(int c, FILE *f) {
	return ITM_SendChar(c);
}

void gpio_init()
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	
	GPIO_SetBits(GPIOD, GPIO_Pin_12);
}

void spi_init()
{
	GPIO_InitTypeDef GPIO_InitStruct;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	
	GPIO_StructInit(&GPIO_InitStruct);
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_DOWN;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
	GPIO_Init(GPIOB, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14;
	GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	GPIO_SetBits(GPIOA, GPIO_Pin_4);
	GPIO_ResetBits(GPIOB, GPIO_Pin_13);
	GPIO_ResetBits(GPIOB, GPIO_Pin_15);
	
	//SPI2_ReadWrite(0xaa);
}

RTC_TimeTypeDef  RTC_TimeStructure;
void rtc_init()
{
	RTC_InitTypeDef RTC_InitStructure;
	
  /* Enable the PWR clock */
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

  /* Allow access to RTC */
  PWR_BackupAccessCmd(ENABLE);
  
  /* Reset RTC Domain */
  RCC_BackupResetCmd(ENABLE);
  RCC_BackupResetCmd(DISABLE);

	RCC_LSICmd(ENABLE);
	while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY)==RESET)
	{
	}
  /* Select the RTC Clock Source */
  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
  
  /* Configure the RTC data register and RTC prescaler */
  /* ck_spre(1Hz) = RTCCLK(LSI) /(AsynchPrediv + 1)*(SynchPrediv + 1)*/
  RTC_InitStructure.RTC_AsynchPrediv = 0x1F;
  RTC_InitStructure.RTC_SynchPrediv  = 0x3FF;
  RTC_InitStructure.RTC_HourFormat   = RTC_HourFormat_24;
  RTC_Init(&RTC_InitStructure);
  
  /* Set the time to 00h 00mn 00s AM */
  RTC_TimeStructure.RTC_H12     = RTC_H12_AM;
  RTC_TimeStructure.RTC_Hours   = 0;
  RTC_TimeStructure.RTC_Minutes = 0;
  RTC_TimeStructure.RTC_Seconds = 0;  
  RTC_SetTime(RTC_Format_BCD, &RTC_TimeStructure);
	
	/* Enable the RTC Clock */
	RCC_RTCCLKCmd(ENABLE);

	/* Wait for RTC APB registers synchronisation */
	RTC_WaitForSynchro();
}

void uip_log(char *msg)
{
	printf("%s", msg);
}

static struct timer periodic_timer, arp_timer;
static int i;

void uip_polling(void)
{
	uip_len = tapdev_read(uip_buf);
	if (uip_len > 0) {
		if (BUF->type == htons(UIP_ETHTYPE_IP)) {
			uip_arp_ipin();
			uip_input();
			/* If the above function invocation resulted in data that
				 should be sent out on the network, the global variable
				 uip_len is set to a value > 0. */
			if (uip_len > 0) {
					uip_arp_out();
					tapdev_send(uip_buf, uip_len);
			}
		} 
		else if (BUF->type == htons(UIP_ETHTYPE_ARP)) {
			uip_arp_arpin();
			/* If the above function invocation resulted in data that
				 should be sent out on the network, the global variable
				 uip_len is set to a value > 0. */
			if (uip_len > 0) {
					tapdev_send(uip_buf, uip_len);
			}
		}
	} 
	else if (timer_expired(&periodic_timer)) {
		timer_reset(&periodic_timer);
		for (i = 0; i < UIP_CONNS; i++) {
			uip_periodic(i);
			/* If the above function invocation resulted in data that
				 should be sent out on the network, the global variable
				 uip_len is set to a value > 0. */
			if (uip_len > 0) {
				uip_arp_out();
				tapdev_send(uip_buf, uip_len);
			}
		}

#if UIP_UDP
		for (i = 0; i < UIP_UDP_CONNS; i++) {
			uip_udp_periodic(i);
			/* If the above function invocation resulted in data that
				 should be sent out on the network, the global variable
				 uip_len is set to a value > 0. */
			if (uip_len > 0) {
					uip_arp_out();
					tapdev_send(uip_buf, uip_len);
			}
		}
#endif /* UIP_UDP */
		/* Call the ARP timer function every 10 seconds. */
		if (timer_expired(&arp_timer)) {
			timer_reset(&arp_timer);
			uip_arp_timer();
		}
	}
}

void resolv_found(char *name, u16_t *ipaddr)
{
	if (ipaddr == NULL) {
			printf("Host '%s' not found.\r\n", name);
	} else {
			printf("Found name '%s' = %d.%d.%d.%d\r\n", name,
						 htons(ipaddr[0]) >> 8,
						 htons(ipaddr[0]) & 0xff,
						 htons(ipaddr[1]) >> 8,
						 htons(ipaddr[1]) & 0xff);
	}
	if(strstr(name, HOST_OPENWEATHER))
	{
		host_openweather_found = 1;
	}
	else if(strstr(name, HOST_BINGMAP))
	{
		host_bingmap_found = 1;
	}
}

__IO uint32_t g_RunTime = 0;

void blinky(void * pv)
{
	RTC_TimeTypeDef RTC_TimeStruct; 
	while(1)
	{
		vTaskDelay(1000);
		RTC_GetTime(RTC_Format_BCD, &RTC_TimeStruct);
		printf("Time: %x:%x:%x\r\n", RTC_TimeStruct.RTC_Hours,RTC_TimeStruct.RTC_Minutes, RTC_TimeStruct.RTC_Seconds);
		GPIO_ToggleBits(GPIOD, GPIO_Pin_12);
	}
}

void uip_tick_task(void * pv)
{
	while(1)
	{
		vTaskDelay(10);
		g_RunTime++;	// 10ms
	}
}

void uip_polling_task(void * pv)
{
	while(1)
	{
		uip_polling();
		vTaskDelay(5);
	}
}

void data_polling_task(void * pv)
{
	uip_ipaddr_t *ipaddr = NULL;
	static uip_ipaddr_t addr;
	struct uip_conn * server_conn;
	xTimeOutType x_timeout;
	portTickType openTimeout;
	
	while(1)
	{
		if(host_bingmap_found == 1)
		{
			if (uiplib_ipaddrconv(HOST_BINGMAP, (unsigned char *)&addr) == 0) 
			{
				ipaddr = (uip_ipaddr_t *)resolv_lookup(HOST_BINGMAP);
				if (ipaddr == NULL) 
				{
					printf("IP of %s not found...\r\n", HOST_BINGMAP);
					host_bingmap_found = 0;
					vTaskDelay(1000);
					continue;
				}
			}
			else
			{
				ipaddr = &addr;
			}
			printf("Waiting for connection to BingMap Lat Lon...\r\n");
			server_conn = uip_connect((uip_ipaddr_t *)ipaddr,HTONS(HOST_PORT));
			if(server_conn)
			{
				vTaskSetTimeOutState(&x_timeout);
				openTimeout = 60000;
				while(xTaskCheckForTimeOut(&x_timeout, &openTimeout) == pdFALSE)
				{
					vTaskDelay(100);
					if(server_bingmap_connected==10)
					{
						host_bingmap_found = 2;
						break;
					}
				}
			}
			uip_close();
		}
		else if(host_bingmap_found == 2)
		{
			if (uiplib_ipaddrconv(HOST_BINGMAP, (unsigned char *)&addr) == 0) 
			{
				ipaddr = (uip_ipaddr_t *)resolv_lookup(HOST_BINGMAP);
				if (ipaddr == NULL) 
				{
					printf("IP of %s not found...\r\n", HOST_BINGMAP);
					host_bingmap_found = 0;
					vTaskDelay(1000);
					continue;
				}
			}
			else
			{
				ipaddr = &addr;
			}
			printf("Waiting for connection to BingMap Elevation...\r\n");
			server_conn = uip_connect((uip_ipaddr_t *)ipaddr,HTONS(HOST_PORT));
			if(server_conn)
			{
				vTaskSetTimeOutState(&x_timeout);
				openTimeout = 60000;
				while(xTaskCheckForTimeOut(&x_timeout, &openTimeout) == pdFALSE)
				{
					vTaskDelay(100);
					if(server_bingmap_connected==20)
					{
						host_bingmap_found = 3;
						break;
					}
				}
			}
			uip_close();
		}
		else if(host_openweather_found == 1)
		{
			if (uiplib_ipaddrconv(HOST_OPENWEATHER, (unsigned char *)&addr) == 0) 
			{
				ipaddr = (uip_ipaddr_t *)resolv_lookup(HOST_OPENWEATHER);
				if (ipaddr == NULL) 
				{
					printf("IP of %s not found...\r\n", HOST_OPENWEATHER);
					host_openweather_found = 0;
					vTaskDelay(1000);
					continue;
				}
			}
			else
			{
				ipaddr = &addr;
			}
			printf("Waiting for connection to OpenWeatherMap...\r\n");
			server_conn = uip_connect((uip_ipaddr_t *)ipaddr,HTONS(HOST_PORT));
			if(server_conn)
			{
				vTaskSetTimeOutState(&x_timeout);
				openTimeout = 60000;
				while(xTaskCheckForTimeOut(&x_timeout, &openTimeout) == pdFALSE)
				{
					vTaskDelay(100);
					if(server_openweather_connected == 2)
					{
						RTC_SetTime(RTC_Format_BCD, &RTC_TimeStructure);
						break;
					}
				}
			}
			server_openweather_connected = 0;
			uip_close();
			vTaskDelay(60000);
		}
		vTaskDelay(1000);
	}
	
	/*
	while(1)
	{
		//uip_ipaddr(ipaddr, 128, 199, 109, 89); //128.199.109.89
		if(host_openweather_found)
		{
			if (uiplib_ipaddrconv(HOST_OPENWEATHER, (unsigned char *)&addr) == 0) 
			{
				ipaddr = (uip_ipaddr_t *)resolv_lookup(HOST_OPENWEATHER);

				if (ipaddr == NULL) 
				{
					printf("%s not responding...\r\n", HOST_OPENWEATHER);
				}
				else
				{
					printf("Connecting to OpenWeather....");
					server_conn = uip_connect((uip_ipaddr_t *)ipaddr,HTONS(HOST_PORT));
					if(server_conn)
					{
						vTaskSetTimeOutState(&x_timeout);
						openTimeout = 60000;
						while(xTaskCheckForTimeOut(&x_timeout, &openTimeout) == pdFALSE)
						{
							vTaskDelay(5);
							if(server_openweather_connected == 2)
							{
								RTC_SetTime(RTC_Format_BCD, &RTC_TimeStructure);
								break;
							}
						}
						server_openweather_connected = 0;
						uip_close();
						vTaskDelay(10000);
					}
				}
			}
			vTaskDelay(1000);
		}
//		else if(host_bingmap_found)
//		{
//			if (uiplib_ipaddrconv(HOST_BINGMAP, (unsigned char *)&addr) == 0) 
//			{
//				ipaddr = (uip_ipaddr_t *)resolv_lookup(HOST_BINGMAP);

//				if (ipaddr == NULL) 
//				{
//					printf("%s not responding...\r\n", HOST_BINGMAP);
//				}
//				else
//				{
//					server_conn = uip_connect((uip_ipaddr_t *)ipaddr,HTONS(HOST_PORT));
//					if(server_conn)
//					{
//						vTaskSetTimeOutState(&x_timeout);
//						openTimeout = 60000;
//						while(xTaskCheckForTimeOut(&x_timeout, &openTimeout) == pdFALSE)
//						{
//							vTaskDelay(5);
//							if(server_bingmap_connected == 2)
//							{
//								host_bingmap_found = 0;
//								break;
//							}
//						}
//						server_bingmap_connected = 0;
//						uip_close();
//					}
//				}
//			}
//			vTaskDelay(1000);
//		}
		else
		{
			printf("Waiting for internet connection...\r\n");
			vTaskDelay(1000);
		}
	}
	*/
}

int main(void)
{
	uip_ipaddr_t ipaddr;
	
	printf("New Program Started!\r\n");
	
	timer_set(&periodic_timer, CLOCK_SECOND / 2);
	timer_set(&arp_timer, CLOCK_SECOND * 10);
	
	gpio_init();
	spi_init();
	rtc_init();
	tapdev_init();
	
	uip_arp_init();
	__enable_irq();
	uip_init();
	uip_ipaddr(ipaddr, 192, 168, 100, 200);
	uip_sethostaddr(ipaddr);
	uip_ipaddr(ipaddr, 192, 168, 100, 1);
	uip_setdraddr(ipaddr);
	uip_ipaddr(ipaddr, 255, 255, 255, 0);
	uip_setnetmask(ipaddr);
	
	resolv_init();
	uip_ipaddr(ipaddr, 192, 168, 100, 1); // Google DNS: 208, 67, 222, 222
	resolv_conf(ipaddr);
	resolv_query(HOST_BINGMAP);
	resolv_query(HOST_OPENWEATHER);
	
	xTaskCreate(blinky,"BLINKY",200,NULL,1,NULL);
	xTaskCreate(uip_tick_task,"TOGGLE",200,NULL,1,NULL);
	xTaskCreate(uip_polling_task,"POLLING",200,NULL,1,NULL);
	xTaskCreate(data_polling_task,"CONNECT",200,NULL,1,NULL);
 	vTaskStartScheduler();
	while(1);
}
