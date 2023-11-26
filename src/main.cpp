#include <LEDMatrixDriver.hpp>
// This sketch will 'flood fill' your LED matrix using the hardware SPI driver Library by Bartosz Bielawski.
// Example written 16.06.2017 by Marko Oette, www.oette.info

// Define the ChipSelect pin for the led matrix (Dont use the SS or MISO pin of your Arduino!)
// Other pins are Arduino specific SPI pins (MOSI=DIN, SCK=CLK of the LEDMatrix) see https://www.arduino.cc/en/Reference/SPI
#define LEDMATRIX_CS_PIN 7
#define TETRIS_RIGHT_PIN 2
#define TETRIS_LEFT_PIN 3
#define TETRIS_DOWN_PIN 4
#define TETRIS_ROTATE_PIN 5

// Number of 8x8 segments you are connecting
#define LEDMATRIX_SEGMENTS 4
const int LEDMATRIX_WIDTH = LEDMATRIX_SEGMENTS * 8;


// The LEDMatrixDriver class instance
LEDMatrixDriver lmd(LEDMATRIX_SEGMENTS, LEDMATRIX_CS_PIN);

// Helper function to print a byte as binary for debugging
void printBinary(uint8_t num) {
    // Loop for each bit position
    for (int i = sizeof(num) * 8 - 1; i >= 0; i--) {
        // Extract the bit at position 'i' using shift and AND operation
        int bit = (num >> i) & 1;
        Serial.print(bit);
    }
    Serial.println();
}

const uint8_t blocks[][4] = {
  {0x02,0x02,0x03,0x00}, // L
  {0x01,0x01,0x03,0x00}, // J
  {0x01,0x01,0x01,0x01}, // I
  {0x03,0x03,0x00,0x00}, // O
  {0x02,0x07,0x00,0x00}, // T
  {0x06,0x03,0x00,0x00}, // Z
  {0x03,0x06,0x00,0x00}  // S
};

// 32 rows of 8 bits for the main grid that is displayed
// extra 4 rows for the top of the grid as top safe area
// extra 1 row for the bottom of the grid as bottom safe area
const int DISPLAY_Y_START = 4;                               //defines what y coordinate the top of the led matrix is
const int DISPLAY_Y_END = DISPLAY_Y_START + LEDMATRIX_WIDTH; //defines what y coordinate the bottom of the led matrix is
const int GRID_HEIGHT = LEDMATRIX_WIDTH + DISPLAY_Y_START + 1; // 37

uint8_t placedBlocks[GRID_HEIGHT] = { 
  0x00, 0x00, 0x00, 0x00, // y = 0
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00
};

uint8_t getBlockElement(uint8_t *block, int x, int blockIndex){
  if (x < 0 ){
    return block[blockIndex] >> x * -1;
  }else{
    return block[blockIndex] << x;
  }
}

void displayBlock(uint8_t *block, int x, int y){
  for (int i = 0; i < 4; i++){
    lmd.setColumn(y + i, getBlockElement(block, x, i));
  }
}



void displayBlockAndGrid(uint8_t *block, int x, int y){
  lmd.clear();

  int blockIndex = 0;
  for (int displayGrid = 0; displayGrid < LEDMATRIX_WIDTH; displayGrid++){
    int indexGrid = displayGrid + DISPLAY_Y_START;
    uint8_t newRow = placedBlocks[indexGrid];

    if (indexGrid >= y && indexGrid < y + 4){
        newRow = newRow | getBlockElement(block, x, blockIndex++);
    }
    lmd.setColumn(displayGrid, newRow);
  }
  lmd.display();
}

int getBlockFeetIndex(uint8_t *block){
  int blockFeet;
  for (blockFeet = 3; blockFeet >= 0; blockFeet--){
    if (block[blockFeet]) break;
  }
  return blockFeet;
}

