# PRODIGY_CS_04
This project consists of a simple keylogger implemented in C++ (`keylogger.cpp`) and a corresponding Python server (`server.py`). The keylogger captures keystrokes from a Linux input device, encrypts them using a basic XOR cipher, and sends the encrypted data to a local server. The server decrypts and logs the keystrokes to a file.
**Note:** This project is intended for educational purposes only. Unauthorized use of keyloggers to capture keystrokes without consent is illegal and unethical.

## Features
- **Keylogger (C++):**
  - Captures keystrokes from a specified or default keyboard input device using `libevdev`.
  - Supports basic key mapping (e.g., letters, numbers, symbols) with shift key detection.
  - Encrypts keystrokes using an XOR cipher with a hardcoded key (`"secret"`).
  - Sends encrypted data to a server every 5 seconds or on program termination.
  - Logs encrypted data locally to `keylog.txt` if server communication fails.
- **Server (Python):**
  - Listens for incoming connections on `127.0.0.1:12345`.
  - Decrypts received data using the same XOR cipher.
  - Appends decrypted keystrokes to `keylog.txt`.
 
## Prerequisites
**For `keylogger.cpp`:**
  - Operating System: Linux (requires `/dev/input/eventX` devices).
  - Compiler: GCC or any C++11-compatible compiler.
  - Library: `libevdev` (for handling input devices).
  - Permissions: Root privileges may be required to access input devices.
**For `server.py`:**
  - Python: Version 3.x.
  - Modules: No external modules required (uses standard `socket` library).

## Notes
- **Security:** The XOR encryption is very basic and not secure for real-world use. For a production environment, use proper encryption (e.g., AES).
- **Device Access:** The keylogger requires access to `/dev/input/eventX`. Run with `sudo` or adjust permissions as needed.
- **Limitations:** The keymap is incomplete and supports only a subset of keys. Special keys may appear as `[KEY_NAME]` or `[UNK:code]`.
- **Ethics:** Use this code responsibly and only with explicit permission from the device owner.

### Troubleshooting
- **"Failed to open device":** Ensure you have permissions (`sudo`) and the device path is valid.
- **"Connection failed":** Verify the server is running and listening on `127.0.0.1:12345`.
- **No output:** Check if the correct input device is being used (run ls `/dev/input/event*` to list devices).
