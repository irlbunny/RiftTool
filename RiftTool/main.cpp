#include <iostream>
#include <string>
#include "util.h"

typedef enum CommandType_ {
  CommandType_Unknown = -1,
  CommandType_Eeprom = 0,
  CommandType_Serial = 1
} CommandType;

int main(int argc, char *argv[]) {
  CommandType commandType = CommandType_Unknown;

  // EEPROM command type
  uint8_t eepromBlockNum = 0;
  char *eepromFileName = nullptr;
  bool eepromIsReading = false;
  bool eepromIsDumping = false;
  bool eepromIsWriting = false;
  
  // Serial command type
  char *serialNumber = nullptr;

  if (argc < 2 || (argc >= 2 && strcmp(argv[1], "help") == 0)) {
helpMessage:
    std::cout << "Usage: RiftTool.exe [command] (...)\n";
    std::cout << "\nExamples:\n";
    std::cout << "\"RiftTool.exe help\" - displays info/usage for this program\n";
    std::cout << "\"RiftTool.exe eeprom r [1-128]\" - reads eeprom block into console\n";
    std::cout << "\"RiftTool.exe eeprom d dump.bin\" - dumps eeprom into a file\n";
    std::cout << "\"RiftTool.exe eeprom w dump.bin\" - writes to eeprom from file\n";
    std::cout << "\"RiftTool.exe serial WMHD0000000XYZ\" - changes your headset serial number (16 characters max)\n";
    std::cout << "\nWARNING: It is highly recommended that you dump your EEPROM before touching other commands.\n";
    return 0;
  }

  if (argc >= 3 && strcmp(argv[1], "eeprom") == 0) {
    commandType = CommandType_Eeprom;

    // Dumping/Writing
    if (argc >= 4 && strcmp(argv[2], "r") == 0) {
      eepromBlockNum = std::stoi(argv[3]);
      eepromIsReading = true;
      goto executeProgram;
    } else if (argc >= 4 && strcmp(argv[2], "d") == 0) {
      eepromFileName = argv[3];
      eepromIsDumping = true;
      goto executeProgram;
    } else if (argc >= 4 && strcmp(argv[2], "w") == 0) {
      if (rift_tool::Util::FileExists(argv[3])) {
        eepromFileName = argv[3];
        eepromIsWriting = true;
        goto executeProgram;
      }

      std::cout << "File to write to EEPROM does not exist!\n\n";
    }
  } else if (argc >= 3 && strcmp(argv[1], "serial") == 0) {
    commandType = CommandType_Serial;

    if (strlen(argv[2]) <= 16) {
      serialNumber = argv[2];
      goto executeProgram;
    }

    std::cout << "Serial number is too big!\n\n";
  }

  goto helpMessage;

executeProgram:
  if (hid_init()) {
    std::cout << "Unable to initialize HIDAPI.\n";
    return -1;
  }

  auto hidPaths = rift_tool::Util::GetHidPaths(0x2833, 0x0031, 1);
  if (hidPaths.size() < 1) {
    std::cout << "No headsets detected, is it plugged in? RiftTool *only* works on Rift CV1.\n";
    return -1;
  } else if (hidPaths.size() > 1) {
    std::cout << "RiftTool can only detect one headset at a time.\n";
    std::cout << "Please disconnect any headsets you do not intend on using with RiftTool.\n";
    return -1;
  }

  hid_device *deviceHandle = hid_open_path(hidPaths[0]);
  static wchar_t deviceSerialNumber[MAX_STRING];
  hid_get_serial_number_string(deviceHandle, deviceSerialNumber, MAX_STRING);
  std::wcout << L"Headset S/N: " << deviceSerialNumber << L"\n";

  if (commandType == CommandType_Eeprom) {
    static uint8_t eepromBlock[EEPROM_BLOCK_SIZE];
    static uint8_t eepromBuffer[EEPROM_SIZE];

    if (eepromIsReading) {
      if (!rift_tool::Util::ReadEepromBlock(deviceHandle, eepromBlockNum - 1, eepromBlock)) {
        printf("Reading EEPROM block #%i failed.\n", eepromBlockNum);
        return 0;
      }

      std::cout << "Result: ";
      for (int i = 0; i < EEPROM_BLOCK_SIZE; i++) {
        printf("%02X ", eepromBlock[i]);
      }
    } else if (eepromIsDumping) {
      std::cout << "Dumping EEPROM into \"" << eepromFileName << "\", please wait...\n";

      rift_tool::Util::ReadEeprom(deviceHandle, eepromBuffer);

      FILE *file;
      fopen_s(&file, eepromFileName, "wb");
      fwrite(eepromBuffer, sizeof(eepromBuffer), 1, file);
      fclose(file);

      std::cout << "Finished dumping EEPROM into \"" << eepromFileName << "\"!\n";
    } else if (eepromIsWriting) {
      std::cout << "Writing \"" << eepromFileName << "\" into EEPROM, please wait...\n";

      FILE *file;
      fopen_s(&file, eepromFileName, "rb");
      fread(eepromBuffer, sizeof(eepromBuffer), 1, file);
      fclose(file);

      rift_tool::Util::WriteEeprom(deviceHandle, eepromBuffer);

      std::cout << "Finished writing \"" << eepromFileName << "\" into EEPROM!\n";
    }
  } else if (commandType == CommandType_Serial) {
    static uint8_t serialBlock[EEPROM_BLOCK_SIZE];
    if (!rift_tool::Util::ReadEepromBlock(deviceHandle, 0, serialBlock)) {
      printf("Reading EEPROM block #0 failed.\n");
      return 0;
    }

    // Clear and copy our new serial number
    memset(&serialBlock[0x10], 0, 16);
    memcpy_s(&serialBlock[0x10], 16, serialNumber, strlen(serialNumber));

    if (!rift_tool::Util::WriteEepromBlock(deviceHandle, 0, serialBlock)) {
      printf("Writing EEPROM block #0 failed.\n");
      return 0;
    }

    std::cout << "Changed headset serial number to \"" << serialNumber << "\".\n";
    std::cout << "Please unplug and replug in your headset for the changes to take effect!\n";
  }

  return 0;
}
