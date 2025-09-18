#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include <ctype.h>
#include "eyes.h"

constexpr uint32_t ANIMATION_TASK_STACK_SIZE = 4096;
constexpr uint32_t SOUND_TASK_STACK_SIZE = 2048;
constexpr UBaseType_t ANIMATION_TASK_PRIORITY = 1;
constexpr UBaseType_t SOUND_TASK_PRIORITY = 1;
constexpr size_t SOUND_DATA_BUFFER_SIZE = 512;
constexpr int SOUND_PWM_CHANNEL = 0;
constexpr int SOUND_RESOLUTION = 8;                     // 8 bit resolution
constexpr int SOUND_ON = (1 << (SOUND_RESOLUTION - 1)); // 50% duty cycle
constexpr int SOUND_OFF = 0;                            // 0% duty cycle

AsyncWebServer server(WEB_SERVER_PORT);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);
HTTPClient http;

TaskHandle_t animationTaskHandle = nullptr;
TaskHandle_t soundTaskHandle = nullptr;
TaskHandle_t wakingUpAnimationTaskHandle = nullptr;

volatile bool touchDetected = false;
volatile bool touchRequestInProgress = false;

unsigned long lastTouchTime = 0;

/**
 * @brief Plays a tone on a specified pin.
 * @param pin The pin to play the tone on.
 * @param frequency The frequency of the tone in Hz.
 * @param duration The duration of the tone in milliseconds.
 */
void playTone(int pin, int frequency, int duration)
{
    if (frequency == 0)
    {
        vTaskDelay(pdMS_TO_TICKS(duration));
        return;
    }
    ledcSetup(SOUND_PWM_CHANNEL, frequency, SOUND_RESOLUTION); // Set up PWM channel
    ledcAttachPin(pin, SOUND_PWM_CHANNEL);                     // Attach channel to pin
    ledcWrite(SOUND_PWM_CHANNEL, SOUND_ON);
    vTaskDelay(pdMS_TO_TICKS(duration));
    ledcWrite(SOUND_PWM_CHANNEL, SOUND_OFF);
    ledcDetachPin(pin);
}

/**
 * @brief Initializes the OLED display.
 */
void initializeDisplay()
{
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        while (true)
            ; // Halt execution
    }
    display.clearDisplay();
}

/**
 * @brief Sets up WiFi connection.
 */
void connectToWiFi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi...");
    // Wait for connection.
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    String ipAddress = WiFi.localIP().toString();
    Serial.println("\r\nConnected!");
    Serial.print("IP Address: ");
    Serial.println(ipAddress);
}

/**
 * @brief Converts a hex string to a byte array.
 * @param hex The input hex string (lowercase).
 * @param out The output byte array.
 * @param outMax The maximum size of the output array.
 * @return The number of bytes written, or 0 on error.
 */
static size_t hexToBytes(const String &hex, uint8_t *out, size_t outMax)
{
    size_t len = hex.length();
    if (len % 2 != 0)
        return 0; // Not an even number of hex digits

    size_t outIndex = 0;
    for (size_t i = 0; i < len; i += 2)
    {
        char hi_c = hex[i];
        char lo_c = hex[i + 1];

        int hi, lo;

        if (hi_c >= '0' && hi_c <= '9')
            hi = hi_c - '0';
        else if (hi_c >= 'a' && hi_c <= 'f')
            hi = 10 + (hi_c - 'a');
        else
            return 0; // Invalid character

        if (lo_c >= '0' && lo_c <= '9')
            lo = lo_c - '0';
        else if (lo_c >= 'a' && lo_c <= 'f')
            lo = 10 + (lo_c - 'a');
        else
            return 0; // Invalid character

        out[outIndex++] = (hi << 4) | lo;
    }
    return outIndex;
}

/**
 * @brief Task to generate and display eye animations on the OLED screen.
 * @param pvParameters A pointer to a String object containing a single hex-encoded blob of eye parameters.
 *                     Each frame is represented by 5 floats: pupil_y, pupil_x, eyebrows_low, pupil_size, eyebrow_angle.
 *                     This task is responsible for deleting the String object.
 */
