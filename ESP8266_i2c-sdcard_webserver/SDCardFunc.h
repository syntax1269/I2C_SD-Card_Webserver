/*

- getFileNamesFromSD() Returns the global std::vector<std::pair<String, uint32_t>> fileNames containing file names and sizes previously retrieved from the SD card.
- getDirectoryNamesFromSD() Returns the global std::vector<String> directoryNames containing directory names previously retrieved from the SD card.
- CustDelay(uint16_t mils) Pauses execution for mils milliseconds while allowing background tasks (like WiFi) to run using yield() . No return value.
- setSDCardTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) Sends the specified date and time components to the I2C SD card module using the 'C' command to set its internal clock. Prints status/errors to Serial. No return value.
- sendFilename(const char* filename) Helper function to send a filename to the I2C SD card module using the 'F' command. Returns true on success, false on I2C error.
- storetoSD(const char* filename, char command, const char* msg) Writes ( command='W' ) or appends ( command='A' ) the string msg to the specified filename on the I2C SD card. Handles sending the filename ('F' command) and then the data in chunks, ensuring subsequent chunks always use append ('A'). Prints errors to Serial. No return value.
- ReadFromSD(const char* filename) Reads the entire content of the specified filename from the I2C SD card and prints it to the Serial monitor. It first gets the file size ('S' command) and then reads the data ('R' command) in chunks. Prints status/errors to Serial. No return value.
- GetFileSize(const char* filename) Gets the size of the specified filename on the I2C SD card using the 'F' (filename) and 'S' (size) commands. Returns the file size as an int (uint32_t internally), or -1 on I2C error.
- checkExists(const char* path, bool isDirectory) Checks if a given path exists on the I2C SD card. Uses command 'E' if isDirectory is false (checking for a file) or 'K' if isDirectory is true (checking for a directory), after sending the path with 'F'. Returns true if the path exists as the specified type, false otherwise or on error. Prints status/errors to Serial.
- removeFile(const char* filename) Deletes the specified filename from the I2C SD card using the 'F' (filename) and 'X' (remove file) commands. Returns true on success, false on failure or I2C error. Prints status/errors to Serial.
- mkdir(const char* dirname) Creates a directory with the specified dirname on the I2C SD card using the 'F' (filename/dirname) and 'M' (make directory) commands. Returns true if the directory was created or already existed, false on I2C error. Prints status/errors to Serial.
- rmdir(const char* dirname) Removes the specified directory dirname from the I2C SD card using the 'F' (filename/dirname) and 'D' (remove directory) commands. Returns true on success, false if the directory doesn't exist, is not empty, or on I2C error. Prints status/errors to Serial.
- queryCardType() Sends a command ('Q') to the I2C SD card module to query the type of SD card present. It reads a single byte response, interprets it as the card type (e.g., SDv1, SDv2, SDHC/SDXC, MMC), and prints the result to the Serial monitor. No parameters required. Used to identify the SD card type connected to the system.
- getvolsize() Queries the I2C SD card module for volume information (such as total size and free space) and typically prints this information to the Serial monitor. No parameters required. Used to inspect the storage capacity and available space on the SD card.
- dirListFromSD(const char* dirname) Sends a command ('L') to the I2C SD card module to list the contents of the specified directory dirname . It reads the response, parses filenames and sizes, populates the global fileNames (vector of pairs) and directoryNames (vector of strings) with the results, and prints the formatted directory listing to the Serial monitor. No return value. Used to inspect the contents of a directory on the SD card.
- listDirectory(const char* dirname) Sends a command ('L') to the I2C SD card module to list the contents of the specified directory dirname . It reads the response, parses filenames and sizes, populates the global fileNames (vector of pairs) and directoryNames (vector of strings) with the results, and prints the formatted directory listing to the Serial monitor. No return value. Used to inspect the contents of a directory on the SD card.
- listDirectory_HTML(const char* dirname) Reads the contents (files and subdirectories) of the specified directory dirname on the I2C SD card and returns an HTML formatted String representing the directory listing. Used for displaying directory contents in a web interface.

*/


#include <vector>     // Include the vector library
#include <WString.h>  // Include for Arduino String class
#include <utility>    // Include for std::pair
#define I2C_SDCARD 0x6e
bool SDCARDBUSY = false;
bool Detected_i2cSDCard = false;
uint8_t i2cSDCarderrcnt = 0;
// Global dynamic arrays for filenames (with size) and directory names
std::vector<std::pair<String, uint32_t>> fileNames;  // Store pairs of <filename, size>
std::vector<String> directoryNames;

// Functions to access the stored names (optional)
std::vector<std::pair<String, uint32_t>> getFileNamesFromSD() {
  return fileNames;
}

std::vector<String> getDirectoryNamesFromSD() {
  return directoryNames;
}

void CustDelay(uint16_t mils){
  unsigned long start = millis();
      while( millis() - start < mils){ // Pauses without impacting other cpu functions like WiFi
       yield();
     }
}

// --- Function to Set Time on SD Card Module ---
void setSDCardTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  Serial.print("Sending time to SD Card Module: ");
  Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);

  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('C');  // Clock Set command
  Wire.write((uint8_t)(year % 100));  // Send YY (e.g., 25 for 2025)
  Wire.write(month);
  Wire.write(day);
  Wire.write(hour);
  Wire.write(minute);
  Wire.write(second);

  uint8_t error = Wire.endTransmission();  // Send STOP

  if (error == 0) {
    Serial.println("Time sent successfully.");
  } else {
    Serial.print("Error sending time: I2C Error ");
    Serial.println(error);
  }
}

// --- Helper Function to Send Filename ---
bool sendFilename(const char* filename) {
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('F'); // Filename command
  Wire.write(filename);
  uint8_t error = Wire.endTransmission(true); // Send STOP after filename
  if (error != 0) {
    Serial.print("  [Error] Failed to send filename '"); Serial.print(filename);
    Serial.print("'. I2C Error: "); Serial.println(error);
    return false;
  }
  // Serial.print("  Filename '"); Serial.print(filename); Serial.println("' sent.");
 // delay(5); // Short delay for bridge processing
  return true;
}


