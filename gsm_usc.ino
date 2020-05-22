#include <TimerOne.h>

// Definitions
#define Success 1U
#define Failure 0U

// Pins
// The GSM board's RXD and TXD are connected to Serial1 on the Arduino Mega.
// We use Serial (the USB serial ports) for debugging messages to the computer.
const int led = 13; // Used to report failure conditions from the GSM board
const int gsmPowerSwitch = 7; // This is the pin on the GSM board that functions as the "power switch". This needs to be operated when the board is powered on.
const int rfTxData = 6; // This is the data pin for the 433mhz transmitter

// These are used for debugging the GSM board
const bool sendStartupMessage = false;
char phoneNumber[] = "2065551212";    // Used to send messages out from the board. For my US carrier, this is a 10 digit phone number.
char msg[] = "Arduino GSM board booted up\r\n";   // This is a debugging message sent from the board. MUST be terminated by \r\n.

// These variables are used by the Arduino to handle the serial data received from the board
const unsigned int gprsRxBufferLength = 255;
char gprsRxBuffer[gprsRxBufferLength];
unsigned int gprsBufferCount = 0;

// Timer handling
unsigned long  Time_Cont = 0;

// Variables related to the collar data
char remoteId[] = "11011101010110110"; // This is the ID corresponding to the transmitter, and is paired to the receiver.
char collarSelect[] = "000"; // The transmitter can control two collars - this is collar 1.

// This string is used as a "key" to verify command messages received over GSM
char key[] = "COMMAND";
char terminator[] = "---";

void setup() {
  // Turn the LED off
  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  // Initialize the timer interrupt
  Timer1.initialize(1000);
  Timer1.attachInterrupt(Timer1_handler);

  // Initialize serial comms
  Serial.begin(9600);
  Serial1.begin(115200);

  Serial.println("Starting GSM-controlled shock collar controller...");

  // Start the GSM board
  pinMode(gsmPowerSwitch, OUTPUT);
  digitalWrite(gsmPowerSwitch, HIGH);
  delay(2500);
  digitalWrite(gsmPowerSwitch, LOW);
  // wait for the board to come up
  delay(2500);
  // Run the script to set up the GSM board
  initGprs();

  Serial.println("GSM board initialized!");

  if (sendStartupMessage == true) {
    Serial.println("Sending SMS through GSM board");
    sendMessage(phoneNumber, msg);
    delay(1000);
    Serial.println("Sent message");
  }

  clrGprsRxBuffer();
  delay(1000);
}

void loop() {
  checkSms();
  delay(1000);
}

void checkSms() {
  clrGprsRxBuffer();
  Serial.println("Checking for messages...");
  delay(100);
  Serial1.write("AT+CMGL=\"ALL\"\r\n");
  delay(100);
  gprsReadBuffer();
  Serial.println(gprsRxBuffer);

  // On this board, if we receive a message, it will be prefixed with the string "+CIEV"
  if (strstr(gprsRxBuffer, "+CIEV"))  {
    char rxMsg[255];
    memset(rxMsg, 0, sizeof(rxMsg));
    strncpy(rxMsg, gprsRxBuffer, sizeof(rxMsg));
    handleRxSms(rxMsg);
  }
}

