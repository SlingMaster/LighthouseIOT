/* r_action.ino */
#ifdef USE_IOT

// ======================================
void initRobotPin() {
  // INPUT_PULLUP
  pinMode(MB_LED_PIN, OUTPUT);
  pinMode(TOWER_LIGHT_PIN, OUTPUT);
  digitalWrite(MB_LED_PIN, HIGH);
  digitalWrite(TOWER_LIGHT_PIN, LOW);
  delay(1000);
}

// ======================================
void  prepareUpdate() {
  ONflag = true;
  changePower();
  FPSdelay = 50;
  SetEnvironmentState(9, 9, 9, false);
}

// ======================================
byte getSkyColorIndex() {
  byte index;
  time_t currentLocalTime = getCurrentLocalTime();
  byte hh = hour(currentLocalTime);

  if ((hh < 7) | (hh > 21)) {
    index = 0;
  } else {
    if (hh < 10) index = 1;
    if (hh > 17) index = 3;
    if ((hh >= 10) & (hh <= 17)) index = 2;
  }
  return index;
}

// ======================================
void lightControls() {
  if (!dawnFlag) {
    if (!ONflag) {
      for (uint8_t i = modes[currentMode].Brightness; i > 5; i = constrain(i - 8, 5, modes[currentMode].Brightness)) {
        FastLED.setBrightness(i);
        FastLED.delay(1);
      }
      // LOG.println("• Set Low Brightness");
    } else { /* default */
      setBrightnessLevel();
    }
  }
}

// ======================================
String FomatXX(int val) {
  if (val < 10) {
    return "0" + String(val);
  } else {
    return String(val);
  }
}

// ======================================
void ExternalMotionAction(char *inputBuffer) {
  uint8_t valid = 0, i = 0;
  uint8_t cmd;
  while (inputBuffer[i])   {   //пакет должен иметь вид IOT,%U,%U,%U,%U,%U соответственно ON/OFF, №эффекта,яркость,скорость,масштаб
    if (inputBuffer[i] == ',')  {
      valid++;  //Проверка на правильность пакета (по количеству запятых)
    }
    i++;
  }
  if (valid == 3)   {                                 //Если пакет правильный выделяем лексемы,разделённые запятыми, и присваиваем параметрам эффектов
    char *tmp = strtok (inputBuffer, ",");            //Первая лексема IOT пропускается
    tmp = strtok (NULL, ",");
    cmd = atoi(tmp);
    tmp = strtok (NULL, ",");
    light_sensor = atoi(tmp);
    tmp = strtok (NULL, ",");
    s_temp = atoi(tmp);

#ifdef GENERAL_DEBUG
    //    LOG.println ("\n\r  ===== INBOUND ==== |  SET  |");
    //    LOG.println       (" | CMD | NIGHT | T°C | DELAY |");
    //    LOG.printf_P(PSTR(" | %03d | %05d | %03d | %05d |\n\r"), cmd, light_sensor, s_temp, FPSdelay);
#endif //GENERAL_DEBUG

    if (!ONflag) {
      /* lamp on for 2 min */
      if (cmd == 1) {
        currentMode = EFF_FAV;
        smartLampOff(2);
      }
    }
  }
}

// ======================================
String getTimeStr() {
  time_t currentLocalTime = getCurrentLocalTime();
  byte hh = hour(currentLocalTime);
  byte mm = minute(currentLocalTime);
  byte ss = second(currentLocalTime);
  return FomatXX(hh) + ":" + FomatXX(mm) + ":" + FomatXX(ss);
}

// ======================================
String TimeStr(uint32_t data) {
  time_t currentLocalTime = data;
  byte hh = hour(currentLocalTime);
  byte mm = minute(currentLocalTime);
  byte ss = second(currentLocalTime);
  return FomatXX(hh) + ":" + FomatXX(mm) + ":" + FomatXX(ss);
}

// ======================================
void getForecast() {
  /* потрібно зареєструватись на сайті https://openweathermap.org/weather-conditions
     отримати API_KEY
     для Харкова рядок має такий вигляд  https://api.openweathermap.org/data/2.5/weather?q=Kharkiv,UA&lang=ua&units=metric&APPID= «API_KEY»
  */
  // https://openweathermap.org/weather-conditions

  // example |  https://randomnerdtutorials.com/decoding-and-encoding-json-with-arduino-or-esp8266/

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

  // Ignore SSL certificate validation
  client->setInsecure();

  //create an HTTPClient instance
  HTTPClient https;

  // Initializing an HTTPS communication using the secure client
  LOG.print("[HTTPS] begin...\n\r");
  String w_url = "https://api.openweathermap.org/data/2.5/weather?q=Kharkiv,UA&lang=ua&units=metric&APPID=" + API_KEY;
  if (https.begin(*client, w_url)) {  // HTTPS
    LOG.print("[HTTPS] GET...\n\r");
    // start connection and send HTTP header
    int httpCode = https.GET();
    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled

#ifdef GENERAL_DEBUG
      LOG.printf("[HTTPS] GET... code: %d\n\r", httpCode);
      LOG.print("HTTP_CODE_OK • "); LOG.println(HTTP_CODE_OK);
#endif //GENERAL_DEBUG

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https.getString();
        updateRealState(payload);
      }
    } else {
      LOG.printf("[HTTPS] GET... failed, error: %s\n\r", https.errorToString(httpCode).c_str());
    }

    https.end();
  } else {
    LOG.printf("[HTTPS] Unable to connect\n\r");
  }
}

