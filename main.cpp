#include <ESP8266WiFi.h>
#include <Servo.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>

// Define the servo object and pins for servo and beep
Servo ObjServo;
const int ServoGPIO = D4;
const int beepPin = D3;
const int beepPinGround = D2;

// Create an ESP8266WebServer object on port 80
ESP8266WebServer server(80);

// Define DNS server and access point IP address
const byte DNS_PORT = 53;
DNSServer dnsServer;
IPAddress apIP(192, 168, 112, 1);

// Function to check if a string is an IP address
bool isIp(const String &str) {
    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (c != '.' && (c < '0' || c > '9')) {
            return false;
        }
    }
    return true;
}

// Function to convert IP address to string
String toStringIp(const IPAddress &ip) {
    String res = "";
    for (int i = 0; i < 3; i++) {
        res += String((ip >> (8 * i)) & 0xFF) + ".";
    }
    res += String(((ip >> 8 * 3)) & 0xFF);
    return res;
}

// Function to redirect to captive portal for non-IP requests
bool captivePortal() {
    if (!isIp(server.hostHeader())) {
        Serial.println("Request redirected to captive portal");
        server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
        server.send(302, "text/plain", "");
        server.client().stop();
        return true;
    }
    return false;
}

// Function to handle 404 Not Found errors
void handleNotFound() {
    if (captivePortal()) {
        return;
    }

    String requestedURL = server.uri();

    String temp = "<!DOCTYPE HTML>\
                    <html>\
                    <head>\
                    <title>404 Not Found</title>\
                    </head>\
                    <body>\
                    <h1>404 Not Found</h1>\
                    <p>The requested URL '" + requestedURL + "' was not found on this server.</p>\
                    </body>\
                    </html>";

    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.setContentLength(temp.length());

    server.send(404, "text/html", temp);
    server.client().stop();
}

  const char* htmlCode = R"=====(
<html>
<head>
    <title>Manometer</title>
    <style>
        canvas { border: none; }
        body { display: flex; flex-direction: column; justify-content: center; max-width: 400px; zoom: 200%; }
        #menue { display: flex; justify-content: center; gap: 30px; }
        #anzeige { position: relative; top: -140px; border-top: 1px black solid; padding-top: 10px; width: 400px; margin: 0 auto; }
        #anzeige::before { content: 'RESTDRUCK: '; }
        #timerDisplay { position: relative; top: -120px; margin: 0 auto; width: 400px; }
        #startpress { position: relative; top: -100px; border-top: 1px solid lightgrey; width: 400px; margin: 0 auto; padding-top: 10px; }
        #startpress input { width: 200px; margin: 20px; }
        #p_rate { position: relative; top: -90px; margin: 0 auto; }
        #p_rate input { width: 200px; margin: 20px; }
        #startbutton { width: 25%; margin: 15px; padding: 30px; top: -80px; position: relative; }
        #buttonPause { width: 25%; margin: 15px; padding: 30px; top: -80px; position: relative; }
        #resetbutton { width: 25%; margin: 15px; padding: 30px; top: -80px; position: relative; }
        #menue { margin: 20px; }
        #timerSettings { display: none; }
        #timerDiv { display: initial; position: relative; top: -100px; margin: 0 auto; display: none; gap: 10px; border-top: 1px solid black; width: 400px; justify-content: center; padding: 5px; }
        #timerDiv label { margin: auto; margin-left: 0px; }
        #buttonDiv { display: flex; justify-content: center; }
        #minutesField { height: 50px; }
        #secondsField { display: inline; height: 50px; }
        #settingsPopup button { margin: 10px; }
    </style>
    <meta charset="utf-8">
