#include "LPC17xx.h"
#include <string.h>
#include <stdio.h>
void get_thingspeak_status(void);
void parse_led_sta_tus(void);
void wait_response(unsigned int timeout_ms);
volatile char esp_rx_buffer[512];
volatile unsigned int esp_rx_index = 0;
volatile unsigned int esp_data_received = 0;
char field1_value[10]; // holds field1 string
void delay_ms(unsigned int ms) {
    for (unsigned int i = 0; i < ms ; i++)
	for ( volatile unsigned int j = 0; j < 6000 ; j++);
}

void uart0_init(void) {
    LPC_SC->PCONP |= (1 << 3); // Enable UART0 power
    LPC_PINCON->PINSEL0 |= (1 << 4) | (1 << 6); // P0.2 TXD0, P0.3 RXD0

    LPC_UART0->LCR = 0x83; // 8-bit data, enable DLAB
    LPC_UART0->DLM = 0;
    LPC_UART0->DLL = 0x0E; // Baud rate 9600
    LPC_UART0->LCR = 0x03; // Disable DLAB
}

void uart1_init(void) {
    LPC_SC->PCONP |= (1 << 4); // Enable UART1 power
    LPC_PINCON->PINSEL4 |= (1 << 1) | (1 << 3); // P2.0 TXD1, P2.1 RXD1

    LPC_UART1->LCR = 0x83; // 8-bit data, enable DLAB
    LPC_UART1->DLM = 0;
    LPC_UART1->DLL = 0x0E; // Baud rate 9600
    LPC_UART1->LCR = 0x03; // Disable DLAB
    LPC_UART1->IER = 0x01; // Enable RX interrupt
    NVIC_EnableIRQ(UART1_IRQn);
}

void uart0_sendchar(char c) {
    while (!(LPC_UART0->LSR & 0x20));
    LPC_UART0->THR = c;
}

void uart1_sendchar(char c) {
    while (!(LPC_UART1->LSR & 0x20));
    LPC_UART1->THR = c;
}

void uart0_print(const char *str) {
    while (*str) uart0_sendchar(*str++);
}

void uart1_print(const char *str) {
    while (*str) uart1_sendchar(*str++);
}


void UART1_IRQHandler(void) {
    if (LPC_UART1->IIR & 0x04) {  
        char ch = LPC_UART1->RBR;
        if (esp_rx_index < sizeof(esp_rx_buffer) - 1) {
            esp_rx_buffer[esp_rx_index++] = ch;
						if(ch == '\n'){
							esp_rx_buffer[esp_rx_index] = '\0'; // Null terminate after every char
            esp_data_received = 1; 
					}
        }
				else{
					esp_rx_index=0;
				}
				
    }
}




void send_command(const char *cmd,int timeout_ms)
{
	uart0_print("SEND:");
	uart0_print(cmd);
	uart0_print("\r\n");
	uart1_print(cmd);
	uart1_print("\r\n");
	wait_response(timeout_ms);
	
	
	
}
void wait_response(unsigned int timeout_ms)
{
	unsigned int wait=0;
	while(wait<timeout_ms)
	{
		if(esp_data_received)
		{
			uart0_print("RESPONSE:");
			uart0_print((char *)esp_rx_buffer);
			//memset((char *)esp_rx_buffer, 0, sizeof(esp_rx_buffer)); //extra
			esp_data_received=0;
			esp_rx_index=0;
			wait=0;
			
		}
		else{
			delay_ms(10);
			wait+=10;
		}
	}
}

void parse_led_status(void) {
	uart0_print("\r\n[JSON]");
	//uart0_print("");
    char *feeds_ptr = strstr((char *)esp_rx_buffer, "\"feeds\":[{");

    if (feeds_ptr ) {
        // Look for field1 after feeds
        char *field_ptr = strstr(feeds_ptr, "\"field1\":\"");

        if (field_ptr) {
            field_ptr += strlen("\"field1\":\"");

            char field1_value[16];
            uint8_t i = 0;

            while (*field_ptr && *field_ptr != '"' && i < sizeof(field1_value) - 1) {
                field1_value[i++] = *field_ptr++;
                
            }
            field1_value[i] = '\0';

            uart0_print("Field1 value: ");
            uart0_print(field1_value);
            uart0_print("\r\n");

            // Control GPIO pin P1.19
            LPC_GPIO1->FIODIR |= (1 << 19);
            if (field1_value[0]=='1') {
                LPC_GPIO1->FIOSET = (1 << 19);  // LED ON
            } else {
                LPC_GPIO1->FIOCLR = (1 << 19);  // LED OFF
            }
        } else {
            uart0_print("Field1 not found inside feeds.\r\n");
        }
    } else {
        uart0_print("Feeds section not found in response.\r\n");
    }
}

int main(void) 
	{
		LPC_GPIO1->FIODIR|=0x0FF80000;
		LPC_GPIO1->FIOCLR=0x0FF80000;
    uart0_init();  
    uart1_init();  
delay_ms(3000);
    uart0_print("ESP01 initialization...\r\n");
    delay_ms(2000);
	



    send_command("AT",1000);
    send_command("AT+CWMODE=1",1000);
    

    send_command("AT+CWJAP=\"Nithya\",\"Asdfghjkl\"",4000);
	
     while (1) {
        get_thingspeak_status();
        parse_led_status();
        delay_ms(10000); // Check every 10s
    }
}




void get_thingspeak_status(void) {
    send_command("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80",4000);
   // delay_ms(2000);

     char http_get[] = "GET /channels/2956684/fields/1.json?api_key=4DTSGA2V1PF8WX4U&results=1 HTTP/1.1\r\n"
			"Host: api.thingspeak.com\r\n"
		  "Connection: close\r\n\r\n";

    char cip_send_cmd[32];
    sprintf(cip_send_cmd, "AT+CIPSEND=%d", strlen(http_get));
    send_command(cip_send_cmd,2000);
    //delay_ms(1000);

   uart1_print(http_get);

    wait_response(8000);

    
}