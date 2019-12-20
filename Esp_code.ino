#include"WiFi.h"
#include"HTTPClient.h"
#include"ArduinoJson.h"
#include"ThingSpeak.h"
#include<Wire.h>
#include<Arduino.h>

char*wifi_ssid="esw-m19@iiith";
char*wifi_pwd="e5W-eMai@3!20hOct";


//Set web server port number to 80
WiFiServer server(80);

//Variable to store the HTTP request
String header;

//Decode HTTP GET value
String valueString = String(5);
int pos1 = 0;
int pos2 = 0;

String cse_ip = "onem2m.iiit.ac.in";

//StaticJsonBuffer<200>jsonBuffer;
//JsonObject&user_data=jsonBuffer.createObject();
//JsonObject&temp_user_data=jsonBuffer.createObject();
//JsonObject&sensor_data=jsonBuffer.createObject();

//Onemtom
String cse_port = "443";
String om2mserver = "http://" + cse_ip + ":" + cse_port + "/~/in-cse/in-name/";

//Thingspeak
const char * myWriteAPIKey = "NK3AMTK25MUQDCP7";
const char * server_t = "api.thingspeak.com";
unsigned long myChannelNumber = 906378;
String myStatus = "";

WiFiClient client;
unsigned long myTalkBackID = 36107;
const char * myTalkBackKey = "BQG33AKQC4K3NSOE";

//********FUNCTIONSUSED**********

String createCI(String om2mserver, String ae, String cnt, String val);
void distcontroller();
int httpPOST(String uri, String postMessage, String & response, long d);
long dist(long trigPin, long echoPin);
void thingspeak_post(long distance_1, long distance_2);
void onem2m_post(long distance_1, long distance_2);
void talkback(long d);

//************************

String createCI(String om2mserver, String ae, String cnt, String val) {
    HTTPClient http;
    http.begin(om2mserver + ae + "/" + cnt + "/");

    http.addHeader("X-M2M-Origin", "admin:admin");
    http.addHeader("Content-Type", "application/json;ty=4");
    http.addHeader("Content-Length", "100");
    http.addHeader("Connection", "close");
    int code = http.POST("{\"m2m:cin\":{\"cnf\":\"text/plain:0\",\"con\":" + String(val) + "}}");
    http.end();
    Serial.print(code);
    delay(300);
    //returnsomething
    return (String(code));
}

//definespinsnumbers
const long trigPin1 = 2;
const long echoPin1 = 5;
const long trigPin2 = 16;
const long echoPin2 = 17;

const long motorPin1 = 25;
const long motorPin2 = 26;
const long encoder_pin = 18;

//definesvariables
long duration, distance_1, distance_2;
long distance;
long p = 100;
int pulsesperturn = 1;
long pulses, rpm, timeold, timeold_post, timeold_m;
int typeflag = 4;
void IRAM_ATTR isr() {
    pulses++;
    //Serial.println(pulses);
}

void setup() {
    Serial.begin(115200);
    //Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    ThingSpeak.begin(client); //Initialize Thing Speak
    WiFi.disconnect();
    delay(100);

    WiFi.begin(wifi_ssid, wifi_pwd);
    timeold_post = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - timeold_post <= 10000) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Unable to connect to Wifi\n");
    } else {
        Serial.println("Connection successful!\n");
        //Print local IP address and start webserver
        Serial.println("");
        Serial.println("WiFi connected.");
        Serial.println("IP address:");
        Serial.println(WiFi.localIP());
        server.begin();
    }
    pinMode(trigPin1, OUTPUT); //Sets the trig Pin as an Output
    pinMode(echoPin1, INPUT); //Sets the echo Pin as an Input
    pinMode(trigPin2, OUTPUT); //Sets the trig Pin as an Output
    pinMode(echoPin2, INPUT); //Sets the echo Pin as an Input
    pinMode(motorPin1, OUTPUT);
    pinMode(motorPin2, OUTPUT);
    digitalWrite(motorPin1, LOW); //permanent
    pinMode(18, INPUT_PULLUP); //the interrupt pin
    attachInterrupt(18, isr, FALLING); //falling edge ie when there is hole
    pulses = 0; //initializing pulses
    rpm = 0; //intializing rpm
    timeold = millis(); //initailizng time
    timeold_post = millis(); //posting to thingspeak server
    timeold_m = millis(); //posting to om2m server

}