</head>
<body onload="init()">
    <div id="menue">
        <select id="functionSelect" onchange="toggleFunction()">
            <option value="current">Barometer Function</option>
            <option value="timer">Timer Function</option>
        </select>
    </div>
    <canvas id="manometer" width="400" height="400"></canvas>
    <div id="anzeige"></div>
    <div id="timerDisplay">Verstrichene Zeit: 00:00</div>
    <div id="timerDiv">
        <label>Timer (Min: Sec):</label>
    </div>
    <script>
            // Get the canvas element and its 2D context
    var canvas = document.getElementById('manometer');
    var context = canvas.getContext('2d');

    // Define variables for the center of the canvas, radius, and pressure range
    var x = canvas.width / 2;
    var y = canvas.height / 2;
    var radius = canvas.width / 2 - 10;
    var minBar = 0;
    var maxBar = 300;
    var rotBereich = 50; // Rotation range for the red section

    // Initialize timer and interval-related variables
    var timerId = null;
    var elapsedTime = 0;
    var intervalId = null;
    var paused = false;
    var currentPressure;
    var countdownInterval; // Variable to store the interval for the countdown
    var remainingTime; // Variable to store the remaining time

    // Set the frames per second
    var fps = 60;

    // Set default start pressure to 300
    var startDruck = 300;

    // Set default pressure drop rate to 1
    var druckAbfallRate = 1;

    // Initialize the pressure bar
    var bar = startDruck;

    // FUNCTION TO DRAW THE MANOMETER ON THE CANVAS
    // Function to draw the manometer gauge on the canvas, including outer circle, pressure indicators,
    // inner circle, and red section to indicate low pressure.
    // Utilizes variables: context, canvas, x, y, radius, minBar, maxBar, rotBereich.
    // - context: the 2D rendering context of the canvas
    // - canvas: the canvas element
    // - x, y: coordinates of the center of the gauge
    // - radius: radius of the gauge
    // - minBar, maxBar: minimum and maximum pressure values
    // - rotBereich: range indicating low pressure
    function drawManometer() {
        // Clear the canvas
        context.clearRect(0, 0, canvas.width, canvas.height);

        // Draw outer circle
        context.beginPath();
        context.arc(x, y, radius + 10, Math.PI, 0, false);
        context.lineWidth = 2;
        context.strokeStyle = '#000000';
        context.stroke();

        // Draw pressure indicators
        context.font = "20px Arial";
        context.textAlign = "center";
        context.textBaseline = "middle";
        for (var i = 0; i <= 6; i++) {
            var grad = i * 50;
            var angle = (((grad / (maxBar - minBar)) * (Math.PI)) - (Math.PI));
            var xGrad = x + Math.cos(angle) * (radius - 20);
            var yGrad = y + Math.sin(angle) * (radius - 20);
            context.fillText(grad.toString(), xGrad, yGrad);
        }

        // Draw inner circle
        context.beginPath();
        context.arc(x, y, radius + 5, Math.PI, 0, false);
        context.lineWidth = 5;
        context.strokeStyle = '#000000';
        context.stroke();

        // Draw red section to indicate low pressure
        context.beginPath();
        context.arc(x, y, radius - 3, (((50 - rotBereich) / (maxBar - minBar)) * (Math.PI)) + Math.PI, ((50 / (maxBar - minBar)) * (Math.PI)) + Math.PI, false);
        context.lineWidth = 10;
        context.strokeStyle = '#FF0000';
        context.stroke();
    }

    // Function to draw the pointer on the gauge based on the current pressure.
    // Utilizes variables: context, canvas, x, y, radius, minBar, maxBar.
    // - context: the 2D rendering context of the canvas
    // - canvas: the canvas element
    // - x, y: coordinates of the center of the gauge
    // - radius: radius of the gauge
    // - minBar, maxBar: minimum and maximum pressure values
    function drawZeiger(bar) {
        var angle = (((bar - minBar) / (maxBar - minBar)) * (Math.PI)) + Math.PI;
        var xZeiger = x + Math.cos(angle) * (radius - 70);
        var yZeiger = y + Math.sin(angle) * (radius - 70);
        context.beginPath();
        context.arc(x, y, radius - 65, 0, 2 * Math.PI);
        context.fillStyle = '#FFFFFF';
        context.fill();
        context.beginPath();
        context.moveTo(x, y);
        context.lineTo(xZeiger, yZeiger);
        context.lineWidth = 5;
        context.strokeStyle = '#FF0000';
        context.stroke();
        var anzeige = document.getElementById("anzeige");
        anzeige.innerHTML = bar.toFixed(2);
    }

    // Function to update the elapsed time display.
    // Utilizes variables: elapsedTime.
    // - elapsedTime: the total elapsed time in seconds
    function updateElapsedTime() {
        var timeDisplay = document.getElementById("timerDisplay");
        var minutes = Math.floor(elapsedTime / 60);
        var seconds = elapsedTime % 60;
        timeDisplay.innerHTML = "Elapsed Time: " + (minutes < 10 ? "0" : "") + minutes + ":" + (seconds < 10 ? "0" : "") + seconds;
    }

    // Initialization function.
    // Invokes drawManometer() and drawZeiger() functions to initialize the manometer and set the gauge pointer to the start pressure.
    // Utilizes variables: startDruck.
    // - startDruck: the initial pressure value
    function init() {
        drawManometer();
        drawZeiger(startDruck); // Set the gauge pointer to the start pressure during initialization
    }

    // Function to update the SSID
    function updateSSID() {
        fetch("/getSSID")
        .then(response => response.text())
        .then(data => {
            document.getElementById("settingsTextField").value = "Current SSID: " + data;
        })
        .catch(error => {
            console.error('Error:', error);
        });
    }

    // Function to open the settings popup.
    // Creates a popup container with input field for SSID, a save button, and a hint for Wi-Fi connection.
    // Utilizes variables: settingsPopup, settingsTextField.
    // - settingsPopup: ID of the popup container element.
    // - settingsTextField: ID of the input field for SSID.
    function openSettingsPopup() {
        updateSSID();

        // Creating the popup container
        var popupContainer = document.createElement("div");
        popupContainer.setAttribute("id", "settingsPopup");
        popupContainer.style.position = "fixed";
        popupContainer.style.top = "50%";
        popupContainer.style.left = "50%";
        popupContainer.style.transform = "translate(-50%, -50%)";
        popupContainer.style.background = "#fff";
        popupContainer.style.padding = "20px";
        popupContainer.style.border = "2px solid #000";
        popupContainer.style.zIndex = "9999";
        popupContainer.style.height = "200px";

        // Creating the text field
        var textField = document.createElement("input");
        textField.setAttribute("type", "text");
        textField.setAttribute("id", "settingsTextField");
        textField.style.marginBottom = "10px";

        // Creating a label for the text field
        var label = document.createElement("label");
        label.setAttribute("for", "settingsTextField");
        label.textContent = "SSID: ";

        // Adding the label and text field to the popup container
        popupContainer.appendChild(label);
        popupContainer.appendChild(textField);

        // Hint for Wi-Fi connection
        var hint = document.createElement("p");
        hint.textContent = "Note: After changing the SSID, the Wi-Fi connection must be reestablished.";
        hint.style.marginTop = "5px";
        popupContainer.appendChild(hint);

        // Creating the save button
        var saveButton = document.createElement("button");
        saveButton.innerHTML = "Save";
        saveButton.addEventListener("click", function() {
            var userInput = document.getElementById("settingsTextField").value;
            // Send the SSID to the D1 Mini and restart it
            fetch("/saveSSID", {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: 'ssid=' + encodeURIComponent(userInput)
            })
            .then(response => {
                if (!response.ok) {
                    throw new Error('Network response was not ok');
                }
                console.log('SSID saved. Restarting...');
                // Optionally show a confirmation to the user or update the UI
                closeSettingsPopup(); // Close the popup after saving
            })
            .catch(error => {
                console.error('Error:', error);
            });
        });
        popupContainer.appendChild(saveButton);

        // Append the popup container to the body
        document.body.appendChild(popupContainer);
    }

    // Function to close the settings popup
    function closeSettingsPopup() {
        var popup = document.getElementById("settingsPopup");
        if (popup) {
            document.body.removeChild(popup);
        }
    }

    // Create sliders for setting start pressure and pressure drop rate
    var startDruckSlider = document.createElement("input");
    startDruckSlider.setAttribute("type", "range");
    startDruckSlider.setAttribute("min", minBar);
    startDruckSlider.setAttribute("max", maxBar);
    startDruckSlider.setAttribute("value", maxBar);
    startDruckSlider.setAttribute("step", "10");
    startDruckSlider.setAttribute("style", "width: 200px;");
    startDruckSlider.oninput = function() {
        startDruck = parseInt(this.value);
        drawZeiger(startDruck);
    };

    // Create div to contain start pressure slider
    var startDruckText = document.createTextNode("Start Pressure (bar): ");
    var startDruckDiv = document.createElement("div");
    startDruckDiv.setAttribute("id", "startpress");
    startDruckDiv.appendChild(startDruckText);
    startDruckDiv.appendChild(startDruckSlider);
    document.body.appendChild(startDruckDiv);

    // Create slider for setting pressure drop rate
    var druckAbfallSlider = document.createElement("input");
    druckAbfallSlider.setAttribute("type", "range");
    druckAbfallSlider.setAttribute("min", "0.208");
    druckAbfallSlider.setAttribute("max", "10");
    druckAbfallSlider.setAttribute("value", "0.208");
    druckAbfallSlider.setAttribute("step", "1");
    druckAbfallSlider.setAttribute("style", "width: 200px;");
    druckAbfallSlider.oninput = function() {
        druckAbfallRate = parseInt(this.value);
    };

    // Create text node for displaying pressure drop rate label
    var druckAbfallText = document.createTextNode("Pressure Drop Rate (bar/s): "+ druckAbfallSlider.value);
    
    // Create div for containing pressure drop rate slider
    var druckAbfallDiv = document.createElement("div");
    druckAbfallDiv.setAttribute("id", "p_rate");
    druckAbfallDiv.appendChild(druckAbfallText);
    druckAbfallDiv.appendChild(druckAbfallSlider);
    document.body.appendChild(druckAbfallDiv);

    // Create and append the start button
    var startButton = document.createElement("button");
    startButton.innerHTML = "Start";
    startButton.setAttribute("id", "startbutton");
    startButton.addEventListener("click", handleStartButtonClickBarometer); // Add event listener
    document.body.appendChild(startButton);

    // Create pause button (initially hidden)
    var pauseButton = document.createElement("button");
    pauseButton.innerHTML = "Pause";
    pauseButton.setAttribute("id", "buttonPause");
    pauseButton.style.display = "none"; // Initially hidden
    pauseButton.addEventListener("click", handlePauseButtonClick); // Add event listener
    document.body.appendChild(pauseButton);

    // Create reset button and append it to the document body
    var resetButton = document.createElement("button");
    resetButton.setAttribute("id", "resetbutton");
    resetButton.innerHTML = "Reset";
    resetButton.addEventListener("click", resetManometer); // Add event listener
    document.body.appendChild(resetButton);

    // Create a div to contain the buttons
    var buttonDiv = document.createElement("div");
    buttonDiv.setAttribute("id", "buttonDiv"); // Set ID for the div
    buttonDiv.appendChild(startButton);
    buttonDiv.appendChild(pauseButton);
    buttonDiv.appendChild(resetButton);
    document.body.appendChild(buttonDiv); // Append the div at the end of the body
    
    // Set the frames per second
    var fps = 60;

    // Set default start pressure to 300
    var startDruck = 300;

    // Set default pressure drop rate to 0,208
    //var druckAbfallRate = 0.208;

    // Initialize the pressure bar
    var bar = startDruck;

