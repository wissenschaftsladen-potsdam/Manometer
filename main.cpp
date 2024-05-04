#include <ESP8266WiFi.h>
#include <Servo.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// Define the servo object and pins for servo and beep
Servo ObjServo;
static const int ServoGPIO = D4;
static const int beepPin = D3;
static const int beepPinGround = D2;

// Create an ESP8266WebServer object on port 80
ESP8266WebServer server(80);

// Define variables for storing header and sensor values
String header;
String valueString = String(0);
String pressureValueString = String(0);
int positon1 = 0;
int positon2 = 0;

// Define DNS server and access point IP address
const char DNS_PORT = 53;
DNSServer dnsServer;
IPAddress apIP(192, 168, 112, 1);

// Initialize a string for temporary use
String temp = "";

// Function to check if a string is an IP address
boolean isIp(String str)
{
    for (int i = 0; i < str.length(); i++)
    {
        int c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9'))
        {
            return false;
        }
    }
    return true;
}

// Function to convert IP address to string
String toStringIp(IPAddress ip)
{
    String res = "";
    for (int i = 0; i < 3; i++)
    {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}

// Function to redirect to captive portal for non-IP requests
bool captivePortal()
{
    if (!isIp(server.hostHeader()))
    {
        Serial.println("Request redirected to captive portal");
        server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
        server.send(302, "text/plain", "");
        server.client().stop();
        return true;
    }
    return false;
}

// Function to handle 404 Not Found errors
void handleNotFound()
{
    if (captivePortal())
    {
        return;
    }

    String requestedURL = server.uri();

    String temp = "<!DOCTYPE HTML>";
    temp += "<html><head>";
    temp += "<title>404 Not Found</title>";
    temp += "</head><body>";
    temp += "<h1>404 Not Found</h1>";
    temp += "<p>The requested URL '" + requestedURL + "' was not found on this server.</p>";
    temp += "</body></html>";

    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(temp.length());

    server.send(404, "text/html", temp);
    server.client().stop();
}

void handleRoot()
{
    String temp = "";
    // HTML Header
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);

    // HTML Content
    temp += "<!DOCTYPE html>";
    temp += "<html>";
    temp += "<head>";
    temp += "<title>Manometer</title>";
    temp += "<style>";
    temp += "canvas {border: none;} body {display: flex;flex-direction: column;justify-content: center;max-width: 400px;zoom: 200%;} #menue {display: flex;justify-content: center;gap: 30px;} #anzeige {position: relative;top: -140px;border-top: 1px black solid;padding-top: 10px;width: 400px;margin: 0 auto;} #anzeige::before {content: 'RESTDRUCK: ';} #timerDisplay {position: relative;top: -120px;margin: 0 auto;width: 400px;} #startpress {position: relative;top: -100px;border-top: 1px solid lightgrey;width: 400px;margin: 0 auto;padding-top: 10px;} #startpress input {width: 200px;margin: 20px;} #p_rate {position: relative;top: -90px;margin: 0 auto;} #p_rate input {width: 200px;margin: 20px;} #startbutton {width: 25%;margin: 15px;padding: 30px;top: -80px;position: relative;} #buttonPause {width: 25%;margin: 15px;padding: 30px;top: -80px;position: relative;} #resetbutton {width: 25%;margin: 15px;padding: 30px;top: -80px;position: relative;} #menue {margin: 20px;} #timerSettings {display: none;} #timerDiv {display: initial;position: relative;top: -100px;margin: 0 auto;display: none;gap: 10px;border-top: 1px solid black;width: 400px;justify-content: center;padding: 5px;} #timerDiv label {margin: auto;margin-left: 0px;} #buttonDiv {display: flex;justify-content: center;} #minutesField {height: 50px;} #secondsField {display: inline;height: 50px;} #settingsPopup button{margin: 10px;}";
    temp += "</style>";
    temp += "<meta charset='utf-8'>";
    temp += "</head>";
    temp += "<body onload='init()'>";
    temp += "<div id='menue'>";
    temp += "<select id='functionSelect' onchange='toggleFunction()'>";
    temp += "<option value='current'>Barometer Function</option>";
    temp += "<option value='timer'>Timer Function</option>";
    temp += "</select>";
    temp += "</div>";
    temp += "<canvas id='manometer' width='400' height='400'></canvas>";
    temp += "<div id='anzeige'></div>";
    temp += "<div id='timerDisplay'>Verstrichene Zeit: 00:00</div>";
    temp += "<div id='timerDiv'>";
    temp += "<label>Timer (Min:Sec):</label>";
    temp += "</div>";
    temp += "<script>";
    temp += "// Get the canvas element and its 2D context";
    temp += "var canvas = document.getElementById('manometer');";
    temp += "var context = canvas.getContext('2d');";
    temp += "// Define variables for the center of the canvas, radius, and pressure range";
    temp += "var x = canvas.width / 2;";
    temp += "var y = canvas.height / 2;";
    temp += "var radius = canvas.width / 2 - 10;";
    temp += "var minBar = 0;";
    temp += "var maxBar = 300;";
    temp += "var rotBereich = 50; // Rotation range for the red section";
    temp += "// Initialize timer and interval-related variables";
    temp += "var timerId = null;";
    temp += "var elapsedTime = 0;";
    temp += "var intervalId = null;";
    temp += "var paused = false;";
    temp += "var currentPressure;";
    temp += "var countdownInterval; // Variable to store the interval for the countdown";
    temp += "var remainingTime; // Variable to store the remaining time";
    temp += "// FUNCTION TO DRAW THE MANOMETER ON THE CANVAS";
    temp += "// Function to draw the manometer gauge on the canvas, including outer circle, pressure indicators,";
    temp += "// inner circle, and red section to indicate low pressure.";
    temp += "// Utilizes variables: context, canvas, x, y, radius, minBar, maxBar, rotBereich.";
    temp += "// - context: the 2D rendering context of the canvas";
    temp += "// - canvas: the canvas element";
    temp += "// - x, y: coordinates of the center of the gauge";
    temp += "// - radius: radius of the gauge";
    temp += "// - minBar, maxBar: minimum and maximum pressure values";
    temp += "// - rotBereich: range indicating low pressure";
    temp += "function drawManometer() {";
    temp += "    // Clear the canvas";
    temp += "    context.clearRect(0, 0, canvas.width, canvas.height);";
    temp += "    // Draw outer circle";
    temp += "    context.beginPath();";
    temp += "    context.arc(x, y, radius + 10, Math.PI, 0, false);\n";
    temp += "    context.lineWidth = 2;";
    temp += "    context.strokeStyle = '#000000';";
    temp += "    context.stroke();";
    temp += "    // Draw pressure indicators";
    temp += "    context.font = '20px Arial';";
    temp += "    context.textAlign = 'center';";
    temp += "    context.textBaseline = 'middle';";
    temp += "    for (var i = 0; i <= 6; i++) {";
    temp += "        var grad = i * 50;";
    temp += "        var angle = (((grad / (maxBar - minBar)) * (Math.PI)) - (Math.PI));";
    temp += "        var xGrad = x + Math.cos(angle) * (radius - 20);";
    temp += "        var yGrad = y + Math.sin(angle) * (radius - 20);";
    temp += "        context.fillText(grad.toString(), xGrad, yGrad);";
    temp += "    }";
    temp += "    // Draw inner circle";
    temp += "    context.beginPath();";
    temp += "    context.arc(x, y, radius + 5, Math.PI, 0, false);";
    temp += "    context.lineWidth = 5;";
    temp += "    context.strokeStyle = '#000000';";
    temp += "    context.stroke();";
    temp += "    // Draw red section to indicate low pressure";
    temp += "    context.beginPath();";
    temp += "    context.arc(x, y, radius - 3, (((50 - rotBereich) / (maxBar - minBar)) * (Math.PI)) + Math.PI, ((50 / (maxBar - minBar)) * (Math.PI)) + Math.PI, false);";
    temp += "    context.lineWidth = 10;";
    temp += "    context.strokeStyle = '#FF0000';";
    temp += "    context.stroke();";
    temp += "}";
    temp += "// Create sliders for setting start pressure and pressure drop rate\n";
    temp += "var startDruckSlider = document.createElement('input');";
    temp += "startDruckSlider.setAttribute('type', 'range');";
    temp += "startDruckSlider.setAttribute('min', minBar);";
    temp += "startDruckSlider.setAttribute('max', maxBar);";
    temp += "startDruckSlider.setAttribute('value', maxBar);";
    temp += "startDruckSlider.setAttribute('step', '10');";
    temp += "startDruckSlider.setAttribute('style', 'width: 200px;');";
    temp += "startDruckSlider.oninput = function () {";
    temp += "    startDruck = parseInt(this.value);";
    temp += "    drawZeiger(startDruck);";
    temp += "};";
    temp += "// Create div to contain start pressure slider\n";
    temp += "var startDruckText = document.createTextNode('Start Pressure (bar): ');";
    temp += "var startDruckDiv = document.createElement('div');";
    temp += "startDruckDiv.setAttribute('id', 'startpress');";
    temp += "startDruckDiv.appendChild(startDruckText);";
    temp += "startDruckDiv.appendChild(startDruckSlider);";
    temp += "document.body.appendChild(startDruckDiv);";
    temp += "// Create slider for setting pressure drop rate\n";
    temp += "var druckAbfallSlider = document.createElement('input');";
    temp += "druckAbfallSlider.setAttribute('type', 'range');";
    temp += "druckAbfallSlider.setAttribute('min', '1');";
    temp += "druckAbfallSlider.setAttribute('max', '10');";
    temp += "druckAbfallSlider.setAttribute('value', '1');";
    temp += "druckAbfallSlider.setAttribute('step', '1');";
    temp += "druckAbfallSlider.setAttribute('style', 'width: 200px;');";
    temp += "druckAbfallSlider.oninput = function () {";
    temp += "druckAbfallRate = parseInt(this.value);";
    temp += "};";
    temp += "// Create text node for displaying pressure drop rate label\n";
    temp += "var druckAbfallText = document.createTextNode('Pressure Drop Rate (bar/s): ');";
    temp += "// Create div for containing pressure drop rate slider";
    temp += "var druckAbfallDiv = document.createElement('div');";
    temp += "druckAbfallDiv.setAttribute('id', 'p_rate');";
    temp += "druckAbfallDiv.appendChild(druckAbfallText);";
    temp += "druckAbfallDiv.appendChild(druckAbfallSlider);";
    temp += "document.body.appendChild(druckAbfallDiv);";
    temp += "// Create and append the start button";
    temp += "var startButton = document.createElement('button');";
    temp += "startButton.innerHTML = 'Start';";
    temp += "startButton.setAttribute('id', 'startbutton');";
    temp += "startButton.addEventListener('click', handleStartButtonClickBarometer);";
    temp += "// Create pause button (initially hidden)";
    temp += "var pauseButton = document.createElement('button');";
    temp += "pauseButton.innerHTML = 'Pause';";
    temp += "pauseButton.setAttribute('id', 'buttonPause');";
    temp += "pauseButton.style.display = 'none';";
    temp += "pauseButton.addEventListener('click', handlePauseButtonClick);";
    temp += "document.body.appendChild(pauseButton);";
    temp += "// Create reset button and append it to the document body";
    temp += "var resetButton = document.createElement('button');";
    temp += "resetButton.setAttribute('id', 'resetbutton');";
    temp += "resetButton.innerHTML = 'Reset';";
    temp += "resetButton.addEventListener('click', resetManometer);";
    temp += "document.body.appendChild(resetButton);";
    temp += "// Create a div to contain the buttons";
    temp += "var buttonDiv = document.createElement('div');";
    temp += "buttonDiv.setAttribute('id', 'buttonDiv');";
    temp += "buttonDiv.appendChild(startButton);";
    temp += "buttonDiv.appendChild(pauseButton);";
    temp += "buttonDiv.appendChild(resetButton);";
    temp += "document.body.appendChild(buttonDiv);";
    temp += "// Set the frames per second";
    temp += "var fps = 60;";
    temp += "// Set default start pressure to 300";
    temp += "var startDruck = 300;";
    temp += "// Set default pressure drop rate to 1";
    temp += "var druckAbfallRate = 1;";
    temp += "// Initialize the pressure bar";
    temp += "var bar = startDruck;";
    temp += "// Function to draw the pointer on the gauge based on the current pressure<br>";
    temp += "// Function to draw the pointer on the gauge based on the current pressure.";
    temp += "// Utilizes variables: context, canvas, x, y, radius, minBar, maxBar.";
    temp += "// - context: the 2D rendering context of the canvas";
    temp += "// - canvas: the canvas element";
    temp += "// - x, y: coordinates of the center of the gauge";
    temp += "// - radius: radius of the gauge";
    temp += "// - minBar, maxBar: minimum and maximum pressure values";
    temp += "function drawZeiger(bar) {";
    temp += "    var angle = (((bar - minBar) / (maxBar - minBar)) * (Math.PI)) + Math.PI;";
    temp += "    var xZeiger = x + Math.cos(angle) * (radius - 70);";
    temp += "    var yZeiger = y + Math.sin(angle) * (radius - 70);";
    temp += "    context.beginPath();";
    temp += "    context.arc(x, y, radius - 65, 0, 2 * Math.PI);";
    temp += "    context.fillStyle = '#FFFFFF';";
    temp += "    context.fill();";
    temp += "    context.beginPath();";
    temp += "    context.moveTo(x, y);";
    temp += "    context.lineTo(xZeiger, yZeiger);";
    temp += "    context.lineWidth = 5;";
    temp += "    context.strokeStyle = '#FF0000';";
    temp += "    context.stroke();";
    temp += "    var anzeige = document.getElementById('anzeige');";
    temp += "    anzeige.innerHTML = bar.toFixed(2);";
    temp += "}";
    temp += "// Function to update the elapsed time display<br>";
    temp += "// Function to update the elapsed time display.";
    temp += "// Utilizes variables: elapsedTime.";
    temp += "// - elapsedTime: the total elapsed time in seconds";
    temp += "function updateElapsedTime() {";
    temp += "    var timeDisplay = document.getElementById('timerDisplay');";
    temp += "    var minutes = Math.floor(elapsedTime / 60);";
    temp += "    var seconds = elapsedTime % 60;";
    temp += "    timeDisplay.innerHTML = 'Elapsed Time: ' + (minutes < 10 ? '0' : '') + minutes + ':' + (seconds < 10 ? '0' : '') + seconds;";
    temp += "}";
    temp += "// Initialization function.";
    temp += "// Invokes drawManometer() and drawZeiger() functions to initialize the manometer and set the gauge pointer to the start pressure.";
    temp += "// Utilizes variables: startDruck.";
    temp += "// - startDruck: the initial pressure value";
    temp += "function init() {";
    temp += "    drawManometer();";
    temp += "    drawZeiger(startDruck); // Set the gauge pointer to the start pressure during initialization";
    temp += "}";
    temp += "function updateSSID() {";
    temp += "    fetch('/getSSID')";
    temp += "    .then(response => response.text())";
    temp += "    .then(data => {";
    temp += "        document.getElementById('settingsTextField').value = 'Current SSID: ' + data;";
    temp += "    })";
    temp += "    .catch(error => {";
    temp += "        console.error('Error:', error);";
    temp += "    });";
    temp += "}";
    temp += "// Function to open the settings popup.";
    temp += "// Creates a popup container with input field for SSID, a save button, and a hint for Wi-Fi connection.";
    temp += "// Utilizes variables: settingsPopup, settingsTextField.";
    temp += "// - settingsPopup: ID of the popup container element.";
    temp += "// - settingsTextField: ID of the input field for SSID.";
    temp += "// TODO this function listens for a click event on the 'Save' button to handle user input and calls closeSettingsPopup() to close the popup after saving. This function could benefit from adding a cancel button to close the popup without saving.";
    temp += "function openSettingsPopup() {";
    temp += "    updateSSID();";
    temp += "    // Creating the popup container";
    temp += "    var popupContainer = document.createElement('div');";
    temp += "    popupContainer.setAttribute('id', 'settingsPopup');";
    temp += "    popupContainer.style.position = 'fixed';";
    temp += "    popupContainer.style.top = '50%';";
    temp += "    popupContainer.style.left = '50%';";
    temp += "    popupContainer.style.transform = 'translate(-50%, -50%)';";
    temp += "    popupContainer.style.background = '#fff';";
    temp += "    popupContainer.style.padding = '20px';";
    temp += "    popupContainer.style.border = '2px solid #000';";
    temp += "    popupContainer.style.zIndex = '9999';";
    temp += "    popupContainer.style.height = '200px';";
    temp += "    // Creating the text field";
    temp += "    var textField = document.createElement('input');";
    temp += "    textField.setAttribute('type', 'text');";
    temp += "    textField.setAttribute('id', 'settingsTextField');";
    temp += "    textField.style.marginBottom = '10px';";
    temp += "    // Creating a label for the text field";
    temp += "    var label = document.createElement('label');";
    temp += "    label.setAttribute('for', 'settingsTextField');";
    temp += "    label.textContent = 'SSID: ';";
    temp += "    // Adding the label and text field to the popup container";
    temp += "    popupContainer.appendChild(label);";
    temp += "    popupContainer.appendChild(textField);";
    temp += "    updateSSID();";
    temp += "    // Hint for Wi-Fi connection";
    temp += "    var hint = document.createElement('p');";
    temp += "    hint.textContent = 'Note: After changing the SSID, the Wi-Fi connection must be reestablished.';";
    temp += "    hint.style.marginTop = '5px';";
    temp += "    popupContainer.appendChild(hint);";
    temp += "    // Creating the save button";
    temp += "    var saveButton = document.createElement('button');";
    temp += "    saveButton.innerHTML = 'Save';";
    temp += "    saveButton.addEventListener('click', function() {";
    temp += "        var userInput = document.getElementById('settingsTextField').value;";
    temp += "        // Send the SSID to the D1 Mini and restart it";
    temp += "        fetch('/saveSSID', {";
    temp += "            method: 'POST',";
    temp += "            headers: {";
    temp += "                'Content-Type': 'application/x-www-form-urlencoded',";
    temp += "            },";
    temp += "            body: 'ssid=' + encodeURIComponent(userInput)";
    temp += "        })";
    temp += "        .then(response => {";
    temp += "            if (!response.ok) {";
    temp += "                throw new Error('Network response was not ok');";
    temp += "            }";
    temp += "            console.log('SSID saved. Restarting...');";
    temp += "            // Optionally show a confirmation to the user or update the UI";
    temp += "            closeSettingsPopup(); // Close the popup after saving";
    temp += "        })";
    temp += "        .catch(error => {";
    temp += "            console.error('Error:', error);";
    temp += "        });";
    temp += "    });";
    temp += "// Creating the close button";
    temp += "    var closeButton = document.createElement('button');";
    temp += "    closeButton.innerHTML = 'Abbrechen';";
    temp += "    closeButton.addEventListener('click', function() {";
    temp += "        closeSettingsPopup(); // Close the popup when close button is clicked";
    temp += "    });";
    temp += "    // Adding the save and close buttons to the popup container";
    temp += "    popupContainer.appendChild(saveButton);";
    temp += "    popupContainer.appendChild(closeButton);";
    temp += "    // Adding the popup container to the page";
    temp += "    document.body.appendChild(popupContainer);";
    temp += "}";
    temp += "";
    temp += "// Function to close the settings popup.";
    temp += "// Removes the settings popup from the DOM if it exists.";
    temp += "// Uses variables: None.";
    temp += "function closeSettingsPopup() {";
    temp += "    var popup = document.getElementById('settingsPopup');";
    temp += "    if (popup) {";
    temp += "        popup.parentNode.removeChild(popup);";
    temp += "    }";
    temp += "}";
    temp += "";
    temp += "    // Erstellen des Einstellungen-Buttons";
    temp += "    var settingsButton = document.createElement('button');";
    temp += "    settingsButton.innerHTML = 'Einstellungen';";
    temp += "    settingsButton.addEventListener('click', openSettingsPopup);";
    temp += "    // Hinzuf�gen des Einstellungen-Buttons zur Men�-Div";
    temp += "    var menuDiv = document.getElementById('menue');";
    temp += "    menuDiv.appendChild(settingsButton);";
    temp += "";
    temp += "// Creating the settings button";
    temp += "var settingsButton = document.createElement('button');";
    temp += "settingsButton.innerHTML = 'Settings';";
    temp += "settingsButton.addEventListener('click', openSettingsPopup);";
    temp += "// Adding the settings button to the menu div";
    temp += "var menuDiv = document.getElementById('menu');";
    temp += "menuDiv.appendChild(settingsButton);";
    temp += "";
    temp += "// Creating the timer div";
    temp += "var timerDiv = document.createElement('div');";
    temp += "timerDiv.setAttribute('id', 'timerDiv');";
    temp += "timerDiv.style.display = 'none'; // Hide by default";
    temp += "document.body.appendChild(timerDiv);";
    temp += "// Function to start the countdown timer";
    temp += "// Function to start the countdown timer.";
    temp += "// Calculates the total remaining time in seconds based on the input fields for minutes and seconds.";
    temp += "// Calculates the steps for updating the barometer based on the total remaining time.";
    temp += "// Initializes variables for the current time and current bar value.";
    temp += "// Updates the display with the remaining time and current bar value.";
    temp += "// Starts the countdown interval if it's not already running.";
    temp += "// Updates the countdown every second, updating time, bar value, display, and pointer position.";
    temp += "// Stops the countdown and updates the display when the time is up.";
    temp += "// Uses variables: countdownInterval, paused, formatTime, drawZeiger.";
    temp += "function startCountdown() {";
    temp += "    // Minutes and seconds from input fields";
    temp += "    var minutes = parseInt(document.getElementById('minutesField').value);";
    temp += "    var seconds = parseInt(document.getElementById('secondsField').value);";
    temp += "    // Total remaining seconds";
    temp += "    var totalSeconds = minutes * 60 + seconds;";
    temp += "    // Starting pressure value for the bar";
    temp += "    var startBar = 300;";
    temp += "    // Calculate steps for the bar";
    temp += "    var steps = startBar / totalSeconds;";
    temp += "    // Update frequency in milliseconds";
    temp += "    var updateInterval = 1000;";
    temp += "    // Initial time for the countdown";
    temp += "    var currentTime = totalSeconds;";
    temp += "    // Initialize pointer";
    temp += "    var currentBarValue = startBar;";
    temp += "    // Start value for the display";
    temp += "    document.getElementById('timerDisplay').innerHTML = 'Remaining Time: ' + formatTime(currentTime);";
    temp += "    document.getElementById('anzeige').innerHTML = 'Bar: ' + Math.round(currentBarValue);";
    temp += "    // If a countdown is already running, don't start again";
    temp += "    if (!countdownInterval) {";
    temp += "        // Start countdown";
    temp += "        countdownInterval = setInterval(function() {";
    temp += "            // If not paused, update the countdown";
    temp += "            if (!paused) {";
    temp += "                // Update time";
    temp += "                currentTime--;";
    temp += "                // Update bar";
    temp += "                currentBarValue -= steps;";
    temp += "                // Update display";
    temp += "                document.getElementById('timerDisplay').innerHTML = 'Remaining Time: ' + formatTime(currentTime);";
    temp += "                document.getElementById('anzeige').innerHTML = 'Bar: ' + Math.round(currentBarValue);";
    temp += "                // Update pointer";
    temp += "                drawZeiger(currentBarValue);";
    temp += "                // If time is up, stop the countdown";
    temp += "                if (currentTime <= 0) {";
    temp += "                    clearInterval(countdownInterval);";
    temp += "                    document.getElementById('timerDisplay').innerHTML = 'Elapsed Time: ' + formatTime(0);";
    temp += "                    document.getElementById('anzeige').innerHTML = 'Bar: 0';";
    temp += "                    drawZeiger(0);";
    temp += "                }";
    temp += "            }";
    temp += "        }, updateInterval);";
    temp += "    }";
    temp += "}";
    temp += "";
    temp += "// Function to format time in MM:SS format.";
    temp += "// Converts total seconds into minutes and remaining seconds, then returns the formatted time.";
    temp += "// Uses the pad function to ensure two-digit formatting.";
    temp += "// Uses the pad function.";
    temp += "function formatTime(seconds) {";
    temp += "    var minutes = Math.floor(seconds / 60);";
    temp += "    var remainingSeconds = seconds % 60;";
    temp += "    return pad(minutes) + ':' + pad(remainingSeconds);";
    temp += "}";
    temp += "";
    temp += "// Function to pad single-digit numbers with leading zeros.";
    temp += "// Adds a leading zero to single-digit numbers to ensure two-digit formatting.";
    temp += "// Used by the formatTime function.";
    temp += "function pad(val) {";
    temp += "    return val < 10 ? '0' + val : val;";
    temp += "}";
    temp += "";
    temp += "// Function to pause the countdown.";
    temp += "// Sets the paused flag to true, indicating the countdown is paused.";
    temp += "function pauseCountdown() {";
    temp += "    paused = true;";
    temp += "}";
    temp += "";
    temp += "// Function to resume the countdown.";
    temp += "// Sets the paused flag to false, indicating the countdown is resumed.";
    temp += "function resumeCountdown() {";
    temp += "    paused = false;";
    temp += "}";
    temp += "// Event handler for the pause button click.";
    temp += "// Stops the countdown or barometer function based on the current state.";
    temp += "// If the button's text is 'Pause', it changes it to 'Resume' and saves the current pressure value.";
    temp += "// If the button's text is 'Resume', it changes it back to 'Pause' and resumes the barometer function with the saved pressure value.";
    temp += "function handlePauseButtonClick() {";
    temp += "    var pauseButton = document.getElementById('buttonPause');";
    temp += "    clearInterval(intervalId); // Stop countdown or barometer function";
    temp += "    if (pauseButton.innerHTML === 'Pause') {";
    temp += "        pauseButton.innerHTML = 'Resume';";
    temp += "        // Save the current pressure value";
    temp += "        currentPressure = bar;";
    temp += "    } else {";
    temp += "        pauseButton.innerHTML = 'Pause';";
    temp += "        // Resume the barometer function with the saved pressure value";
    temp += "        intervalId = setInterval(function () {";
    temp += "            if (!paused) {";
    temp += "                elapsedTime++;";
    temp += "                bar -= druckAbfallRate / fps;";
    temp += "                if (bar < minBar) {";
    temp += "                    bar = minBar;";
    temp += "                    clearInterval(intervalId);";
    temp += "                }";
    temp += "                drawZeiger(bar);";
    temp += "                updateElapsedTime();";
    temp += "            }";
    temp += "        }, 1000 / fps);";
    temp += "    }";
    temp += "}";
    temp += "";
    temp += "// Function to update the display and bar based on the remaining time and start pressure.";
    temp += "// It calculates the remaining time in seconds and updates the timer display.";
    temp += "// Then it calculates the bar value based on the remaining time and the start pressure, and updates the barometer display accordingly.";
    temp += "function updateDisplayAndBar(seconds, startBar) {";
    temp += "    // Update display";
    temp += "    document.getElementById('timerDisplay').innerHTML = 'Remaining Time: ' + formatTime(seconds);";
    temp += "    // Update bar";
    temp += "    var bar = startBar * (seconds / (parseInt(document.getElementById('minutesField').value) * 60));";
    temp += "    drawZeiger(bar);";
    temp += "    document.getElementById('anzeige').innerHTML = 'Bar: ' + Math.round(bar);";
    temp += "}";
    temp += "";
    temp += "// Function to reset the timer to its initial state.";
    temp += "// It calculates the remaining time based on the values entered in the input fields for minutes and seconds.";
    temp += "// Then it stops the countdown, clears the interval, and resets the display and event listeners for the start and pause buttons.";
    temp += "// Finally, it resets the pointer on the gauge to the default pressure value.";
    temp += "function resetTimer() {";
    temp += "    var minutes = parseInt(document.getElementById('minutesField').value);";
    temp += "    var seconds = parseInt(document.getElementById('secondsField').value);";
    temp += "    remainingTime = minutes * 60 + seconds;";
    temp += "    stopCountdown();";
    temp += "    clearInterval(intervalId);";
    temp += "    var pauseButton = document.getElementById('buttonPause');";
    temp += "    var startButton = document.getElementById('startbutton');";
    temp += "    pauseButton.style.display = 'none';";
    temp += "    startButton.style.display = 'block';";
    temp += "    pauseButton.innerHTML = 'Pause';";
    temp += "    startButton.removeEventListener('click', handleStartButtonClickTimer);";
    temp += "    startButton.addEventListener('click', handleStartButtonClickTimer);";
    temp += "    clearInterval(countdownInterval);";
    temp += "    countdownInterval = null;";
    temp += "    // Reset for the pointer";
    temp += "    drawZeiger(300);";
    temp += "}";
    temp += "";
    temp += "// Function to reset the manometer to its initial state.";
    temp += "// It resets the display for the manometer and the elapsed time to their initial values.";
    temp += "// Then it resets the internal bar value to the default pressure value and updates the pointer on the gauge accordingly.";
    temp += "// If the countdown is running, it stops it and hides the pause button while showing the start button.";
    temp += "function resetManometer() {";
    temp += "    // Reset the display to the initial value";
    temp += "    var timerDisplay = document.getElementById('timerDisplay');";
    temp += "    timerDisplay.innerHTML = 'Elapsed Time: 00:00';";
    temp += "    // Reset the display for the manometer to the initial value";
    temp += "    var anzeige = document.getElementById('anzeige');";
    temp += "    anzeige.innerHTML = startDruck;";
    temp += "    // Reset the internal bar value to the initial value";
    temp += "    bar = startDruck;";
    temp += "    // Set pointer to the start pressure";
    temp += "    drawZeiger(startDruck);";
    temp += "    // If the countdown is running, stop it";
    temp += "    stopCountdown();";
    temp += "    // Reset and hide the pause button";
    temp += "    var pauseButton = document.getElementById('buttonPause');";
    temp += "    pauseButton.innerHTML = 'Pause';";
    temp += "    pauseButton.style.display = 'none';";
    temp += "    // Show the start button";
    temp += "    var startButton = document.getElementById('startbutton');";
    temp += "    startButton.style.display = 'block';";
    temp += "}";
    temp += "// Function to handle the click event of the start button in the timer option.";
    temp += "// It initiates the countdown timer when the start button is clicked.";
    temp += "// It hides the start button and displays the pause button with its text set to 'Pause'.";
    temp += "// It adds an event listener to the pause button to handle its click event.";
    temp += "function handleStartButtonClickTimer() {";
    temp += "    // Logic for the start button click event in the timer option";
    temp += "    startCountdown(); // Example: startCountdown();";
    temp += "";
    temp += "    // Change the start button to a pause button";
    temp += "    var startButton = document.getElementById('startbutton');";
    temp += "    startButton.style.display = 'none'; // Hide start button";
    temp += "";
    temp += "    var pauseButton = document.getElementById('buttonPause');";
    temp += "    pauseButton.style.display = 'block'; // Show pause button";
    temp += "    pauseButton.innerHTML = 'Pause'; // Reset text to 'Pause'";
    temp += "";
    temp += "    // Add event listener for the pause button";
    temp += "    pauseButton.removeEventListener('click', handlePauseButtonTimer);";
    temp += "    pauseButton.addEventListener('click', handlePauseButtonTimer);";
    temp += "}";
    temp += "";
    temp += "// Function to handle the click event of the start button in the barometer option.";
    temp += "// It resets the manometer to its initial state.";
    temp += "// It retrieves the start pressure value and pressure drop rate from sliders.";
    temp += "// It resets the elapsed time.";
    temp += "// It starts the barometer function with a setInterval call to update pressure values.";
    temp += "// It hides the start button and displays the pause button with its text set to 'Pause'.";
    temp += "// It updates the event listener for the pause button to handle its click event.";
    temp += "function handleStartButtonClickBarometer() {";
    temp += "    // First reset the manometer";
    temp += "    resetManometer();";
    temp += "    // Start value for pressure";
    temp += "    var startDruck = parseFloat(startDruckSlider.value);";
    temp += "    // Pressure drop rate";
    temp += "    var druckAbfallRate = parseFloat(druckAbfallSlider.value);";
    temp += "    // Reset elapsed time";
    temp += "    elapsedTime = 0;";
    temp += "    // Start the barometer function";
    temp += "    intervalId = setInterval(function () {";
    temp += "        if (!paused) {";
    temp += "            elapsedTime++;";
    temp += "            bar -= druckAbfallRate / fps;";
    temp += "            if (bar < minBar) {";
    temp += "                bar = minBar;";
    temp += "                clearInterval(intervalId);";
    temp += "            }";
    temp += "            drawZeiger(bar);";
    temp += "            updateElapsedTime();";
    temp += "        }";
    temp += "    }, 1000 / fps);";
    temp += "    // Change the start button to a pause button";
    temp += "    var startButton = document.getElementById('startbutton');";
    temp += "    startButton.style.display = 'none'; // Hide start button";
    temp += "    var pauseButton = document.getElementById('buttonPause');";
    temp += "    pauseButton.style.display = 'block'; // Show pause button";
    temp += "    // Reset the pause button to 'Pause'";
    temp += "    pauseButton.innerHTML = 'Pause';";
    temp += "    // Update event listener for the pause button";
    temp += "    pauseButton.removeEventListener('click', handlePauseButtonClick);";
    temp += "    pauseButton.addEventListener('click', handlePauseButtonClick);";
    temp += "}";
    temp += "// Function to stop the countdown.";
    temp += "// It clears the interval to stop the countdown.";
    temp += "// It resets the display to show 'Remaining Time: 00:00'.";
    temp += "function stopCountdown() {";
    temp += "    clearInterval(intervalId); // Clear interval to stop the countdown";
    temp += "    var timerDisplay = document.getElementById('timerDisplay');";
    temp += "    timerDisplay.innerHTML = 'Remaining Time: 00:00'; // Reset display to zero";
    temp += "}";
    temp += "// Function for handling the click event of the pause button in the timer option.";
    temp += "// If the button text is 'Pause', it pauses the countdown timer and changes the button text to 'Continue'.";
    temp += "// If the button text is 'Continue', it resumes the countdown timer and changes the button text back to 'Pause'.";
    temp += "function handlePauseButtonTimer() {";
    temp += "    // Logic for the pause button click event in the timer option";
    temp += "    var pauseButton = document.getElementById('buttonPause');";
    temp += "    if (pauseButton.innerHTML === 'Pause') {";
    temp += "        // Pause the timer";
    temp += "        pauseCountdown(); // Pause timer";
    temp += "        pauseButton.innerHTML = 'Continue';";
    temp += "    } else {";
    temp += "        // Resume the timer";
    temp += "        resumeCountdown(); // Resume timer";
    temp += "        pauseButton.innerHTML = 'Pause';";
    temp += "    }";
    temp += "}";
    temp += "// Function for calculating the remaining time based on the values entered in the minutes and seconds input fields.";
    temp += "// Returns the total remaining time in seconds.";
    temp += "function calculateRemainingTime() {";
    temp += "    // Read minutes and seconds from input fields";
    temp += "    var minutes = parseInt(document.getElementById('minutesField').value);";
    temp += "    var seconds = parseInt(document.getElementById('secondsField').value);";
    temp += "    // Calculate total remaining seconds";
    temp += "    return minutes * 60 + seconds;";
    temp += "}";
    temp += "// Function to toggle between timer and barometer options based on the selected value in the dropdown menu.";
    temp += "// Retrieves DOM elements for necessary components such as the function select dropdown, timer div, input fields, and buttons.";
    temp += "// Determines the selected value from the dropdown menu.";
    temp += "// If 'timer' is selected:";
    temp += "// - Displays timer input fields (minutes and seconds) if they don't already exist.";
    temp += "// - Clears any existing interval for countdown.";
    temp += "// - Sets the display style of timer related elements to show them.";
    temp += "// - Hides barometer related elements.";
    temp += "// - Updates event listeners for start, reset, and pause buttons to handle timer functionality.";
    temp += "// If 'barometer' is selected:";
    temp += "// - Hides timer input fields.";
    temp += "// - Shows barometer related elements.";
    temp += "// - Stops the countdown if it's running.";
    temp += "// - Updates event listeners for start, reset, and pause buttons to handle barometer functionality.";
    temp += "// Ensures the start button is visible regardless of the selected option.";
    temp += "function toggleFunction() {";
    temp += "    // Retrieve DOM elements";
    temp += "    var functionSelect = document.getElementById('functionSelect'),";
    temp += "        selectedValue = functionSelect.options[functionSelect.selectedIndex].value,";
    temp += "        timerDiv = document.getElementById('timerDiv'),";
    temp += "        minutesField = document.getElementById('minutesField'),";
    temp += "        secondsField = document.getElementById('secondsField'),";
    temp += "        startButton = document.getElementById('startbutton'),";
    temp += "        resetButton = document.getElementById('resetbutton'),";
    temp += "        pauseButton = document.getElementById('buttonPause');";
    temp += "    // Check the selected option value";
    temp += "    if (selectedValue === 'timer') {";
    temp += "        // Display timer fields";
    temp += "        if (!minutesField) {";
    temp += "            minutesField = document.createElement('input');";
    temp += "            minutesField.setAttribute('type', 'number');";
    temp += "            minutesField.setAttribute('id', 'minutesField');";
    temp += "            minutesField.setAttribute('placeholder', 'Min');";
    temp += "            minutesField.setAttribute('min', '0');";
    temp += "            minutesField.setAttribute('max', '59');";
    temp += "            minutesField.setAttribute('value', '5');";
    temp += "            minutesField.style.display = 'inline';";
    temp += "            timerDiv.appendChild(minutesField);";
    temp += "        }";
    temp += "        if (!secondsField) {";
    temp += "            secondsField = document.createElement('input');";
    temp += "            secondsField.setAttribute('type', 'number');";
    temp += "            secondsField.setAttribute('id', 'secondsField');";
    temp += "            secondsField.setAttribute('placeholder', 'Sec');";
    temp += "            secondsField.setAttribute('min', '0');";
    temp += "            secondsField.setAttribute('max', '59');";
    temp += "            secondsField.setAttribute('value', '0');";
    temp += "            secondsField.style.display = 'inline';";
    temp += "            timerDiv.appendChild(secondsField);";
    temp += "        }";
    temp += "        clearInterval(intervalId);";
    temp += "        timerDiv.style.display = 'flex';";
    temp += "        document.getElementById('p_rate').style.display = 'none';";
    temp += "        document.getElementById('startpress').style.display = 'none';";
    temp += "        startButton.removeEventListener('click', handleStartButtonClickBarometer);";
    temp += "        startButton.addEventListener('click', handleStartButtonClickTimer);";
    temp += "        resetButton.removeEventListener('click', resetManometer);";
    temp += "        resetButton.addEventListener('click', resetTimer); // Assign resetTimer here";
    temp += "        pauseButton.removeEventListener('click', handlePauseButtonClick);";
    temp += "        pauseButton.addEventListener('click', handlePauseButtonTimer);";
    temp += "    } else {";
    temp += "        // Display barometer fields";
    temp += "        timerDiv.style.display = 'none';";
    temp += "        document.getElementById('p_rate').style.display = 'block';";
    temp += "        document.getElementById('startpress').style.display = 'block';";
    temp += "        stopCountdown();";
    temp += "        startButton.removeEventListener('click', handleStartButtonClickTimer);";
    temp += "        startButton.addEventListener('click', handleStartButtonClickBarometer);";
    temp += "        resetButton.removeEventListener('click', resetTimer); // Remove resetTimer here";
    temp += "        resetButton.addEventListener('click', resetManometer);";
    temp += "        pauseButton.removeEventListener('click', handlePauseButtonTimer);";
    temp += "        pauseButton.addEventListener('click', handlePauseButtonClick);";
    temp += "    }";
    temp += "    startButton.style.display = 'block'; // Ensure start button is visible";
    temp += "}";
    temp += "</script>";
    temp += "</body>";
    temp += "</html>";

    // Send HTML response
    server.send(200, "text/html", temp);

    // Clear temp to save memory
    temp = "";
}


