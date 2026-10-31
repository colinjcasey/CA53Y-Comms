# CA53Y Comms

Off-grid text messaging over ESP-NOW for a plain **ESP32 dev board**. No screen needed: the board makes its own Wi-Fi network, you join it from your phone, and you message other CA53Y devices with no internet, router or Bluetooth.

Every CA53Y device repeats the messages it hears, so messages hop from board to board to reach people out of direct range.

## Pick a firmware

| Firmware                | Board                                         | What it does                                                                 |
|-------------------------|-----------------------------------------------|------------------------------------------------------------------------------|
| **Messenger**           | ESP32 dev board ("ESP32 Dev Module", 4 MB)    | Phone portal at 192.168.4.1 for messaging, and repeats everything it hears  |
| **Relay**               | ESP32 dev board                               | No Wi-Fi network or portal. Only repeats. LED flashes on each relay          |
| **Relay (S3 antenna)**  | ESP32-S3-MINI-1U board with external antenna  | Same job as Relay, at 20 dBm through an external antenna, for long range     |

Messenger and Relay are the same sketch (`CA53Y_Comms`), built with `RELAY_ONLY` set to 0 or 1.

## Standard or Long Range

Every firmware comes in two networks:

| Network        | `LONG_RANGE` | Radio                                                                  |
|----------------|--------------|------------------------------------------------------------------------|
| **Standard**   | `0`          | Normal ESP-NOW. Works with every CA53Y device, including the display    |
| **Long Range** | `1`          | Espressif's Long Range (LR) mode at 250 kbps. Each hop reaches farther  |

**Standard and Long Range devices can't hear each other.** Every board in a group, messengers and relays, must use the same setting. On a Long Range messenger, ESP-NOW runs Long Range only while the phone Wi-Fi network stays normal Wi-Fi, so phones connect as usual. The portal shows "Long Range network" or "Standard network" under the device name, and the serial monitor prints it at boot.

Long Range builds in the web flasher write the app without touching saved settings, so switching a messenger to Long Range keeps its name, password and contacts (unless you tick Erase).

## Flash it

### Easiest: from the browser

Open the project's web flasher (GitHub Pages, see below) in **Chrome or Edge** on a computer, plug the board in with a USB *data* cable, and click the button for the firmware you want.

### Arduino IDE

1. **Boards Manager:** install **esp32 by Espressif Systems**, version **3.x** (built and tested on 3.3.12). No other libraries needed.
2. Open `CA53Y_Comms/CA53Y_Comms.ino`. Keep the folder name the same as the `.ino`.
3. For a relay, set `#define RELAY_ONLY 1` near the top. Leave it at `0` for the messenger.
   For a Long Range network, also set `#define LONG_RANGE 1`.
4. Tools > Board: **ESP32 Dev Module**.
5. Upload. If it won't connect, hold **BOOT** while it says "Connecting...".

For the S3 antenna relay, open `CA53Y_Relay_S3/CA53Y_Relay_S3.ino`, pick **ESP32S3 Dev Module**, set USB CDC On Boot to **Enabled**, set `LONG_RANGE` to match your network, and upload. **Attach the antenna first.**

### arduino-cli

```
arduino-cli core install esp32:esp32@3.3.12
# messenger (use -DRELAY_ONLY=1 for a relay, -DLONG_RANGE=1 for Long Range)
arduino-cli compile -b esp32:esp32:esp32 CA53Y_Comms --build-property "compiler.cpp.extra_flags=-DRELAY_ONLY=0 -DLONG_RANGE=0"
arduino-cli upload  -b esp32:esp32:esp32 -p /dev/ttyUSB0 CA53Y_Comms
```

### esptool (prebuilt files)

```
esptool --chip esp32   --port /dev/ttyUSB0 write-flash 0x0 docs/firmware/CA53Y_Comms_Messenger.bin
esptool --chip esp32   --port /dev/ttyUSB0 write-flash 0x0 docs/firmware/CA53Y_Comms_Relay.bin
esptool --chip esp32s3 --port /dev/ttyACM0 write-flash 0x0 docs/firmware/CA53Y_Relay_S3.bin
```

Long Range (keeps saved settings):

