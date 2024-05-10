#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>


WiFiClient espClient;
PubSubClient mqttClient(espClient);
JsonDocument data;

// Define OLED related parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C 

// Initilize Global varibles to keep Date and Time
int days=0;
int hours=0;
int minutes=0;
int seconds=0;
String Date;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

// Initilize varibles to keep alarm datas for 3 alarms
bool alarm_enabled=true;
int n_alarms=3;
int alarm_hours[]={0,0,0};
int alarm_minutes[] = {0,0,0};
bool alarm_triggered[] = {false, false, false}; // Initialized with false for each alarams

// Define Buzzer and LED indicator Pins
#define BUZZER 18                  
#define LED 5

// Define Push Button Pins for various operations
#define PB_CANCEL 34
#define PB_OK 33
#define PB_UP 35
#define PB_DOWN 32

// Initialize DHT pin to get temperature and humidity
#define DHTPIN 25

// Define the NTP server to use for time synchronization
#define NTP_SERVER     "pool.ntp.org"
int UTC_OFFSET; // Define the UTC offset (in seconds) from UTC time.
#define UTC_OFFSET_DST 0 // Define the UTC offset (in seconds) for daylight saving time (DST).

// Define the total number of notes
int n_notes = 8;

// Define the frequencies of each note
int C = 256;
int D = 288;
int E = 324;
int F = 363;
int G = 407;
int A = 456;
int B = 512;
int C_H = 576;

// Store the frequencies of the notes in an array
int notes[] = {C, D, E, F, G, A, B, C_H};

// Define the current mode of operation
int current_mode = 0;

// Define the maximum number of modes
int max_modes = 5;

// Define options for different modes as strings
String options[] = {"1 - Set Time",      // Mode 1: Set Time
                    "2 - Set Alarm 1",  // Mode 2: Set Alarm 1
                    "3 - Set Alarm 2",  // Mode 3: Set Alarm 2
                    "4 - Set Alarm 3",  // Mode 4: Set Alarm 3
                    "5 - Remove Alarm"}; // Mode 5: Remove Alarm


// Initialize Adafruit_SSD1306 display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Initialize DHTesp sensor object
DHTesp dhtSensor;

//Servo related parameters
#define SERVO 26
int servo_angle;
Servo servo_motor;

#define LDR_1 36 // VP pin
#define LDR_2 39 // VN pin

#define MQTT_SERVER "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_DEVICE_ID "Smart-Medibox-210625"
#define DATA_SENDING_TOPIC "MEDIBOX_Send_210625"
#define DATA_RECEPTION_TOPIC "MEDIBOX_Receive_210625"


// LDR Parameters for calculating Luminance and its change.
#define EPSILON 0.03
#define TEMPSILON 0.01
#define HUMIDITY_EPSILON 0.05

#define LINEAR_MAPPING 0
#define FINITE_INFINITY 10000
#define GAMMA 0.7
#define RL10 50
#define RESISTOR 10000
#define VCC 3.3

//Main Switch parameter to check whether the Medibox is on or not


void setup() {

  // Initialize pin modes
  pinMode(BUZZER, OUTPUT);
  pinMode(LED, OUTPUT);

  pinMode(PB_CANCEL, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);

  // Setup DHT sensor
  dhtSensor.setup(DHTPIN,DHTesp::DHT22);

  // Setup Servo
  servo_motor.attach(SERVO);

  // Setup LDR
  pinMode(LDR_1, INPUT);
  pinMode(LDR_2, INPUT);
  
  // Start serial communication
  Serial.begin(9600);

  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
    Serial.println(F("SSD1306 allocation failed"));
    for( ;; );
  }
  
  // Display initialization
  display.display();
  delay(500);

  // Connect to WiFi
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WiFi",0,0,2);
    
  }
  display.clearDisplay();
  print_line("Connected to WiFi",0,0,2);

  // Configure time
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  display.clearDisplay();

  // Welcome message
  print_line("Welcome to Medibox",10,20,2);
  delay(500);
  display.clearDisplay();

  setupMQTT();

}