druckAbfallSlider.oninput = function() {
    druckAbfallRate = parseInt(this.value);
    druckAbfallText.nodeValue = "Pressure Drop Rate (bar/s): " + this.value;
};


    // Function to draw the pointer on the gauge based on the current pressure
    // Utilizes variables: context, canvas, x, y, radius, minBar, maxBar
    // - context: the 2D rendering context of the canvas
    // - canvas: the canvas element
    // - x, y: coordinates of the center of the gauge
    // - radius: radius of the gauge
    // - minBar, maxBar: minimum and maximum pressure values
function drawZeiger(bar) {
    var angle = (((bar - minBar) / (maxBar - minBar)) * (Math.PI)) + Math.PI;
    var xZeiger = x + Math.cos(angle) * (radius - 70);
    var yZeiger = y + Math.sin(angle) * (radius - 70);
    context.beginPath();
    context.arc(x, y, radius - 65, 0, 2 * Math.PI);
    context.fillStyle = '#FFFFFF';
    context.fill();
    context.beginPath();
    context.moveTo(x, y);
    context.lineTo(xZeiger, yZeiger);
    context.lineWidth = 5;
    context.strokeStyle = '#FF0000';
    context.stroke();
    var anzeige = document.getElementById("anzeige");
    anzeige.innerHTML = bar.toFixed(2);
}

