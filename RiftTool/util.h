#pragma once
#include <windows.h>
#include <inttypes.h>
#include <hidapi.h>
#include <optional>
#include <vector>
#include <cstdio>

#define MAX_STRING 255
#define EEPROM_SIZE 0x2000
#define EEPROM_BLOCKS 128
#define EEPROM_BLOCK_SIZE 0x40
#define READ_SLEEP_MS 25
#define WRITE_SLEEP_MS 50

namespace rift_tool {

class Util {
public:
  // Buffer must be the size of EEPROM_BLOCK_SIZE.
  static bool ReadEepromBlock(hid_device *deviceHandle, uint8_t blockNum, uint8_t* buffer) {
    if (blockNum >= EEPROM_BLOCKS) {
      memset(buffer, 0, EEPROM_BLOCK_SIZE);
      return false;
    }

    static uint8_t cmdData[0x45];
    cmdData[0] = 0x21;
    cmdData[1] = 0;
    cmdData[2] = 0;
    cmdData[3] = blockNum;
    cmdData[4] = 0x80;
    memset(&cmdData[5], 0, sizeof(cmdData) - 5);

    int r = hid_send_feature_report(deviceHandle, cmdData, sizeof(cmdData));
    if (r == -1) {
      memset(buffer, 0, EEPROM_BLOCK_SIZE);
      return false;
    }

    Sleep(READ_SLEEP_MS);

    r = hid_get_feature_report(deviceHandle, cmdData, sizeof(cmdData));
    if (r == -1) {
      memset(buffer, 0, EEPROM_BLOCK_SIZE);
      return false;
    }

    memcpy_s(buffer, EEPROM_BLOCK_SIZE, &cmdData[5], EEPROM_BLOCK_SIZE);
    return true;
  }

  // Buffer must be the size of EEPROM_BLOCK_SIZE.
  static bool WriteEepromBlock(hid_device *deviceHandle, uint8_t blockNum, uint8_t *buffer) {
    if (blockNum >= EEPROM_BLOCKS) {
      return false;
    }

    static uint8_t cmdData[0x45];
    cmdData[0] = 0x21;
    cmdData[1] = 0;
    cmdData[2] = 0;
    cmdData[3] = blockNum;
    cmdData[4] = 0;
    memcpy_s(&cmdData[5], EEPROM_BLOCK_SIZE, buffer, EEPROM_BLOCK_SIZE);

    int r = hid_send_feature_report(deviceHandle, cmdData, sizeof(cmdData));
    if (r == -1) {
      return false;
    }

    Sleep(WRITE_SLEEP_MS);

    r = hid_get_feature_report(deviceHandle, cmdData, sizeof(cmdData));
    if (r == -1) {
      return false;
    }

    return true;
  }

  // Buffer must be the size of EEPROM_SIZE.
  static bool ReadEeprom(hid_device *deviceHandle, uint8_t *buffer) {
    int offset = 0;
    for (uint8_t i = 0; i < EEPROM_BLOCKS; i++) {
      if (!ReadEepromBlock(deviceHandle, i, &buffer[offset])) {
        printf("Reading EEPROM block #%i failed.\n", i + 1);
      }
      offset += EEPROM_BLOCK_SIZE;
    }
    return true;
  }

  // Buffer must be the size of EEPROM_SIZE.
  static bool WriteEeprom(hid_device *deviceHandle, uint8_t *buffer) {
    int offset = 0;
    for (uint8_t i = 0; i < EEPROM_BLOCKS; i++) {
      if (!WriteEepromBlock(deviceHandle, i, &buffer[offset])) {
        printf("Writing EEPROM block #%i failed.\n", i + 1);
      }
      offset += EEPROM_BLOCK_SIZE;
    }
    return true;
  }

  static std::vector<char*> GetHidPaths(uint16_t vid, uint16_t pid, std::optional<char> mi = std::nullopt) {
    std::vector<char*> paths;

    struct hid_device_info *devices, *currentDevice;

    devices = hid_enumerate(vid, pid);
    currentDevice = devices;

    while (currentDevice) {
      if (mi.has_value() && currentDevice->interface_number != mi.value()) {
        goto nextDevice;
      }

      paths.push_back(currentDevice->path);

nextDevice:
      currentDevice = currentDevice->next;
    }

    return paths;
  }

  static bool FileExists(LPCSTR szPath) {
    DWORD dwAttrib = GetFileAttributesA(szPath);
    return dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
  }
};

}
