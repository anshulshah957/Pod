/*
 * spi_to_bbb.c
 *
 * Created: 4/20/2017 11:50:15 PM
 *  Author: rgw3d
 */ 

#include "spi_to_bbb.h"


//Used in SPI ISR
volatile uint8_t rx_byte = 0x00;
volatile uint8_t spic_flag = 0;

extern uint8_t sensor_status;
extern uint8_t state;
extern uint8_t sensor_data[SENSOR_DATA_SIZE];
extern uint8_t lock;

#define SPI_TX_START 0xAA
#define SPI_CRC_PASS 0xAA
#define SPI_CRC_FAIL 0xF0

//Data buffers for RX and TX
circular_buffer_t rx_buff;
circular_buffer_t tx_buff;

//Incoming data
#define CMD_DATA_SIZE 5
uint8_t cmd_data[CMD_DATA_SIZE];
uint8_t cmd_idx = 0;
uint16_t received_crc = 0;
uint16_t calculated_crc = 0;

//Sending data
uint8_t cmd_finished = 0;
uint8_t send_data[SENSOR_DATA_SIZE+2];
uint8_t send_idx = 0;
uint8_t send_crc_length = 0;//set equal to maximum value of send_idx
uint16_t send_crc = 0;
uint8_t send_crc_idx = 0;

ISR(SPIC_INT_vect) {
	//Add recieved byte to rx buffer
	uint8_t tmp = SPIC.DATA;
	circular_buffer_push(&rx_buff,tmp);
	
	//Send tx byte if there is one to send
	if(circular_buffer_size(&tx_buff)){
		SPIC.DATA = circular_buffer_pop(&tx_buff);
	}
	spic_flag ++;
}

void init_spi_to_bbb(){
	sysclk_enable_peripheral_clock( &SPIC ); 
	PORTC.DIR = 0x40;		// MISO output; MOSI, SCK, SS inputs
	SPIC.CTRL = 0x40;		// slave mode, mode 0
	SPIC.INTCTRL = 0x03;	// enable interrupts
	PMIC.CTRL = 0x04;       // enable high priority interrupts
	memset(&rx_buff, 0, sizeof(circular_buffer_t));
	memset(&tx_buff, 0, sizeof(circular_buffer_t));
	
	
}
uint8_t size;
void handle_spi_to_bbb(){
	//Loop while we have data in the RX buffer to process
	size = circular_buffer_size(&rx_buff);
	while(circular_buffer_size(&rx_buff)){
		
		rx_byte = circular_buffer_pop(&rx_buff);
		//if(rx_byte == 0xaa)ioport_set_pin_level(LED_0_PIN,LED_0_ACTIVE);
		
		if(rx_byte == SPI_TX_START){
			cmd_idx = CMD_DATA_SIZE;
			//Reset all the send variables/tmp storage
			cmd_finished = 0;
			send_idx = 0;
			send_crc_length = 0;
			send_crc = 0;
			send_crc_idx = 0;
			lock = 1;
			

		}
	
		//If we are receiving command, store it appropriately
		if(cmd_idx > 0){
		ioport_set_pin_level(LED_0_PIN,LED_0_ACTIVE);
			cmd_data[CMD_DATA_SIZE-cmd_idx] = rx_byte;
			cmd_idx--;
			//Finished last storage of incoming data
			if(cmd_idx == 0){
				
				//Check recieved_crc against calculated CRC
				received_crc =	(cmd_data[CMD_DATA_SIZE-1]<<8) | cmd_data[CMD_DATA_SIZE-2];
				calculated_crc = crc_io_checksum(cmd_data, CMD_DATA_SIZE-2, CRC_16BIT);
				//Send appropriate signal if passed/failed
			
				if(calculated_crc == received_crc){
					//SPIC.DATA = SPI_CRC_PASS;
					
					circular_buffer_push(&tx_buff,SPI_CRC_PASS);
					cmd_finished = 1;
				}
				else{
					
					//SPIC.DATA = SPI_CRC_FAIL;
					circular_buffer_push(&tx_buff,SPI_CRC_FAIL);
				}
			}
		}
		else if(cmd_finished){
			//On next pass we will start pipelining data
			/*if(cmd_data[2] == 0){
				memcpy(send_data,sensor_data,SENSOR_DATA_SIZE);//TODO: determine if this takes too long
				send_idx = SENSOR_DATA_SIZE;
			}
			else if(cmd_data[2] == 1){
				//ioport_set_pin_level(LED_0_PIN,LED_0_ACTIVE);
				send_data[0] = sensor_status;
				send_idx = 1;
			}
			else if(cmd_data[2] == 2){
				send_data[0] = state;
				send_idx = 1;
			}
			else{*/
				memcpy(send_data,sensor_data,SENSOR_DATA_SIZE);
				send_data[SENSOR_DATA_SIZE] = sensor_status;
				send_data[SENSOR_DATA_SIZE+1] = state;
				send_idx = SENSOR_DATA_SIZE+2;
			//}
			send_crc_length = send_idx;
			cmd_finished = 0;
		}
	
		if(send_idx > 0){
			//SPIC.DATA = send_data[send_crc_length-send_idx];
			circular_buffer_push(&tx_buff, send_data[send_crc_length-send_idx]);
			send_idx--;
		
			//Calculate CRC
			if(send_idx == 0){
				send_crc = crc_io_checksum(send_data, send_crc_length, CRC_16BIT);
				send_crc_idx = 2;
			}
		
		}
		else if(send_crc_idx > 0){
		
			//SPIC.DATA = send_crc >> ((2-send_crc_idx)*8);
			circular_buffer_push(&tx_buff, send_crc>> ((2-send_crc_idx)*8));
			send_crc_idx--;
			if(send_crc_idx == 0) lock = 0;
		}
	
		spic_flag = 0;
	}
}