bool willLand(uint8_t *block, int x, int y){

  int blockFeetIndex = getBlockFeetIndex(block);
  uint8_t blockFeetShifted = getBlockElement(block, x, blockFeetIndex);
  bool hasHit = false; bool hasTouchedFloor = false;
  int rowGridCheck = y + blockFeetIndex + 1;

  // check if block will hit another block
  // TODO: fix collision system some blocks can phase through part of blocks
  if ((blockFeetShifted & placedBlocks[rowGridCheck]) > 0){
    Serial.println("Block will hit another block");
    hasHit = true;
  }

  // check if block will hit the floor
  if ((rowGridCheck) >= DISPLAY_Y_END){
    Serial.println("Block will hit the floor");
    hasTouchedFloor = true;
  }
  // block has touched element in grid or hit the floor
  return hasHit || hasTouchedFloor;
}

bool addBlockToGrid(uint8_t *block, int x, int y){
  int absoluteBlockFeetIndex = y + getBlockFeetIndex(block); 
  if (absoluteBlockFeetIndex > GRID_HEIGHT){
    Serial.println("Block is out of bounds");
    return false;
  }

  int i = 0;
  Serial.println("Adding block to grid");
  for (int rowGrid = y; rowGrid <= absoluteBlockFeetIndex; rowGrid++){
    placedBlocks[rowGrid] = placedBlocks[rowGrid] | getBlockElement(block, x, i);
    printBinary(getBlockElement(block, x, i++));
  }
  return true;
}

bool isValidPosition(uint8_t *block, int nextX, int nextY){
  // TODO: Check collision with side walls 


  // TODO: check collision with existing placed blocks
  return true;
}



void printBlock(uint8_t *block){
  for (int i = 0; i < 4; i++){
    printBinary(block[i]);
  }
}

void printGrid(){
  for (int i = 0; i < GRID_HEIGHT; i++){
    printBinary(placedBlocks[i]);
  }
}

int x; int y; 
uint8_t currentFallingBlock[4];

void resetblock(){
  x = 3; y = 0;
  memcpy(currentFallingBlock, blocks[random(7)], 4);
}

void resetGrid(){
  for (int i = 0; i < GRID_HEIGHT; i++){
    placedBlocks[i] = 0x00;
  }
  resetblock();
}

void setup() {
	// init the display
  Serial.begin(115200);
	lmd.setEnabled(true);
	lmd.setIntensity(0);   // 0 = low, 10 = high
  pinMode(TETRIS_RIGHT_PIN, INPUT_PULLUP);
  pinMode(TETRIS_LEFT_PIN, INPUT_PULLUP);
  pinMode(TETRIS_DOWN_PIN, INPUT_PULLUP);
  pinMode(TETRIS_ROTATE_PIN, INPUT_PULLUP);
  resetblock();
}

void loop() {

  displayBlockAndGrid(currentFallingBlock, x, ++y);
  
  if (digitalRead(TETRIS_RIGHT_PIN) == LOW && isValidPosition(currentFallingBlock, x+1, y)){
    x++;
  }
  if (digitalRead(TETRIS_LEFT_PIN) == LOW && isValidPosition(currentFallingBlock, x-1, y)){
    x--;
  }
  if (digitalRead(TETRIS_DOWN_PIN) == LOW && y < 32){
    y++;
  }

  if (digitalRead(TETRIS_ROTATE_PIN) == LOW){
    // TODO: implement rotation
    Serial.println("Rotate");
  }

  // check if next position will land
  if (willLand(currentFallingBlock, x, y)){
    Serial.println(y);
    if (y < DISPLAY_Y_START){
      printGrid();
      Serial.println("Game Over");
      resetGrid();
      return;
    }
    addBlockToGrid(currentFallingBlock, x, y);
    // TODO: check if any rows are full, remove, and shift down
    resetblock();
  }
  // TODO: change speed based on level and to not use delay for responsive inputs
  delay(100);
}



