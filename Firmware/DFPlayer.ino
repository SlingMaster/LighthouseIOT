// ======================================
// DFPlayer.ino
// http://educ8s.tv/arduino-mp3-player/
// https://www.youtube.com/watch?v=H2SjPV5ugqw
// ======================================

#ifdef MP3_TX_PIN

#ifdef DF_PLAYER_IS_ORIGINAL
#define ADVERT_TIMER_1 800UL        // Затримка між командою старт та адверт (колі озвучка не іграе)
#define ADVERT_TIMER_2 700UL        // Затримка між проізнесенням хвилин та командою стоп (колі озвучка не іграе)
#define MP3_DELAY 500               // 500 | Затримка прі налаштуванні картки або флешки
#else
#define ADVERT_TIMER_1 1000UL       // Затримка між командою старт та адверт (колі озвучка не іграе)
#define ADVERT_TIMER_2 2000UL       // Затримка між проізнесенням хвилин та командою стоп (колі озвучка не іграе)
#define MP3_DELAY 2000              // Затримка прі налаштуванні картки або флешки
#endif

#define MP3_READ_TIMEOUT  (500UL)
/*
  // При наступлении ночи NIGHT_HOURS_START MP3 переходит на ночной режим
  // Воиспроиведение времени используем метод "ADVERT" или объявление

  // Переменные, которые были использованиы в модуле для анализа
  // dawnFlag - Идет рассвет
  // dawnPosition - Яркость рассвета
  // ONflag - включена/выключена лампа
  7E FF 06 06 00 00 0F FF D5 EF
*/
void PresetMP3() {
  /* Equalizer = 1 */
  send_command(0x07, FEEDBACK, 0, Equalizer);                        // Устанавливаем эквалайзер в положение Equalizer
  delay(mp3_delay);
  // Устанавливаем громкость равной eff_volume (от 0 до 30)
  SetPlayerVolume(volume_cycle);
}

// ======================================
void mp3_setup()   {
  int16_t tmp;
  if ( first_entry == 5 ) {
    first_entry = 0;
    send_command(0x0C, FEEDBACK, 0, 0); //Сброс модуля
    delay(mp3_delay);
    mp3_timer = millis();
#ifdef GENERAL_DEBUG
    LOG.println(F("\n MP3 Reset "));
#endif
    mp3_player_connect = 2;
    return;
  }
  if (mp3_receive_buf[3] == 0x3F) tmp = mp3_receive_buf[6];
  else tmp = -1;
#ifdef GENERAL_DEBUG
  LOG.print (F("\n\rMP3 Player  Statatus • "));  LOG.print(tmp); LOG.print(" | ");
#endif
#ifdef DF_PLAYER_GD3200x
  delay(mp3_delay * 5);
#else
  delay(mp3_delay);
#endif
  send_command(0x06, FEEDBACK, 0, 0);             // Устанавливаем громкость равной 0 (от 0 до 30)
  delay(mp3_delay);
#ifndef CHECK_MP3_CONNECTION
  if (tmp == -1) tmp = 0;                         // Не проверяем, есть ли связь с МР3 плеером
#endif  //CHECK_MP3_CONNECTION
  if (tmp != -1) {                                // Проверяем, есть ли связь с плеером и, если есть, то...
#ifdef DF_PLAYER_IS_ORIGINAL
    if (tmp == 1 || tmp == 3) {
      send_command(0x09, FEEDBACK, 0, 1);         // Устанавливаем источником Flash
    }
    if (tmp == 2) {
      send_command(0x09, FEEDBACK, 0, 2);         // Устанавливаем источником SDкарту
    }
    delay(MP3_DELAY);
#endif

#ifdef DF_PLAYER_GD3200x
    Serial.println("\n Попереднє встановлення папки озвучування");
    //send_command(0x06,FEEDBACK,0,0);                     // Устанавливаем громкость равной 0
    send_command(0x17, FEEDBACK, 0, mp3_folder);         // Попереднє встановлення папки озвучування
    //send_command(0x0F,FEEDBACK,99,1);                    //Старт 1-й трек в 99-й папці (Тиша)
    delay(mp3_delay * 10);                                    // ----------????????????---------
    send_command(0x06, FEEDBACK, 0, 0);                  // Устанавливаем громкость равной 0
    delay(mp3_delay);

    send_command(0x0E, FEEDBACK, 0, 0);                  // Пауза Stop
    delay(mp3_delay);
#endif

    PresetMP3();
    mp3_player_connect = 4;
    delay(mp3_delay);
    LOG.print(F("\n\rMP3 Player Connect"));
    LOG.print(F(" | Statatus • ")); LOG.println(tmp);

    if (tmp == 2) LOG.println (F("Встановлено SD-картку\n"));
    if (tmp == 1 || tmp == 3) LOG.println (F("Встановлено Флешку\n"));
    if (tmp == 0) LOG.println (F("SD-картку або Флешку не встановлено\n"));

  }  else {
    LOG.println (F("\n\rSD-картка ( флешка ) не встановлена або МР3 плеєр не підключено\n"));
    mp3_player_connect = 0;
  }

  //  SoundMute();
}