void loop() {

  brokerConnectMQTT();

  // Update time and check alarms
  update_time_with_check_alarm();
  
  // Check if the OK button is pressed
  if (digitalRead(PB_OK) == LOW) {
    delay(2000); // Delay to debounce the button
    go_to_menu(); // Go to menu function
  }

  // Check temperature
  float temperature = check_temp();
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  float humidity = data.humidity;

  send_mqtt_data(temperature, humidity);
}

// Function to print a line of text on the display
void print_line(String text, int column, int row , int text_size ) {
  // Set text size, color and cursor position
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);

  // Print the text and display it in newline
  display.println(text);
  display.display();
}

// function to display the current time DD:HH:MM:SS
void print_time_now(void) {

  display.clearDisplay();

  // Print the date
  print_line(Date, 0, 0, 2);

  // Print the time (hours:minutes:seconds)
  print_line("" + String(hours) + ":" + String(minutes) + ":" + String(seconds), 0, 20, 2);
  
}

// Function to update the time variables
void update_time() {

    // Define a struct to hold time information
    struct tm timeinfo;

    // Get the current local time
    getLocalTime(&timeinfo);

    // Extract time and date from timeinfo and convert to integer
    char timeHour[3];
    strftime(timeHour, 3, "%H", &timeinfo);
    hours = atoi(timeHour);

    char timeMinute[3];
    strftime(timeMinute, 3, "%M", &timeinfo);
    minutes = atoi(timeMinute);

    char timeSecond[3];
    strftime(timeSecond, 3, "%S", &timeinfo);
    seconds = atoi(timeSecond);

    char formattedDate[20];
    strftime(formattedDate, 20, "%Y-%m-%d", &timeinfo);
    Date = formattedDate;
}

// Function to update time and check for triggered alarms
void update_time_with_check_alarm(void){

    // Update the current time and print current time
    update_time();
    print_time_now();


    // check if alarms are enabled
    if (alarm_enabled) {
      for (int i = 0; i < n_alarms ; i++) {
        // Check if the alarm is not triggered and the current time matches the alarm time
        if (alarm_triggered[i] == false && hours == alarm_hours[i] && minutes == alarm_minutes[i]) {
          
          // Ring the alarm and mark the alarm as triggered
          ring_alarm(); 
          alarm_triggered[i] = true;
        }
      }
    }
}

// Function to ring the alarm
void ring_alarm() {
  // Clear the display and print "Medicine Time"
  display.clearDisplay();
  print_line("Medicine Time",0, 0, 2);

  // Turn on the LED
  digitalWrite(LED, HIGH);

  bool break_happen=false;

  // Continue ringing the alarm until canceled or button is pressed
  while(break_happen==false && digitalRead(PB_CANCEL)==HIGH){

    for (int i = 0; i < n_notes; i++) {
      // Check if cancel button is pressed, break the loop if pressed
      if (digitalRead(PB_CANCEL) == LOW){
        delay(200); // Delay to debounce the button

        // Two break for break each for loop and while loop
        break_happen=true;
        break;
      }

      // Play the i th note on the buzzer for 500ms and turn off
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(2);
    }
  }

  // Turn off the LED and clear the display
  digitalWrite(LED, LOW);
  display.clearDisplay();
}

