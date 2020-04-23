#include <Esp.h>
#include <math.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "unphone.h"

#include "colours.h"
#include "mazes.h"
#include "private.h"
#include "Rectangle.h"


const int TS_MINX = 150;
const int TS_MINY = 130;
const int TS_MAXX = 3800;
const int TS_MAXY = 4000;

enum PlayerMode {
  MODE_SERVER,
  MODE_CLIENT,
  MODE_NONE
};

enum Page
{
  PAGE_MAIN,
  PAGE_INSTRUCTIONS
};

struct SpeedButton
{
  int speed;
  Rectangle shape;
};

TaskHandle_t Task1;
SemaphoreHandle_t semaphoreDisplay;
SemaphoreHandle_t semaphoreLoop;

PlayerMode playerType = MODE_NONE;
Rectangle player(12, 12, 4, 4);
std::vector<Rectangle> walls;
std::vector<Rectangle> badWalls;
Rectangle goal(280, 440, 20, 20);
int speed = 5;
bool clientIsDisplaying = false;

sensors_event_t event;
char myReadingsBuffer[5];
char otherReadingsBuffer[5];

WiFiUDP udp;
unsigned int localPort = 9999;
IPAddress ipServer(192, 168, 4, 1);
IPAddress ipClient(192, 168, 4, 10);
IPAddress subnet(255, 255, 255, 0);

bool inAnyRectangle(std::vector<Rectangle> rectangles, Rectangle player)
{
  for (int i = 0; i < rectangles.size(); i++)
  {
    if (rectangles[i].intersecting(player))
    {
      return true;
    }
  }
  return false;
}

void displayGameView()
{
  // black background for maze
  unPhone::tftp->fillScreen(COLOUR_BACKGROUND);

  // draw regular walls
  for (int i = 0; i < walls.size(); i++)
  {
    Rectangle *wall = &walls[i];
    unPhone::tftp->fillRect(wall->getX(), wall->getY(), wall->getWidth(), wall->getHeight(), COLOUR_WALL);
  }

  // draw bad walls
  for (int i = 0; i < badWalls.size(); i++)
  {
    Rectangle *wall = &badWalls[i];
    unPhone::tftp->fillRect(wall->getX(), wall->getY(), wall->getWidth(), wall->getHeight(), COLOUR_BAD_WALL);
  }

  // draw goal zone
  unPhone::tftp->fillRect(goal.getX(), goal.getY(), goal.getWidth(), goal.getHeight(), COLOUR_GOAL);

  char buffer[32];
  sprintf(buffer, "Current speed: %d", speed);

  unPhone::tftp->setTextSize(1.5);
  unPhone::tftp->setTextColor(COLOUR_SPEED_TEXT);

  unPhone::tftp->setCursor(100, 2);
  unPhone::tftp->println(buffer);
}

void displayPlayerMove(void *parameter)
{
  while (true)
  {
    xSemaphoreTake(semaphoreDisplay, portMAX_DELAY);
    Serial.printf("recieved: %s\n", otherReadingsBuffer);
    unPhone::tftp->fillRect(player.getX(), player.getY(), player.getWidth(), player.getHeight(), COLOUR_BACKGROUND);

    int triedSpeed = speed;

    while (triedSpeed > 0)
    {
      int wantedX = player.getX();
      switch (myReadingsBuffer[0])
      {
      case 'L':
        wantedX -= triedSpeed;
        break;
      case 'R':
        wantedX += triedSpeed;
        break;
      }

      Rectangle playerXShift(wantedX, player.getY(), player.getWidth(), player.getHeight());

      if (!inAnyRectangle(walls, playerXShift))
      {
        player.setX(wantedX);
        break;
      }
      triedSpeed--;
    }

    triedSpeed = speed;

    while (triedSpeed > 0)
    {
      int wantedY = player.getY();
      switch (otherReadingsBuffer[1])
      {
      case 'U':
        wantedY -= triedSpeed;
        break;
      case 'D':
        wantedY += triedSpeed;
        break;
      }

      Rectangle playerYShift(player.getX(), wantedY, player.getWidth(), player.getHeight());

      if (!inAnyRectangle(walls, playerYShift))
      {
        player.setY(wantedY);
        break;
      }
      triedSpeed--;
    }

    if (inAnyRectangle(badWalls, player))
    {
      if (speed == 1)
      {
        unPhone::tftp->fillScreen(COLOUR_BACKGROUND);
        showMaze(maze3Walls);
      }
      if (speed > 1)
        speed--;

      unPhone::tftp->fillScreen(COLOUR_BACKGROUND_FAILURE);
      unPhone::tftp->setTextColor(COLOUR_TEXT_END);
      unPhone::tftp->setCursor(20, 160);
      unPhone::tftp->println(":(");
      delay(1000);

      player.setX(12);
      player.setY(12);

      displayGameView();
    }

    if (player.intersecting(goal))
    {
      speed++;

      unPhone::tftp->fillScreen(COLOUR_BACKGROUND_SUCCESS);
      unPhone::tftp->setTextColor(COLOUR_TEXT_END);
      unPhone::tftp->setCursor(20, 160);
      unPhone::tftp->println(":)");
      delay(1000);

      player.setX(20);
      player.setY(20);

      displayGameView();
    }

    unPhone::tftp->fillRect(player.getX(), player.getY(), player.getWidth(), player.getHeight(), COLOUR_PLAYER);
    xSemaphoreGive(semaphoreLoop);
    Serial.println("given loop semaphore");
  }
}

