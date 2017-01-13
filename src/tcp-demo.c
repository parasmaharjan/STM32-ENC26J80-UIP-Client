#include "tcp-demo.h"
#include "uip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Pholchoki: lat=27.5707397&lon=85.4069987
char get_request[400];
const char get_request_openweathermap_1[] = "GET /data/2.5/station/find?lat=";
const char get_request_openweathermap_2[] = "&lon=";
const char get_request_openweathermap_3[] = "&cnt=1&APPID=d33ccc4fe677629315a697ffa4ae097e HTTP/1.1\r\nHost: api.openweather.org\r\n\r\n";

const char get_request_binglatlonmap[] = "GET /REST/v1/Locations/Kathmandu?key=AgMVAaLqK8vvJe6OTRRu57wu0x2zBX1bUaqSizo0QhE32fqEK5fN8Ek4wWmO4QR4 HTTP/1.1\r\nHost: dev.virtualearth.net\r\n\r\n";
const char get_request_bingelemap[] = "GET /REST/v1/Elevation/List?pts=27.693300247192383,85.322502136230469&key=AgMVAaLqK8vvJe6OTRRu57wu0x2zBX1bUaqSizo0QhE32fqEK5fN8Ek4wWmO4QR4 HTTP/1.1\r\nHost: dev.virtualearth.net\r\n\r\n";
const char get_request_bingelevationmap[] = "";
const char http_ok_str[] = "HTTP/1.1 200 OK\r\n";
const char content_length_str[] = "Content-Length: ";
const char http_header_end_str[] = "\r\n\r\n";
const char temp_str[] = "\"temp\":";
const char pressure_str[] = "\"pressure\":";
const char humidity_str[] = "\"humidity\":";
const char wind_speed_str[] = "\"speed\":";
const char wind_deg_str[] = "\"deg\":";
const char coordinates_str[] = "\"coordinates\":[";
const char elevation_str[] = "\"elevations\":[";
long content_length;
char * pointer;
//uint8_t json_data[600];
uint8_t http_state = 0;

double longitude = 85.322, latitude = 27.693, elevation, temperature, pressure, humidity, wind_speed, wind_deg;
uint8_t server_openweather_connected = 0;
uint8_t server_bingmap_connected = 0;

static uip_ipaddr_t *ipaddr = NULL;

void tcp_appcall()
{
	if(uip_conn->rport == HTONS(80))
	{
		ipaddr = (uip_ipaddr_t *)resolv_lookup("dev.virtualearth.net");
		if(uip_ipaddr_cmp(ipaddr,uip_conn->ripaddr))
		{
			if(uip_connected())
			{
				printf("Connected to BingMap...\r\n");
				if(server_bingmap_connected == 0)
				{
					server_bingmap_connected = 1;
					uip_send(get_request_binglatlonmap, strlen(get_request_binglatlonmap));
				}
				if(server_bingmap_connected == 10)
				{
					server_bingmap_connected = 11;
					uip_send(get_request_bingelemap, strlen(get_request_bingelemap));
				}
			}
			if(uip_newdata())
			{
				if(server_bingmap_connected == 1)
				{
					switch(http_state)
					{
						case 0:
						{
							pointer = strstr(uip_appdata, http_ok_str);
							if(pointer)
							{
								http_state = 1;
							}
							else
							{
								http_state = 0;
								break;
							}
						}
						case 1:
						{
							pointer = strstr(uip_appdata, coordinates_str);
							pointer = pointer+sizeof(coordinates_str)-1;
							latitude = strtod(pointer, &pointer);
							pointer = pointer + 1; // For comma ','
							longitude = strtod(pointer, NULL);
							printf("Latitude: %.3f\tLongitude: %.3f\r\n", latitude, longitude);
							server_bingmap_connected = 10;
							http_state = 0;
						}
						default:
							break;
					}
				}
				if(server_bingmap_connected == 11)
				{
					switch(http_state)
					{
						case 0:
						{
							pointer = strstr(uip_appdata, http_ok_str);
							if(pointer)
							{
								http_state = 1;
							}
							else
							{
								http_state = 0;
								break;
							}
						}
						case 1:
						{
							pointer = strstr(uip_appdata, elevation_str);
							pointer = pointer+sizeof(elevation_str)-1;
							elevation = strtod(pointer, &pointer);
							printf("Elevation: %.3f\r\n", elevation);
							server_bingmap_connected = 20;
							http_state = 0;
						}
						default:
							break;
					}
				}
			}
		}
		ipaddr = (uip_ipaddr_t *)resolv_lookup("api.openweathermap.org");
		if(uip_ipaddr_cmp(ipaddr,uip_conn->ripaddr))
		{
			if(uip_connected())
			{
				printf("Connected to OpenWeatherMap...\r\n");
				if(server_openweather_connected == 0)
				{
					server_openweather_connected = 1;
					sprintf(get_request,"%s%f%s%f%s", get_request_openweathermap_1,latitude,get_request_openweathermap_2,longitude,get_request_openweathermap_3);
					uip_send(get_request, strlen(get_request));
				}
			}
			if(uip_newdata())
			{
				if(server_openweather_connected == 1)
				{
					switch(http_state)
					{
						case 0:
						{
							pointer = strstr(uip_appdata, http_ok_str);
							if(pointer)
							{
								http_state = 1;
							}
							else
							{
								http_state = 0;
								break;
							}
						}
						case 1:	
						{
							/* Temperature */
							pointer = strstr(pointer, temp_str);
							pointer = pointer+sizeof(temp_str)-1;
							temperature = strtod(pointer, &pointer);
							
							/* Pressure */
							pointer = strstr(pointer, pressure_str);
							pointer = pointer+sizeof(pressure_str)-1;
							pressure = strtod(pointer, &pointer);
							
							/* Humidity */
							pointer = strstr(pointer, humidity_str);
							pointer = pointer+sizeof(humidity_str)-1;
							humidity = strtod(pointer, &pointer);
							
							/* Wind Speed */
							pointer = strstr(pointer, wind_speed_str);
							pointer = pointer+sizeof(wind_speed_str)-1;
							wind_speed = strtod(pointer, &pointer);
							
							/* Wind Deg */
							pointer = strstr(pointer, wind_deg_str);
							pointer = pointer+sizeof(wind_deg_str)-1;
							wind_deg = strtod(pointer, &pointer);
							
							printf("Temp:%.1f\r\nPressure:%.1f\r\nHumidity:%.1f\r\nWind Speed:%.1f\r\nWind Deg:%.1f\r\n\r\n", (float)temperature, (float)pressure, (float)humidity, (float)wind_speed, (float)wind_deg);
							server_openweather_connected = 2;
							http_state = 0;
						}
						default:
							break;
					}
				}
			}
		}
	}
}