void storetoSD(const char* filename, char command, const char* msg) {
  /* Command  Name  Description
     'Filename'  Specifies the filename [8.3 filename structure].
     'W'  Write data  Writes data to the file, overwriting if necessary.
     'A'  Append data Appends data to the end of the file, if it already exists.
  */

  // Send filename first
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('F');
  if (strlen(filename) < 31){
    Serial.print("File name: ");
    Serial.println(filename);
  Wire.write(filename, strlen(filename)); // Sends the entire string content (excluding the null terminator) in potentially a more optimized way.
  } else {
    Serial.print("File name: ");
    Serial.println(filename);
  while (*filename) Wire.write(*filename++); // Sends the filename one character at a time, stopping just before the null terminator.
  }
  uint8_t error = Wire.endTransmission();  // Send STOP after filename
  if (error != 0) {
    Serial.print("I2C Error sending filename for storetoSD: ");
    Serial.println(error);
    return;
  }
  CustDelay(5);  // Small CustDelay after sending filename

  // Calculate message length
  const size_t msgLen = strlen(msg);
  if (msgLen == 0) {
    Serial.println("Warning: storetoSD called with empty message.");
    // If command is 'W', we might want to explicitly truncate the file here.
    // This requires a separate command on the ATtiny or handling 0-length write.
    // For now, just return.
    return;
  }

  const size_t bufferSize = 31;  // Max I2C buffer size - 1 for command byte
  size_t offset = 0;

  // --- Strategy: Send first chunk with original command, subsequent chunks with 'A' ---

  // Send first chunk
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write(command);  // Use the initial command ('W' or 'A')
  size_t bytesToWrite = min(bufferSize, msgLen - offset);
  Wire.write(msg + offset, bytesToWrite);
  error = Wire.endTransmission(true);  // Send STOP
  if (error != 0) {
    Serial.print("I2C Error during first write chunk: ");
    Serial.println(error);
    return;
  }
  offset += bytesToWrite;
 CustDelay(5);

  // Send subsequent chunks (if any) ALWAYS using 'A' (append)
  while (offset < msgLen) {
    Wire.beginTransmission(I2C_SDCARD);
    Wire.write('A');  // <<< ALWAYS use Append for subsequent chunks
    bytesToWrite = min(bufferSize, msgLen - offset);
    Wire.write(msg + offset, bytesToWrite);
    error = Wire.endTransmission(true);  // Send STOP
    if (error != 0) {
      Serial.print("I2C Error during subsequent append chunk: ");
      Serial.println(error);
      return;
    }
    offset += bytesToWrite;
   CustDelay(5);
  }
}

void ReadFromSD(const char* filename) {
  // Send Filename
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('F');
  if (strlen(filename) < 31){
    Serial.print("File name: ");
    Serial.println(filename);
  Wire.write(filename, strlen(filename)); // Sends the entire string content (excluding the null terminator) in potentially a more optimized way.
  } else {
    Serial.print("File name: ");
    Serial.println(filename);
  while (*filename) Wire.write(*filename++); // Sends the filename one character at a time, stopping just before the null terminator.
  }
  uint8_t error = Wire.endTransmission();  // Send STOP
  if (error != 0) {
    Serial.print("I2C Error sending filename for read: ");
    Serial.println(error);
    return;
  }
  CustDelay(5);

  // Get File Size
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('S');
  error = Wire.endTransmission(false);  // Keep connection active for requestFrom
  if (error != 0) {
    Serial.print("I2C Error sending 'S' command: ");
    Serial.println(error);
    return;
  }
 CustDelay(5);

  uint32_t size = 0;
  uint8_t bytesRead = Wire.requestFrom(I2C_SDCARD, 4, 1);  // Request 4 bytes, send STOP
  if (bytesRead == 4) {
    for (int i = 0; i < 4; i++) size = (size << 8) | Wire.read();
  } else {
    Serial.print("Error reading size, expected 4 bytes, got ");
    Serial.println(bytesRead);
    // Consume any remaining bytes if necessary
    while (Wire.available()) Wire.read();
    return;
  }

  Serial.print("File Size: ");
  Serial.println(size);

  if (size == 0) {
    Serial.println("File is empty or not found.");
    return;
  }

  Serial.println("--- File Start ---");

  // Send Read Command
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('R');
  error = Wire.endTransmission(false);  // Keep connection active for requestFrom
  if (error != 0) {
    Serial.print("I2C Error sending 'R' command: ");
    Serial.println(error);
    return;
  }
 CustDelay(5);

  // Read data in chunks
  const int readChunkSize = 32;
  uint32_t bytesRemaining = size;

  while (bytesRemaining > 0) {
    int bytesToRequest = min((int)bytesRemaining, readChunkSize);
    bytesRead = Wire.requestFrom(I2C_SDCARD, bytesToRequest, 0);  // Don't send STOP yet

    if (bytesRead > 0) {
      for (int i = 0; i < bytesRead; i++) {
        if (Wire.available()) {
          Serial.print((char)Wire.read());
        } else {
          Serial.println("\nError: Wire not available during read chunk.");
          Wire.endTransmission();  // Send STOP on error
          return;
        }
      }
      bytesRemaining -= bytesRead;
    } else {
      Serial.print("\nError reading file chunk, expected ");
      Serial.print(bytesToRequest);
      Serial.println(" bytes, got 0.");
      Wire.endTransmission();  // Send STOP on error
      return;
    }
    CustDelay(1);  // Small CustDelay between read requests
  }

  Wire.endTransmission();  // Send STOP after the last chunk is read
  Serial.println("\r\n--- File END ---");
}


