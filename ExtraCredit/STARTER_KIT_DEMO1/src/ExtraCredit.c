/**
 * \file
 *
 * \brief Extra Credit Terminal reflector
 *
 */

#include <asf.h>
#include <string.h>
#include "ExtraCredit.h"

#define USART_SERIAL                     ((usart_if) EDBG_UART_MODULE)
#define USART_SERIAL_BAUDRATE            9600
#define USART_SERIAL_CHAR_LENGTH         US_MR_CHRL_8_BIT
#define USART_SERIAL_PARITY              US_MR_PAR_NO
#define USART_SERIAL_STOP_BIT            false

static usart_serial_options_t usart_options = {
	.baudrate = USART_SERIAL_BAUDRATE,
	.charlength = USART_SERIAL_CHAR_LENGTH,
	.paritytype = USART_SERIAL_PARITY,
	.stopbits = USART_SERIAL_STOP_BIT
};

#define MAX_ROWS 4
#define MAX_COLUMN 128
#define CHAR_WIDTH 6
#define BUFFER_SIZE 128

static uint32_t current_column = 0;
static char in_buffer[MAX_ROWS][BUFFER_SIZE];
static uint32_t buffer_index = 0;
static uint32_t buffer_row = 0;
static char* pBuffer;


/**
 * \brief reset the buffers and the screen
 */
static void reset_buffer(void)
{
	in_buffer[0][0] = 0;
	buffer_index = 0;
	pBuffer = in_buffer[0];
	current_column = 0;
	buffer_row = 0;
	ssd1306_clear();
}


static void write_debug(const char* string)
{
	usart_serial_write_packet(USART_SERIAL, (uint8_t*)string, strlen(string));
}

/**
 * \brief setup debug and anything else for the applicant response
 */
void init_extra_credit(void)
{
	usart_serial_init(USART_SERIAL, &usart_options);
	reset_buffer();
	write_debug("\r\nReady...\r\n");
	ssd1306_clear();
}


/**
 * \brief shift the text rows up after a CR/LF, etc
 */
static void shift_rows(void)
{
	ssd1306_clear();
	for (int i=0; i<MAX_ROWS-1; i++)
	{
		strncpy(in_buffer[i], in_buffer[i+1], sizeof(in_buffer[i]));
		ssd1306_set_page_address(i);
		ssd1306_set_column_address(0);
		ssd1306_write_text(in_buffer[i]);
	}
	in_buffer[MAX_ROWS-1][0] = 0;
	buffer_index = 0;
}


/**
 * \brief go to a new line (shift up if needed)
 */
static void new_line(void)
{
	buffer_row++;
	if (buffer_row >= MAX_ROWS)
	{
		shift_rows();
		buffer_row = MAX_ROWS-1;
	}
	buffer_index = 0;
	current_column = 0;
	pBuffer = in_buffer[buffer_row];
}


/**
 * \brief write a char to the screen
 * \returns width of the char
 */
static uint32_t write_char(uint8_t c)
{
	uint8_t *char_ptr;
	uint8_t i;
	
	if ((c < 0x7F) && (c >= 32))
	{
		char_ptr = font_table[c - 32];
		for (i = 1; i <= char_ptr[0]; i++) 
		{
			ssd1306_write_data(char_ptr[i]);
		}
		ssd1306_write_data(0x00);
		return char_ptr[0];
	}
	
	return 0;
}


/**
 * \brief process commands from the terminal
 * process char received
 */
static void process_command(const char* string)
{	
	char command[BUFFER_SIZE];
	strncpy(command, string, sizeof(command));
	
	char* context=0;
	char* token = strtok_r(command, "\r\n \t", &context);
	if (token != 0)
	{
		if (strncmp(token, "led", 3) == 0)
		{
			char* number = strtok_r(NULL, "\r\n \t", &context);
			char* ON = strtok_r(NULL, "\r\n \t", &context);
			
			if ((number != 0) 
			  && (ON != 0))
			{
				bool led_off = false;
				if (*ON == '0')
				{
					led_off = true;
				}
				
				if (*number == '1')
				{
					ioport_set_pin_level(IO1_LED1_PIN, led_off);
				}
				else if (*number == '2')
				{
					ioport_set_pin_level(IO1_LED2_PIN, led_off);
				}
				else if (*number == '3')
				{
					ioport_set_pin_level(IO1_LED3_PIN, led_off);
				}
			}
		}
		else if (strncmp(token, "help", 4) == 0)
		{
			write_debug("\r\nvalid commands:\r\nled (1|2|3) (0|1)\r\nrestart\r\nhelp\r\n");
		}
		else if (strncmp(token, "restart", 4) == 0)
		{
			reset_buffer();
			write_debug("\r\nReady...\r\n");
			ssd1306_clear();
			ioport_set_pin_level(IO1_LED1_PIN, !IO1_LED1_ACTIVE);
			ioport_set_pin_level(IO1_LED2_PIN, !IO1_LED2_ACTIVE);
			ioport_set_pin_level(IO1_LED3_PIN, !IO1_LED3_ACTIVE);
		}
	}
	
}


/**
 * \brief check the serial port for data
 * process char received
 */
void check_serial(void)
{
	uint32_t val = 0;
	usart_read(USART_SERIAL, &val);
	
	val = val & 0xff;
	if ((val > 0) && (val <= 127))
	{
		
	// process special chars
	
		if (val < 32)
		{
		// carriage return
			if (val == 0x0d)
			{
				char command[BUFFER_SIZE];
				strncpy(command, in_buffer[buffer_row], sizeof(command));
				new_line();
				process_command(command);
				usart_serial_write_packet(USART_SERIAL, (uint8_t*)"\r\n", 2);
			}
			
		// clear screen (ctrl-l)
			else if (val == 0x0c)
			{
				reset_buffer();
				usart_serial_write_packet(USART_SERIAL, (uint8_t*)"\r\n", 2);
			}
			
			return;
		}
		
	// echo the byte
	
		usart_write(USART_SERIAL, val);
		
	// store in the buffer
	
		pBuffer[0] = (uint8_t)(val);
		pBuffer[1] = 0;
	
	// put it on screen
		
		ssd1306_set_page_address(buffer_row);
		ssd1306_set_column_address(current_column);
		uint32_t width = write_char(*pBuffer) + 1;
		current_column += width;
		
	// check for end of line, etc
		pBuffer++;
		buffer_index++;
		if (buffer_index >= sizeof(in_buffer)-1)
		{
			reset_buffer();
		}
		
		if (current_column >  (MAX_COLUMN - CHAR_WIDTH))
		{
			new_line();
		}
	}
}

