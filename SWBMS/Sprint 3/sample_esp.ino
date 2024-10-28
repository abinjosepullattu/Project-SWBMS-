void setup() {
   Serial.begin(9600);

}

void loop() {
  char buffer[20]="";
  if(Serial.available()>0){


    Serial.readBytesUntil('\n',buffer,20);
    Serial.println(buffer);
  }

}
