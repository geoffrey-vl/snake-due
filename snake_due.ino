/************************************

  Play Snake on Arduino Due and Adafruit ILI9341

 ****************************************************/
 
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

// For the Adafruit shield, these are the default.
#define TFT_DC 9
#define TFT_CS 10

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

uint16_t WIDTH, HEIGHT, MIN_SNAKE_X, MIN_SNAKE_Y, MAX_SNAKE_X, MAX_SNAKE_Y;



////////////////////////////// MOVEMEMNT ///////////////////////


enum class Direction {
  DIR_LEFT,
  DIR_RIGHT,
  DIR_UP,
  DIR_DOWN
};


struct Position {
  uint16_t x;
  uint16_t y;

  static Position createRandomPosition() {
    Position pos;
    pos.x = random(MIN_SNAKE_X, MAX_SNAKE_X);
    pos.y = random(MIN_SNAKE_Y, MAX_SNAKE_Y);
    pos.alignToGrid();
    return pos;
  }

  void alignToGrid() {
    if(x%5 != 0)
      x-=(x%5);
    if(y%5 != 0)
      y-=(y%5);
  }

  bool operator==(const Position& p) {
    if(p.x == x && p.y == y)
      return true;
    return  false;
  }
};



Direction input;



//////////////////////////// GAME OBJECTS //////////////////

class SnakeLimb {
 public:   
  Position pos;
  Position previousPos;
  uint16_t w=5;
  bool healthy;
  bool isHead;
  bool isTail;

  void draw(Adafruit_ILI9341 &tft) {
    /* x, y ->.___
     *        |   |
     *        |___|
     */

    //erase previous
    if(isTail) {
      tft.fillRect(previousPos.x, previousPos.y, w, w, ILI9341_BLACK);
    }


    //draw current
    if(healthy) {
      if(isHead) {
        tft.fillRect(pos.x, pos.y, w, w, ILI9341_YELLOW);
      }
      else {
        tft.fillRect(pos.x, pos.y, w, w, ILI9341_GREEN);
      }
    } else {
      if(isHead) {
        tft.fillRect(pos.x, pos.y, w, w, ILI9341_ORANGE);
      }
      else {
        tft.fillRect(pos.x, pos.y, w, w, ILI9341_RED);
      }
    }
  }
};


class Snake {
public:
  Direction direction;
  Position pos;

  Snake() {

  }

  void init(Position initPos, Direction dir) {
    pos = initPos;
    direction = dir;
    input = dir;
    head=0;
    nrOfLimps=1;
    limbAdded=false;
    limbs[head].pos = pos;
    limbs[head].previousPos = limbs[head].pos;
    limbs[head].healthy = true;
    limbs[head].isHead = true;
    limbs[head].isTail = true;
  }

  void updatePos(Position newPos) {
    pos = newPos;
    
    //update head
    limbs[head].previousPos = limbs[head].pos;
    limbs[head].pos = pos;

    //update others
    for(uint16_t i=1; i<nrOfLimps; i++) {
      //check if previous tail has moved enough to uncover the new tail
      if(limbs[i].isTail && limbAdded==true) {
        if(limbs[i].pos.x == limbs[i-1].pos.x) {
            if(limbs[i].pos.y < limbs[i-1].pos.y+5 || limbs[i].pos.y > limbs[i-1].pos.y-5)
              limbAdded=false;
        }
        if(limbs[i].pos.y == limbs[i-1].pos.y) {
            if(limbs[i].pos.x < limbs[i-1].pos.x+5 || limbs[i].pos.x > limbs[i-1].pos.x-5)
              limbAdded=false;
        }
      }

      //update
      if(!limbs[i].isTail || limbAdded==false) {
        limbs[i].previousPos = limbs[i].pos;
        limbs[i].pos = limbs[i-1].previousPos;
      }

    }
  }
  
  void draw(Adafruit_ILI9341 &tft) {
    for(uint16_t i=0; i<nrOfLimps; i++) {
      limbs[i].draw(tft);
    }
  }

  void updateHealth(bool isAlive) {
    for(uint16_t i=0; i<nrOfLimps; i++) {
      limbs[i].healthy = isAlive;
    }
  }