void animationTask(void *pvParameters)
{
    String *paramsHex = static_cast<String *>(pvParameters);

    // Each frame is defined by 5 floats.
    constexpr size_t NUM_PARAMS_PER_FRAME = 5;
    constexpr size_t FRAME_PARAMS_SIZE = NUM_PARAMS_PER_FRAME * sizeof(float);

    // We can't know the number of frames in advance, so we'll allocate a reasonably large buffer for the decoded data.
    // Let's use the same size as before, which was MAX_ANIMATION_FRAMES * FRAME_BUFFER_SIZE.
    // This should be more than enough for the parameters.
    static uint8_t decodedData[MAX_ANIMATION_FRAMES * FRAME_BUFFER_SIZE];

    size_t decodedLen = hexToBytes(*paramsHex, decodedData, sizeof(decodedData));
    Serial.printf("Decoding %u chars of hex data for eye animation...\r\n", paramsHex->length());
    if (decodedLen == 0)
    {
        Serial.printf("Decoding hex of eye animation failed.\r\n");
        goto cleanup;
    }

    // Buffer for one frame of the eye image
    static unsigned char frameBuffer[FRAME_BUFFER_SIZE];

    // Draw closed eyes at the beginning
    Eyes::draw_closed(frameBuffer);
    display.clearDisplay();
    display.drawBitmap(0, 0, frameBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    display.display();
    vTaskDelay(pdMS_TO_TICKS(100));

    // Draw half-open eyes
    Eyes::draw_half_open(frameBuffer);
    display.clearDisplay();
    display.drawBitmap(0, 0, frameBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    display.display();
    vTaskDelay(pdMS_TO_TICKS(100));

    // Process each set of frame parameters from the decoded data
    for (size_t i = 0; i + FRAME_PARAMS_SIZE <= decodedLen; i += FRAME_PARAMS_SIZE)
    {
        float *params = reinterpret_cast<float *>(decodedData + i);
        float pupil_y = params[0];
        float pupil_x = params[1];
        float eyebrows_low = params[2];
        float pupil_size = params[3];
        float eyebrow_angle = params[4];

        // Generate the eye image for the current frame
        Eyes::draw_open(pupil_y, pupil_x, eyebrows_low, pupil_size, eyebrow_angle, frameBuffer);

        // Display the generated frame
        display.clearDisplay();
        display.drawBitmap(0, 0, frameBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
        display.display();
        vTaskDelay(pdMS_TO_TICKS(FRAME_DELAY_MS));
    }

    // Draw half-open eyes at the end
    Eyes::draw_half_open(frameBuffer);
    display.clearDisplay();
    display.drawBitmap(0, 0, frameBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    display.display();
    vTaskDelay(pdMS_TO_TICKS(100));

    // Draw closed eyes at the end
    Eyes::draw_closed(frameBuffer);
    display.clearDisplay();
    display.drawBitmap(0, 0, frameBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    display.display();
    vTaskDelay(pdMS_TO_TICKS(200));

cleanup:
    delete paramsHex;
    animationTaskHandle = nullptr;
    vTaskDelete(nullptr); // Task deletes itself
}

/**
 * @brief Task to play a sound on the buzzer.
 * @param pvParameters A pointer to a String object containing hex encoded sound data.
 *                     Sound data format (hex bytes): [freq_high, freq_low, dur_high, dur_low, ...]
 *                     This task is responsible for deleting the String object.
 */
void soundTask(void *pvParameters)
{
    String *soundHex = static_cast<String *>(pvParameters);

    uint8_t soundData[SOUND_DATA_BUFFER_SIZE];
    size_t decodedLen = hexToBytes(*soundHex, soundData, sizeof(soundData));
    Serial.printf("Decoding %u chars of hex data for sound...\r\n", soundHex->length());
    if (decodedLen == 0)
    {
        Serial.printf("Decoding hex of sound data failed.\r\n");
        goto cleanup;
    }

    // Sound data consists of pairs of (uint16_t frequency, uint16_t duration)
    for (size_t i = 0; i + 3 < decodedLen; i += 4)
    {
        uint16_t freq = (soundData[i] << 8) | soundData[i + 1];
        uint16_t duration = (soundData[i + 2] << 8) | soundData[i + 3];
        playTone(BUZZER_PIN, freq, duration);
    }

cleanup:
    delete soundHex;
    soundTaskHandle = nullptr;
    vTaskDelete(nullptr); // Task deletes itself
}

/**
 * @brief Task to display a "waking up" indicator (Z's) on the OLED screen.
 *        This task will run for a maximum of 3 minutes and then terminate itself.
 * @param pvParameters Not used.
 */
void wakingUpAnimationTask(void *pvParameters)
{
    unsigned long startTime = millis();

    const int num_steps = 3;
    int current_step = 0;

    // Base position for the smallest 'z'
    const int base_x = 118;
    const int base_y = 18;

    while (millis() - startTime < TOUCH_TIMEOUT_MS)
    {
        // Clear the area for the animation
        display.fillRect(118, 0, 10, 25, SSD1306_BLACK);

        int x, y;

        switch (current_step)
        {
        case 0: // Smallest Z (5x5)
            x = base_x;
            y = base_y;
            display.drawLine(x, y, x + 4, y, SSD1306_WHITE);
            display.drawLine(x + 4, y, x, y + 4, SSD1306_WHITE);
            display.drawLine(x, y + 4, x + 4, y + 4, SSD1306_WHITE);
            break;
        case 1: // Medium Z (6x6)
            x = base_x + 1;
            y = base_y - 7;
            display.drawLine(x, y, x + 5, y, SSD1306_WHITE);
            display.drawLine(x + 5, y, x, y + 5, SSD1306_WHITE);
            display.drawLine(x, y + 5, x + 5, y + 5, SSD1306_WHITE);
            break;
        case 2: // Largest Z (7x7)
            x = base_x + 2;
            y = base_y - 14;
            display.drawLine(x, y, x + 6, y, SSD1306_WHITE);
            display.drawLine(x + 6, y, x, y + 6, SSD1306_WHITE);
            display.drawLine(x, y + 6, x + 6, y + 6, SSD1306_WHITE);
            break;
        }

        display.display();

        current_step = (current_step + 1) % num_steps;
        vTaskDelay(pdMS_TO_TICKS(300 + (current_step == 0 ? 200 : 0)));
    }

    // Final clear of the area when task finishes
    display.fillRect(118, 0, 10, 25, SSD1306_BLACK);
    display.display();

    wakingUpAnimationTaskHandle = nullptr;
    touchRequestInProgress = false;
    vTaskDelete(nullptr); // Task deletes itself
}

/**
 * @brief Stops an existing task if it's running and starts a new one.
 * @param taskCode Pointer to the function to be executed by the task.
 * @param taskName A descriptive name for the task.
 * @param stackSize The size of the task stack in words.
 * @param parameter A value that will be passed as the task's parameter.
 * @param priority The priority at which the task should run.
 * @param taskHandle A pointer to the handle of the task being managed.
 */
void startTask(TaskFunction_t taskCode, const char *taskName, uint32_t stackSize, void *parameter, UBaseType_t priority, TaskHandle_t *taskHandle)
{
    // If a task is already running, delete it.
    // The task is responsible for freeing its own memory (e.g., the String parameter).
    // This design assumes the old task will be killed before it can clean up,
    // leading to a memory leak of the parameter. For this project's scope,
    // we accept this risk. A more robust solution would use a queue to pass data
    // and signal the task to terminate and clean up gracefully.
    if (*taskHandle != nullptr)
    {
        vTaskDelete(*taskHandle);
        *taskHandle = nullptr; // Nullify the handle after deletion
    }

    // Create the new task
    xTaskCreate(
        taskCode,
        taskName,
        stackSize,
        parameter,
        priority,
        taskHandle);
}

/**
 * @brief Handles a web request, extracts a parameter, and starts a corresponding task.
 * @param request The HTTP request object.
 * @param paramName The name of the parameter to extract from the request.
 * @param taskCode Pointer to the function to be executed by the task.
 * @param taskName A descriptive name for the task.
 * @param stackSize The size of the task stack in words.
 * @param priority The priority at which the task should run.
 * @param taskHandle A pointer to the handle of the task being managed.
 */
void handleTaskRequest(AsyncWebServerRequest *request, const char *paramName, TaskFunction_t taskCode, const char *taskName, uint32_t stackSize, UBaseType_t priority, TaskHandle_t *taskHandle)
{
    // Stop the waking up animation if it's running
    if (wakingUpAnimationTaskHandle != nullptr)
    {
        vTaskDelete(wakingUpAnimationTaskHandle);
        wakingUpAnimationTaskHandle = nullptr;
        touchRequestInProgress = false;
        // Clear the 'Z' area after stopping the task
        display.fillRect(85, 0, 25, 30, SSD1306_BLACK);
        display.display();
    }

    if (request->hasParam(paramName, true))
    {
        String *data = new String(request->getParam(paramName, true)->value());
        startTask(taskCode, taskName, stackSize, (void *)data, priority, taskHandle);
        request->send(200, "text/plain", "OK");
    }
    else
    {
        String errorMsg = "Bad Request: '";
        errorMsg += paramName;
        errorMsg += "' parameter missing.";
        request->send(400, "text/plain", errorMsg);
    }
}

/**
 * @brief Configures and starts the asynchronous web server.
 */
void setupWebServer()
{
    server.on("/draw", HTTP_POST, [](AsyncWebServerRequest *request)
              { handleTaskRequest(request, "frames", animationTask, "Animation Task", ANIMATION_TASK_STACK_SIZE, ANIMATION_TASK_PRIORITY, &animationTaskHandle); });

    server.on("/play", HTTP_POST, [](AsyncWebServerRequest *request)
              { handleTaskRequest(request, "sound", soundTask, "Sound Task", SOUND_TASK_STACK_SIZE, SOUND_TASK_PRIORITY, &soundTaskHandle); });

    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(404, "text/plain", "Not found"); });

    server.begin();
    Serial.println("HTTP server started.");
}

/**
 * @brief Sends an HTTP GET request when the touch sensor is activated.
 */
void sendTouchRequest()
{
    touchRequestInProgress = true;
    // Start the waking up animation
    startTask(wakingUpAnimationTask, "Waking Up Animation", 2048, NULL, 1, &wakingUpAnimationTaskHandle);
    Serial.println("Touch detected! Sending GET request...");

    http.begin(TOUCH_TARGET_URL);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
        String payload = http.getString();
        Serial.println("Payload: " + payload);
    }
    else
    {
        Serial.printf("GET failed, error: %s\r\n", http.errorToString(httpCode).c_str());
        vTaskDelete(wakingUpAnimationTaskHandle);
        wakingUpAnimationTaskHandle = nullptr;
        touchRequestInProgress = false;
    }
    http.end();
}

void IRAM_ATTR handleTouchInterrupt()
{
    touchDetected = true;
}

void setup()
{
    Serial.begin(115200);

    pinMode(TOUCH_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TOUCH_PIN), handleTouchInterrupt, FALLING);

    initializeDisplay();
    connectToWiFi();
    setupWebServer();

    Serial.println("Setup complete. Server is running.");

    // Draw initial closed eyes.
    static unsigned char frameBuffer[FRAME_BUFFER_SIZE];
    Eyes::draw_closed(frameBuffer);
    display.clearDisplay();
    display.drawBitmap(0, 0, frameBuffer, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
    display.display();
}

void loop()
{
    // Check if the interrupt has been triggered
    if (touchDetected)
    {
        touchDetected = false; // Reset the flag
        if (!touchRequestInProgress && (millis() - lastTouchTime > TOUCH_DEBOUNCE_MS))
        {
            lastTouchTime = millis();
            sendTouchRequest();
        }
    }
    // Async web server and FreeRTOS tasks run in the background.
}