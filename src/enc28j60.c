/***************************************************************************
** COPYRIGHT: Copyright (c) 2008-2011
** file         : D:\Programs\uip-1.0\stm32\src\tapdev.c
** summary      :
**
** version      : v0.1
** author       : gang.cheng
** Date         : 2013--3--13
** * Change Logs:
** Date     Version     Author      Notes
***************************************************************************/
#include "enc28j60.h"
#include "uip.h"
#include "stm32f4xx.h"
/*
    静态IP地址、网关地址、子网掩码、硬件mac地址
*/
#define UIP_IPADDR0     192
#define UIP_IPADDR1     168
#define UIP_IPADDR2     100
#define UIP_IPADDR3     91

#define UIP_DRIPADDR0   192
#define UIP_DRIPADDR1   168
#define UIP_DRIPADDR2   100
#define UIP_DRIPADDR3   1

#define UIP_NETMASK0    255
#define UIP_NETMASK1    255
#define UIP_NETMASK2    255
#define UIP_NETMASK3    0

#define MAX_ADDR_LEN    6

static uint8_t  Enc28j60Bank = 0xff;
static uint16_t NextPacketPtr;

#define     ENC28J60_CS         GPIO_Pin_4
#define     ENC28J60_CSL()      GPIO_ResetBits(GPIOA, ENC28J60_CS)
#define     ENC28J60_CSH()      GPIO_SetBits(GPIOA, ENC28J60_CS)

unsigned char msk[] = {0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1};
#define PCK() (bits[0] << 7 | bits[1] << 6 | bits[2] << 5 | bits[3] << 4 | bits[4] << 3 | bits[5] << 2 | bits[6] << 1 | bits[7])

#define SS GPIO_Pin_4
#define SCK GPIO_Pin_13
#define MISO GPIO_Pin_14
#define MOSI GPIO_Pin_15

#define DELAY_CYCLES  20     //more precise than micro second delay,  1/4 of SPI bus frequency , depends on MCU master clock, 
#define DELAY_CYCLES_P0  1  //propogation pre
#define DELAY_CYCLES_P1  3  //propogation post
#define DELAY_CYCLES_C0 1   //capture pre (SCK edge -> capture) usually smaller delay
#define DELAY_CYCLES_C1 3   //capture post ( capture -> SCK edge)  usually bigger delay 


static void delayCycles(int cycles)
{
	int i;
  for(i=0; i<cycles;i++)
		__nop();
}

uint8_t SPI2_ReadWrite(uint8_t b)
{
	uint8_t _bit = 0;
	uint8_t reply=0;
  char bits[8] = {0, 0, 0, 0, 0, 0, 0, 0};  //reading buffer    
	for(_bit = 0;_bit < 8;_bit++)
	{
		// changing MOSI big while SCK low, propogation 
		GPIO_WriteBit(GPIOB,MOSI, (b & msk[_bit]));
		delayCycles(DELAY_CYCLES_P1);

		GPIO_SetBits(GPIOB,SCK);
		delayCycles(DELAY_CYCLES_C0);
	
		bits[_bit] = GPIO_ReadInputDataBit(GPIOB,MISO); // reading at the middle of SCK pulse
		delayCycles(DELAY_CYCLES_C1);
		 
		GPIO_ResetBits(GPIOB,SCK);  // data will change at falling edge
		delayCycles(DELAY_CYCLES_P0);

	}
	GPIO_ResetBits(GPIOB,MOSI);
	delayCycles(DELAY_CYCLES);

	reply = PCK();
	
	return reply;
}

unsigned char enc28j60ReadOp(unsigned char op, unsigned char address)
{
	unsigned char dat = 0;

	ENC28J60_CSL();
	delayCycles(DELAY_CYCLES);
	dat = op | (address & ADDR_MASK);
	SPI2_ReadWrite(dat);
	dat = SPI2_ReadWrite(0x00);
	// do dummy read if needed (for mac and mii, see datasheet page 29)
	if (address & 0x80)
	{
			dat = SPI2_ReadWrite(0x00);
	}   
	// release CS   
	ENC28J60_CSH();
	delayCycles(DELAY_CYCLES);
  return dat;
}

void enc28j60WriteOp(unsigned char op, unsigned char address, unsigned char data)
{
	unsigned char dat = 0;

	ENC28J60_CSL();
	delayCycles(DELAY_CYCLES);
	// issue write command
	dat = op | (address & ADDR_MASK);
	SPI2_ReadWrite(dat);
	// write data
	dat = data;
	SPI2_ReadWrite(dat);
	ENC28J60_CSH();
	delayCycles(DELAY_CYCLES);
}