void screenTouchVibrate(){
  unPhone::vibe(true);
  delay(80);
  unPhone::vibe(false);
}

TS_Point touchScreenMap(TS_Point point)
{
  point.x = 320 - map(point.x, TS_MINX, TS_MAXX, 0, 320);
  point.y = 480 - map(point.y, TS_MINY, TS_MAXY, 0, 480);

  return point;
}

void splashScreen()
{
  unPhone::tftp->fillScreen(COLOUR_SPLASH_BACKGROUND);
  unPhone::tftp->setTextSize(10);

  for (int i = 0; i < 10; i++)
  {
    if (i % 2 == 0)
    {
      unPhone::tftp->setTextColor(COLOUR_SPLASH_TEXT1);
    }
    else if (i % 3 == 0)
    {
      unPhone::tftp->setTextColor(COLOUR_SPLASH_TEXT2);
    }
    else
    {
      unPhone::tftp->setTextColor(COLOUR_SPLASH_TEXT3);
    }

    unPhone::tftp->setCursor(40, 178);
    unPhone::tftp->print("MAZE");
    unPhone::tftp->setCursor(40, 278);
    unPhone::tftp->print("GAME");

    delay(320);

    screenTouchVibrate();
  }
}

void IRAM_ATTR setupMenuButton1()
{
  playerType = MODE_SERVER;
  Serial.println("setupMenuButton1 - Server player selected");
}

void IRAM_ATTR setupMenuButton3()
{
  playerType = MODE_SERVER;
  Serial.println("setupMenuButton3 - Client player selected");
}

void showInstructionPage(Rectangle returnTouch)
{
  Serial.println("showInstructionPage - Detaching player select interrupts");
  detachInterrupt(digitalPinToInterrupt(unPhone::BUTTON1));
  detachInterrupt(digitalPinToInterrupt(unPhone::BUTTON3));

  unPhone::tftp->fillScreen(COLOUR_MENU_BACKGROUND);

  unPhone::tftp->fillRect(20, 20, 280, 440, COLOUR_MENU_BOX);

  unPhone::tftp->setTextSize(2);
  unPhone::tftp->setTextColor(COLOUR_MENU_TEXT);

  unPhone::tftp->setCursor(30, 100);
  unPhone::tftp->println(" > TILT ME");
  unPhone::tftp->setCursor(30, 140);
  unPhone::tftp->println(" > GREEN = GOOD");
  unPhone::tftp->setCursor(30, 160);
  unPhone::tftp->println(" > RED = BAD");
  unPhone::tftp->setCursor(30, 180);
  unPhone::tftp->println(" > PLAY ME");
  unPhone::tftp->setCursor(30, 310);
  unPhone::tftp->println("Made by: ");
  unPhone::tftp->setCursor(30, 330);
  unPhone::tftp->println("Joseph Cheverton-Wynne");
  unPhone::tftp->setCursor(30, 350);
  unPhone::tftp->println("and ");
  unPhone::tftp->setCursor(30, 370);
  unPhone::tftp->println("Samuel John Taylor ");
  
  unPhone::tftp->fillRect(returnTouch.getX(), returnTouch.getY(), returnTouch.getWidth(), returnTouch.getHeight(), COLOUR_CHANGE_PAGE_BOX); //Left outer
  unPhone::tftp->setCursor(110, 445);
  unPhone::tftp->println("Return");

  unPhone::tftp->setTextSize(3);
  unPhone::tftp->setCursor(30, 40);
  unPhone::tftp->print("Instructions:");
}

