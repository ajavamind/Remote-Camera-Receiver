
/*
  ESP8266 Blink by Simon Peter
  Blink the blue LED on the ESP-01 module
  This example code is in the public domain

  The blue LED on the ESP-01 module is connected to GPIO1
  (which is also the TXD pin; so we cannot use Serial.print() at the same time)

  Note that this sketch uses LED_BUILTIN to find the pin with the internal LED
*/

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
}

// the loop function runs over and over again forever
void loop() {
  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
  // but actually the LED is on; this is because
  // it is active low on the ESP-01)
  Serial.println("LED ON");
  delay(1000);                      // Wait for a second
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  Serial.println("LED OFF");
  delay(1000);                      // Wait for two seconds (to demonstrate the active low LED)
  test();
}

void test() {
    Serial.printf("LED GPIO=%d\n", LED_BUILTIN);
    Serial.printf("D0 GPIO=%d\n", D0);
    Serial.printf("D1 GPIO=%d\n", D1);
    Serial.printf("D2 GPIO=%d\n", D2);
    Serial.printf("D3 GPIO=%d\n", D3);
    Serial.printf("D4 GPIO=%d\n", D4);
    Serial.printf("D5 GPIO=%d\n", D5);
    Serial.printf("D6 GPIO=%d\n", D6);
    Serial.printf("D7 GPIO=%d\n", D7);
    Serial.printf("D8 GPIO=%d\n", D8);
}