// Function to navigate to the menu
void go_to_menu(void) {

  // Continue displaying the menu while the cancel button is not pressed
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    // Print the current menu option
    print_line(options[current_mode], 2, 0, 0);

    // Wait for a button press and store the pressed button
    int pressed = wait_for_button_press();
    
    // If the up button is pressed
    if (pressed == PB_UP) {

      // Move to the next menu option and wrap around if exceeded the max
      current_mode += 1;
      current_mode %= max_modes;
      delay(200); // Delay to debounce the button

    }

    // If the OK button is pressed exucute selected mode
    else if (pressed == PB_OK) {
      delay(200);
      run_mode(current_mode);
    }

    // If the down button is pressed
    else if (pressed ==PB_DOWN) {
      delay(200);
      // Move to the previous menu option and wrap around if reaching below zero
      current_mode -= 1;
      if (current_mode < 0) {
      current_mode = max_modes - 1;
      }
    }

    // If the cancel button is pressed
    else if (pressed == PB_CANCEL) {
      delay(200);
      // Exit the menu
      break;
      }
  }
}

// Function to wait for a button press and return the pressed button
int wait_for_button_press() {

  // Loop indefinitely until a button press is detected
  while (true) {

    // Check if the up, down, ok or cancel button is pressed then return relavant button code
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    }
    else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    }
    else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
    else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    }

    // Update the time to ensure accurate time display while waiting for button press
    update_time();
 }
}

// Function to execute the selected mode
void run_mode(int mode) {
  // Check the selected mode and execute corresponding actions

  // Set the time if mode = 0
  if (mode == 0) {
    set_time();
  }

  // Set alarm based on the mode index if (2 <= mode <=3)
  else if (mode == 1 || mode == 2||mode == 3) {
    set_alarm(mode - 1);
  }

  // Disable all alarms if mode = 4
  else if (mode == 4) {
    alarm_enabled = false;
  }
}

// Mode functions
// Mode 0: Set Time
// Mode 1,2,3: Set Alarm

// Function to set the device time offset - Mode 0: Set Time
void set_time() {

  // Initialize variables to store offset hours and minutes
  int hour_init = hours;
  int minute_init = minutes;

  // While loop to set offset hours
  while (true) {
    display.clearDisplay();
    print_line("Enter offset hours: " + String(hour_init), 0, 0, 2);

    // Wait for a button press and store the pressed button
    int pressed = wait_for_button_press();

    // If the up button is pressed, increment the hour offset
    if (pressed == PB_UP) {
      delay(200);
      hour_init += 1;
      hour_init = hour_init % 24; // Ensure hour remains within 0-23 range
    }

    // If the down button is pressed, decrement the hour offset
    else if (pressed == PB_DOWN) {
      delay(200);
      hour_init -= 1;
      if (hour_init < 0) {
      hour_init = 23; // Wrap around to 23 when hour goes below 0
      }
    }

    // If the OK or cancel button is pressed, exit the loop
    else if (pressed == PB_OK) {
      delay(200);
      break;
    }
    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }
  
  // While loop to set offset minutes
  while (true) {
    display.clearDisplay();
    print_line("Enter offset minute: " + String(minute_init), 0, 0, 2);

    // Wait for a button press and store the pressed button
    int pressed = wait_for_button_press();

    // If the up button is pressed, increment the minute offse
    if (pressed == PB_UP) {
      delay(200);
      minute_init += 1;
      minute_init = minute_init % 60; // Ensure minute remains within 0-59 range
    }

    // If the down button is pressed, decrement the minute offset
    else if (pressed == PB_DOWN) {
      delay(200);
      minute_init -= 1;
      if (minute_init < 0) {
        minute_init = 59; // Wrap around to 59 when minute goes below 0
      }
    }

    // If the OK or cancel button is pressed, exit the loop
    else if (pressed == PB_OK) {
      delay(200);
      break;
    }
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  // Calculate the UTC offset in seconds
  UTC_OFFSET = hour_init * 3600 + minute_init * 60;
  // Configure the time with the calculated UTC offset
  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
  
  // Clear the display and print a confirmation message
  display.clearDisplay();
  print_line("Time is set", 0, 0, 2);
  delay(1000);

}

// Function to set the specified alarm (Alarm 1, Alarm 2, or Alarm 3) - Mode 1,2,3: Set Alarm
void set_alarm(int alarm) {

  // Initialize temporary variables to store the hour and minute of the alarm
  int temp_hour = alarm_hours[alarm];

  // While loop to set hour for alarm
  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 0, 0, 2);

    // Wait for a button press and store the pressed button
    int pressed = wait_for_button_press();

    // According to the up or down button is pressed, increment or decrement the hour
    if (pressed == PB_UP) {
      delay(200);
      temp_hour += 1;
      temp_hour = temp_hour % 24;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_hour -= 1;
      if (temp_hour < 0) {
      temp_hour = 23;
      }
    }

    // If the OK button is pressed, update the alarm hour and exit the loop
    else if (pressed == PB_OK) {
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }

    // If the cancel button is pressed, exit the loop without updating the alarm
    else if (pressed == PB_CANCEL){
      delay(200);
      break;
    }
  }

  // Initialize temporary variables to store the hour and minute of the alarm
  int temp_minute = alarm_minutes[alarm];

  // While loop to set minute for alarm
  while (true) {
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute), 0, 0, 2);

    // Wait for a button press and store the pressed button
    int pressed = wait_for_button_press();

    // According to the up or down button is pressed, increment or decrement the minute
    if (pressed == PB_UP) {
      delay(200);
      temp_minute += 1;
      temp_minute = temp_minute % 60;
    }

    else if (pressed == PB_DOWN) {
      delay(200);
      temp_minute -= 1;
      if (temp_minute < 0) {
        temp_minute = 59;
      }
    }

    // If the OK button is pressed, update the alarm minute and exit the loop
    else if (pressed == PB_OK) {
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }

    // If the cancel button is pressed, exit the loop without updating the alarm
    else if (pressed == PB_CANCEL) {
      delay(200);
      break;
    }
  }

  // Display a confirmation message after setting the alarm
  display.clearDisplay();
  print_line("Alarm is set", 0, 0, 2);
  delay(1000);
}