// Function to update the elapsed time display
// Utilizes variables: elapsedTime
// - elapsedTime: the total elapsed time in seconds
function updateElapsedTime() {
    var timeDisplay = document.getElementById("timerDisplay");
    var minutes = Math.floor(elapsedTime / 60);
    var seconds = elapsedTime % 60;
    timeDisplay.innerHTML = "Elapsed Time: " + (minutes < 10 ? "0" : "") + minutes + ":" + (seconds < 10 ? "0" : "") + seconds;
}

// Initialization function
// Invokes drawManometer() and drawZeiger() functions to initialize the manometer and set the gauge pointer to the start pressure
// Utilizes variables: startDruck
// - startDruck: the initial pressure value
function init() {
    drawManometer();
    drawZeiger(startDruck); // Set the gauge pointer to the start pressure during initialization
}

function updateSSID() {
    fetch("/getSSID")
    .then(response => response.text())
    .then(data => {
        document.getElementById("settingsTextField").value = "Current SSID: " + data;
    })
    .catch(error => {
        console.error('Error:', error);
    });
}

// Erstellen des Einstellungen-Buttons
var settingsButton = document.createElement("button");
settingsButton.innerHTML = "Einstellungen";
settingsButton.addEventListener("click", openSettingsPopup);

// Hinzufügen des Einstellungen-Buttons zur Menü-Div
var menuDiv = document.getElementById("menue");
menuDiv.appendChild(settingsButton);