void enc28j60ReadBuffer(unsigned int len, unsigned char* data)
{
	ENC28J60_CSL(); // issue read command
	SPI2_ReadWrite(ENC28J60_READ_BUF_MEM);
	while (len)
	{
			len--;        // read data
			*data = (unsigned char)SPI2_ReadWrite(0);
			data++;
	}   *data='\0';
	ENC28J60_CSH();
}
void enc28j60WriteBuffer(unsigned int len, unsigned char* data)
{
	ENC28J60_CSL(); // issue write command
	SPI2_ReadWrite(ENC28J60_WRITE_BUF_MEM);
	while (len)
	{
			len--;
			SPI2_ReadWrite(*data);
			data++;
	}
	ENC28J60_CSH();
}

void enc28j60SetBank(unsigned char address)
{
	// set the bank (if needed)
	if ((address & BANK_MASK) != Enc28j60Bank)
	{
			// set the bank
			enc28j60WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL1|ECON1_BSEL0));
			enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, (address & BANK_MASK)>>5);
			Enc28j60Bank = (address & BANK_MASK);
	}
}

unsigned char enc28j60Read(unsigned char address)
{
	// set the bank
	enc28j60SetBank(address);
	// do the read
	return enc28j60ReadOp(ENC28J60_READ_CTRL_REG, address);
}

void enc28j60Write(unsigned char address, unsigned char data)
{
	// set the bank
	enc28j60SetBank(address);
	// do the write
	enc28j60WriteOp(ENC28J60_WRITE_CTRL_REG, address, data);
}

unsigned char temp;
void enc28j60PhyWrite(unsigned char address, unsigned int data)
{   
	// set the PHY register address
	enc28j60Write(MIREGADR, address);
// write the PHY data
	enc28j60Write(MIWRL, data);
	enc28j60Write(MIWRH, data>>8);
// wait until the PHY write completes
	temp = enc28j60Read(MISTAT);
  while (temp & MISTAT_BUSY)
	{
			//Del_10us(1);
			//_nop_();
	}
}

void enc28j60clkout(unsigned char clk)
{
	//setup clkout: 2 is 12.5MHz:
	enc28j60Write(ECOCON, clk & 0x7);

}

unsigned char rev = 0xFF;

void enc28j60Init(unsigned char* macaddr)
{
	// init enc28j60
	//ENC28J60_CSH();

	//enc28j60SetSCK();
	//enc28j60HWreset();

	// perform system reset
	enc28j60WriteOp(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);
	
	// Del_1ms(250);
	// check CLKRDY bit to see if reset is complete
	// The CLKRDY does not work. See Rev. B4 Silicon Errata point. Just wait.
	//while(!(enc28j60Read(ESTAT) & ESTAT_CLKRDY));
	// do bank 0 stuff
	// initialize receive buffer
	// 16-bit transfers, must write low byte first
	// set receive buffer start address NextPacketPtr = RXSTART_INIT;
	// Rx start
	enc28j60Write(ERXSTL, RXSTART_INIT&0xFF);
	enc28j60Write(ERXSTH, RXSTART_INIT>>8);
	// set receive pointer address
	enc28j60Write(ERXRDPTL, RXSTART_INIT&0xFF);
	enc28j60Write(ERXRDPTH, RXSTART_INIT>>8);
	// RX end
	enc28j60Write(ERXNDL, RXSTOP_INIT&0xFF);
	enc28j60Write(ERXNDH, RXSTOP_INIT>>8);
	// TX start
	enc28j60Write(ETXSTL, TXSTART_INIT&0xFF);
	enc28j60Write(ETXSTH, TXSTART_INIT>>8);
	// TX end
	enc28j60Write(ETXNDL, TXSTOP_INIT&0xFF);
	enc28j60Write(ETXNDH, TXSTOP_INIT>>8);
	// do bank 1 stuff, packet filter:
	// For broadcast packets we allow only ARP packtets
	// All other packets should be unicast only for our mac (MAADR)
	//
	// The pattern to match on is therefore
	// Type     ETH.DST
	// ARP      BROADCAST
	// 06 08 -- ff ff ff ff ff ff -> ip checksum for theses bytes=f7f9
	// in binary these poitions are:11 0000 0011 1111
	// This is hex 303F->EPMM0=0x3f,EPMM1=0x30
	enc28j60Write(ERXFCON, ERXFCON_UCEN|ERXFCON_CRCEN|ERXFCON_PMEN);
	enc28j60Write(EPMM0, 0x3f);
	enc28j60Write(EPMM1, 0x30);
	enc28j60Write(EPMCSL, 0xf9);
	enc28j60Write(EPMCSH, 0xf7);
	//
	//
	// do bank 2 stuff
	// enable MAC receive
	enc28j60Write(MACON1, MACON1_MARXEN|MACON1_TXPAUS|MACON1_RXPAUS);
	// bring MAC out of reset
	enc28j60Write(MACON2, 0x00);
	// enable automatic padding to 60bytes and CRC operations
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, MACON3,
									MACON3_PADCFG0|MACON3_TXCRCEN|MACON3_FRMLNEN|MACON3_FULDPX);
	// set inter-frame gap (non-back-to-back)
	enc28j60Write(MAIPGL, 0x12);
	enc28j60Write(MAIPGH, 0x0C);
	// set inter-frame gap (back-to-back)
	enc28j60Write(MABBIPG, 0x12);
	// Set the maximum packet size which the controller will accept
	// Do not send packets longer than MAX_FRAMELEN:
	enc28j60Write(MAMXFLL, MAX_FRAMELEN&0xFF);
	enc28j60Write(MAMXFLH, MAX_FRAMELEN>>8);
	// do bank 3 stuff
	// write MAC address
	// NOTE: MAC address in ENC28J60 is byte-backward
	enc28j60Write(MAADR5, macaddr[0]);
	enc28j60Write(MAADR4, macaddr[1]);
	enc28j60Write(MAADR3, macaddr[2]);
	enc28j60Write(MAADR2, macaddr[3]);
	enc28j60Write(MAADR1, macaddr[4]);
	enc28j60Write(MAADR0, macaddr[5]);

	enc28j60PhyWrite(PHCON1, PHCON1_PDPXMD);

	// no loopback of transmitted frames
	enc28j60PhyWrite(PHCON2, PHCON2_HDLDIS);
	// switch to bank 0 enc28j60SetBank(ECON1);
	// enable interrutps
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, EIE, EIE_INTIE|EIE_PKTIE);
	// enable packet reception
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);
	
	if(enc28j60Read(MAADR5)==0x04)
	{
		__nop();
	}
}