int GetFileSize(const char* filename) {
  const char* fname = filename;  // Keep original pointer for printing
  // Send Filename
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('F');
  Wire.write(filename, strlen(filename));
  uint8_t error = Wire.endTransmission();  // Send STOP
  if (error != 0) {
    Serial.print("I2C Error sending filename for GetFileSize: ");
    Serial.println(error);
    return -1;  // Indicate error
  }
 CustDelay(5);

  // Send Size Command
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('S');
  error = Wire.endTransmission(false);  // Keep connection active for requestFrom
  if (error != 0) {
    Serial.print("I2C Error sending 'S' command for GetFileSize: ");
    Serial.println(error);
    return -1;
  }
 CustDelay(5);

  // Request Size
  uint32_t size = 0;
  uint8_t bytesRead = Wire.requestFrom(I2C_SDCARD, 4, 1);  // Request 4 bytes, send STOP
  if (bytesRead == 4) {
    for (int i = 0; i < 4; i++) size = (size << 8) | Wire.read();
  } else {
    Serial.print("Error reading size for GetFileSize, expected 4 bytes, got ");
    Serial.println(bytesRead);
    while (Wire.available()) Wire.read();  // Consume any remaining bytes
    return -1;                             // Indicate error
  }

  // Serial.print("\r\nSelected File: "); Serial.print(fname); Serial.println(" - Size: " + String(size));
  return size;  // Return the size (can be 0 if file is empty or doesn't exist)
}

// --- Function to Check if Path Exists ('E' for files, 'K' for directories) ---
bool checkExists(const char* path, bool isDirectory) {
  Serial.print("--- Checking if "); Serial.print(isDirectory ? "Directory" : "File");
  Serial.print(" '"); Serial.print(path); Serial.print("' exists ('");
  Serial.print(isDirectory ? 'K' : 'E'); Serial.println("') ---");

  if (!sendFilename(path)) return false; // Send filename first

  Wire.beginTransmission(I2C_SDCARD);
  Wire.write(isDirectory ? 'K' : 'E'); // Send appropriate command
  uint8_t error = Wire.endTransmission(false); // Send command, NO STOP
  if (error != 0) {
    Serial.print("  [Error] Failed to send check command. I2C Error: "); Serial.println(error);
    Wire.endTransmission();
    return false; // Indicate uncertainty
  }

  uint8_t bytesReceived = Wire.requestFrom(I2C_SDCARD, 1, 1); // Request 1 byte, send STOP
  if (bytesReceived == 1) {
    uint8_t result = Wire.read();
    Serial.print("  Result: "); Serial.print(result);
    if (result == 1) {
      Serial.println(" (Exists)");
      Wire.endTransmission();
      return true;
    } else {
      Serial.println(" (Does Not Exist or Not a Dir)");
      Wire.endTransmission();
      return false;
    }
  } else {
    Wire.endTransmission();
    Serial.println("  [Error] Did not receive expected byte for existence check.");
    return false; // Indicate uncertainty
  }
}

bool removeFile(const char* filename) {
  const char* fname = filename;  // Keep original pointer for printing
  // Send Filename
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('F');
  Wire.write(filename, strlen(filename));
  uint8_t error = Wire.endTransmission();  // Send STOP
  if (error != 0) {
    Serial.print("I2C Error sending filename for removeFile: ");
    Serial.println(error);
    return false;
  }
 CustDelay(5);

  // Send Remove File Command
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('X');                      // 'X' for remove file
  error = Wire.endTransmission(false);  // Keep connection active for requestFrom
  if (error != 0) {
    Serial.print("I2C Error sending 'X' command: ");
    Serial.println(error);
    return false;
  }
  CustDelay(5);

  // Request Result (1 byte: 1 for success, 0 for failure)
  bool success = false;
  uint8_t bytesRead = Wire.requestFrom(I2C_SDCARD, 1, 1);  // Request 1 byte, send STOP
  if (bytesRead == 1) {
    success = (Wire.read() == 1);
  } else {
    Serial.print("Error reading removeFile status, expected 1 byte, got ");
    Serial.println(bytesRead);
    while (Wire.available()) Wire.read();  // Consume any remaining bytes
    return false;                          // Assume failure on error
  }

  if (success) {
    Serial.print("Successfully removed file: ");
    Serial.println(fname);
    return true;
  } else {
    Serial.print("Failed to remove file: ");
    Serial.println(fname);
    return false;
  }
}

bool mkdir(const char* dirname) {
  const char* dname = dirname;  // Keep original pointer for printing
  // Send Directory Name (using 'F' command)
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('F');
  Wire.write(dirname, strlen(dirname));
  uint8_t error = Wire.endTransmission();  // Send STOP
  if (error != 0) {
    Serial.print("I2C Error sending dirname for mkdir: ");
    Serial.println(error);
    return false;
  }
  CustDelay(5);

  // Send Make Directory Command
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('M');                      // 'M' for make directory
  error = Wire.endTransmission(false);  // Keep connection active for requestFrom
  if (error != 0) {
    Serial.print("I2C Error sending 'M' command: ");
    Serial.println(error);
    return false;
  }
 CustDelay(5);

  // Request Result (1 byte: 1 for success, 0 for failure)
  bool success = false;
  uint8_t bytesRead = Wire.requestFrom(I2C_SDCARD, 1, 1);  // Request 1 byte, send STOP
  if (bytesRead == 1) {
    success = (Wire.read() == 1);
  } else {
    Serial.print("Error reading mkdir status, expected 1 byte, got ");
    Serial.println(bytesRead);
    while (Wire.available()) Wire.read();  // Consume any remaining bytes
    return false;                          // Assume failure on error
  }

  if (success) {
    // Serial.print("Successfully created directory: "); Serial.println(dname);
    return true;
  } else {
    // Serial.print("Failed to create directory (might already exist): "); Serial.println(dname);
    // It's common for mkdir to fail if the directory exists, so treat failure as potentially okay
    return true;  // Let's return true even if it failed (might already exist)
                  // Or return false if strict success is needed: return false;
  }
}