// Function to check temperature and humidity levels
float check_temp(void) {

    // Get temperature and humidity data from the sensor
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    
    // Display temperature status whether it is high or low
    if (data.temperature > 32) {
        display.clearDisplay();
        print_line("TEMP HIGH", 0, 40, 1);
    } else if (data.temperature < 26) {
        display.clearDisplay();
        print_line("TEMP LOW", 0, 40, 1);
    }

    // Display humidity status whether it is high or low
    if (data.humidity > 80) {
        display.clearDisplay();
        print_line("HUMD HIGH", 0, 50, 1);
    } else if (data.humidity < 60) {
        display.clearDisplay();
        print_line("HUMD LOW", 0, 50, 1);
    }
    float temperature = data.temperature;
    return temperature;
}

// Function to turn the servo motor to a specified angle
void turn_servo_motor(int angle)
{
    // Check if the desired angle is different from the current angle
    if (servo_angle != angle)
    {
        // Write the new angle to the servo motor
        servo_motor.write(angle); // Servo motor can only turn between 0 and 180 degrees.

        // Add a small delay to allow the servo motor to reach the desired position
        delay(20);

        // Print a message indicating the angle by which the slider has turned
        Serial.println("Slider turned by an angle of: " + String(angle) + " degrees.");

        // Update the current angle of the servo motor
        servo_angle = angle;
    }
    else
    {
        // Print a message indicating that the slider is already at the desired angle
        Serial.println("Slider already at the desired angle.");
    }
}


// Function to establish connection with MQTT broker
// Keeps attempting connection until successful
void brokerConnectMQTT()
{
    // Keep attempting to connect to the MQTT broker until successful
    while (!mqttClient.connected())
    {
        // Print a message indicating the attempt to connect
        Serial.println("Attempting MQTT connection...");

        // Try to connect to the MQTT broker with the device ID
        if (mqttClient.connect(MQTT_DEVICE_ID))
        {
            // If connection is successful, print a success message
            Serial.println("connected");

            // Subscribe to the topic for receiving data
            mqttClient.subscribe(DATA_RECEPTION_TOPIC);
        }
        else
        {
            // If connection fails, print an error message with the state code
            Serial.print("failed with state ");
            Serial.print(mqttClient.state());

            // Wait for a short period before retrying
            delay(5000);
        }
    }

    // Keep the MQTT client loop running
    mqttClient.loop();
}