void distcontroller() {
    long ontime;
    long d = dist(trigPin1, echoPin1);
    distance_1 = d;
    long d2 = dist(trigPin2, echoPin2);
    distance_2 = d2;
    if (d2 < d) d = d2;

    if (typeflag == 4);
    else if (typeflag == 1)
        d = 0;
    else if (typeflag == 2)
        d = 30;
    else if (typeflag == 3)
        d = 100;

    if (d >= 60) {
        digitalWrite(motorPin2, HIGH);
        delay(p);
    } else if (d <= 10) {
        digitalWrite(motorPin2, LOW);
        delay(p);
    } else {
        int delay = (d - 10) * 2;
        digitalWrite(motorPin2, HIGH);
        delayMicroseconds(delay * 500);
        digitalWrite(motorPin2, LOW);
        int dd = p - delay;
        delayMicroseconds(dd * 10);
    }
}

//General function to POST to ThingSpeak
int httpPOST(String uri, String postMessage, String & response) {

    bool connectSuccess = false;
    connectSuccess = client.connect("api.thingspeak.com", 80);

    if (!connectSuccess) {
        return -301;
    }

    postMessage += "&headers=false";

    String Headers = String("POST") + uri + String("HTTP/1.1\r\n") +
        String("Host:api.thingspeak.com\r\n") +
        String("Content-Type:application/x-www-form-urlencoded\r\n") +
        String("Connection:close\r\n") +
        String("Content-Length:") + String(postMessage.length()) +
        String("\r\n\r\n");

    client.print(Headers);
    client.print(postMessage);

    long startWaitForResponseAt = millis();
    while (client.available() == 0 && millis() - startWaitForResponseAt < 5000) {
        distcontroller();
        delay(100);
    }

    if (client.available() == 0) {
        return -304; //Didn'tgetserverresponseintime
    }

    if (!client.find(const_cast < char * > ("HTTP/1.1"))) {
        return -303; //Couldn'tparseresponse(didn'tfindHTTP/1.1)
    }

    int status = client.parseInt();
    if (status != 200) {
        return status;
    }

    if (!client.find(const_cast < char * > ("\n\r\n"))) {
        return -303;
    }

    String tempString = String(client.readString());
    response = tempString;

    return status;

}

long dist(long trigPin, long echoPin) {
    //Clears the trig Pin
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH); //Sets the trigPin on HIGH state for 10 microseconds
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    //Reads the echoPin,returns the soundwave travel time in microseconds
    duration = pulseIn(echoPin, HIGH);
    //Calculating the distance
    distance = duration * 0.034 / 2;
    //Prints the distance on the Serial Monitor
    Serial.print("Distance:");
    Serial.println(distance);
    return distance;
}

void thingspeak_post(long distance_1, long distance_2) {
    Serial.println("Trying to post\n");
    timeold_post = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - timeold_post <= 10000) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Unable to connect to Wifi\n");
    } else {
        Serial.println("Connection successful:)\n");

        //set the fields with the valuesM
        ThingSpeak.setField(1, distance_1);
        ThingSpeak.setField(2, distance_2);
        ThingSpeak.setField(3, rpm);

        //write to the ThingSpeak channel
        int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
        if (x == 200) {
            Serial.println("Channel update successful.");
        } else {
            Serial.println("Problem updating channel.HTTP error code" + String(x));
        }

        //Updatingthetime
        timeold_post = millis();
    }
}

void onem2m_post(long distance_1, long distance_2) {
    Serial.println("Trying to post to OM2M\n");
    timeold_m = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - timeold_m <= 10000) {
        delay(500);
        Serial.println("Connecting to WiFi..");
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Unable to connect to Wifi\n");
    } else {
        Serial.println("Connection successful:)\n");

        //when single sensor gives multiple values
        String sensor1 = "(" + String(distance_1) + "," + String(distance_2) + ")";

        //when single sensor gives single value
        String sensor2 = String(rpm);

        //Make it a single string
        String sensor_string = sensor1 + "," + sensor2;

        //Make it OneM2M complaint
        sensor_string = "\"" + sensor_string + "\""; //DONOTCHANGETHISLINE

        //Send data to OneM2M server
        Serial.println(createCI(om2mserver, "Team16_IOT_Wireless_Control", "node_1", sensor_string));
        Serial.println("Posted to Onemtom\n");

        //Updatingthetime
        timeold_m = millis();
    }
}


