#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>

// Wi-Fi credentials
const char* ssid = "ESP32_File_Server";
const char* password = "12345678";

// Initialize the web server on port 80
WebServer server(80);

// Define the chip select pin for the SD card
const int chipSelect = 5;

// HTML page for the web interface
const char* htmlPage = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 File Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
  <h1>ESP32 File Server</h1>
  <h2>Upload File</h2>
  <form method="POST" action="/upload" enctype="multipart/form-data">
    <input type="file" name="file">
    <input type="submit" value="Upload">
  </form>
  <h2>Download Files</h2>
  <ul>
    %FILE_LIST%
  </ul>
</body>
</html>
)rawliteral";

// Function to list files on the SD card
String listFiles() {
  String fileList = "";
  File root = SD.open("/");
  File file = root.openNextFile();
  while(file){
    if(!file.isDirectory()){
      fileList += "<li><a href=\"/download?file=";
      fileList += file.name();
      fileList += "\">";
      fileList += file.name();
      fileList += "</a></li>";
    }
    file = root.openNextFile();
  }
  file.close();
  return fileList;
}

// Handle the root path
void handleRoot() {
  String s = htmlPage;
  s.replace("%FILE_LIST%", listFiles());
  server.send(200, "text/html", s);
}

// Handle file upload
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = "/"+upload.filename;
    if(!SD.begin(chipSelect)){
      Serial.println("Card Mount Failed");
      return;
    }
    File file = SD.open(filename.c_str(), FILE_WRITE);
    if(!file){
      Serial.println("Failed to open file for writing");
      return;
    }
    file.close();
  } 
  else if(upload.status == UPLOAD_FILE_WRITE){
    File file = SD.open("/"+upload.filename, FILE_APPEND);
    if(file){
      file.write(upload.buf, upload.currentSize);
      file.close();
    }
    else{
      Serial.println("Failed to open file for appending");
    }
  }
  else if(upload.status == UPLOAD_FILE_END){
    Serial.printf("Upload End: %s, %u bytes\n", upload.filename.c_str(), upload.totalSize);
    server.sendHeader("Location", "/");
    server.send(303);
  }
}

// Handle file download
void handleFileDownload(){
  if(server.hasArg("file")){
    String filename = server.arg("file");
    if(SD.exists("/" + filename)){
      File file = SD.open("/" + filename, FILE_READ);
      server.streamFile(file, "application/octet-stream");
      file.close();
    }
    else{
      server.send(404, "text/plain", "File Not Found");
    }
  }
  else{
    server.send(400, "text/plain", "Bad Request");
  }
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  delay(1000);
  
  // Initialize SD card
  if(!SD.begin(chipSelect)){
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
    return;
  }
  
  Serial.println("Initializing SD card...");
  
  // Initialize Wi-Fi in Access Point mode
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  // Define routes
  server.on("/", handleRoot);
  server.on("/upload", HTTP_POST, [](){}, handleFileUpload);
  server.on("/download", HTTP_GET, handleFileDownload);
  
  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop(){
  server.handleClient();
}