// ======================================
void mp3_loop()   {
  if ((millis() - mp3_event_timer > mp3_delay)) {
    mp3_event_timer = millis();
    if (mp3_counter % 2U) {
      //    LOG.print("mp3_loop | isSoundPlay • ");
      //    LOG.println(isSoundPlay);
      if ( read_command(1) != -1) {
        if (advert_flag) {

          /* restore volume level and equalizer */
          advert_flag = false;
          scenario++;
          LOG.printf_P(PSTR("\n\r  • Stop Advert Play | Scenario %02d | Progress %03d |\n\r"), scenario, progress );

          if (isSoundPlay) {
            if (ONflag) {
              send_command(0x0D, FEEDBACK, 0, 0);  // play
              delay(mp3_delay);
              SetPlayerVolume(volume_cycle);
            }
          }
        } else {
          isSoundPlay = false;
          LOG.printf_P(PSTR("\n\r  • Stop Sound Play | Scenario %02d | Progress %03d |\n\r"), scenario, progress );
          SetPlayerVolume(0);
          if (!ONflag) StopSong();
        }
        LOG.println("-----------------------------------");
        LOG.printf_P(PSTR("  Scenario • %03d | FPSdelay • %03d |\n\r"), scenario, FPSdelay);
      }
      UpdateState();
    }
    /* volume level fade */
    FadeVolume();
    mp3_counter++;
  }
}

// ======================================
int16_t send_command(int8_t cmd, uint8_t feedback, uint8_t dat1, uint8_t dat2) {
  uint8_t mp3_send_buf[8] = {0x7E, 0xFF, 06, 0x06, 00, 00, 00, 0xEF};
  // send command MP3 player
  mp3_send_buf[3] = cmd;  // Команда
  mp3_send_buf[4] = feedback; // 0x00 = Без ответа, 0x01 = с ответом (подтверждением)
  mp3_send_buf[5] = dat1; // параметр 1
  mp3_send_buf[6] = dat2; // параметр 2

  for (uint8_t i = 0; i < 8; i++) {
    mp3.write(mp3_send_buf[i]);
    delay(3);
  }
#ifdef MP3_DEBUG
  LOG.println();
  LOG.print(F("mp3_sending:"));
  for (uint8_t i = 0; i < 8; i++) {
    LOG.print(mp3_send_buf[i], HEX);
    LOG.print(F(" "));
  }
  LOG.println();
#endif  //MP3_DEBUG

  if (!feedback && (cmd < 0x30)) {
    return 0xFF00;
  }
  else if ( feedback && (cmd < 0x30)) {
    return read_command (MP3_READ_TIMEOUT);
  }
  else if (feedback && (cmd >= 0x30)) {
    if (read_command (MP3_READ_TIMEOUT) == -1) return -1;
    if (read_command (MP3_READ_TIMEOUT) == -1) return -1;
    return (((int16_t)mp3_receive_buf[5]) << 8) + mp3_receive_buf[6];
  }
  else if (!feedback && (cmd >= 0x30)) {
    if (read_command (MP3_READ_TIMEOUT) == -1) return -1;
    return (((int16_t)mp3_receive_buf[5]) << 8) + mp3_receive_buf[6];
  }
}