```
cd docs/firmware/lr
esptool --chip esp32   --port /dev/ttyUSB0 write-flash 0x1000 CA53Y_Comms_Messenger_LR.bootloader.bin 0x8000 CA53Y_Comms_Messenger_LR.partitions.bin 0xe000 boot_app0.bin 0x10000 CA53Y_Comms_Messenger_LR.bin
esptool --chip esp32   --port /dev/ttyUSB0 write-flash 0x1000 CA53Y_Comms_Relay_LR.bootloader.bin 0x8000 CA53Y_Comms_Relay_LR.partitions.bin 0xe000 boot_app0.bin 0x10000 CA53Y_Comms_Relay_LR.bin
esptool --chip esp32s3 --port /dev/ttyACM0 write-flash 0x0 CA53Y_Relay_S3_LR.bootloader.bin 0x8000 CA53Y_Relay_S3_LR.partitions.bin 0xe000 boot_app0.bin 0x10000 CA53Y_Relay_S3_LR.bin
```

## Using the messenger

1. Join the Wi-Fi network named after the device, like `CA53Ycomms-1234abC`. The sign-in sheet should pop up; if not, open **http://192.168.4.1**.
2. **First boot:** the network is open. Pick your name and a Wi-Fi password (8 or more characters). After that the network needs the password, and one phone can be connected at a time.
3. **Add a contact:** tap a device in **Nearby**, or type the 7 characters after `CA53Ycomms-`. Someone who messages you first is added automatically.
4. **Encrypt a conversation:** set a **Shared PIN** on the contact. They must set the same PIN for you. Messages are AES-128-GCM encrypted; relays pass them on but can't read them.

Messages are kept in memory and clear on restart. Name, password and contacts are saved. Hold **BOOT** for 5 seconds to erase everything; the device ID is kept.

LED: blinks while you have unread messages. On a relay, it flashes each time it repeats something.

## How repeating works

Every CA53Y packet starts with the same 51-byte header: magic bytes `0xCA 0x53`, protocol version 2, type, flags, sender, recipient, message ID, hops left and a resend counter. A board that hears a message or receipt that isn't addressed to it lowers the hop count by one and rebroadcasts the packet unchanged after a short random delay. It skips anything it has already repeated. Messages can make up to 8 hops. Location beacons (the Nearby list) aren't repeated.

Messengers repeat too, even before they're set up, so every board you add extends the mesh. Encryption covers the message and the header except the hop count and resend counter, which change in flight, so relays never break encrypted messages.

## Works with

- [CA53Y Comms Display](https://github.com/colinjcasey/CA53Y-Comms-Display) for the 2.8 inch ESP32 touch display. Direct messages work both ways, including PINs. The messenger doesn't have channels, but it repeats channel messages.
- Anything else that speaks the protocol. The full packet format is in the display repo's [PROTOCOL.md](https://github.com/colinjcasey/CA53Y-Comms-Display/blob/main/PROTOCOL.md).

All devices must use the same `ESPNOW_CHANNEL` (1), `MAX_HOPS` (8) and `LONG_RANGE` setting. The display firmware is Standard only for now.

## Troubleshooting

| Problem                                | Fix                                                                                                 |
|----------------------------------------|-----------------------------------------------------------------------------------------------------|
| No serial port appears                 | Use a data cable; install the CP210x or CH340 driver your board needs                               |
| Can't find the Wi-Fi network           | You flashed the Relay firmware. Flash Messenger instead                                             |
| Two boards can't see each other        | Check both are on the same network (Standard vs Long Range): portal header or serial boot line      |
| Forgot the Wi-Fi password              | Hold BOOT for 5 seconds to erase the board, then set it up again                                    |
| "Not delivered"                        | Out of range with no repeater in between. Add a relay, or move closer                               |
| Other device never shows up            | It needs v2 firmware, the same channel, and Settings > "Show this device in other people's Nearby list" on |

## Publishing the web flasher

The `docs/` folder is a ready-made flasher page. In the GitHub repo: Settings > Pages > Source **Deploy from a branch**, branch `main`, folder **/docs**. The page will be at `https://<your-user>.github.io/<repo>/`.

When you change the firmware, rebuild and replace the files in `docs/firmware/`, or push a tag like `v1.0.1` and download the files the **Build firmware** action attaches to the release.

## License

MIT. See [LICENSE](LICENSE). The CA53Y name and logo belong to CA53Y IND.