// Function to initialize HTTP server and handle various routes
void InitializeHTTPServer()
{
    server.on("/", handleRoot);
    server.on("/generate_204", handleRoot);
    server.on("/favicon.ico", handleRoot);
    server.on("/fwlink", handleRoot);
    server.on("/generate_204", handleRoot);
    server.onNotFound(handleNotFound);
    server.begin();
}

void setup()
{
    Serial.begin(115200);
    pinMode(beepPinGround, OUTPUT);
    digitalWrite(beepPinGround, LOW);
    pinMode(beepPin, OUTPUT);
    digitalWrite(beepPin, LOW);
    ObjServo.attach(ServoGPIO, 500, 2800);
    Serial.print("Making connection to ");

    // Set up SoftAP for captive portal
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP("Drucksensor");
    Serial.println(WiFi.softAPIP());
    dnsServer.start(DNS_PORT, "*", apIP);
    server.begin();
    InitializeHTTPServer();

    // Handle request for current SSID
    server.on("/getSSID", HTTP_GET, []() {
        server.send(200, "text/plain", WiFi.softAPSSID());
        });

    // Handle request to save new SSID and restart D1 Mini
    server.on("/saveSSID", HTTP_POST, []() {
        String newSSID = server.arg("ssid");
        // Hier kannst du den neuen SSID-Wert speichern, z.B. in EEPROM
        Serial.print("New SSID: ");
        Serial.println(newSSID);
        server.send(200, "text/plain", "SSID saved. Restarting...");
        delay(1000); // Kurze Verz�gerung, um die Antwort an den Client zu senden
        ESP.restart(); // Neustart des D1 Mini
        });
}


void loop()
{
    dnsServer.processNextRequest();
    server.handleClient();
}
