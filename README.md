# Universal Standoff 2 tokenpizder

Native handshake extractor for Standoff 2
Author: [DoubleLuc](https://github.com/t0mil0v-rev)

## Windows (No ADB required)


Reads the handshake(playerTicket) directly from the emulator's process memory without ADB, root or files transfer.

### Requirements & Supported Emulators
- Windows 10/11 x64
- Supported emulators:
  - **BlueStacks 5** (`HD-Player.exe`)
  - **LDPlayer 9 / 4** (`dnplayer.exe`)
  - **NoxPlayer** (`Nox.exe`)
  - **MEmu Play** (`MEmu.exe`)

### How to use
1. Launch the emulator and log in to Standoff 2 (stay in the main menu).
2. Build or run `tokenpizder.exe`:
   ```bat
   build.bat
   tokenpizder.exe
   ```
3. The executable will output the handshake directly to `stdout`.


