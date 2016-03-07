#include <SHA204.h>
#include <SHA204I2C.h>
#include <SHA204Definitions.h>
#include <SHA204ReturnCodes.h>

// Uncomment this to enable locking the configuration zone.
// *** BE AWARE THAT THIS PREVENTS ANY FUTURE CONFIGURATION CHANGE TO THE CHIP ***
// It is still possible to change the key, and this also enable random key generation
//#define LOCK_CONFIGURATION

// Uncomment this to enable locking the data zone.
// *** BE AWARE THAT THIS PREVENTS THE KEY TO BE CHANGED ***
// It is not required to lock data, key cannot be retrieved anyway, but by locking
// data, it can be guaranteed that nobody even with physical access to the chip,
// will be able to change the key.
//#define LOCK_DATA

// Uncomment this to skip key storage (typically once key has been written once)
//#define SKIP_KEY_STORAGE

// Uncomment this to skip key data storage (once configuration is locked, key
// will aways randomize)
// Uncomment this to skip key generation and use 'user_key_data' as key instead.
#define USER_KEY_DATA

#ifdef USER_KEY_DATA
#define MY_HMAC_KEY 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
                    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
const uint8_t user_key_data[32] = {MY_HMAC_KEY};
#endif

SHA204I2C sha204;

uint16_t calculateAndUpdateCrc(uint8_t length, uint8_t *data, uint16_t current_crc)
{
  uint8_t counter;
  uint16_t crc_register = current_crc;
  uint16_t polynom = 0x8005;
  uint8_t shift_register;
  uint8_t data_bit, crc_bit;

  for (counter = 0; counter < length; counter++)
  {
    for (shift_register = 0x01; shift_register > 0x00; shift_register <<= 1) 
    {
      data_bit = (data[counter] & shift_register) ? 1 : 0;
      crc_bit = crc_register >> 15;

      // Shift CRC to the left by 1.
      crc_register <<= 1;

      if ((data_bit ^ crc_bit) != 0)
        crc_register ^= polynom;
    }
  }
  return crc_register;
}

uint16_t write_config_and_get_crc()
{
  uint16_t crc = 0;
  uint8_t config_word[4];
  uint8_t tx_buffer[SHA204_CMD_SIZE_MAX];
  uint8_t rx_buffer[SHA204_RSP_SIZE_MAX];
  uint8_t ret_code;
  bool do_write;

  // We will set default settings from datasheet on all slots. This means that we can use slot 0 for the key
  // as that slot will not be readable (key will therefore be secure) and slot 8 for the payload digest
  // calculationon as that slot can be written in clear text even when the datazone is locked.
  // Other settings which are not relevant are kept as is.

  for (int i=0; i < 88; i += 4)
  {
    do_write = true;
    if (i == 20)
    {
      config_word[0] = 0x8F;
      config_word[1] = 0x80;
      config_word[2] = 0x80;
      config_word[3] = 0xA1;
    }
    else if (i == 24)
    {
      config_word[0] = 0x82;
      config_word[1] = 0xE0;
      config_word[2] = 0xA3;
      config_word[3] = 0x60;
    }
    else if (i == 28)
    {
      config_word[0] = 0x94;
      config_word[1] = 0x40;
      config_word[2] = 0xA0;
      config_word[3] = 0x85;
    }
    else if (i == 32)
    {
      config_word[0] = 0x86;
      config_word[1] = 0x40;
      config_word[2] = 0x87;
      config_word[3] = 0x07;
    }
    else if (i == 36)
    {
      config_word[0] = 0x0F;
      config_word[1] = 0x00;
      config_word[2] = 0x89;
      config_word[3] = 0xF2;
    }
    else if (i == 40)
    {
      config_word[0] = 0x8A;
      config_word[1] = 0x7A;
      config_word[2] = 0x0B;
      config_word[3] = 0x8B;
    }
    else if (i == 44)
    {
      config_word[0] = 0x0C;
      config_word[1] = 0x4C;
      config_word[2] = 0xDD;
      config_word[3] = 0x4D;
    }
    else if (i == 48)
    {
      config_word[0] = 0xC2;
      config_word[1] = 0x42;
      config_word[2] = 0xAF;
      config_word[3] = 0x8F;
    }
    else if (i == 52 || i == 56 || i == 60 || i == 64)
    {
      config_word[0] = 0xFF;
      config_word[1] = 0x00;
      config_word[2] = 0xFF;
      config_word[3] = 0x00;
    }
    else if (i == 68 || i == 72 || i == 76 || i == 80)
    {
      config_word[0] = 0xFF;
      config_word[1] = 0xFF;
      config_word[2] = 0xFF;
      config_word[3] = 0xFF;
    }
    else
    {
      // All other configs are untouched
      ret_code = sha204.read(tx_buffer, rx_buffer, SHA204_ZONE_CONFIG, i);
      if (ret_code != SHA204_SUCCESS)
      {
        Serial.print("Failed to read config. Response: ");
        Serial.println(ret_code, HEX);
        return 0; //halt();
      }
      // Set config_word to the read data
      config_word[0] = rx_buffer[SHA204_BUFFER_POS_DATA+0];
      config_word[1] = rx_buffer[SHA204_BUFFER_POS_DATA+1];
      config_word[2] = rx_buffer[SHA204_BUFFER_POS_DATA+2];
      config_word[3] = rx_buffer[SHA204_BUFFER_POS_DATA+3];
      do_write = false;
    }

    // Update crc with CRC for the current word
    crc = calculateAndUpdateCrc(4, config_word, crc);

    // Write config word
    if (do_write)
    {
      ret_code = sha204.execute(SHA204_WRITE, SHA204_ZONE_CONFIG,
                                i >> 2, 4, config_word, 0, NULL, 0, NULL,
                                WRITE_COUNT_SHORT, tx_buffer, WRITE_RSP_SIZE, rx_buffer);
      if (ret_code != SHA204_SUCCESS)
      {
        Serial.print("Failed to write config word at address ");
        Serial.print(i);
        Serial.print(". Response: ");
        Serial.println(ret_code, HEX);
        return 0; //halt();
      }
    }
  }
  return crc;
}

