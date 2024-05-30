/*  sound_msg.ino
    https://zvukogram.com/?r=search&s=%D1%88%D1%83%D0%BC+%D0%BC%D0%BE%D1%80%D1%8F
    https://zvukogram.com/category/zvuk-shtorma/
    https://voxworker.com/ua  | Якісна озвучка тексту онлайн українською мовою
    https://ytmp3.nu/J7Dm/    | online converter YouTube to MP3
    https://elevenlabs.io/    | Якісна озвучка тексту онлайн | голос Michael
*/

#ifdef USE_IOT
// ======================================
void SoundBell() {
  if (!isNight()) {
    PlaySoundAdvert(1, 10);
    /* timeline correction */
    if (scenario > 0) scenario--;
  }
}

// ======================================
void SoundMute() {
  send_command(0x06, FEEDBACK, 0, 0);
  delay(mp3_delay);

  //  send_command(0x0F, FEEDBACK, 50, 250);    /* play silence */
  //  delay(mp3_delay);

  isSoundPlay = false;
  send_command(0x16, FEEDBACK, 0, 0);           /* stop */
  delay(mp3_delay);
#ifdef GENERAL_DEBUG
  LOG.printf_P(PSTR("%3d | Sound Mute |\n\r"), progress);
#endif //GENERAL_DEBUG
}

// ======================================
void getMaxTrack(byte folder) {
  send_command(0x4E, FEEDBACK, 0, folder);
  delay(500);
}

// ======================================
uint8_t GetVolume(byte volume) {
  uint8_t new_volume = volume * eff_volume / 100;
  return constrain(new_volume, 1, 30);
}

// ======================================
void PlaySoundAdvert(byte sound_effect, uint8_t volume) {

#ifdef GENERAL_DEBUG
  LOG.printf_P(PSTR("%3d | Play ADVERT | Sound Effect •%3d | volume •%2d \n\r"), progress, sound_effect, volume);
#endif

  progress++;
  if (sound) {
    if (isNight() & sound_effect < 90 &  currentMode != 5U) return;
    /* set pause play sound */
    send_command(0x0E, FEEDBACK, 0, 0);
    delay(mp3_delay);

    /* set volume advert sound */

    SetPlayerVolume(volume);

    /* play advert sound */
    send_command(0x13, FEEDBACK, 0, sound_effect);
    delay(mp3_delay);
    advert_flag = true;
    if (custom_eff) { /* for youtube conditions*/
      if (floor(sound_effect / 10) == 5U) {
        deltaValue = 0;
        quickIntro = true;
      }
    }
  }
}

// ======================================
void PlayCycledSoundEffect(byte folder) {
  if (!ONflag) return;
  if (sound) {
    if (isNight()) return;
#ifdef GENERAL_DEBUG
    LOG.printf_P(PSTR("%3d | Play Cycled Sound Effect | Cur Folder • %3d | volume • %2d\n\r"), progress, folder, volume_cycle);
#endif //GENERAL_DEBUG

    SetPlayerVolume(volume_cycle);
    /* play folder */
    send_command(0x17, FEEDBACK, 0, folder);
    delay(mp3_delay);
    isSoundPlay = true;
  }
}

// ======================================
void PlaySong(uint16_t song, byte volume) {
  time_t currentLocalTime = getCurrentLocalTime();
  isSoundPlay = true;

  SetPlayerVolume(volume);
  send_command(0x12, FEEDBACK, 0, song);
  delay(mp3_delay);

#ifdef GENERAL_DEBUG
  LOG.printf_P(PSTR("[ > ] Play Song %4d | volume%3d"), song, volume);
  LOG.println("%");
#endif //GENERAL_DEBUG
}

// ======================================
void PlayAllSong(byte volume) {
  time_t currentLocalTime = getCurrentLocalTime();
  isSoundPlay = true;
  SetPlayerVolume(volume);

  //  send_command(0x12, FEEDBACK, 0, dayOfWeek(currentLocalTime));
  send_command(0x12, FEEDBACK, 0, 0);
  delay(mp3_delay);

#ifdef GENERAL_DEBUG
  LOG.printf_P(PSTR("[ > ] Play All Song | volume%3d"), volume);
  LOG.println("%");
#endif //GENERAL_DEBUG
}

// ======================================
void PlayCurSound(byte folder, byte snd, byte volume) {
  isSoundPlay = true;
  SetPlayerVolume(volume);
  send_command(0x0F, FEEDBACK, folder, snd);
  delay(mp3_delay);
#ifdef GENERAL_DEBUG
  LOG.printf_P(PSTR("%3d | PlayCurSound | Folder %3d | Sound %3d\n\r"), progress,  folder, snd);
#endif //GENERAL_DEBUG
}

#endif