// ======================================
void updateRealState(String payload) {
  const uint16_t m30 = 2700;
  uint32_t tt;
  uint32_t curTime = getCurrentLocalTime();
  /*
    2 • Thunderstorm
    3 • Drizzle
    5 • Rain
    6 • Snow
    7 • Atmosphere Fog
    8 • Clear
  */

  // Parse JSON object ----
  StaticJsonDocument<1024> doc;
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    LOG.print(F("deserializeJson() failed: "));
    LOG.println(error.f_str());
    return;
  }

  weather_id = doc["weather"][0]["id"];
  String icon = doc["weather"][0]["icon"];
  byte ico_id = icon.substring(0, 2).toInt();
  uint32_t dt = doc["dt"];
  uint16_t timezone = doc["timezone"];
  tt =  doc["sys"]["sunrise"];
  uint32_t sunrise = tt + timezone;
  tt =  doc["sys"]["sunset"];
  uint32_t sunset = tt + timezone;

  byte id_group = floor(weather_id / 100);
  switch (id_group) {
    case 2: /* Thunderstorm */
      clouds = 6;
      break;
    case 3: /* Drizzle */
      clouds = 4;
      break;
    case 5: /* Rain */
      clouds = (weather_id > 505) ? 4 : 2;
      break;
    case 6: /* Snow */
      clouds = (weather_id > 605) ? 4 : 2;
      break;
    case 7: /* Fog */
      clouds = 5;
      break;
    case 8: /* clouds */
      clouds = weather_id % 800;
      break;
    default: /* clear */
      clouds = 0;
      break;
  }

  if (curTime < (sunrise - m30) | curTime > (sunset + m30))   tod = 0;
  if (curTime > (sunrise + m30) & curTime < (sunset - m30))   tod = 2;
  if (curTime > (sunrise - m30) & curTime < (sunrise + m30))  tod = 1;
  if (curTime > (sunset - m30) & curTime < (sunset + m30))    tod = 3;

#ifdef GENERAL_DEBUG
  String todValue[4] = {"Night", "Sunrise", "Day", "Sunset"};
  LOG.print("\n\r            • openweathermap.org • ico_id "); LOG.println(ico_id);
  LOG.println("   TIME  |    ID    | SUNRIZE  |  SUNSET  | TOD |");
  LOG.print(TimeStr(dt)); LOG.printf(" |    %3d  ", weather_id);
  LOG.print(" | "); LOG.print(TimeStr(sunrise));
  LOG.print(" | "); LOG.print(TimeStr(sunset));  LOG.print(" | "); LOG.println(todValue[tod]);
#endif //GENERAL_DEBUG

  /* select sounds folder */
  cur_folder = (currentMode == (MODE_AMOUNT - 1)) ? GetSoundSource() : currentMode + 1;
#ifdef GENERAL_DEBUG
  LOG.print("   Night | " + TimeStr(curTime) + " | " + TimeStr(sunrise - m30) + " | " + TimeStr(sunset + m30) + " | ");
  LOG.println( (curTime < (sunrise - m30) | curTime > (sunset + m30)) );

  LOG.print(" Sunrise | " + TimeStr(curTime) + " | " + TimeStr(sunrise - m30) + " | " + TimeStr(sunrise + m30) + " | ");
  LOG.println( (curTime > (sunrise - m30) & curTime < (sunrise + m30)) );

  LOG.print("     Day | " + TimeStr(curTime) + " | " + TimeStr(sunrise - m30) + " | " + TimeStr(sunset - m30) + " | ");
  LOG.println( (curTime > (sunrise + m30) & curTime < (sunset - m30)) );

  LOG.print("  Sunset | " + TimeStr(curTime) + " | " + TimeStr(sunset - m30) + " | " + TimeStr(sunset + m30) + " | ");
  LOG.println(  (curTime > (sunset - m30)) & (curTime < (sunset + m30) ) );
  LOG.println("");
#endif //GENERAL_DEBUG
}

// ======================================
void UpdateState() {
  if (!dawnFlag) {
    time_t currentLocalTime = getCurrentLocalTime();
    byte hh = hour(currentLocalTime);
    byte mm = minute(currentLocalTime);
    byte ss = second(currentLocalTime);

#ifdef GENERAL_DEBUG
    if (ss == 0) {
      LOG.println(" • " + TimeStr(currentLocalTime) + "  | Update State |");
      LOG.println(" | ID. | TOD | CLOUDS |");
      LOG.printf(" | %3d |%3d  |%4d    |\n\r", weather_id, tod, clouds);
    }
#endif //GENERAL_DEBUG

    if (mm == 59 & ss == 0) getForecast();
    if (mm == 0 & ss == 0) SoundBell();
    if (hh == 12 & mm == 0 & ss == 10) {
      currentMode = 6;
      if (ONflag) {
        runEffect();
      } else {
        ONflag = true;
        runEffect();
        smartLampOff(20);
      }
    }
  }
}

// ======================================
// END
// ======================================

#endif