void showMainPage(Rectangle returnTouch)
{  
  unPhone::tftp->fillScreen(COLOUR_MENU_BACKGROUND);

  unPhone::tftp->setTextColor(COLOUR_MENU_TEXT);
  unPhone::tftp->setTextWrap(true);

  unPhone::tftp->setTextSize(2);

  unPhone::tftp->fillRect(20, 155, 280, 315, COLOUR_MENU_BOX);

  unPhone::tftp->setCursor(30, 178);
  unPhone::tftp->print("Select player type:");
  unPhone::tftp->setCursor(30, 204);
  unPhone::tftp->println(" >  Player 1 - Server");
  unPhone::tftp->setCursor(30, 220);
  unPhone::tftp->println(" >  Player 2 - Client");

  unPhone::tftp->setCursor(30, 280);
  unPhone::tftp->println(" Set starting speed: ");
  
  unPhone::tftp->fillRect(returnTouch.getX(), returnTouch.getY(), returnTouch.getWidth(), returnTouch.getHeight(), COLOUR_CHANGE_PAGE_BOX);
  unPhone::tftp->setCursor(85, 445);
  unPhone::tftp->println("Instructions");

  Serial.println("showMainPage - Attaching player select interrupts");

  attachInterrupt(digitalPinToInterrupt(unPhone::BUTTON1), setupMenuButton1, FALLING);
  attachInterrupt(digitalPinToInterrupt(unPhone::BUTTON3), setupMenuButton3, FALLING);
}

void speedTouchHandler(TS_Point point, SpeedButton speedButtons[])
{  
  for (int i = 0; i < 4; i++)
  {
    if (speedButtons[i].shape.pointInRect(point.x, point.y))
    {
      speed = speedButtons[i].speed;
      Serial.println("speedTouchHandler - Setting speed to: ");
      Serial.print(speed);
    }
  }
}

void showSpeedButtons(SpeedButton speedButtons[])
{
  unPhone::tftp->setTextColor(COLOUR_SPEED_SELECT_TEXT);
  unPhone::tftp->setTextSize(4);

  for (int i = 0; i < 4; i++)
  {
    Rectangle currentButton = speedButtons[i].shape;
    int innerBoxColour = speed == speedButtons[i].speed ? COLOUR_SPEED_SELECTED_INNER : COLOUR_SPEED_INNER;
    int outerBoxColour = speed == speedButtons[i].speed ? COLOUR_SPEED_SELECTED_OUTER : COLOUR_SPEED_OUTER;
    unPhone::tftp->fillRect(currentButton.getX(), currentButton.getY(), currentButton.getWidth(), currentButton.getHeight(), outerBoxColour);
    unPhone::tftp->fillRect(currentButton.getX() + 5, currentButton.getY() + 5, currentButton.getWidth() - 10, currentButton.getHeight() - 10, innerBoxColour);

    unPhone::tftp->setCursor(currentButton.getX() + (currentButton.getWidth() / 3), currentButton.getY() + (currentButton.getHeight() / 3));
    
    unPhone::tftp->println(speedButtons[i].speed);
  }
}

void showPlayerSelect(Rectangle serverBox, Rectangle clientBox)
{
  unPhone::tftp->setTextSize(7);

  unPhone::tftp->fillRect(serverBox.getX(), serverBox.getY(), serverBox.getWidth(), serverBox.getHeight(), COLOUR_PLAYER_SELECT_OUTER); //Left outer
  unPhone::tftp->fillRect(25, 20, 125, 125, COLOUR_PLAYER_SELECT_INNER);                                                                //Left Inner

  unPhone::tftp->fillRect(clientBox.getX(), clientBox.getY(), clientBox.getWidth(), clientBox.getHeight(), COLOUR_PLAYER_SELECT_OUTER); //Left outer
  unPhone::tftp->fillRect(170, 20, 125, 125, COLOUR_PLAYER_SELECT_INNER);                                                               //Right Inner

  unPhone::tftp->setCursor(67, 70);
  unPhone::tftp->print("1");
  unPhone::tftp->setCursor(212, 70);
  unPhone::tftp->print("2");
}