// Creating the settings button
var settingsButton = document.createElement("button");
settingsButton.innerHTML = "Settings";
settingsButton.addEventListener("click", openSettingsPopup);

// Adding the settings button to the menu div
var menuDiv = document.getElementById("menu");
menuDiv.appendChild(settingsButton);

// Function to open the settings popup
// Creates a popup container with input field for SSID, a save button, and a hint for Wi-Fi connection
// Utilizes variables: settingsPopup, settingsTextField
// - settingsPopup: ID of the popup container element
// - settingsTextField: ID of the input field for SSID
// TODO: this function listens for a click event on the "Save" button to handle user input and calls closeSettingsPopup() to close the popup after saving. This function could benefit from adding a cancel button to close the popup without saving.
function openSettingsPopup() {
    updateSSID();

    // Creating the popup container
    var popupContainer = document.createElement("div");
    popupContainer.setAttribute("id", "settingsPopup");
    popupContainer.style.position = "fixed";
    popupContainer.style.top = "50%";
    popupContainer.style.left = "50%";
    popupContainer.style.transform = "translate(-50%, -50%)";
    popupContainer.style.background = "#fff";
    popupContainer.style.padding = "20px";
    popupContainer.style.border = "2px solid #000";
    popupContainer.style.zIndex = "9999";
    popupContainer.style.height = "200px";

    // Creating the text field
    var textField = document.createElement("input");
    textField.setAttribute("type", "text");
    textField.setAttribute("id", "settingsTextField");
    textField.style.marginBottom = "10px";

    // Creating a label for the text field
    var label = document.createElement("label");
    label.setAttribute("for", "settingsTextField");
    label.textContent = "SSID: ";

    // Adding the label and text field to the popup container
    popupContainer.appendChild(label);
    popupContainer.appendChild(textField);
    updateSSID()

    // Hint for Wi-Fi connection
    var hint = document.createElement("p");
    hint.textContent = "Note: After changing the SSID, the Wi-Fi connection must be reestablished.";
    hint.style.marginTop = "5px";
    popupContainer.appendChild(hint);

    // Creating the save button
    var saveButton = document.createElement("button");
    saveButton.innerHTML = "Save"; 

    // Adding the save and close buttons to the popup container
    popupContainer.appendChild(saveButton);
    popupContainer.appendChild(closeButton);  
};
// Creating the close button
var closeButton = document.createElement("button");
closeButton.innerHTML = "Abbrechen";
closeButton.addEventListener("click", function() {
    closeSettingsPopup(); // Close the popup when close button is clicked
});

// Adding the popup container to the page
document.body.appendChild(popupContainer);

// Function to close the settings popup.
// Removes the settings popup from the DOM if it exists.
// Uses variables: None.
function closeSettingsPopup()
{
    var popup = document.getElementById("settingsPopup");
    if (popup)
    {
        popup.parentNode.removeChild(popup);
    }
}

// Creating the timer div
var timerDiv = document.createElement("div");
timerDiv.setAttribute("id", "timerDiv");
timerDiv.style.display = "none"; // Hide by default
document.body.appendChild(timerDiv);

// Function to start the countdown timer
// Function to start the countdown timer.
// Calculates the total remaining time in seconds based on the input fields for minutes and seconds.
// Calculates the steps for updating the barometer based on the total remaining time.
// Initializes variables for the current time and current bar value.
// Updates the display with the remaining time and current bar value.
// Starts the countdown interval if it's not already running.
// Updates the countdown every second, updating time, bar value, display, and pointer position.
// Stops the countdown and updates the display when the time is up.
// Uses variables: countdownInterval, paused, formatTime, drawZeiger.
function startCountdown()
{
    // Minutes and seconds from input fields
    var minutes = parseInt(document.getElementById("minutesField").value);
    var seconds = parseInt(document.getElementById("secondsField").value);

    // Total remaining seconds
    var totalSeconds = minutes * 60 + seconds;

    // Starting pressure value for the bar
    var startBar = 300;

    // Calculate steps for the bar
    var steps = startBar / totalSeconds;

    // Update frequency in milliseconds
    var updateInterval = 1000;

    // Initial time for the countdown
    var currentTime = totalSeconds;

    // Initialize pointer
    var currentBarValue = startBar;

    // Start value for the display
    document.getElementById("timerDisplay").innerHTML = "Remaining Time: " + formatTime(currentTime);
    document.getElementById("anzeige").innerHTML = "Bar: " + Math.round(currentBarValue);

    // If a countdown is already running, don't start again
    if (!countdownInterval)
    {
        // Start countdown
        countdownInterval = setInterval(function() {
            // If not paused, update the countdown
            if (!paused)
            {
                // Update time
                currentTime--;

                // Update bar
                currentBarValue -= steps;

                // Update display
                document.getElementById("timerDisplay").innerHTML = "Remaining Time: " + formatTime(currentTime);
                document.getElementById("anzeige").innerHTML = "Bar: " + Math.round(currentBarValue);

                // Update pointer
                drawZeiger(currentBarValue);

                // If time is up, stop the countdown
                if (currentTime <= 0)
                {
                    clearInterval(countdownInterval);
                    document.getElementById("timerDisplay").innerHTML = "Elapsed Time: " + formatTime(0);
                    document.getElementById("anzeige").innerHTML = "Bar: 0";
                    drawZeiger(0);
                }
            }
        }, updateInterval);
    }
}