bool rmdir(const char* dirname) {
  const char* dname = dirname;  // Keep original pointer for printing
  // Send Directory Name (using 'F' command)
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('F');
  Wire.write(dirname, strlen(dirname));
  uint8_t error = Wire.endTransmission();  // Send STOP
  if (error != 0) {
    Serial.print("I2C Error sending dirname for rmdir: ");
    Serial.println(error);
    return false;
  }
 CustDelay(5);

  // Send Remove Directory Command
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('D');                      // 'D' for remove directory
  error = Wire.endTransmission(false);  // Keep connection active for requestFrom
  if (error != 0) {
    Serial.print("I2C Error sending 'D' command: ");
    Serial.println(error);
    return false;
  }
 CustDelay(5);

  // Request Result (1 byte: 1 for success, 0 for failure)
  bool success = false;
  uint8_t bytesRead = Wire.requestFrom(I2C_SDCARD, 1, 1);  // Request 1 byte, send STOP
  if (bytesRead == 1) {
    success = (Wire.read() == 1);
  } else {
    Serial.print("Error reading rmdir status, expected 1 byte, got ");
    Serial.println(bytesRead);
    while (Wire.available()) Wire.read();  // Consume any remaining bytes
    return false;                          // Assume failure on error
  }

  if (success) {
    Serial.print("Successfully removed directory: ");
    Serial.println(dname);
    return true;
  } else {
    Serial.print("Failed to remove directory (might not exist or not empty): ");
    Serial.println(dname);
    return false;
  }
}

// --- Function to Query Card Type ('Q') ---
void queryCardType() {
  Serial.println("\n--- Querying Card Type ('Q') ---");
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('Q');
  uint8_t error = Wire.endTransmission(false); // Send command, NO STOP
  if (error != 0) {
    Serial.print("  [Error] Failed to send 'Q' command. I2C Error: "); Serial.println(error);
    return;
  }

  uint8_t bytesReceived = Wire.requestFrom(I2C_SDCARD, 1, 1); // Request 1 byte, send STOP
  if (bytesReceived == 1) {
    uint8_t cardType = Wire.read();
    Serial.print("  Card Type Detected: ");
    Serial.print(cardType);
    switch (cardType) {
      case 0: Serial.println(" (Unknown/Error)"); break;
      case 1: Serial.println(" (SDv1)"); break;
      case 2: Serial.println(" (SDv2)"); break;
      case 3: Serial.println(" (SDHC/SDXC)"); break;
      default: Serial.println(" (Invalid Response)"); break;
    }
  } else {
    Serial.println("  [Error] Did not receive expected byte for card type.");
  }
}

// Function to get Volume Info (FAT Type, Size etc.)
// Based on user serial output and 'V' command protocol
void getvolsize() {
  Serial.println("Requesting volume data...");

  // Send Volume Info Command
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('V');
  uint8_t error = Wire.endTransmission(false);  // Keep connection active for requestFrom
  if (error != 0) {
    Serial.print("I2C Error sending 'V' command: ");
    Serial.println(error);
    return;
  }
 // CustDelay(10);  // Give slave time to prepare data

  // Request 10 bytes: Status(1) + FAT Type(1) + Blocks(4) + Clusters(4)
  uint8_t bytesRead = Wire.requestFrom(I2C_SDCARD, 10, 1);  // Request 10 bytes, send STOP

  if (bytesRead == 10) {
    uint8_t status = Wire.read();
    Serial.print("Status received: 0x");
    Serial.print(status, HEX);

    if (status == 0x01) {  // Success status from ATtiny
    Serial.println(" Success!");
      uint8_t fatType = Wire.read();
      uint32_t volBlocks = 0;
      uint32_t volClusters = 0;

      // Read Blocks per Cluster (4 bytes LSB first)
      for (int i = 0; i < 4; i++) {
        volBlocks |= (uint32_t)Wire.read() << (i * 8);
      }

      // Read Cluster Count (4 bytes LSB first)
      for (int i = 0; i < 4; i++) {
        volClusters |= (uint32_t)Wire.read() << (i * 8);
      }

      // --- Corrected FAT Type Printing ---
      Serial.print("FAT Type: ");
      switch (fatType) {
        case 12: Serial.println("FAT12"); break;
        case 16: Serial.println("FAT16"); break;
        case 32: Serial.println("FAT32"); break;  // Correctly handle 32
        default:
          Serial.print("Unknown/ExFAT (Recvd Byte: ");
          Serial.print(fatType);
          Serial.println(")");
          break;
      }
      // --- End Corrected Printing ---
      // Serial.print("Recvd Byte: "); Serial.println(fatType); // You can keep or remove this debug line

      Serial.print("Blocks per Cluster: ");
      Serial.println(volBlocks);
      Serial.print("Cluster Count: ");
      Serial.println(volClusters);

      // Calculate and print size (using 64-bit intermediate to prevent overflow)
      if (volBlocks > 0 && volClusters > 0) {
        uint64_t totalBytes = (uint64_t)volClusters * volBlocks * 512;  // Assuming 512 bytes/block (standard)
        double sizeKB = totalBytes / 1024.0;
        double sizeMB = sizeKB / 1024.0;
        double sizeGB = sizeMB / 1024.0;

        Serial.print("Calculated Volume Size (KB): ");
        Serial.println((unsigned long)sizeKB);  // Cast for printing
        Serial.print("Volume size (MB):  ");
        Serial.println(sizeMB, 2);  // Print with 2 decimal places
        Serial.print("Volume size (GB):  ");
        Serial.println(sizeGB, 2);  // Print with 2 decimal places
      }

    } else if (status == 0xFF) {
      Serial.println("Error: Slave reported failure initializing volume.");
      // Consume remaining bytes if any (should be 9 left)
      for (int i = 0; i < 9 && Wire.available(); ++i) Wire.read();
    } else {
      Serial.print("Error: Received unexpected status byte: 0x");
      Serial.println(status, HEX);
      // Consume remaining bytes if any
      while (Wire.available()) Wire.read();
    }
  } else {
    Serial.print("Error reading volume info, expected 10 bytes, got ");
    Serial.println(bytesRead);
    while (Wire.available()) Wire.read();  // Consume any remaining bytes
  }
}

// --- Directory Listing Functions ---

