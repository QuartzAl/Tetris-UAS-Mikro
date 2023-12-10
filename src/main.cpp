#include <LEDMatrixDriver.hpp>
#include <TM1637Display.h>
#include <Arduino.h>

// Define the connections pins TM1637_CLK, TM1637_DIO
#define CLK 9
#define DIO 8

// Define the connections pins LEDMatrix Max7219 chip select (the rest uses hardware SPI pins)
#define LEDMATRIX_CS_PIN 7

// Define the connection pins for the buttons
#define TETRIS_LEFT_PIN 2
#define TETRIS_RIGHT_PIN 3
#define TETRIS_DOWN_PIN 4
#define TETRIS_ROTATE_PIN 5

// Number of 8x8 segments you are connecting
#define LEDMATRIX_SEGMENTS 4
const int LEDMATRIX_WIDTH = LEDMATRIX_SEGMENTS * 8;

struct coordVector {
  int x;
  int y;
};

// The LEDMatrixDriver class instance
LEDMatrixDriver lmd(LEDMATRIX_SEGMENTS, LEDMATRIX_CS_PIN);

// The TM1637Display class instance
TM1637Display display = TM1637Display(CLK, DIO);

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
    {0x02, 0x02, 0x03, 0x00}, // L
    {0x01, 0x01, 0x03, 0x00}, // J
    {0x01, 0x01, 0x01, 0x01}, // I
    {0x03, 0x03, 0x00, 0x00}, // O
    {0x02, 0x07, 0x00, 0x00}, // T
    {0x06, 0x03, 0x00, 0x00}, // Z
    {0x03, 0x06, 0x00, 0x00}  // S
};

// 32 rows of 8 bits for the main grid that is displayed
// extra 4 rows for the top of the grid as top safe area
// extra 1 row for the bottom of the grid as bottom safe area
const int DISPLAY_Y_START =
    4; // defines what y index the top of the led matrix is
const int DISPLAY_Y_END =
    DISPLAY_Y_START + LEDMATRIX_WIDTH; // defines what y index the bottom
                                       // of the led matrix is
const int GRID_HEIGHT = LEDMATRIX_WIDTH + DISPLAY_Y_START + 1; // 37