// Function to format time in MM:SS format.
// Converts total seconds into minutes and remaining seconds, then returns the formatted time.
// Uses the pad function to ensure two-digit formatting.
// Uses the pad function.
function formatTime(seconds)
{
    var minutes = Math.floor(seconds / 60);
    var remainingSeconds = seconds % 60;
    return pad(minutes) + ":" + pad(remainingSeconds);
}

// Function to pad single-digit numbers with leading zeros.
// Adds a leading zero to single-digit numbers to ensure two-digit formatting.
// Used by the formatTime function.
function pad(val)
{
    return val < 10 ? "0" + val : val;
}

// Function to pause the countdown.
// Sets the paused flag to true, indicating the countdown is paused.
function pauseCountdown()
{
    paused = true;
}

// Function to resume the countdown.
// Sets the paused flag to false, indicating the countdown is resumed.
function resumeCountdown()
{
    paused = false;
}

// Event handler for the pause button click.
// Stops the countdown or barometer function based on the current state.
// If the button's text is "Pause", it changes it to "Resume" and saves the current pressure value.
// If the button's text is "Resume", it changes it back to "Pause" and resumes the barometer function with the saved pressure value.
function handlePauseButtonClick()
{
    var pauseButton = document.getElementById("buttonPause");
    clearInterval(intervalId); // Stop countdown or barometer function

    if (pauseButton.innerHTML === "Pause")
    {
        pauseButton.innerHTML = "Resume";
        // Save the current pressure value
        currentPressure = bar;
    }
    else
    {
        pauseButton.innerHTML = "Pause";
        // Resume the barometer function with the saved pressure value
        intervalId = setInterval(function() {
            if (!paused)
            {
                elapsedTime++;
                bar -= druckAbfallRate / fps;
                if (bar < minBar)
                {
                    bar = minBar;
                    clearInterval(intervalId);
                }
                drawZeiger(bar);
                updateElapsedTime();
            }
        }, 1000 / fps);
    }
}

// Function to update the display and bar based on the remaining time and start pressure.
// It calculates the remaining time in seconds and updates the timer display.
// Then it calculates the bar value based on the remaining time and the start pressure, and updates the barometer display accordingly.
function updateDisplayAndBar(seconds, startBar)
{
    // Update display
    document.getElementById("timerDisplay").innerHTML = "Remaining Time: " + formatTime(seconds);

    // Update bar
    var bar = startBar * (seconds / (parseInt(document.getElementById("minutesField").value) * 60));
    drawZeiger(bar);
    document.getElementById("anzeige").innerHTML = "Bar: " + Math.round(bar);
}

// Function to reset the timer to its initial state.
// It calculates the remaining time based on the values entered in the input fields for minutes and seconds.
// Then it stops the countdown, clears the interval, and resets the display and event listeners for the start and pause buttons.
// Finally, it resets the pointer on the gauge to the default pressure value.
function resetTimer()
{
    var minutes = parseInt(document.getElementById("minutesField").value);
    var seconds = parseInt(document.getElementById("secondsField").value);
    remainingTime = minutes * 60 + seconds;

    stopCountdown();
    clearInterval(intervalId);

    var pauseButton = document.getElementById("buttonPause");
    var startButton = document.getElementById("startbutton");

    pauseButton.style.display = "none";
    startButton.style.display = "block";
    pauseButton.innerHTML = "Pause";

    startButton.removeEventListener("click", handleStartButtonClickTimer);
    startButton.addEventListener("click", handleStartButtonClickTimer);

    clearInterval(countdownInterval);
    countdownInterval = null;

    // Reset for the pointer
    drawZeiger(300);
}