// Helper to parse the streamed directory data
void parseDirStream() {
  fileNames.clear();
  directoryNames.clear();

  bool receiving = true;
  while (receiving) {
    uint8_t entryType;
    String entryName = "";
    uint32_t entrySize = 0;

    // 1. Read Entry Type (or End Marker)
    uint8_t bytesRead = Wire.requestFrom(I2C_SDCARD, 1, 0);  // Request 1 byte, keep connection
    if (bytesRead == 1) {
      entryType = Wire.read();
      if (entryType == 0xFF) {  // End marker
        receiving = false;
        break;  // Exit the loop
      }
      if (entryType != 'F' && entryType != 'D') {
        Serial.print("\nError: Invalid entry type received: ");
        Serial.println((char)entryType);
        receiving = false;  // Stop parsing on error
        break;
      }
    } else {
      Serial.println("\nError reading entry type.");
      receiving = false;
      break;
    }

    // 2. Read Entry Name (null-terminated)
    bool nameDone = false;
    while (!nameDone) {
      bytesRead = Wire.requestFrom(I2C_SDCARD, 1, 0);  // Read one byte at a time
      if (bytesRead == 1) {
        char c = Wire.read();
        if (c == '\0') {
          nameDone = true;
        } else {
          entryName += c;
        }
      } else {
        Serial.println("\nError reading entry name.");
        receiving = false;
        nameDone = true;  // Exit inner loop
      }
    }
    if (!receiving) break;  // Exit outer loop if error occurred

    // 3. Read Entry Size (4 bytes, LSB first) - Only if it's a file
    if (entryType == 'F') {
      bytesRead = Wire.requestFrom(I2C_SDCARD, 4, 0);  // Request 4 bytes
      if (bytesRead == 4) {
        for (int i = 0; i < 4; i++) {
          entrySize |= (uint32_t)Wire.read() << (8 * i);
        }
      } else {
        Serial.print("\nError reading entry size, expected 4, got ");
        Serial.println(bytesRead);
        receiving = false;
        break;
      }
    } else {                                           // It's a directory ('D'), size is implicitly 0 for listing purposes
      bytesRead = Wire.requestFrom(I2C_SDCARD, 4, 0);  // Still need to read the 4 size bytes sent by slave
      if (bytesRead == 4) {
        for (int i = 0; i < 4; i++) Wire.read();  // Read and discard size bytes for directory
      } else {
        Serial.print("\nError reading dummy size for directory, expected 4, got ");
        Serial.println(bytesRead);
        receiving = false;
        break;
      }
    }


    // 4. Store the entry
    if (entryType == 'F') {
      fileNames.push_back({ entryName, entrySize });
    } else {  // 'D'
      directoryNames.push_back(entryName);
    }
    CustDelay(1);  // Small CustDelay before requesting next entry part
  }            // End while(receiving)

  Wire.endTransmission();  // Send STOP after finishing or error
}


// Main function to initiate and display directory listing
void dirListFromSD(const char* dirname) {
  Serial.println("\r\n----Directory " + String(dirname) + " Start-------");

  // 1. Send Directory Name
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('F');
  Wire.write(dirname, strlen(dirname));
  uint8_t error = Wire.endTransmission();  // Send STOP
  if (error != 0) {
    Serial.print("I2C Error sending dirname for dirList: ");
    Serial.println(error);
    Serial.println("----Directory End-------");
    return;
  }
 CustDelay(5);

  // 2. Send List Command
  Wire.beginTransmission(I2C_SDCARD);
  Wire.write('L');
  error = Wire.endTransmission(false);  // Keep connection active for requestFrom
  if (error != 0) {
    Serial.print("I2C Error sending 'L' command: ");
    Serial.println(error);
    Serial.println("----Directory End-------");
    return;
  }
  //CustDelay(10);  // Give slave a bit more time to open dir and get first entry

  // 3. Parse the stream
  parseDirStream();  // This function now handles reading and populating vectors

  // 4. Print the results from vectors
  Serial.println("Directory listing:");
  for (const auto& dir : directoryNames) {
    Serial.println("  [DIR] " + dir);
  }
  for (const auto& filePair : fileNames) {
    Serial.println("  " + filePair.first + " \t Size: " + String(filePair.second));
  }

  Serial.println("\r\n----Directory End-------");
}

// --- Function to List Directory Contents ('L') ---
void listDirectory(const char* dirname) {
    Serial.print("--- Listing Directory '"); Serial.print(dirname); Serial.println("' ('L') ---");
    if (!sendFilename(dirname)) return;

    Wire.beginTransmission(I2C_SDCARD);
    Wire.write('L');
    uint8_t error = Wire.endTransmission(false); // Send command, NO STOP
    if (error != 0) {
        Serial.print("  [Error] Failed to send 'L' command. I2C Error: "); Serial.println(error);
        return;
    }

    Serial.println("  Type | Size       | Name");
    Serial.println("  ----------------------------");

    bool firstEntry = true;
    while (true) {
        // Request Type byte
        uint8_t bytesReceived = Wire.requestFrom(I2C_SDCARD, 1, 0); // NO STOP yet
        if (bytesReceived != 1) {
            Serial.println("  [Error] Failed to receive Type byte.");
            Wire.endTransmission(true); // Send STOP to abort
            return;
        }
        uint8_t entryType = Wire.read();

        if (entryType == 0xFF) { // End of listing marker
            if (firstEntry) {
                Serial.println("  (Directory is empty or does not exist)");
            }
            Wire.endTransmission(true); // Send final STOP
            break;
        }
        firstEntry = false;

        // Read Name (null-terminated string)
        String entryName = "";
        while (true) {
            bytesReceived = Wire.requestFrom(I2C_SDCARD, 1, 0); // NO STOP
            if (bytesReceived != 1) {
                 Serial.println("\n  [Error] Failed to receive Name byte.");
                 Wire.endTransmission(true); // Send STOP to abort
                 return;
            }
            char c = Wire.read();
            if (c == '\0') break; // End of name
            entryName += c;
        }

        // Read Size (4 bytes, LSB first)
        uint32_t entrySize = 0;
        bytesReceived = Wire.requestFrom(I2C_SDCARD, 4, 0); // NO STOP
        if (bytesReceived == 4) {
             for (int i = 0; i < 4; i++) {
                entrySize |= ((uint32_t)Wire.read() << (8 * i));
             }
        } else {
             Serial.print("\n  [Error] Failed to receive Size bytes (received ");
             Serial.print(bytesReceived); Serial.println(").");
             Wire.endTransmission(true); // Send STOP to abort
             return;
        }

        // Print entry
        Serial.print("  ");
        Serial.print((char)entryType); // 'D' or 'F'
        Serial.print("    | ");
        if (entryType == 'F') {
            char sizeBuf[11];
            sprintf(sizeBuf, "%10lu", entrySize); // Format size right-aligned
            Serial.print(sizeBuf);
        } else {
            Serial.print("         -"); // Placeholder for directory size
        }
        Serial.print(" | ");
        Serial.println(entryName);

        // The ATtiny automatically prepares for the next entry type request
        // or sends 0xFF if done. The loop continues.
    }
     Serial.println("  ----------------------------");
     Wire.endTransmission();
}