void handleRxSms(char *rxMsg) {
  // This function parses a received SMS for commands
  // We can do further checking here to do extra verification on the message
  // eg, from what phone number it was received, etc if we wanted

  // For the purposes of this proof of concept, we will expect our commands to be in this format:
  // KEY ACTION INTENSITY TERMINATOR
  //   KEY = the magic string we defined up above. This is a shared secret, and allows us to ignore messages that aren't commands (eg spam messages)
  //   ACTION = the action to perform. As above, we use:
  //     L = Light the LED
  //     B = Sound the beeper
  //     V = Vibrate
  //     S = Shock
  //   Note: We left the option for additional "actions" - for example, macros.
  //   INTENSITY - an int from 0-100.
  //   TERMINATOR - another magic string that ends the command

  Serial.println("Entered received SMS handler");

  if (strstr(rxMsg, key))  {
    // We found our key in the SMS. Continue processing!
    while (1) {
      if (!strstr(rxMsg, terminator))  {
        Serial.println("Parse error - terminator not found");
        break;
      }

      char *spos = strstr(rxMsg, key);
      char *epos = strstr(rxMsg, terminator);

      char rxCommand[100];
      memset(rxCommand, 0, sizeof(rxCommand));
      strncpy(rxCommand, spos, epos - spos - 1); // Copy the relevant part of the SMS message from the raw data in gprsRxBuffer.
      Serial.println("Copied command:");
      Serial.println(rxCommand);

      // Break up the command and verify the components
      char * pch;
      char action[20];
      memset(action, 0, sizeof(action));
      char intensity[3];
      memset(intensity, 0, sizeof(intensity));
      int i = 0;
      pch = strtok(rxCommand, " ");
      while (pch != NULL) {
        switch (i)  {
          case 0:
            // no-op, we don't need to store the key
            break;
          case 1:
            strncpy(action, pch, sizeof(action));
            break;
          case 2:
            strncpy(intensity, pch, sizeof(intensity));
            break;
          default:
            Serial.println("Warning: More arguments were provided than expected");
            break;
        }

        i++;
        pch = strtok (NULL, " ");
      }

      Serial.print("Action: ");
      Serial.println(action);
      Serial.print("Intensity: ");
      Serial.println(intensity);

      // Set some variables we'll need later
      int intensityValue = 0;

      // Do some checking on the received data
      // we can't use a switch here :(
      char action1[5];
      char action2[5];
      if (strcmp(action, "L") == 0)  {
        strcpy(action1, "1000");
        strcpy(action2, "1110");
      }
      else if (strcmp(action, "B") == 0)  {
        strcpy(action1, "0100");
        strcpy(action2, "1101");
      }
      else if (strcmp(action, "V") == 0) {
        strcpy(action1, "0010");
        strcpy(action2, "1011");
      }
      else if (strcmp(action, "S") == 0) {
        strcpy(action1, "0001");
        strcpy(action2, "0111");
      }
      else {
        Serial.print("Parse error: Invalid action received ");
        Serial.println(action);
        break;
      }

      // actually check this later
      intensityValue = atoi(intensity);

      //else  {
      //  Serial.print("Parse error: Invalid intensity received ");
      //  Serial.print(intensity);
      //  Serial.println(", must be an int between 0 and 100");
      //  break;
      //}

      // If we've made it here, we're ready to concatente together a string of 0s and 1s for the RF transmitter
      char rfData[43];
      memset(rfData, 0, sizeof(rfData));
      int j = 0;

      // holy fuck fix this
      char zero[] = "0";
      char one[] = "1";

      // bits 0 and 1 are static
      rfData[0] = zero[0];
      rfData[1] = one[0];

      // set the collar select to 000 - collar 1
      for (i = 2; i < 5; i++) {
        rfData[i] = zero[0];
      }

      // set the action (first instance)
      j = 0;
      for (i = 5; i < 9; i++) {
        rfData[i] = action1[j];

        j++;
      }

      // set the remote ID
      j = 0;
      for (i = 9; i < 26; i++) {
        rfData[i] = remoteId[j];
        j++;
      }

      // set the intensity
      j = 6;
      // fix this too
      int bitReadResult;

      for (i = 26; i < 33; i++) {
        Serial.print("bitRead ");
        Serial.print(j);
        Serial.print(": ");
        Serial.println(bitRead(intensityValue, j));
        bitReadResult = bitRead(intensityValue, j);
        rfData[i] = bitReadResult + 48;
        j--;
      }

      // set the action (second instance)
      j = 0;
      for (i = 33; i < 37; i++) {
        rfData[i] = action2[j];
        j++;
      }

      // set the collar select to 111 - collar 1
      for (i = 37; i < 40; i++) {
        rfData[i] = one[0];
      }

      // bits 40 and 41 are static
      rfData[40] = zero[0];
      rfData[41] = zero[0];

      for (i = 0; i < 43; i++)  {
        Serial.print("rfData char ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(rfData[i]);
      }

      Serial.print("Output string built: ");
      Serial.println(rfData);

      sendRf(rfData);

      // success - break
      break;
    }
  }
}

void sendRf(String command)  {
  // ST -> On for 1330s
  // 1 -> On for 750us, off for 300us
  // 0 -> On for 220us, off for 830us
  // Based on testing with the scope, it looks like each step through the loop takes about 30-40us, which means that we need to subtract this from the delay at the end of each

  int timeCorrection = 35;

  int j = 42;


  Serial.print("Input string: ");
  Serial.println(command);

  // First, made sure we're low
  digitalWrite(rfTxData, LOW);
  delayMicroseconds(250);

  //send the "start" pulse
  digitalWrite(rfTxData, HIGH);
  delayMicroseconds(1330);

  // Iterate over the command
  String currChar = "";
  for (int i = 0; i < j; i++)  {
    currChar = command.charAt(i);
    //Serial.print("Handling character ");
    //Serial.print(i);
    //Serial.print(" of ");
    //Serial.print(j);
    //Serial.print(" - ");
    //Serial.print(currChar);
    //Serial.print("... ");

    if (currChar == "0")  {
      digitalWrite(rfTxData, HIGH);
      delayMicroseconds(220);
      digitalWrite(rfTxData, LOW);
      delayMicroseconds(830 - timeCorrection);
      //Serial.print("Wrote a 0 to the collar\n");
    }

    else if (currChar == "1")  {
      digitalWrite(rfTxData, HIGH);
      delayMicroseconds(750);
      digitalWrite(rfTxData, LOW);
      delayMicroseconds(300 - timeCorrection);
      //Serial.print("Wrote a 1 to the collar\n");
    }

    else  {
      Serial.print("Could not interpret character - skipping\n");
    }

  }

  Serial.print("Reached end of command\n\n");

}

void sendMessage(char *number, char *msg)
{
  Serial.println("Entered sendMessage function");
  delay(250);
  char send_buf[25];
  memset(send_buf, 0, 25);    //清空
  strcpy(send_buf, "AT+CMGS=\"");
  strcat(send_buf, number);
  strcat(send_buf, "\"\r\n");
  delay(1000);
  if (sendCommand(send_buf, ">", 3000, 10) == Success)  {
    delay(200);
    Serial.println("sendMessage step 1 AT cmd success");
    delay(200);
  }
  else {
    errorLog(7);
    delay(200);
    Serial.println("sendMessage step 1 AT cmd failed");
    delay(200);
  }

  if (sendCommand(msg, msg, 3000, 10) == Success) {
    delay(200);
    Serial.println("sendMessage step 2 message success");
    delay(200);
  }
  else {
    errorLog(8);
    delay(200);
    Serial.println("sendMessage step 2 message failed");
    delay(200);
  }

  memset(send_buf, 0, 20);    //清空
  send_buf[0] = 0x1a;
  send_buf[1] = '\0';
  if (sendCommand(send_buf, "OK\r\n", 10000, 5) == Success) {
    delay(200);
    Serial.println("sendMessage step 3 success");
    delay(200);
  }
  else {
    errorLog(9);
    delay(200);
    Serial.println("sendMessage step 3 failed");
    delay(200);
  }
}


void initGprs()
{
  if (sendCommand("AT+RST\r\n", "OK\r\n", 3000, 10) == Success);
  else errorLog(1);

  if (sendCommand("AT\r\n", "OK\r\n", 3000, 10) == Success);
  else errorLog(2);

  delay(10);

  if (sendCommandReceive2Keyword("AT+CPIN?\r\n", "READY", "OK\r\n", 3000, 10) == Success);
  else errorLog(3);
  delay(10);

  if (sendCommandReceive2Keyword("AT+CREG?\r\n", "CREG: 1", "OK\r\n", 3000, 2) == Success);
  else errorLog(4);
  delay(10);

  if (sendCommand("AT+CMGF=1\r\n", "OK\r\n", 3000, 10) == Success);
  else errorLog(5);
  delay(10);

  if (sendCommand("AT+CSCS=\"GSM\"\r\n", "OK\r\n", 3000, 10) == Success);
  else errorLog(6);
  delay(10);
}

void(* resetFunc) (void) = 0;

void errorLog(int num)
{
  delay(100);
  Serial.print("ERROR");
  Serial.println(num);
  while (1)
  {
    digitalWrite(led, HIGH);
    delay(100);

    //if (sendCommand("AT\r\n", "OK", 100, 10) == Success)
    //{
    //  Serial.print("\r\nRESET!!!!!!\r\n");
    //  resetFunc();
    //}
    //digitalWrite(L, LOW);
  }
}

unsigned int sendCommand(char *Command, char *Response, unsigned long Timeout, unsigned char Retry)
{
  clrGprsRxBuffer();
  for (unsigned char n = 0; n < Retry; n++)
  {
    Serial.print("\r\n---------send AT Command:---------\r\n");
    Serial.write(Command);

    delay(250);
    Serial1.write(Command);
    delay(250);

    Time_Cont = 0;
    while (Time_Cont < Timeout)
    {
      gprsReadBuffer();
      if (strstr(gprsRxBuffer, Response) != NULL)
      {
        Serial.print("\r\n==========receive AT Command:==========\r\n");
        Serial.print(gprsRxBuffer); //输出接收到的信息
        clrGprsRxBuffer();
        return Success;
      }
    }
    Time_Cont = 0;
  }
  Serial.print("\r\n==========receive AT Command:==========\r\n");
  Serial.print(gprsRxBuffer);//输出接收到的信息
  clrGprsRxBuffer();
  return Failure;
}

unsigned int sendCommandReceive2Keyword(char *Command, char *Response, char *Response2, unsigned long Timeout, unsigned char Retry)
{
  clrGprsRxBuffer();
  for (unsigned char n = 0; n < Retry; n++)
  {
    Serial.print("\r\n---------send AT Command:---------\r\n");
    Serial.write(Command);

    Serial1.write(Command);
    delay(100);

    Time_Cont = 0;
    while (Time_Cont < Timeout)
    {
      gprsReadBuffer();
      if (strstr(gprsRxBuffer, Response) != NULL && strstr(gprsRxBuffer, Response2) != NULL)
      {
        Serial.print("\r\n==========receive AT Command:==========\r\n");
        Serial.print(gprsRxBuffer); //输出接收到的信息
        clrGprsRxBuffer();
        return Success;
      }
    }
    Time_Cont = 0;
  }
  Serial.print("\r\n==========receive AT Command:==========\r\n");
  Serial.print(gprsRxBuffer);//输出接收到的信息
  clrGprsRxBuffer();
  return Failure;
}



void gprsReadBuffer() {
  while (Serial1.available() > 0)
  {
    gprsRxBuffer[gprsBufferCount++] = Serial1.read();
    if (gprsBufferCount == gprsRxBufferLength)clrGprsRxBuffer();
  }
}

void clrGprsRxBuffer(void)
{
  memset(gprsRxBuffer, 0, gprsRxBufferLength);      //清空
  gprsBufferCount = 0;
}

void Timer1_handler(void)
{
  Time_Cont++;
}
