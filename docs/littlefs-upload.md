# Uploading web files to ESP32 LittleFS

The dashboard (HTML, CSS, JS) lives in `firmware/gateway-esp32/data/`.
These files must be uploaded to the ESP32's flash filesystem separately from the sketch.

## Step 1: Install the upload plugin

Arduino IDE 2.x needs a plugin to upload LittleFS data.

1. Download the latest `.vsix` file from:
   https://github.com/lorol/arduino-esp32fs-plugin/releases
   
   If that plugin doesn't support IDE 2.x, use this one instead:
   https://github.com/earlephilhower/arduino-littlefs-upload/releases

2. Find your Arduino IDE plugins folder:
   - Windows: `%USERPROFILE%\.arduinoIDE\plugins\`
   - If the `plugins` folder doesn't exist, create it

3. Copy the `.vsix` file into that folder

4. Restart Arduino IDE

## Step 2: Upload the web files

1. Open `firmware/gateway-esp32/gateway-esp32.ino` in Arduino IDE
2. Connect the ESP32 via USB
3. Select the correct board and port
4. Press `Ctrl+Shift+P` (command palette)
5. Type "Upload LittleFS" and select it
6. Wait for the upload to finish (you'll see progress in the output panel)

## Step 3: Upload the sketch

After the filesystem is uploaded, compile and upload the sketch as normal
(the Upload button or Ctrl+U).

## File structure

```
firmware/gateway-esp32/
  data/                    <-- this folder becomes the LittleFS filesystem
    index.html             <-- dashboard page
    css/style.css          <-- styles
    js/app.js              <-- dashboard logic
    config/                <-- will hold JSON config files (Phase 2)
  gateway-esp32.ino        <-- the sketch
  device.h
  secrets.h
```

## Troubleshooting

- If the page shows "Not found", the LittleFS upload didn't work. Re-upload.
- If the page loads but shows "Lost connection", the sketch is running but
  the API isn't responding. Check Serial Monitor at 115200 baud.
- The ESP32's LittleFS partition is about 1.5 MB, plenty for the web files.