// --- Function to Generate HTML Directory Listing ('L') ---
String listDirectory_HTML(const char* dirname, int page = 1, int perPage = 20) { // Max file lising 20, page navigation
    String html = "";
    html.reserve(11264);

    html += "<!DOCTYPE html>\n<html>\n<head>\n<title>Directory: ";
    html += dirname;
    html += "</title>\n";
    html += "<style>\n";
    html += "body { font-family: sans-serif; }\n";
    html += "table { border-collapse: collapse; width: 30%; }\n";
    html += "th, td { border: 1px solid #ddd; padding: 8px; }\n";
    html += "th { background-color: #f2f2f2; }\n";
    html += "a { text-decoration: none; color: blue; }\n";
    html += "a:hover { text-decoration: underline; }\n";
    html += "</style>\n";
    html += "</head>\n<body>\n";
    html += "<h1>Directory Listing: ";
    html += dirname;
    html += "</h1>\n";

    String currentDir = String(dirname);
    if (currentDir != "/") {
        if (currentDir.endsWith("/")) {
            currentDir = currentDir.substring(0, currentDir.length() - 1);
        }
        int lastSlash = currentDir.lastIndexOf('/');
        String parentDir;
        if (lastSlash <= 0) {
            parentDir = "/";
        } else {
            parentDir = currentDir.substring(0, lastSlash);
            if (parentDir.length() == 0) parentDir = "/";
        }
        html += "<p><a href=\"./listSDCard?DIR=";
        html += parentDir;
        html += "\">&#8592; Go up</a></p>\n";
    }

    if (!sendFilename(dirname)) {
        Wire.setClock(i2c_bus_Clock); //back to defualt
        CustDelay(5);
        Wire.beginTransmission(I2C_SDCARD);
        byte errorsd = Wire.endTransmission();
       if (errorsd == 0) {
         Detected_i2cSDCard = true;
       } else {
        if (i2cSDCarderrcnt > 5) {
           Detected_i2cSDCard = false;
        }
         i2cSDCarderrcnt++;
       }
        html += "<p>Error: Could not set directory path on device.</p>";

        html += "</body></html>";
        return html;
    }
    Wire.setClock(200000);
    CustDelay(5);
    Wire.beginTransmission(I2C_SDCARD);
    Wire.endTransmission();
    CustDelay(5);
    Wire.beginTransmission(I2C_SDCARD);
    Wire.write('L');
    uint8_t error = Wire.endTransmission(false);
    if (error != 0) {
        html += "<p>Error: Failed to send 'L' command. I2C Error: ";
        html += String(error);
        html += "</p>";
        html += "</body></html>";
        return html;
    }

    html += "<table>\n";
    html += "<tr><th align=center>Type</th><th align=center>Delete</th><th align=center>Name</th><th align=center>Size (Bytes)</th></tr>\n";

    bool firstEntry = true;
    bool i2c_error = false;
    const int maxEntries = 128; // Prevent infinite loops
    int entryCount = 0;
    int startIdx = (page - 1) * perPage;
    int endIdx = page * perPage;
    int totalEntries = 0;

    // We'll collect all entries first to count total and then display only the current page
    struct Entry {
        uint8_t entryType;
        String entryName;
        uint32_t entrySize;
    };
    Entry entries[maxEntries];

    while (totalEntries < maxEntries) {
        yield(); // Prevent watchdog reset

        uint8_t bytesReceived = Wire.requestFrom(I2C_SDCARD, 1, 0);
        if (bytesReceived != 1) {
            break;
        }
        uint8_t entryType = Wire.read();

        if (entryType == 0xFF) {
            break;
        }

        // Read Name (null-terminated string) into a fixed buffer
        char entryNameBuf[32] = {0};
        uint8_t nameIdx = 0;
        int nameTimeout = 0;
        while (true) {
            yield();
            bytesReceived = Wire.requestFrom(I2C_SDCARD, 1, 0);
            if (bytesReceived != 1) {
                break;
            }
            char c = Wire.read();
            if (c == '\0') break;
            if (nameIdx < sizeof(entryNameBuf) - 1) {
                entryNameBuf[nameIdx++] = c;
            }
            if (++nameTimeout > 64) {
                break;
            }
        }

        // Read Size (4 bytes, LSB first)
        uint32_t entrySize = 0;
        bytesReceived = Wire.requestFrom(I2C_SDCARD, 4, 0);
        if (bytesReceived == 4) {
            for (int i = 0; i < 4; i++) {
                entrySize |= ((uint32_t)Wire.read() << (8 * i));
            }
        }

        entries[totalEntries].entryType = entryType;
        entries[totalEntries].entryName = String(entryNameBuf);
        entries[totalEntries].entrySize = entrySize;
        totalEntries++;
    }

    // Now display only the entries for the current page
    if (totalEntries == 0) {
        html += "<tr><td colspan='4'>(Directory is empty)</td></tr>\n";
    } else {
        for (int i = startIdx; i < endIdx && i < totalEntries; i++) {
            uint8_t entryType = entries[i].entryType;
            String entryNameBuf = entries[i].entryName;
            uint32_t entrySize = entries[i].entrySize;

            html += "<tr>\n";
            html += "<td align=center>[";
            html += (char)entryType;
            html += "]</td>\n";

            // Delete Column
            html += "<td align=center>";
            if (entryType == 'F') {
                html += "<form method='POST' action='/deleteFile' style='display:inline;' onsubmit=\"return confirm('Delete file ";
                html += entryNameBuf;
                html += "?');\">";
                html += "<input type='hidden' name='file' value='";
                String deletePath = String(dirname);
                if (deletePath.length() > 1 && !deletePath.endsWith("/")) {
                    deletePath += "/";
                } else if (deletePath == "/") {
                    deletePath = "/";
                }
                deletePath += entryNameBuf;
                html += deletePath;
                html += "'/>";
                html += "<button type='submit' style='color:red;'>Delete</button>";
                html += "</form>";
            } else {
                html += "&mdash;";
            }
            html += "</td>\n";

            // Name Column (with link)
            html += "<td align=right><a href=\"";
            String entryPath = String(dirname);
            if (entryPath.length() > 1 && !entryPath.endsWith("/")) {
                entryPath += "/";
            } else if (entryPath == "/") {
                entryPath = "/";
            }
            entryPath += entryNameBuf;

            if (entryType == 'D') {
                html += "./listSDCard?DIR=";
                html += entryPath;
                html += "/\">";
                html += entryNameBuf;
                html += "/";
            } else {
                html += ".";
                html += entryPath;
                html += "\">";
                html += entryNameBuf;
            }
            html += "</a></td>\n";

            html += "<td>";
            if (entryType == 'F') {
                html += String(entrySize);
            } else {
                html += "---";
            }
            html += "</td>\n";
            html += "</tr>\n";
        }
    }

    // Pagination controls
    int totalPages = (totalEntries + perPage - 1) / perPage;
    html += "</table>\n";
    html += "<div style='margin-top:10px;'>";
    if (page > 1) {
        html += "<a href='/listSDCard?DIR=";
        html += dirname;
        html += "&page=";
        html += String(page - 1);
        html += "'>&laquo; Prev</a> ";
    }
    html += " Page ";
    html += String(page);
    html += " of ";
    html += String(totalPages);
    if (page < totalPages) {
        html += " <a href='/listSDCard?DIR=";
        html += dirname;
        html += "&page=";
        html += String(page + 1);
        html += "'>Next &raquo;</a>";
    }
    html += "</div>";
    html += "</body>\n</html>\n";

    return html;
}

