
#define WIFI_Serial Serial1
#define Debug_Serial Serial

#define ESP_RESET_PIN   	67
#define ESP_CH_DOWN_PIN 	44
#define ESP_PROG_PIN		A5

#define MY_BAUDRATE 115200

void setup() {
  
  pinMode(ESP_RESET_PIN,OUTPUT);
  pinMode(ESP_CH_DOWN_PIN,OUTPUT);
  pinMode(ESP_PROG_PIN,OUTPUT);
	
  digitalWrite(ESP_RESET_PIN,HIGH);
  digitalWrite(ESP_CH_DOWN_PIN,HIGH);
  
  //set prog pin low
  digitalWrite(ESP_PROG_PIN,LOW);
  
  //reset esp
   digitalWrite(ESP_RESET_PIN,LOW);
   delay(100);
   digitalWrite(ESP_RESET_PIN,HIGH);
  // put your setup code here, to run once:
  WIFI_Serial.begin(MY_BAUDRATE);
  Debug_Serial.begin(MY_BAUDRATE);
}

void wifi_write(const uint8_t c)
{
  //TODO find out why normal, buffered send do not work correctly
	
  //workaround send uart data to ESP8266 directly without buffering
  Uart* _pUart =(Uart*) USART0;
  // Check if the transmitter is ready
    while ((_pUart->UART_SR & UART_SR_TXRDY) != UART_SR_TXRDY) 
    {
        
    }

  // Send character
   _pUart->UART_THR = c;
}

void loop() {
  // put your main code here, to run repeatedly:
  if(WIFI_Serial.available())
  {
    Debug_Serial.write(WIFI_Serial.read());
  }
  
  if(Debug_Serial.available())
  {
    wifi_write(Debug_Serial.read());
  }
}