void checkrequests() {
    WiFiClient client = server.available(); //Listen for incoming clients
    if (client) { //If a new client connects,
        Serial.println("NewClient."); //print a message out in the serial port
        String currentLine = ""; //make a String to hold incoming data from the client
        while (client.connected()) { //loop while the client's connected
            if (client.available()) { //if there's bytes to read from the client,
                char c = client.read(); //read a byte,then
                Serial.write(c); //print it out the serial monitor
                header += c;
//                Serial.println(header+"******************************************");
                if (c == '\n') { //if the byte is a new line character
                    //ifthecurrentlineisblank,yougottwonewlinecharactersinarow.
                    //that'stheendoftheclientHTTPrequest,sosendaresponse:
                    if (currentLine.length() == 0) {
                        //HTTPheadersalwaysstartwitharesponsecode(e.g.HTTP/1.1200OK)
                        //andacontent-typesotheclientknowswhat'scoming,thenablankline:
                        client.println("HTTP/1.1200OK");
                        client.println("Content-type:text/html");
                        client.println("Connection:close");
                        client.println();

                        //DisplaytheHTMLwebpage
                        client.println("<!DOCTYPE html><html lang=\"en\">");
                        client.println("<head><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1.0\"><meta http-equiv=\"X-UA-Compatible\" content=\"ie=edge\">");
                        client.println("<title>Document</title>");
                        //client.println("<link rel=\"icon\"href=\"data:,\">");
                        //CSStostyletheon/offbuttons
                        //Feelfreetothebackground-colorandfont-sizeattributestofityourpreferences
                        //client.println("<style>@importurl('https://fonts.googleapis.com/css?family=Raleway:300,400');body{background:#2D3142;font-family:'Raleway',sans-serif;}/*Heading*/h1{font-size:1.5em;text-align:center;padding:70px 0 0 0;color:#EF8354;font-weight:300;letter-spacing:1px;}span{/border:2pxsolid#4F5D75;/padding:10px;}#container{width:100%;margin:0auto;padding:50px 0 150px 0;display:flex;flex-direction:row;flex-wrap:wrap;justify-content:center;}/ButtonStyles/.button{display:inline-flex;height:40pxwidth:150px;border:2px solid #BFC0C0;margin:20px 20px 20px 20px;color:#BFC0C0;text-transform:uppercase;text-decoration:none;font-size:.8em;letter-spacing:1.5px;align-items:center;justify-content:center;overflow:hidden;}a{color:#BFC0C0;text-decoration:none;letter-spacing:1px;}/SixthButton/.button-6{position:relative;overflow:hidden;cursor:pointer;}.button-6 a {position:relative;transition:all .45s ease-Out;}.spin{width:0;height:0;opacity:0;left:70px;top:20px;transform:rotate(0deg);background:none;position:absolute;transition:all .5s ease-Out;}.button-6:hover .spin{width:200%;height:500%;opacity:1;left:-70px;top:-70px;background:#BFC0C0;transform:rotate(80deg);}.button-6:hover a{color:#2D3142;}.disp{background-color:#EF8354;margin:auto;width:25%;height:50px;border:3px solid #BFC0C0;text-align:center;padding:10px;}.displaytext{font-family:'Raleway',sans-serif;font-size:xx-large;text-align:center;}@media screen and (min-width:1000px){h1{font-size:2.2em;}#container{width:50%;}}</style>");
                        client.println("<style>");
                        client.println("@import url('https://fonts.googleapis.com/css?family=Raleway:300,400');");
                        client.println("body {");
                        client.println("  background: #2D3142;");
                        client.println("  font-family: 'Raleway', sans-serif;");
                        client.println("}");

                        client.println("/* Heading */");

                        client.println("h1 {");
                        client.println("  font-size: 1.5em;");
                        client.println("  text-align: center;");
                        client.println("  padding: 70px 0 0 0;");
                        client.println("  color: #EF8354;");
                        client.println("  font-weight: 300;");
                        client.println("  letter-spacing: 1px;");
                        client.println("}");

                        client.println("span {");
                        client.println("  /* border: 2px solid #4F5D75; */");
                        client.println("  padding: 10px;");
                        client.println("}");

                        client.println("/* Layout Styling */");

                        client.println("#container {");
                        client.println("  width: 100%;");
                        client.println("  margin: 0 auto;");
                        client.println("  padding: 50px 0 150px 0;");
                        client.println("  display: flex;");
                        client.println("  flex-direction: row;");
                        client.println("  flex-wrap: wrap;");
                        client.println("  justify-content: center;");
                        client.println("}");

                        client.println("/* Button Styles */");

                        client.println(".button {");
                        client.println("  display: inline-flex;");
                        client.println("  height: 40px;");
                        client.println("  width: 150px;");
                        client.println("  border: 2px solid #BFC0C0;");
                        client.println("  margin: 20px 20px 20px 20px;");
                        client.println("  color: #BFC0C0;");
                        client.println("  text-transform: uppercase;");
                        client.println("  text-decoration: none;");
                        client.println("  font-size: .8em;");
                        client.println("  letter-spacing: 1.5px;");
                        client.println("  align-items: center;");
                        client.println("  justify-content: center;");
                        client.println("  overflow: hidden;");
                        client.println("}");

                        client.println("a {");
                        client.println("  color: #BFC0C0;");
                        client.println("  text-decoration: none;");
                        client.println("  letter-spacing: 1px;");
                        client.println("}");

                        client.println("/* Sixth Button */");

                        client.println(".button-6 {");
                        client.println("  position: relative;");
                        client.println("  overflow: hidden;");
                        client.println("  cursor: pointer;");
                        client.println("}");

                        client.println(".button-6 a {");
                        client.println("  position: relative;");
                        client.println("  transition: all .45s ease-Out;");
                        client.println("}");

                        client.println(".spin {");
                        client.println("  width: 0;");
                        client.println("  height: 0;");
                        client.println("  opacity: 0;");
                        client.println("  left: 70px;");
                        client.println("  top: 20px;");
                        client.println("  transform: rotate(0deg);");
                        client.println("  background: none;");
                        client.println("  position: absolute;");
                        client.println("  transition: all .5s ease-Out;");
                        client.println("}");

                        client.println(".button-6:hover .spin {");
                        client.println("  width: 200%;");
                        client.println("  height: 500%;");
                        client.println("  opacity: 1;");
                        client.println("  left: -70px;");
                        client.println("  top: -70px;");
                        client.println("  background: #BFC0C0;");
                        client.println("  transform: rotate(80deg);");
                        client.println("}");

                        client.println(".button-6:hover a {");
                        client.println("  color: #2D3142;");
                        client.println("}");

                        client.println(".disp");
                        client.println("{");
                        client.println("    background-color: #EF8354;");
                        client.println("    margin: auto;");
                        client.println("    width: 25%;");
                        client.println("    height: 50px;");
                        client.println("    border: 3px solid #BFC0C0;");
                        client.println("    text-align: center;");
                        client.println("    padding: 10px;");

                        client.println("}");

                        client.println(".displaytext");
                        client.println("{");
                        client.println("    font-family: 'Raleway', sans-serif;");
                        client.println("    font-size: xx-large;");
                        client.println("    text-align: center;");
                        client.println("}");

                        client.println("@media screen and (min-width:1000px) {");
                        client.println("  h1 {");
                        client.println("    font-size: 2.2em;");
                        client.println("  }");
                        client.println("  #container {");
                        client.println("    width: 50%;");
                        client.println("  }");
                        client.println("}");
                        client.println("</style>");
                        client.println("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script>");
                        //WebPage
                        client.println("</head><body><h1><b><span>IOT </span>CONTROLLING MOTOR</b></h1><!--FlexContainer--><div id=\"container\"><div class=\"button button-6\" value=\"STOP\" onclick=\"change(this)\"><div class=\"spin\"></div><a href=\"#\">STOP</a></div><div class=\"button button-6\" value=\"SLOW\" onclick=\"change(this)\"><div class=\"spin\"></div><a href=\"#\">SLOW</a></div><div class=\"button button-6\" value=\"FAST\" onclick=\"change(this)\"><div class=\"spin\"></div><a href=\"#\">FAST</a></div><div class=\"button button-6\" value=\"GO\" onclick=\"change(this)\"><div class=\"spin\"></div><a href=\"#\">GO</a></div><!--EndContainer--></div><div id=\"display\" class=\"disp\"></div>");
                        //client.println("<script>function change(x){console.log(x.getAttribute('value'));var=x.getAttribute('value');if(x.getAttribute('value')==\"STOP\"){document.getElementById(\"display\").innerHTML=\"STOP\"; document.getElementById('display').classList.add(\"displaytext\");}if(x.getAttribute('value')==\"SLOW\"){document.getElementById(\"display\").innerHTML=\"SLOW\"; document.getElementById('display').classList.add(\"displaytext\");}if(x.getAttribute('value')==\"FAST\"){document.getElementById(\"display\").innerHTML=\"FAST\"; document.getElementById('display').classList.add(\"displaytext\");}if(x.getAttribute('value')==\"GO\"){document.getElementById(\"display\").innerHTML=\"GO\"; document.getElementById('display').classList.add(\"displaytext\");}}");
                        client.println("<script>");
                        client.println("function change(x)");
                        client.println("{   ");
                        client.println("    console.log(x.getAttribute('value'));");
                        client.println("    if(x.getAttribute('value')==\"STOP\")");
                        client.println("    {");
                        client.println("        document.getElementById(\"display\").innerHTML=\"STOP\"");
                        client.println("        document.getElementById('display').classList.add(\"displaytext\");");

                        client.println("    }");
                        client.println("    if(x.getAttribute('value')==\"SLOW\")");
                        client.println("    {");
                        client.println("        document.getElementById(\"display\").innerHTML=\"SLOW\"");
                        client.println("        document.getElementById('display').classList.add(\"displaytext\");");

                        client.println("    }");
                        client.println("    if(x.getAttribute('value')==\"FAST\")");
                        client.println("    {");
                        client.println("        document.getElementById(\"display\").innerHTML=\"FAST\"");
                        client.println("        document.getElementById('display').classList.add(\"displaytext\");");

                        client.println("    }");
                        client.println("    if(x.getAttribute('value')==\"GO\")");
                        client.println("    {");
                        client.println("        document.getElementById(\"display\").innerHTML=\"GO\"");
                        client.println("        document.getElementById('display').classList.add(\"displaytext\");");

                        client.println("    }");

                        client.println("$.ajaxSetup({timeout:1000});");
                         client.println("    console.log(x.getAttribute('value'));");
                        client.println("$.get(\"/?value=\"+x.getAttribute('value')+\"&\");{Connection:close};}</script>");

                        client.println("</body></html>");

                        //GET/?value=180&HTTP/1.1
                        Serial.println("DIDNT ENTER 777777777777777777777777777  "+ header);
                        if (header.indexOf("GET /?value=") >= 0) {
                            pos1 = header.indexOf('=');
                            pos2 = header.indexOf('&');
                            valueString = header.substring(pos1 + 1, pos2);

                            Serial.println(valueString);
                            if (valueString == "STOP") {
                                typeflag = 1;
                            }

                            if (valueString == "SLOW") {
                                typeflag = 2;
                            }
                            if (valueString == "FAST") {
                                typeflag = 3;
                            }
                            if (valueString == "GO") {
                                typeflag = 4;
                            }

                            Serial.println(valueString);
                            Serial.println("***************************************************\n");
                        }
                        //The HTTP response ends with another blank line
                        client.println();
                        //Break out of the while loop
                        break;
                    } else { //if you got a new line, then clear current Line
                        currentLine = "";
                    }
                } else if (c != '\r') { //if you got anything else but a carriage return character,
                    currentLine += c; //add it to the end of the current Line
                }
            }
        }
        //Clear the header variable
        header = "";
        //Close the connection
        client.stop();
        Serial.println("Client disconnected.");
        Serial.println("");
    }
}

void loop() {

    //creating the PWM
    checkrequests();
    distcontroller();
    if (millis() - timeold >= 5000) {
        rpm = ((60 * 1000 / pulsesperturn) / (millis() - timeold) * pulses) / 36;
        rpm /= 20;
        Serial.print("--------------------------------------------------------------RPM=");
        pulses = 0;
        Serial.println(rpm);
        timeold = millis();
    }

    if (millis() - timeold_post >= 60000) {
        thingspeak_post(distance_1, distance_2);
    }
    if (millis() - timeold_m >= 300000) {
        onem2m_post(distance_1, distance_2);
    }
}