uint8_t placedBlocks[GRID_HEIGHT] = {
    0x00, 0x00, 0x00, 0x00, // y = 0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

uint8_t getBlockElement(uint8_t *block, int x, int blockIndex) {
  if (x < 0) {
    return block[blockIndex] >> x * -1;
  } else {
    return block[blockIndex] << x;
  }
}

int getBlockFeetIndex(uint8_t *block) {
  int blockFeet;
  for (blockFeet = 3; blockFeet >= 0; blockFeet--) {
    if (block[blockFeet])
      break;
  }
  return blockFeet;
}

int getLeftMostBlockIndex(uint8_t *block) {
  uint8_t mask = 0x08;
  int smallestShiftBy = 5;
  for (int rowIndex = 0; rowIndex <= 3; rowIndex++) {
    for (int shiftBy = 0; shiftBy <= 3; shiftBy++) {
      if ((block[rowIndex] & (mask >> shiftBy)) > 0 &&
          shiftBy < smallestShiftBy) {
        smallestShiftBy = shiftBy;
        break;
      }
    }
  }
  return 3 - smallestShiftBy;
}

void displayBlockAndGrid(uint8_t *block, int x, int y) {
  lmd.clear();
  int blockIndex = 0;

  if (y < DISPLAY_Y_START) {
    blockIndex = DISPLAY_Y_START - y;
  }

  for (int displayGrid = 0; displayGrid < LEDMATRIX_WIDTH; displayGrid++) {
    int indexGrid = displayGrid + DISPLAY_Y_START;
    uint8_t newRow = placedBlocks[indexGrid];

    if (indexGrid >= y && indexGrid < y + 4) {
      newRow = newRow | getBlockElement(block, x, blockIndex++);
    }
    lmd.setColumn(displayGrid, newRow);
  }
  lmd.display();
}

bool willLand(uint8_t *block, int x, int y) {
  int blockIndex = 0;
  int nextBlockFeetIndex = getBlockFeetIndex(block) + y + 1;

  // check if block will hit grid
  for (int nextRowGrid = y + 1; nextRowGrid <= nextBlockFeetIndex;
       nextRowGrid++) {
    uint8_t blockRow = getBlockElement(block, x, blockIndex++);
    uint8_t nextGridRow = placedBlocks[nextRowGrid];

    if ((blockRow & nextGridRow) > 0) {
      Serial.println("Block will hit grid");
      return true;
    }
  }

  int blockFeetGridCoords = y + getBlockFeetIndex(block) + 1;

  // check if block will hit the floor
  if ((blockFeetGridCoords) >= DISPLAY_Y_END) {
    Serial.println("Block will hit the floor");
    return true;
  }
  // block has touched element in grid or hit the floor
  return false;
}

bool addBlockToGrid(uint8_t *block, int x, int y) {
  int gridBlockFeetIndex = y + getBlockFeetIndex(block);
  if (gridBlockFeetIndex > DISPLAY_Y_END) {
    return false;
  }

  int i = 0;
  for (int rowGrid = y; rowGrid <= gridBlockFeetIndex; rowGrid++) {
    placedBlocks[rowGrid] =
        placedBlocks[rowGrid] | getBlockElement(block, x, i++);
  }
  return true;
}

bool isValidXPosition(uint8_t *block, int nextX, int y) {
  // check right side
  if (nextX < 0) {
    Serial.println("Block hit right wall");
    return false;
  }
  // check left side
  int nextLeftSideGridIndex = getLeftMostBlockIndex(block) + nextX;
  if (nextLeftSideGridIndex > 7) {
    Serial.println("Block hit left wall");
    return false;
  }
  // check blcok collision
  int blockRowIndex = 0;
  for (int rowGridIndex = y; rowGridIndex <= y + getBlockFeetIndex(block);
       rowGridIndex++) {
    uint8_t blockRow = getBlockElement(block, nextX, blockRowIndex++);
    uint8_t gridRow = placedBlocks[rowGridIndex];
    if ((blockRow & gridRow) > 0) {
      Serial.println("Block hit existing block");
      return false;
    }
  }
  return true;
}

void rotateBlock(uint8_t *block) {
  coordVector points[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
  int pointIndex = 0;
  for (int rowIndex = 0; rowIndex <= 3; rowIndex++) {
    uint8_t blockRow = block[rowIndex];
    for (int shiftBy = 0; shiftBy <= 3; shiftBy++) {
      if ((blockRow & (0x01 << shiftBy)) > 0) {
        points[pointIndex++] = {shiftBy, rowIndex};
      }
    }
  }
  // rotate points by  90 degrees
  for (int i = 0; i < 4; i++) {
    points[i] = {-points[i].y, points[i].x};
  }

  // shift all points from negative to positive
  // find smallest x and y
  int smallestX = 0;
  int smallestY = 0;
  for (int i = 0; i < 4; i++) {
    if (points[i].x < smallestX) {
      smallestX = points[i].x;
    }
    if (points[i].y < smallestY) {
      smallestY = points[i].y;
    }
  }
  // add by smallest to make all points positive
  for (int i = 0; i < 4; i++) {
    points[i].x += abs(smallestX);
    points[i].y += abs(smallestY);
  }
  // empty current block
  for (int i = 0; i < 4; i++) {
    block[i] = 0x00;
  }
  // copy points to block in uint8_t form
  for (int i = 0; i < 4; i++) {
    block[points[i].y] |= 0x01 << points[i].x;
  }
}

int clearFullRows() {
  int clearedRows = 0;
  for (int i = 0; i < GRID_HEIGHT; i++) {
    if (placedBlocks[i] == 0xff) {
      Serial.println("Row is full, clearing...");
      clearedRows++;
      // shift blocks down
      for (int j = i; j > 0; j--) {
        placedBlocks[j] = placedBlocks[j - 1];
      }
    }
  }
  return clearedRows;
}

int x;
int y;
uint8_t currentFallingBlock[4];

void resetblock() {
  x = 3;

  memcpy(currentFallingBlock, blocks[analogRead(A0) % 7], 4);
  y = DISPLAY_Y_START - getBlockFeetIndex(currentFallingBlock) - 1;
}

void resetGrid() {
  for (int i = 0; i < GRID_HEIGHT; i++) {
    placedBlocks[i] = 0x00;
  }
  resetblock();
}

void displayClosingAnimation() {
  for (int i = 0; i < LEDMATRIX_WIDTH/2; i++) {
    lmd.setColumn(i, 0xff);
    lmd.setColumn(LEDMATRIX_WIDTH - 1 - i, 0xff);
    lmd.display();
    delay(50);
  }
  delay(100);
  for (int i = 0; i < LEDMATRIX_WIDTH/2; i++) {
    lmd.setColumn(i, 0x00);
    lmd.setColumn(LEDMATRIX_WIDTH - 1 - i, 0x00);
    lmd.display();
    delay(50);
  }
}

unsigned long last_time;
unsigned long drop_delay;
unsigned long last_input;
unsigned long current_time;
int last_left_input;
int last_right_input;
unsigned long last_down_input;
int last_rotate_input;
unsigned long points;

void setup() {
  // init the display
  Serial.begin(115200);
  lmd.setEnabled(true);
  lmd.setIntensity(0); // 0 = low, 10 = high
  display.setBrightness(4); // 0=dimmest, 7=brightest

  pinMode(TETRIS_LEFT_PIN, INPUT_PULLUP);
  pinMode(TETRIS_RIGHT_PIN, INPUT_PULLUP);
  pinMode(TETRIS_DOWN_PIN, INPUT_PULLUP);
  pinMode(TETRIS_ROTATE_PIN, INPUT_PULLUP);

  // random seed
  pinMode(A0, INPUT);

  resetblock();

  last_time = millis();
  last_input = last_time;
  drop_delay = 500;
  last_left_input = HIGH;
  last_right_input = HIGH;
  last_down_input = 0;
  last_rotate_input = HIGH;
  points = 0;
  display.showNumberDec(points, false, 4, 0);
}

void loop() {
  current_time = millis();

  if (current_time - last_time >= drop_delay) {
    last_time = current_time;
    Serial.println(y);

    if (willLand(currentFallingBlock, x, y)) {
      Serial.println(y);

      if (y < DISPLAY_Y_START) {
        Serial.println("Game Over");
        resetGrid();
        displayClosingAnimation();
        return;
      }
      addBlockToGrid(currentFallingBlock, x, y);
      int clearedRows = clearFullRows();
      if (clearedRows > 0) {
        // drop_delay = drop_delay / 2;
        points += 1 * clearedRows * clearedRows;
        display.showNumberDec(points, false, 4, 0);
      }
      resetblock();
    }
    y++;
  }

  if (digitalRead(TETRIS_LEFT_PIN) == LOW &&
      digitalRead(TETRIS_LEFT_PIN
    ) != last_right_input) {
    if (isValidXPosition(currentFallingBlock, x + 1, y)) {
      x++;
    }
  }
  if (digitalRead(TETRIS_RIGHT_PIN) == LOW &&
      digitalRead(TETRIS_RIGHT_PIN) != last_left_input) {
    if (isValidXPosition(currentFallingBlock, x - 1, y)) {
      x--;
    }
  }
  if (digitalRead(TETRIS_DOWN_PIN) == LOW &&
      current_time - last_down_input >= 50) {
    if (!willLand(currentFallingBlock, x, y)) {
      y++;
    }
    last_down_input = current_time;
  }

  if (digitalRead(TETRIS_ROTATE_PIN) == LOW &&
      digitalRead(TETRIS_ROTATE_PIN) != last_rotate_input) {
    Serial.println("Rotate");
    uint8_t rotatedBlock[4];
    memcpy(rotatedBlock, currentFallingBlock, 4);
    rotateBlock(rotatedBlock);

    if (isValidXPosition(rotatedBlock, x, y)) {
      memcpy(currentFallingBlock, rotatedBlock, 4);
    } else {
      Serial.println("Invalid rotation");
    }
  }
  displayBlockAndGrid(currentFallingBlock, x, y);

  last_left_input = digitalRead(TETRIS_RIGHT_PIN);
  last_right_input = digitalRead(TETRIS_LEFT_PIN
);
  last_rotate_input = digitalRead(TETRIS_ROTATE_PIN);
  delay(50);
}