// Function to reset the manometer to its initial state.
// It resets the display for the manometer and the elapsed time to their initial values.
// Then it resets the internal bar value to the default pressure value and updates the pointer on the gauge accordingly.
// If the countdown is running, it stops it and hides the pause button while showing the start button.
function resetManometer()
{
    // Reset the display to the initial value
    var timerDisplay = document.getElementById("timerDisplay");
    timerDisplay.innerHTML = "Elapsed Time: 00:00";

    // Reset the display for the manometer to the initial value
    var anzeige = document.getElementById("anzeige");
    anzeige.innerHTML = startDruck;

    // Reset the internal bar value to the initial value
    bar = startDruck;

    // Set pointer to the start pressure
    drawZeiger(startDruck);

    // If the countdown is running, stop it
    stopCountdown();

    // Reset and hide the pause button
    var pauseButton = document.getElementById("buttonPause");
    pauseButton.innerHTML = "Pause";
    pauseButton.style.display = "none";

    // Show the start button
    var startButton = document.getElementById("startbutton");
    startButton.style.display = "block";
}

// Function to handle the click event of the start button in the timer option.
// It initiates the countdown timer when the start button is clicked.
// It hides the start button and displays the pause button with its text set to "Pause".
// It adds an event listener to the pause button to handle its click event.
function handleStartButtonClickTimer()
{
    // Logic for the start button click event in the timer option
    startCountdown(); // Example: startCountdown();

    // Change the start button to a pause button
    var startButton = document.getElementById("startbutton");
    startButton.style.display = "none"; // Hide start button

    var pauseButton = document.getElementById("buttonPause");
    pauseButton.style.display = "block"; // Show pause button
    pauseButton.innerHTML = "Pause"; // Reset text to "Pause"

    // Add event listener for the pause button
    pauseButton.removeEventListener("click", handlePauseButtonTimer);
    pauseButton.addEventListener("click", handlePauseButtonTimer);
}

// Function to handle the click event of the start button in the barometer option.
// It resets the manometer to its initial state.
// It retrieves the start pressure value and pressure drop rate from sliders.
// It resets the elapsed time.
// It starts the barometer function with a setInterval call to update pressure values.
// It hides the start button and displays the pause button with its text set to "Pause".
// It updates the event listener for the pause button to handle its click event.
function handleStartButtonClickBarometer()
{

    // First reset the manometer
    resetManometer();

    // Start value for pressure
    var startDruck = parseFloat(startDruckSlider.value);

    // Pressure drop rate
    var druckAbfallRate = parseFloat(druckAbfallSlider.value);

    // Reset elapsed time
    elapsedTime = 0;

    // Start the barometer function
    intervalId = setInterval(function() {
        if (!paused)
        {
            elapsedTime++;
            bar -= druckAbfallRate / fps;
            if (bar < minBar)
            {
                bar = minBar;
                clearInterval(intervalId);
            }
            drawZeiger(bar);
            updateElapsedTime();
        }
    }, 1000 / fps);

    // Change the start button to a pause button
    var startButton = document.getElementById("startbutton");
    startButton.style.display = "none"; // Hide start button

    var pauseButton = document.getElementById("buttonPause");
    pauseButton.style.display = "block"; // Show pause button

    // Reset the pause button to "Pause"
    pauseButton.innerHTML = "Pause";

    // Update event listener for the pause button
    pauseButton.removeEventListener("click", handlePauseButtonClick);
    pauseButton.addEventListener("click", handlePauseButtonClick);
}

// Function to stop the countdown.
// It clears the interval to stop the countdown.
// It resets the display to show "Remaining Time: 00:00".
function stopCountdown()
{
    clearInterval(intervalId); // Clear interval to stop the countdown
    var timerDisplay = document.getElementById("timerDisplay");
    timerDisplay.innerHTML = "Remaining Time: 00:00"; // Reset display to zero
}

// Function for handling the click event of the pause button in the timer option.
// If the button text is "Pause", it pauses the countdown timer and changes the button text to "Continue".
// If the button text is "Continue", it resumes the countdown timer and changes the button text back to "Pause".
function handlePauseButtonTimer()
{
    // Logic for the pause button click event in the timer option
    var pauseButton = document.getElementById("buttonPause");

    if (pauseButton.innerHTML === "Pause")
    {
        // Pause the timer
        pauseCountdown(); // Pause timer
        pauseButton.innerHTML = "Continue";
    }
    else
    {
        // Resume the timer
        resumeCountdown(); // Resume timer
        pauseButton.innerHTML = "Pause";
    }
}