  void addLimb() {
    //make current last limb no longer tail
    limbs[nrOfLimps-1].isTail = false;
    
    nrOfLimps++;
    
    limbs[nrOfLimps-1].pos = limbs[nrOfLimps-2].pos;
    limbs[nrOfLimps-1].previousPos = limbs[nrOfLimps-2].previousPos;
    limbs[nrOfLimps-1].isTail = true;
    limbs[nrOfLimps-1].healthy = true;
    limbAdded = true;
  }

  bool canTurnLeft() {
    if((direction == Direction::DIR_UP || direction == Direction::DIR_DOWN)) {
      return true;
    }
    return false;
  }

  bool canTurnRight() {
    if((direction == Direction::DIR_UP || direction == Direction::DIR_DOWN)) {
      return true;
    }
    return false;
  }

  bool canTurnUp() {
    if((direction == Direction::DIR_LEFT || direction == Direction::DIR_RIGHT)) {
      return true;
    }
    return false;
  }

  bool canTurnDown() {
    if((direction == Direction::DIR_LEFT || direction == Direction::DIR_RIGHT)) {
      return true;;
    }
    return false;
  }

  bool hasBodyAtPosition(Position p) {
    for(uint16_t i=1; i<nrOfLimps; i++) {
      if(limbs[i].pos == p)
        return true;
    }
    return false;
  }


private:
  SnakeLimb limbs[100];
  uint16_t nrOfLimps;
  uint16_t head;
  uint8_t limbAdded;
};

struct Snake snake;


struct Food {
  Position pos;
  uint16_t color = ILI9341_DARKGREY;
  uint16_t w=5;

  void draw(Adafruit_ILI9341 &tft) {
    //draw current
    tft.fillRect(pos.x, pos.y, w, w, color);
  }
};

struct Food food;


////////////////// GAME ENGINE/////////////////

struct GameClock {
  unsigned long microsElapsed;

  GameClock(unsigned long updateIntervalMillis) {
    updateIntervalMicros = updateIntervalMillis*1000;
    microsElapsed = 0;
  }

  bool hasElapsed() {
    unsigned long now = micros(); //overflows each 70 minutes after machine boot
    if(now < lastMicros+updateIntervalMicros) 
      return false;
    microsElapsed = now - lastMicros;
    lastMicros = now;
    return true;
  }

  void reset() {
    microsElapsed = 0;
    lastMicros = micros();
  }

private:
  unsigned long lastMicros;
  unsigned long updateIntervalMicros;
};





struct GameClock inputClock(50); //in millis
struct GameClock logicClock(100); //in millis, degrease value to increase game speed
struct GameClock renderClock(15); //in millis






///////////////// MAIN ///////////////


void setup() {
  Serial.begin(115200);
  Serial.println("ILI9341 Test!"); 

  tft.begin();
  readDiag();

  // if analog input pin 0 is unconnected, random analog
  // noise will cause the call to randomSeed() to generate
  // different seed numbers each time the sketch runs.
  // randomSeed() will then shuffle the random function.
  randomSeed(analogRead(0));
}

void readDiag() {
  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(ILI9341_RDMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDPIXFMT);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDIMGFMT);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(ILI9341_RDSELFDIAG);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX); 
  
  tft.setRotation(3);
  WIDTH = tft.width();
  HEIGHT = tft.height();
  MIN_SNAKE_X = 5;
  MIN_SNAKE_Y = 5;
  MAX_SNAKE_X = WIDTH-10;
  MAX_SNAKE_Y = HEIGHT-10;
}

void loop() {
  drawIntro();
  drawInitialWorld();

  logicClock.reset();
  inputClock.reset();
  renderClock.reset();
  
  playGame();

  drawEndGame();
}


////////////////// INTRO /////////////////

void drawWalls() {
  tft.fillRect(0, 0, WIDTH, 5, ILI9341_BLUE);
  tft.fillRect(0, 0, 5, HEIGHT, ILI9341_BLUE);
  tft.fillRect(0, HEIGHT-5, WIDTH, 5, ILI9341_BLUE);
  tft.fillRect(WIDTH-5, 0, 5, HEIGHT, ILI9341_BLUE);
}