//Callback function for handling received MQTT data
void dataReceptionCallback(char *topic, byte *message, unsigned int length)
{
    // Print the received message topic
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");

    // Initialize a String to store the received data
    String degree;

    // Loop through the received message and append each byte to the String
    for (int i = 0; i < length; i++)
    {
        degree += (char)message[i]; // Convert byte to char and append to the String
    }

    // Print the received data for debugging purposes
    Serial.println(degree);

    // Check if the received topic matches the expected topic
    if (strcmp(topic, DATA_RECEPTION_TOPIC) == 0)
    {
        // If the topic matches, convert the received data to an integer and pass it to the function turn_servo_motor
        turn_servo_motor(degree.toInt());
    }
    else
    {
        // If the topic doesn't match, print an error message
        Serial.println("Invalid command");
    }
}


// Calculates the luminance based on the sensor reading
float getLuminance(float sensorReading)
{
    float lux;

    // Check if linear mapping is enabled
    if (LINEAR_MAPPING)
    {
        // Calculate lux using linear mapping formula
        lux = sensorReading / FINITE_INFINITY;

        // Check if lux exceeds the defined constant FINITE_INFINITY
        if (lux >= FINITE_INFINITY)
            return 1; // Return 1 if lux is greater than or equal to FINITE_INFINITY

        // Return the calculated lux value
        return lux;
    }
    else
    {
        // Calculate voltage based on sensor reading and VCC
        float voltage = sensorReading / 4096.0 * VCC;

        // Calculate resistance based on voltage and known resistor value
        float resistance = RESISTOR * voltage / (VCC - voltage);

        // Calculate lux using non-linear mapping formula
        lux = pow(RL10 * 1000 * pow(10, GAMMA) / resistance, (1 / GAMMA));

        // Check if lux exceeds the defined constant FINITE_INFINITY
        if (lux >= FINITE_INFINITY)
            return 1; // Return 1 if lux is greater than or equal to FINITE_INFINITY

        // Normalize lux value and return
        return lux / FINITE_INFINITY;
    }
}


// Function to send MQTT data including temperature and LDR readings
void send_mqtt_data(float temperature, float humidity)
{
    // Buffer to hold JSON data
    char dataJson[100];

    // Read LDR sensor values and convert to lux
    float ldr1 = getLuminance(analogRead(LDR_1));
    float ldr2 = getLuminance(analogRead(LDR_2));

    // Check if the difference between current and previous readings exceeds threshold values
    if (fabs(data["LDR1"].as<float>() - ldr1) >= EPSILON || fabs(data["LDR2"].as<float>() - ldr2) >= EPSILON || fabs(data["Temperature"].as<float>() - temperature) >= TEMPSILON || fabs(data["Humidity"].as<float>() - humidity) >= HUMIDITY_EPSILON)
    {
        // Update JSON data with new sensor readings
        data["LDR1"] = ldr1;
        data["LDR2"] = ldr2;
        data["Temperature"] = temperature;
        data["Humidity"] = humidity;

        // Serialize JSON data to string
        serializeJson(data, dataJson);

        // Print JSON data for debugging
        Serial.println(dataJson);

        // If MQTT client is not connected, attempt to connect
        if (!mqttClient.connected())
        {
            brokerConnectMQTT();
        }

        // Publish JSON data to the MQTT topic
        mqttClient.publish(DATA_SENDING_TOPIC, dataJson);
    }
}


// Setup MQTT client
void setupMQTT()
{
    // Set MQTT server and port
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

    // Set callback function to handle received MQTT data
    mqttClient.setCallback(dataReceptionCallback);
}