void handleMainPage(TS_Point point, Rectangle serverBox, Rectangle clientBox, Rectangle speedArea, Rectangle returnTouch, SpeedButton speedButtons[], Page * currentPage){    
    if (serverBox.pointInRect(point.x, point.y))
    {
      screenTouchVibrate();
      Serial.println("setupPlayerType - Server Selected");

      playerType = MODE_SERVER;      
    }
    else if (clientBox.pointInRect(point.x, point.y))
    {
      screenTouchVibrate();
      Serial.println("setupPlayerType - Client Selected");

      playerType = MODE_CLIENT;
    }
    else if (speedArea.pointInRect(point.x, point.y))
    {
      screenTouchVibrate();
      Serial.println("setupPlayerType - Speed Area Touched");

      speedTouchHandler(point, speedButtons);
      showSpeedButtons(speedButtons);
    }
    else if (returnTouch.pointInRect(point.x, point.y))
    {
      screenTouchVibrate();
      Serial.println("setupPlayerType - Opening Instruction page");

      *currentPage = PAGE_INSTRUCTIONS;
      showInstructionPage(returnTouch);
    }
}

void handleInstructionsPage(TS_Point point, Rectangle serverBox, Rectangle clientBox, Rectangle returnTouch, SpeedButton speedButtons[], Page * currentPage)
{
  if (returnTouch.pointInRect(point.x, point.y))
  {
    screenTouchVibrate();
    Serial.println("handleInstructionsPage - Opening main page");
     
    showMainPage(returnTouch);          
    showPlayerSelect(serverBox, clientBox);
    showSpeedButtons(speedButtons);

    *currentPage = PAGE_MAIN;
  }
}

void setupPlayerType()
{
  unPhone::tftp->fillScreen(COLOUR_MENU_BACKGROUND);  

  Page currentPage = PAGE_MAIN;
  // Buttons defined as rectangles to use 
  Rectangle serverBox(20, 15, 135, 135);
  Rectangle clientBox(165, 15, 135, 135);
  Rectangle returnTouch(20, 430, 280, 40);

  Rectangle speedAreaTouch(20, 325, 280, 100);

  Rectangle speedTouch1(25, 320, 63, 100);
  Rectangle speedTouch2(93, 320, 63, 100);
  Rectangle speedTouch3(161, 320, 63, 100);
  Rectangle speedTouch4(229, 320, 63, 100);

  SpeedButton speedButtons[4] = {
      {3, speedTouch1},
      {5, speedTouch2},
      {7, speedTouch3},
      {9, speedTouch4}
  };

  showMainPage(returnTouch);
  showPlayerSelect(serverBox, clientBox);
  showSpeedButtons(speedButtons);

  // Clears any old data from buffer
  TS_Point point = unPhone::tsp->getPoint();

  while (playerType == MODE_NONE)
  {
    if (unPhone::tsp->touched())
    {            
      while (unPhone::tsp->bufferSize() != 1)
      {
        unPhone::tsp->getPoint();
      }

      point = touchScreenMap(unPhone::tsp->getPoint());

      switch (currentPage)
      {
        case PAGE_MAIN:
          handleMainPage(point, serverBox, clientBox, speedAreaTouch, returnTouch, speedButtons, &currentPage);          
          break;

        case PAGE_INSTRUCTIONS:
          handleInstructionsPage(point, serverBox, clientBox, returnTouch, speedButtons, &currentPage);
          break;
      }
      while (!unPhone::tsp->bufferEmpty())
      {
        unPhone::tsp->getPoint();
      }
    }
  }

  detachInterrupt(digitalPinToInterrupt(unPhone::BUTTON1));
  detachInterrupt(digitalPinToInterrupt(unPhone::BUTTON3));
}

void checkWifiStatus() {
  if (WiFi.status() != WL_CONNECTED) 
  {
    Serial.println("checkWifiStatus - not connected to wifi, attempting to connect...");
    udp.stop();
    WiFi.disconnect();
    WiFi.mode(WIFI_STA); // ESP-32 as client
    WiFi.config(ipClient, ipServer, subnet);
    WiFi.begin(WIFI_NAME, WIFI_PASS);
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
      Serial.println("checkWifiStatus - Connected to wifi!");
    }
    else
    {
      Serial.println("checkWifiStatus - Not connected to wifi!");
    }

    udp.begin(localPort);
  }
}

void loadWalls(std::vector<Rectangle> *vec, const unsigned int wallArray[48])
{
  for (int i = 0; i < 48; i++)
  {
    unsigned int row = wallArray[i];
    for (int j = 0; j < 32; j++)
    {
      if (row & (1 << (31 - j)))
      {
        Rectangle toAdd(j * 10, i * 10, 10, 10);
        vec->push_back(toAdd);
      }
    }
  }
}

void showMaze(const unsigned int mWalls[2][48])
{
  walls.clear();
  badWalls.clear();
  loadWalls(&walls, mWalls[0]);
  loadWalls(&badWalls, mWalls[1]);
}