void drawIntro() {
  tft.fillScreen(ILI9341_BLACK);
  yield();

  drawWalls();
  tft.setCursor(75, 100);
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(5);
  tft.println("SNAKE!");
  delay(3*1000);
}

void drawInitialWorld() {
  tft.fillScreen(ILI9341_BLACK);
  yield();
  
  //set snake start point
  Position snakePos = { (WIDTH/2)-2, (HEIGHT/2)-2 };
  snakePos.alignToGrid();
  snake.init( snakePos, Direction::DIR_RIGHT );
  snake.draw(tft);

  createRandomFood();
  food.draw(tft);

  drawWalls();
}


//////////////////// END GAME //////////////////////////

void drawEndGame() {
  //show dead snake
  delay(1*1000);

  //show game over
  tft.fillScreen(ILI9341_BLACK);
  yield();

  drawWalls();
  tft.setCursor(20, 100);
  tft.setTextColor(ILI9341_RED);
  tft.setTextSize(5);
  tft.println("GAME OVER!");
  delay(3*1000);
}


/////////////////////// GAME LOOP /////////////////////////

void playGame() {
  bool isEnded = false;
  
  while(!isEnded) {
    //1. process input
    handleInput();
    //2. update positions, perform collision detection, AI
    updateWorld(isEnded);
    //3. erase screen 
    //  => we don't do this since it takes to much time, instead we'll erase what is no longer needed to render
    //4. render
    render(isEnded);
  }
}

void handleInput() {
  if(!inputClock.hasElapsed())
    return;
  scanInput();
}

void scanInput() {
  //detect input
  while (Serial.available() > 0) {
    // read incoming serial data:
    char inByte = Serial.read(); 
    
    switch (inByte) {
      case 'z': //UP
        input = Direction::DIR_UP;
        break;
      case 's': //DOWN
        input = Direction::DIR_DOWN;
        break;
      case 'q': //LEFT
        input = Direction::DIR_LEFT;
        break;
      case 'd': //RIGHT
        input = Direction::DIR_RIGHT;
        break;
    }
  }
}


void updateWorld(bool &isEnded) {
  if(!logicClock.hasElapsed())
    return;

  snake.direction = getNewDirection();

  Position pos = snake.pos;
  
  uint8_t pixelsToMove = 5;
  switch(snake.direction) {
    case Direction::DIR_UP:
      pos.y-=pixelsToMove;
      break;
    case Direction::DIR_DOWN:
      pos.y+=pixelsToMove;
      break;
    case Direction::DIR_LEFT:
      pos.x-=pixelsToMove;
      break;
    case Direction::DIR_RIGHT:
      pos.x+=pixelsToMove;
      break;
  }

  //collision detection: walls
  if( pos.x < MIN_SNAKE_X || pos.x > MAX_SNAKE_X || pos.y < MIN_SNAKE_Y || pos.y > MAX_SNAKE_Y) {
    isEnded = true;
    snake.updateHealth(false);
    return;
  }
  //collision detection: self
  if(snake.hasBodyAtPosition(pos)) {
    isEnded = true;
    snake.updateHealth(false);
    return;
  }

  snake.updatePos(pos);
  
  //collision detection: food
  if( pos.x == food.pos.x && pos.y == food.pos.y) { //NJAM!!
    createRandomFood();
    snake.addLimb();
  }
}


Direction getNewDirection() {
  switch (input) {
    case Direction::DIR_UP:
      if(snake.canTurnUp()) {   //only allow per 5 pixels movement
        return Direction::DIR_UP;
      }
      break;
    case Direction::DIR_DOWN:
      if(snake.canTurnDown()) {
        return Direction::DIR_DOWN;
      }
      break;
    case Direction::DIR_LEFT:
      if(snake.canTurnLeft()) {
        return Direction::DIR_LEFT;
      }
      break;
    case Direction::DIR_RIGHT:
      if(snake.canTurnRight()) {
        return Direction::DIR_RIGHT;
      }
      break;
  }
  return snake.direction;
}


void render(bool &isEnded) {
  if(!renderClock.hasElapsed() && !isEnded)
    return;

  food.draw(tft);
  
  snake.draw(tft);
}



void createRandomFood() {
  food.pos = Position::createRandomPosition();
}