// read the revision of the chip:
unsigned char enc28j60getrev(void)
{
  return(enc28j60Read(EREVID));
}

void enc28j60PacketSend(unsigned int len, unsigned char* packet)
{
	// Set the write pointer to start of transmit buffer area
	enc28j60Write(EWRPTL, TXSTART_INIT&0xFF);
	enc28j60Write(EWRPTH, TXSTART_INIT>>8);

	// Set the TXND pointer to correspond to the packet size given
	enc28j60Write(ETXNDL, (TXSTART_INIT+len)&0xFF);
	enc28j60Write(ETXNDH, (TXSTART_INIT+len)>>8);

	// write per-packet control byte (0x00 means use macon3 settings)
	enc28j60WriteOp(ENC28J60_WRITE_BUF_MEM, 0, 0x00);

	// copy the packet into the transmit buffer
	enc28j60WriteBuffer(len, packet);

	// send the contents of the transmit buffer onto the network
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);

	// Reset the transmit logic problem. See Rev. B4 Silicon Errata point 12.
	if ((enc28j60Read(EIR) & EIR_TXERIF))
	{
			enc28j60WriteOp(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
	}
}

// Gets a packet from the network receive buffer, if one is available.
// The packet will by headed by an ethernet header.
//      maxlen  The maximum acceptable length of a retrieved packet.
//      packet  Pointer where packet data should be stored.
// Returns: Packet length in bytes if a packet was retrieved, zero otherwise.
unsigned int enc28j60PacketReceive(unsigned int maxlen, unsigned char* packet)
{
	unsigned int rxstat;
	unsigned int len;
	// check if a packet has been received and buffered
	//if( !(enc28j60Read(EIR) & EIR_PKTIF) )
	// The above does not work. See Rev. B4 Silicon Errata point 6.
	if (enc28j60Read(EPKTCNT) ==0)
	{
			return(0);
	}
	
	// Set the read pointer to the start of the received packet
	enc28j60Write(ERDPTL, (NextPacketPtr));
	enc28j60Write(ERDPTH, (NextPacketPtr)>>8);

	// read the next packet pointer
	NextPacketPtr  = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
	NextPacketPtr |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0)<<8;
	
	// read the packet length (see datasheet page 43)
	len  = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
	len |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0)<<8;

	len-=4; //remove the CRC count
	// read the receive status (see datasheet page 43)
	rxstat  = enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0);
	rxstat |= enc28j60ReadOp(ENC28J60_READ_BUF_MEM, 0)<<8;

	// limit retrieve length
	if (len>maxlen-1)
	{
			len=maxlen-1;
	}
	
	// check CRC and symbol errors (see datasheet page 44, table 7-3):
	// The ERXFCON.CRCEN is set by default. Normally we should not
	// need to check this.
	if ((rxstat & 0x80)==0)
	{       // invalid
			len=0;
	}
	else
	{
			// copy the packet from the receive buffer
			enc28j60ReadBuffer(len, packet);
	}
	
	// Move the RX read pointer to the start of the next received packet
	// This frees the memory we just read out
	enc28j60Write(ERXRDPTL, (NextPacketPtr));
	enc28j60Write(ERXRDPTH, (NextPacketPtr)>>8);


	// decrement the packet counter indicate we are done with this packet
	enc28j60WriteOp(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);

	return(len);
}