void tcp_appcall_()
{
	if(uip_conn->rport ==  HTONS(80))
	{
//		ipaddr = (uip_ipaddr_t *)resolv_lookup("api.openweathermap.org");
//		if(uip_ipaddr_cmp(ipaddr,uip_conn->ripaddr))
//		{
//			printf("OPENWEATHER\r\n");
//		}
//		ipaddr = (uip_ipaddr_t *)resolv_lookup("dev.virtualearth.net");
//		if(uip_ipaddr_cmp(ipaddr,uip_conn->ripaddr))
//		{
//			printf("BING\r\n");
//		}
//		return;
		
		if(uip_connected()||uip_rexmit())
		{
			printf("Connected to server!\r\n");
			if(server_openweather_connected == 0)
			{
				server_openweather_connected = 1;
				sprintf(get_request,"%s%f%s%f%s", get_request_openweathermap_1,latitude,get_request_openweathermap_2,longitude,get_request_openweathermap_3);
				printf("%s\r\n", get_request);
				uip_send(get_request, strlen(get_request));
			}
		}
		if(uip_newdata())
		{
			//printf("%s",uip_appdata);
			switch(http_state)
			{
				case 0:
				{
					pointer = strstr(uip_appdata, http_ok_str);
					if(pointer)
					{
						http_state = 1;
					}
					else
					{
						break;
					}
				}
				case 1:
				{
					pointer = strstr(uip_appdata, content_length_str);
					if(pointer)
					{
						pointer = pointer+sizeof(content_length_str)-1;
						content_length = strtol(pointer,&pointer,10);
						http_state = 2;
					}
					else
					{
						break;
					}
				}
				case 2:
				{
					pointer = strstr(pointer, http_header_end_str);
					if(pointer)
					{
						pointer = pointer+sizeof(http_header_end_str)-1;
						
						//// Actual JSON data with length of content_length
						//memcpy(json_data, pointer, content_length);
						//printf("%s", json_data);
						
						/* Temperature */
						pointer = strstr(pointer, temp_str);
						pointer = pointer+sizeof(temp_str)-1;
						temperature = strtod(pointer, &pointer);
						
						/* Pressure */
						pointer = strstr(pointer, pressure_str);
						pointer = pointer+sizeof(pressure_str)-1;
						pressure = strtod(pointer, &pointer);
						
						/* Humidity */
						pointer = strstr(pointer, humidity_str);
						pointer = pointer+sizeof(humidity_str)-1;
						humidity = strtod(pointer, &pointer);
						
						/* Wind Speed */
						pointer = strstr(pointer, wind_speed_str);
						pointer = pointer+sizeof(wind_speed_str)-1;
						wind_speed = strtod(pointer, &pointer);
						
						/* Wind Deg */
						pointer = strstr(pointer, wind_deg_str);
						pointer = pointer+sizeof(wind_deg_str)-1;
						wind_deg = strtod(pointer, &pointer);
						
						printf("Temp:%.1f\r\nPressure:%.1f\r\nHumidity:%.1f\r\nWind Speed:%.1f\r\nWind Deg:%.1f\r\n\r\n", (float)temperature, (float)pressure, (float)humidity, (float)wind_speed, (float)wind_deg);
						server_openweather_connected = 2;
						http_state = 0;
					}
					else
					{
						break;
					}
				}
				default:
				{
					break;
				}
			}
		}
	}
}
