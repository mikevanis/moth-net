struct_message msg;
struct_message outgoingMsg;
esp_now_peer_info_t peerInfo;

int savedWifiChannel;

// On data received
void onDataReceive(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {

  //Add CRC
  if (len == sizeof(msg)) {
    memcpy(&msg, incomingData, sizeof(msg));
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    Serial.print("Received from: ");
    Serial.println(macStr);
    Serial.print("Bytes received: ");
    Serial.println(len);
    Serial.print("Function type: ");
    Serial.println(msg.function);
    if (msg.function == heartbeat) {
      Serial.println("HeartBeat!");
      saveJSON(macStr, msg.sensors);
      //  if (heartbeatReqExpected && millis() - heatbeatReqMillis < HEARTBEATREQ_TIMEOUT) {
      //add mac to online list
      //  } else {
      //   heartbeatReqExpected = false;
      //send online list to UI
      //  }
      setDeviceToConnected(macStr);
    } else if (msg.function == sensor) {
      publishSensorData(macStr, msg.sensors, msg.eventVal);
    }
  }
}

void publishSensorData(String mac, sensorTypes sensors , int event) {
  Serial.println("got sensor information");
  Serial.println(sensors);
  String aio_message = "{\"value\": " + String(event) + ",\"lat\": 51.516651, \"lon\": -0.076840}";
  String publishOut = getFeedByMac(mac);
  if (publishOut != "") {
    //should just do this when you receive it from the interface
    insertFeed(publishOut);
    client.publish((getAIOUser() + String("/f/") + publishOut).c_str(), aio_message.c_str());
  } else {
    Serial.println("no matching mac");
  }
}

void singleChannelESPNOWStartup() {
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_del_peer(broadcastAddress);
  }

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  sendESPNowChannel(getWifiChannel());
}

void multiChannelESPNOWStartup() {
  for (int i = 1 ; i < 12; i++) {
    esp_wifi_set_channel(i, WIFI_SECOND_CHAN_NONE);

    if (esp_now_is_peer_exist(broadcastAddress)) {
      esp_now_del_peer(broadcastAddress);
    }

    // Register peer
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = i;
    peerInfo.encrypt = false;

    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
      return;
    }
    sendESPNowChannel(getWifiChannel());
  }
}

void initESPNOW() {

  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init Success");
  }
  else {
    Serial.println("ESPNow Init Failed");
    //ESP.restart();
  }
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  esp_now_register_recv_cb(onDataReceive);
  esp_now_register_send_cb(OnDataSent);

  multiChannelESPNOWStartup();
  //singleChannelESPNOWStartup();

  esp_wifi_set_channel(WiFi.channel(), WIFI_SECOND_CHAN_NONE);

  if (esp_now_is_peer_exist(broadcastAddress)) {
    esp_now_del_peer(broadcastAddress);
    Serial.println("deleting old peer");
  }

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = getWifiChannel();
  peerInfo.encrypt = false;

  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void setWifiChannel(int wifi) {
  savedWifiChannel = wifi;
}

int getWifiChannel() {
  return savedWifiChannel;
}

void sendBlinkTo(int index) {
  getMacArray(index);
  memcpy(outgoingMsg.mac , macToSend, sizeof(macToSend[0]) * 6);
  outgoingMsg.function = blinking;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &outgoingMsg, sizeof(outgoingMsg));
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
}

void sendSensorTo(int index, int type, int val) {
  getMacArray(index);
  memcpy(outgoingMsg.mac , macToSend, sizeof(macToSend[0]) * 6);
  outgoingMsg.function = action;
  outgoingMsg.sensors = (sensorTypes)type;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &outgoingMsg, sizeof(outgoingMsg));
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
}

void sendBlink() {
  // Send message via ESP-NOW
  outgoingMsg.function = blinking;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &outgoingMsg, sizeof(outgoingMsg));

  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
}

String success;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if (status == 0) {
    success = "Delivery Success :)";
  }
  else {
    success = "Delivery Fail :(";
  }
}

void sendESPNowChannel(int channel) {
  outgoingMsg.function = wifiChannel;
  outgoingMsg.eventVal = channel;
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &outgoingMsg, sizeof(outgoingMsg));

  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
}

unsigned long prevHeartbeat;
#define HEARTBEATFREQ 10000
#define HEARTBEATREQ_TIMEOUT 400
unsigned long heartbeatReqMillis;
bool heartbeatReqExpected = false;

void heartBeatHandler() {
  if (millis() - prevHeartbeat > HEARTBEATFREQ) {
    Serial.println("Sending heartbeat REQ");
    prevHeartbeat = millis();
    setAllToUnconnected();
    sendHeartbeatREQ();
    heartbeatReqMillis = millis();
    heartbeatReqExpected = true;

  }
}

void sendHeartbeatREQ() {
  outgoingMsg.function = heartbeatREQ;
  memcpy(outgoingMsg.mac , broadcastAddress, sizeof(broadcastAddress[0]) * 6);
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &outgoingMsg, sizeof(outgoingMsg));
  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
}