// ======================================
int16_t read_command (uint32_t mp3_read_timeout) {
  uint8_t tmp, flag = 0;
  int16_t Answer = 0;
  uint32_t mp3_read_timer = millis();
  while ((tmp = mp3.read()) != 0x7E)
    if (millis() - mp3_read_timer > mp3_read_timeout) {
      // LOG.println("• MP3 • while");
      return -1;
    }

  mp3_receive_buf[0] = tmp;
  delay(3);
  for (uint8_t i = 1; i < 10; i++) {
    mp3_receive_buf[i] = mp3.read();
    delay(3);
  }
  byte cmd = mp3_receive_buf[3];
#ifdef MP3_DEBUG
  if ( cmd != 0x06) {
    LOG.println();
    LOG.print(F("mp3_received:"));
    for (uint8_t i = 0; i < 10; i++) {
      LOG.print(mp3_receive_buf[i], HEX);
      LOG.print(F(" "));
    }
    LOG.println(" ");
  }
#endif  //MP3_DEBUG

  /* get current volume level */
  if ( cmd == 0x43) {
    volume_level = mp3_receive_buf[6];
  }

  /* get tracks in a folder */
  if ( cmd == 0x4E) {
    songs = mp3_receive_buf[6];
    //#ifdef MP3_DEBUG #endif // MP3_DEBUG
    LOG.printf("• Tracks in a folder • %3d\n\r", songs);

  }
  if ( mp3_receive_buf[2] == 6 && mp3_receive_buf[9] == 0xEF && mp3_receive_buf[3] != 0x40) return (((int16_t)mp3_receive_buf[5]) << 8) + mp3_receive_buf[6];
  else {
    LOG.print("• Error data with this Command • ");  LOG.println(cmd, HEX);
    return -1;
  }
}

// ======================================
void GetMaxSongs(byte folder) {
  send_command(0x4E, FEEDBACK, 0, folder);
  delay(mp3_delay);
}

// ======================================
void SetPlayerVolume(byte volume) {
  volume_target = GetVolume(volume);
#ifdef MP3_DEBUG
  LOG.printf("• SetPlayerVolume • %3d\% | %2d | Cur Volume %2d | Cycle Volume %2d |\n\r", volume, volume_target, volume_level, volume_cycle);
#endif  //MP3_DEBUG
}

// ======================================
void FadeVolume() {
  if (volume_target == volume_level) return;
  if (volume_target > volume_level ) volume_level++;
  if (volume_target < volume_level ) volume_level--;
  send_command(0x06, FEEDBACK, 0, volume_level);
#ifdef MP3_DEBUG
  LOG.printf("• Fade Volume | Current Level %2d | Target %2d | Cycle Level %2d \n\r", volume_level, volume_target, volume_cycle);
#endif  //MP3_DEBUG 
}

// ======================================
void ChangeEqualizer() {
  /* Specify EQ | 0:Normal | 1:Pop | 2:Rock | 3:Jazz | 4:Classic | 5:Bass */
  Equalizer++;
  if (Equalizer > 5U) Equalizer = 0;
  send_command(0x07, FEEDBACK, 0, Equalizer);
  for (uint8_t i = 0U; i < 8; i++) {
    leds[24 + i] = CHSV(Equalizer * 40, 255U, 255U);
  }
  FastLED.delay(1);
#ifdef MP3_DEBUG
  LOG.printf_P(PSTR("• Change Equalizer | mode EQ •%2d\n\r"), Equalizer);
#endif  //MP3_DEBUG 
  delay(600);
}

// ======================================
void PlaySong() {
  isMusicPlay = true;
  send_command(0x12, FEEDBACK, 0, 1);  // play
  delay(mp3_delay);
}

// ======================================
void StopSong() {
  isMusicPlay = false;
  send_command(0x16, FEEDBACK, 0, 0);  // stop
  delay(mp3_delay);
}

// ======================================
void NextSong() {
  send_command(0x01, FEEDBACK, 0, 0);  // next
  delay(mp3_delay);
}

// ======================================
void PrevSong() {
  send_command(0x02, FEEDBACK, 0, 0);  // prev
  delay(mp3_delay);
}

#endif