void setupMaze()
{
  switch (playerType)
  {
  case MODE_SERVER:
    showMaze(maze1Walls);
    break;
  case MODE_CLIENT:
    showMaze(maze2Walls);
    break;
  }
}

void setupClient()
{
  Serial.println("setupClient - setting up client!");
  checkWifiStatus();
}

void setupServer()
{
  // WIFI_NAME and WIFI_PASS stored in private.h
  WiFi.softAP(WIFI_NAME, WIFI_PASS); 
  udp.begin(localPort);
}

void setup()
{
  Serial.begin(115200);
  unPhone::begin();
  unPhone::checkPowerSwitch();

  splashScreen();

  setupPlayerType();

  switch (playerType)
  {
  case MODE_CLIENT:
    setupClient();
    break;
  case MODE_SERVER:
    setupServer();
    break;
  }

  setupMaze();
  displayGameView();

  myReadingsBuffer[0] = 'S';
  myReadingsBuffer[1] = 'S';
  otherReadingsBuffer[0] = 'S';
  otherReadingsBuffer[1] = 'S';

  unPhone::accelp->getEvent(&event);

  semaphoreDisplay = xSemaphoreCreateBinary();
  semaphoreLoop = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(
      displayPlayerMove,   /* Function to implement the task */
      "displayPlayerMove", /* Name of the task */
      10000,               /* Stack size in words */
      NULL,                /* Task input parameter */
      0,                   /* Priority of the task */
      &Task1,              /* Task handle. */
      0);                  /* Core where the task should run */
}

void primeSend()
{
  char x;
  char y;

  float distance = pow(event.acceleration.x, 2) + pow(event.acceleration.y, 2);
  if (distance < 1) {
    x = 'S';
    y = 'S';
  }
  else
  {
    double xDist = event.acceleration.x;
    double yDist = event.acceleration.y;
    double angle = atan2(yDist, xDist);
    if (angle > M_PI / 8 && angle < 7 * M_PI / 8)
    {
      x = 'R';
    }
    else if (angle < -M_PI / 8 && angle > -7 * M_PI / 8)
    {
      x = 'L';
    }
    else 
    {
      x = 'S';
    }

    if (angle > -3 * M_PI / 8 && angle < 3 * M_PI / 8)
    {
      y = 'D';
    }
    else if (angle > 5 * M_PI / 8 || angle < -5 * M_PI / 8)
    {
      y = 'U';
    }
    else
    {
      y = 'S';
    }
  }
  sprintf(myReadingsBuffer, "%c%c", x, y);
}

void loopClient() { 
  checkWifiStatus();
  primeSend();
  udp.beginPacket(ipServer, localPort);
  udp.printf(myReadingsBuffer);
  udp.printf("\r\n");
  udp.endPacket();

  if (clientIsDisplaying)
  {
    Serial.println("loopClient - Client loop waiting for semaphore");
    xSemaphoreTake(semaphoreLoop, portMAX_DELAY);
    clientIsDisplaying = false;
  }

  delay(30);

  //recieve
  int packetSize = udp.parsePacket();
  if (packetSize)
  {
    int len = udp.read(otherReadingsBuffer, 255);
    if (len > 0)
      otherReadingsBuffer[len - 1] = 0;
    

    unPhone::accelp->getEvent(&event);
    xSemaphoreGive(semaphoreDisplay);
    clientIsDisplaying = true;
    Serial.println("loopClient - Waiting for display");
  }
  delay(30);
}

void loopServer()
{
  // recieve
  int packetSize = udp.parsePacket();
  if (packetSize)
  {
    int len = udp.read(otherReadingsBuffer, 255);
    if (len > 0)
      otherReadingsBuffer[len - 1] = 0;
    udp.flush();

    unPhone::accelp->getEvent(&event);
    xSemaphoreGive(semaphoreDisplay);

    primeSend();
    Serial.printf("loopServer - Sending: %s\n", myReadingsBuffer);
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.printf(myReadingsBuffer);
    udp.printf("\r\n");
    udp.endPacket();
    xSemaphoreTake(semaphoreLoop, portMAX_DELAY);
  }
}

void loop()
{
  if (unPhone::button2())
  {
    Serial.println("Resetting ESP");
    ESP.restart();
  }
  switch (playerType)
  {
  case MODE_CLIENT:
    loopClient();
    break;
  case MODE_SERVER:
    loopServer();
    break;
  }
}