void handleDeleteFile() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }
    if (!server.hasArg("file")) {
        server.send(400, "text/plain", "Missing file argument");
        return;
    }
    String filename = server.arg("file");
    bool success = removeFile(filename.c_str());
    if (success) {
        // Redirect back to the directory listing of the parent directory
        String parentDir = filename;
        int lastSlash = parentDir.lastIndexOf('/');
        if (lastSlash > 0) {
            parentDir = parentDir.substring(0, lastSlash);
        } else {
            parentDir = "/";
        }
        server.sendHeader("Location", "/listSDCard?DIR=" + parentDir, true);
        server.send(303, "text/plain", "");
    } else {
        server.send(500, "text/plain", "Failed to delete file");
    }
}

bool loadFromI2CSD(const String& filename) {
    /*
    - The function now uses WiFiClient client = server.client(); to get the underlying TCP connection and explicitly flushes and closes it after sending all data.
    - yield() and CustDelay(1) are used between chunks to allow the ESP8266's networking stack to process outgoing data, which is crucial for large files.
    - The function returns false if any error occurs during chunk sending, ensuring the browser gets a proper connection close.
    */
    String workingFilename = filename;  // Create a mutable copy
    if (workingFilename.endsWith("/")) workingFilename += "index.htm";
    if (workingFilename.endsWith("apple-touch-icon-precomposed.png")) {
        workingFilename = workingFilename.substring(0, workingFilename.lastIndexOf("apple-touch-icon-precomposed.png")) + "apple-touch-icon.png";
    }

    Wire.beginTransmission(I2C_SDCARD);
    byte errorsd = Wire.endTransmission();
    if (errorsd == 0) {
        Detected_i2cSDCard = true;
    } else {
        if (i2cSDCarderrcnt > 5) {
            Detected_i2cSDCard = false;
            return false;
        }
        i2cSDCarderrcnt++;
    }
    if (workingFilename.length() == 0) return false;
    if (!checkExists(workingFilename.c_str(), false)) return false;
    String dataType = "";
    if (workingFilename.endsWith(".src")) workingFilename = workingFilename.substring(0, workingFilename.lastIndexOf("."));
    else if (workingFilename.endsWith(".htm")) dataType = F("text/html");
    else if (workingFilename.endsWith(".css")) dataType = F("text/css");
    else if (workingFilename.endsWith(".js")) dataType = F("application/javascript");
    else if (workingFilename.endsWith(".png")) dataType = F("image/png");
    else if (workingFilename.endsWith(".gif")) dataType = F("image/gif");
    else if (workingFilename.endsWith(".jpg")) dataType = F("image/jpeg");
    else if (workingFilename.endsWith(".svg")) dataType = F("image/svg+xml");
    else if (workingFilename.endsWith(".ico")) dataType = F("image/x-icon");
    else if (workingFilename.endsWith(".xml")) dataType = F("text/xml");
    else if (workingFilename.endsWith(".pdf")) dataType = F("application/pdf");
    else if (workingFilename.endsWith(".zip")) dataType = F("application/zip");
    else if (workingFilename.endsWith(".log")) dataType = F("application/octet-stream");
    else if (workingFilename.endsWith(".txt")) dataType = F("text/plain");
    else if (workingFilename.endsWith(".csv")) dataType = F("text/csv");
    else dataType = F("application/octet-stream");  //no match above the file will just download
    if (server.hasArg("download")) dataType = F("application/octet-stream");

    Wire.beginTransmission(I2C_SDCARD);
    Wire.write('F');
    const char* name = workingFilename.c_str();
    if (strlen(name) < 31) {
        Wire.write(name, strlen(name));
    } else {
        while (*name) Wire.write(*name++);
    }
    uint8_t error = Wire.endTransmission();
    if (error != 0) {
        Serial.print("I2C Error sending filename for read: ");
        Serial.println(error);
        i2cSDCarderrcnt++;
        return false;
    }
    //CustDelay(5);
    Wire.endTransmission(false);

    // Get File Size
    Wire.beginTransmission(I2C_SDCARD);
    Wire.write('S');
    error = Wire.endTransmission(false);
    if (error != 0) {
        Serial.print("I2C Error sending 'S' command: ");
        Serial.println(error);
        i2cSDCarderrcnt++;
        return false;
    }
    //CustDelay(5);
    uint32_t size = 0;
    uint8_t bytesRead = Wire.requestFrom(I2C_SDCARD, 4, 1);
    if (bytesRead == 4) {
        for (int i = 0; i < 4; i++) size = (size << 8) | Wire.read();
    } else {
        Serial.print("Error reading size, expected 4 bytes, got ");
        Serial.println(bytesRead);
        while (Wire.available()) Wire.read();
        return false;
    }
    if (size == 0) {
        Serial.println("File is empty or not found.");
        return false;
    }
    Wire.setClock(i2c_bus_FileDownload); //lets speed up the transfer
    CustDelay(5);
    Wire.beginTransmission(I2C_SDCARD);
    Wire.endTransmission();
    CustDelay(5);

    // Send Read Command
    Wire.beginTransmission(I2C_SDCARD);
    Wire.write('R');
    error = Wire.endTransmission(false);
    if (error != 0) {
        Serial.print("I2C Error sending file read Command!!! Error code: ");
        Serial.println(error);
        i2cSDCarderrcnt++;
        return false;
    }

    // Start chunked response
    server.setContentLength(size);
    server.send(200, dataType, "");  // Send headers first

    const int readChunkSize = 32;
    uint32_t bytesRemaining = size;
    char buffer[readChunkSize];
    SDCARDBUSY = true;

    WiFiClient client = server.client();
    bool errorDuringSend = false;

    while (bytesRemaining > 0) {
        int bytesToRequest = min((int)bytesRemaining, readChunkSize);
        bytesRead = Wire.requestFrom(I2C_SDCARD, bytesToRequest, 0);
        if (bytesRead > 0) {
            for (int i = 0; i < bytesRead; i++) {
                if (Wire.available()) {
                    buffer[i] = Wire.read();
                } else {
                    Serial.println("\nError: Wire not available during read chunk.");
                    Wire.endTransmission();
                    errorDuringSend = true;
                    break;
                }
            }
            if (errorDuringSend) break;
            server.sendContent(buffer, bytesRead);
            yield(); // Allow TCP stack to process
            bytesRemaining -= bytesRead;
        } else {
            Serial.print("\nError reading file chunk, expected ");
            Serial.print(bytesToRequest);
            Serial.println(" bytes, got 0.");
            Wire.endTransmission();
            errorDuringSend = true;
            break;
        }
        //CustDelay(1); // Small delay to allow WiFi stack to process
    }
    Wire.endTransmission();  // Send STOP after the last chunk is read

    // Final empty chunk to signal end of response
    server.sendContent("");
    CustDelay(5);

    // Explicitly flush and close the connection if possible
    if (client && client.connected()) {
        client.flush();
        CustDelay(3);
        client.stop();
    }

    Wire.setClock(i2c_bus_Clock); //back to default
    CustDelay(5);
    Wire.beginTransmission(I2C_SDCARD);
    Wire.endTransmission();
    SDCARDBUSY = false;

    return !errorDuringSend;
}