void write_key(uint8_t* key)
{
  uint8_t tx_buffer[SHA204_CMD_SIZE_MAX];
  uint8_t rx_buffer[SHA204_RSP_SIZE_MAX];
  uint8_t ret_code;

  // Write key to slot 0
  ret_code = sha204.execute(SHA204_WRITE, SHA204_ZONE_DATA | SHA204_ZONE_COUNT_FLAG,
                            0, SHA204_ZONE_ACCESS_32, key, 0, NULL, 0, NULL,
                            WRITE_COUNT_LONG, tx_buffer, WRITE_RSP_SIZE, rx_buffer);
  if (ret_code != SHA204_SUCCESS)
  {
    Serial.print("Failed to write key to slot 0. Response: ");
    Serial.println(ret_code, HEX);
    return; //halt();
  }
}

void personalizeAtSha204(void)
{
  uint8_t tx_buffer[SHA204_CMD_SIZE_MAX];
  uint8_t rx_buffer[SHA204_RSP_SIZE_MAX];
  uint8_t key[32];
  uint8_t ret_code;
  uint8_t lockConfig = 0;
  uint8_t lockValue = 0;
  uint16_t crc;

  Serial.println("ATSHA204 personalization sketch for MySensors usage.");
  Serial.println("----------------------------------------------------");

  sha204.init();

  // Wake device before starting operations
  ret_code = sha204.wakeup(rx_buffer);
  if (ret_code != SHA204_SUCCESS)
  {
    Serial.print("Failed to wake device. Response: ");
    Serial.println(ret_code, HEX);
    //return; //halt();
  }
  
  // Output device revision on console
  ret_code = sha204.dev_rev(tx_buffer, rx_buffer);
  if (ret_code != SHA204_SUCCESS)
  {
    Serial.print("Failed to determine device revision. Response: ");
    Serial.println(ret_code, HEX);
    //return; //halt();
  }
  else
  {
    Serial.print("Device revision: ");
    for (int i=0; i<4; i++)
    {
      if (rx_buffer[SHA204_BUFFER_POS_DATA+i] < 0x10)
      {
        Serial.print('0'); // Because Serial.print does not 0-pad HEX
      }
      Serial.print(rx_buffer[SHA204_BUFFER_POS_DATA+i], HEX);
    }
    Serial.println();
  }

  // Output serial number on console
  ret_code = sha204.serialNumber(rx_buffer);
  if (ret_code != SHA204_SUCCESS)
  {
    Serial.print("Failed to obtain device serial number. Response: ");
    Serial.println(ret_code, HEX);
    //return; //halt();
  }
  else
  {
    Serial.print("Device serial:   ");
    Serial.print('{');
    for (int i=0; i<9; i++)
    {
      Serial.print("0x");
      if (rx_buffer[i] < 0x10)
      {
        Serial.print('0'); // Because Serial.print does not 0-pad HEX
      }
      Serial.print(rx_buffer[i], HEX);
      if (i < 8) Serial.print(',');
    }
    Serial.print('}');
    Serial.println();
    for (int i=0; i<9; i++)
    {
      if (rx_buffer[i] < 0x10)
      {
        Serial.print('0'); // Because Serial.print does not 0-pad HEX
      }
      Serial.print(rx_buffer[i], HEX);
    }
    Serial.println();
  }

  // Read out lock config bits to determine if locking is possible
  ret_code = sha204.read(tx_buffer, rx_buffer, SHA204_ZONE_CONFIG, 0x15<<2);
  if (ret_code != SHA204_SUCCESS)
  {
    Serial.print("Failed to determine device lock status. Response: "); Serial.println(ret_code, HEX);
    //return; //halt();
  }
  else
  {
    lockConfig = rx_buffer[SHA204_BUFFER_POS_DATA+3];
    lockValue = rx_buffer[SHA204_BUFFER_POS_DATA+2];
  }

    //TODO List current configuration before attempting to lock
    Serial.println("Old chip configuration:");
    sha204.dump_configuration();

  if (lockConfig != 0x00)
  {
    // Write config and get CRC for the updated config
    crc = write_config_and_get_crc();

    // List current configuration before attempting to lock
    Serial.println("Chip configuration:");
    sha204.dump_configuration();

#ifdef LOCK_CONFIGURATION
    {
      Serial.println("Locking configuration...");

      // Correct sequence, resync chip
      ret_code = sha204.resync(SHA204_RSP_SIZE_MAX, rx_buffer);
      if (ret_code != SHA204_SUCCESS && ret_code != SHA204_RESYNC_WITH_WAKEUP)
      {
        Serial.print("Resync failed. Response: "); Serial.println(ret_code, HEX);
        return; //halt();
      }

      // Lock configuration zone
      ret_code = sha204.execute(SHA204_LOCK, SHA204_ZONE_CONFIG,
                                crc, 0, NULL, 0, NULL, 0, NULL,
                                LOCK_COUNT, tx_buffer, LOCK_RSP_SIZE, rx_buffer);
      if (ret_code != SHA204_SUCCESS)
      {
        Serial.print("Configuration lock failed. Response: "); Serial.println(ret_code, HEX);
        return; //halt();
      }
      else
      {
        Serial.println("Configuration locked.");

        // Update lock flags after locking
        ret_code = sha204.read(tx_buffer, rx_buffer, SHA204_ZONE_CONFIG, 0x15<<2);
        if (ret_code != SHA204_SUCCESS)
        {
          Serial.print("Failed to determine device lock status. Response: "); Serial.println(ret_code, HEX);
          return; //halt();
        }
        else
        {
          lockConfig = rx_buffer[SHA204_BUFFER_POS_DATA+3];
          lockValue = rx_buffer[SHA204_BUFFER_POS_DATA+2];
        }
      }
    }
#else //LOCK_CONFIGURATION
    Serial.println("Configuration not locked. Define LOCK_CONFIGURATION to lock for real.");
#endif
  }
  else
  {
    Serial.println("Skipping configuration write and lock (configuration already locked).");
    Serial.println("Chip configuration:");
    sha204.dump_configuration();
  }

#ifdef SKIP_KEY_STORAGE
  Serial.println("Disable SKIP_KEY_STORAGE to store key.");
#else
#ifdef USER_KEY_DATA
  memcpy(key, user_key_data, 32);
  Serial.println("Using this user supplied key:");
#else
  // Retrieve random value to use as key
  ret_code = sha204.sha204m_random(tx_buffer, rx_buffer, RANDOM_SEED_UPDATE);
  if (ret_code != SHA204_SUCCESS)
  {
    Serial.print("Random key generation failed. Response: "); Serial.println(ret_code, HEX);
    //return; //halt();
  }
  else
  {
    memcpy(key, rx_buffer+SHA204_BUFFER_POS_DATA, 32);
  }
  if (lockConfig == 0x00)
  {
    Serial.println("Take note of this key, it will never be the shown again:");
  }
  else
  {
    Serial.println("Key is not randomized (configuration not locked):");
  }
#endif
  Serial.print("#define MY_HMAC_KEY ");
  for (int i=0; i<32; i++)
  {
    Serial.print("0x");
    if (key[i] < 0x10)
    {
      Serial.print('0'); // Because Serial.print does not 0-pad HEX
    }
    Serial.print(key[i], HEX);
    if (i < 31) Serial.print(',');
    if (i+1 == 16) Serial.print("\\\n                    ");
  }
  Serial.println();

  // It will not be possible to write the key if the configuration zone is unlocked
  if (lockConfig == 0x00)
  {
    // Write the key to the appropriate slot in the data zone
    Serial.println("Writing key to slot 0...");
    write_key(key);
  }
  else
  {
    Serial.println("Skipping key storage (configuration not locked).");
    Serial.println("The configuration must be locked to be able to write a key.");
  }  
#endif

  if (lockValue != 0x00)
  {
#ifdef LOCK_DATA
    {
      // Correct sequence, resync chip
      ret_code = sha204.sha204c_resync(SHA204_RSP_SIZE_MAX, rx_buffer);
      if (ret_code != SHA204_SUCCESS && ret_code != SHA204_RESYNC_WITH_WAKEUP)
      {
        Serial.print("Resync failed. Response: "); Serial.println(ret_code, HEX);
        return; //halt();
      }

      // If configuration is unlocked, key is not updated. Locking data in this case will cause
      // slot 0 to contain an unknown (or factory default) key, and this is in practically any
      // usecase not the desired behaviour, so ask for additional confirmation in this case.
      if (lockConfig != 0x00)
      {
        while (Serial.available())
        {
          Serial.read();
        }
        Serial.println("*** ATTENTION ***");
        Serial.println("Configuration is not locked. Are you ABSULOUTELY SURE you want to lock data?");
        Serial.println("Locking data at this stage will cause slot 0 to contain a factory default key");
        Serial.println("which cannot be change after locking is done. This is in practically any usecase");
        Serial.println("NOT the desired behavour. Send SPACE character now to lock data anyway...");
        while (Serial.available() == 0);
        if (Serial.read() != ' ')
        {
          Serial.println("Unexpected answer. Skipping lock.");
          return; //halt();
        }
      }

      // Lock data zone
      ret_code = sha204.sha204m_execute(SHA204_LOCK, SHA204_ZONE_DATA | LOCK_ZONE_NO_CRC,
                                        0x0000, 0, NULL, 0, NULL, 0, NULL,
                                        LOCK_COUNT, tx_buffer, LOCK_RSP_SIZE, rx_buffer);
      if (ret_code != SHA204_SUCCESS)
      {
        Serial.print("Data lock failed. Response: "); Serial.println(ret_code, HEX);
        return; //halt();
      }
      else
      {
        Serial.println("Data locked.");

        // Update lock flags after locking
        ret_code = sha204.read(tx_buffer, rx_buffer, SHA204_ZONE_CONFIG, 0x15<<2);
        if (ret_code != SHA204_SUCCESS)
        {
          Serial.print("Failed to determine device lock status. Response: "); Serial.println(ret_code, HEX);
          return; //halt();
        }
        else
        {
          lockConfig = rx_buffer[SHA204_BUFFER_POS_DATA+3];
          lockValue = rx_buffer[SHA204_BUFFER_POS_DATA+2];
        }
      }
    }
#else //LOCK_DATA
    Serial.println("Data not locked. Define LOCK_DATA to lock for real.");
#endif
  }
  else
  {
    Serial.println("Skipping OTP/data zone lock (zone already locked).");
  }

  Serial.println("--------------------------------");
  Serial.println("Personalization is now complete.");
  Serial.print("Configuration is ");
  if (lockConfig == 0x00)
  {
    Serial.println("LOCKED");
  }
  else
  {
    Serial.println("UNLOCKED");
  }
  Serial.print("Data is ");
  if (lockValue == 0x00)
  {
    Serial.println("LOCKED");
  }
  else
  {
    Serial.println("UNLOCKED");
  }
}