// Function for calculating the remaining time based on the values entered in the minutes and seconds input fields.
// Returns the total remaining time in seconds.
function calculateRemainingTime()
{
    // Read minutes and seconds from input fields
    var minutes = parseInt(document.getElementById("minutesField").value);
    var seconds = parseInt(document.getElementById("secondsField").value);

    // Calculate total remaining seconds
    return minutes * 60 + seconds;
}

// Function to start the countdown timer from the current remaining time.
// Uses the remainingTime variable to determine the initial time.
function startCountdownFromCurrentTime()
{
    startCountdown(remainingTime);
}

// Function to toggle between timer and barometer options based on the selected value in the dropdown menu.
// Retrieves DOM elements for necessary components such as the function select dropdown, timer div, input fields, and buttons.
// Determines the selected value from the dropdown menu.
// If "timer" is selected:
// - Displays timer input fields (minutes and seconds) if they don't already exist.
// - Clears any existing interval for countdown.
// - Sets the display style of timer related elements to show them.
// - Hides barometer related elements.
// - Updates event listeners for start, reset, and pause buttons to handle timer functionality.
// If "barometer" is selected:
// - Hides timer input fields.
// - Shows barometer related elements.
// - Stops the countdown if it's running.
// - Updates event listeners for start, reset, and pause buttons to handle barometer functionality.
// Ensures the start button is visible regardless of the selected option.
function toggleFunction()
{
    // Retrieve DOM elements
    var functionSelect = document.getElementById("functionSelect"),
        selectedValue = functionSelect.options[functionSelect.selectedIndex].value,
        timerDiv = document.getElementById("timerDiv"),
        minutesField = document.getElementById("minutesField"),
        secondsField = document.getElementById("secondsField"),
        startButton = document.getElementById("startbutton"),
        resetButton = document.getElementById("resetbutton"),
        pauseButton = document.getElementById("buttonPause");

    // Check the selected option value
    if (selectedValue === "timer")
    {
        // Display timer fields
        if (!minutesField)
        {
            minutesField = document.createElement("input");
            minutesField.setAttribute("type", "number");
            minutesField.setAttribute("id", "minutesField");
            minutesField.setAttribute("placeholder", "Min");
            minutesField.setAttribute("min", "0");
            minutesField.setAttribute("max", "59");
            minutesField.setAttribute("value", "5");
            minutesField.style.display = "inline";
            timerDiv.appendChild(minutesField);
        }

        if (!secondsField)
        {
            secondsField = document.createElement("input");
            secondsField.setAttribute("type", "number");
            secondsField.setAttribute("id", "secondsField");
            secondsField.setAttribute("placeholder", "Sec");
            secondsField.setAttribute("min", "0");
            secondsField.setAttribute("max", "59");
            secondsField.setAttribute("value", "0");
            secondsField.style.display = "inline";
            timerDiv.appendChild(secondsField);
        }

        clearInterval(intervalId);
        timerDiv.style.display = "flex";
        document.getElementById("p_rate").style.display = "none";
        document.getElementById("startpress").style.display = "none";
        startButton.removeEventListener("click", handleStartButtonClickBarometer);
        startButton.addEventListener("click", handleStartButtonClickTimer);
        resetButton.removeEventListener("click", resetManometer);
        resetButton.addEventListener("click", resetTimer); // Assign resetTimer here
        pauseButton.removeEventListener("click", handlePauseButtonClick);
        pauseButton.addEventListener("click", handlePauseButtonTimer);
    }
    else
    {
        // Display barometer fields
        timerDiv.style.display = "none";
        document.getElementById("p_rate").style.display = "block";
        document.getElementById("startpress").style.display = "block";
        stopCountdown();
        startButton.removeEventListener("click", handleStartButtonClickTimer);
        startButton.addEventListener("click", handleStartButtonClickBarometer);
        resetButton.removeEventListener("click", resetTimer); // Remove resetTimer here
        resetButton.addEventListener("click", resetManometer);
        pauseButton.removeEventListener("click", handlePauseButtonTimer);
        pauseButton.addEventListener("click", handlePauseButtonClick);
    }
    startButton.style.display = "block"; // Ensure start button is visible
}

    </script>
</body>
</html>
)=====";

void handleRoot() {
 
     server.send(200, "text/html",htmlCode);
}

void InitializeHTTPServer() {
    server.on("/", handleRoot);
    server.onNotFound(handleNotFound);
    server.begin();

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
        delay(1000); // Kurze Verzögerung, um die Antwort an den Client zu senden
        ESP.restart(); // Neustart des D1 Mini
    });
}

void setup() {
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
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();
}