void RunSDCard_Demo() {
   // 1. Directory Operations
  const char* testDir = "/TESTDIR";
  Serial.println();
  checkExists(testDir, 1); // Check if exists initially
  mkdir(testDir);     // Create it
  checkExists(testDir, 1); // Check again
  listDirectory("/");         // List root to see the new directory
  rmdir(testDir);   // Remove it
  checkExists(testDir, 1); // Check again

  // 2. File Operations
  const char* testFile = "/TEST.TXT";
  const char* fileContent1 = "Line 1. Hello from ESP8266!";
  const char* fileContent2 = "\nLine 2. Appendline";
  const char* fileContent3 = "Line 1. Hello again, from ESP8266!\nLine 2. 12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
  Serial.println();
  checkExists(testFile, 0); // Check if exists initially
  storetoSD(testFile, 'W', fileContent1); // Write (create/overwrite)
  checkExists(testFile, 0); // Check again
  ReadFromSD(testFile);    // Read content
  Serial.print("Appending to ");
  storetoSD(testFile, 'A', fileContent2); // Append
  ReadFromSD(testFile);    // Read appended content
  removeFile(testFile);         // Remove it
  checkExists(testFile, 0); // Check again
  Serial.print("\nWriting lots of data now..");
  storetoSD(testFile, 'W', fileContent3); // Write (create/overwrite)
  checkExists(testFile, 0); // Check again
   Serial.print("\nReading lots of data now..");
  ReadFromSD(testFile);    // Read content

  // 3. Nested Operations & Time Setting
  const char* nestedDir = "/NEST/SUB"; 
  const char* nestedFile = "/NEST/SUB/NESTFILE.TXT";
  const char* nestedContent = "Data in a nested directory.";

  Serial.println();
  setSDCardTime(2024, 7, 26, 10, 30, 00); // Set a specific time
  delay(100); // Give bridge time to process time set

  mkdir("/NEST");      // Create parent first
  mkdir(nestedDir);    // Create nested dir
  checkExists(nestedDir, 1);// Verify nested dir exists
  storetoSD(nestedFile, 'W', nestedContent); // Write file inside
  checkExists(nestedFile, 0); // Verify file exists
  listDirectory(nestedDir);     // List nested directory
  ReadFromSD(nestedFile);  // Read the nested file
  removeFile(nestedFile);       // Clean up file
  rmdir(nestedDir);   // Clean up nested dir
  rmdir("/NEST");     // Clean up parent dir
  checkExists("/NEST", 1);   // Verify parent dir removed

  Serial.println("\n--- Demo Finished ---");

}
