/*
Hardware: 
 - 128x64 SSD1306 OLED screen
 - Some 3V3 microcontroller with 2 ADC pins and 1 I2C
 - Gyro + Acceleration sensor with at least x and y accelerations
    - used Sparkfun 5D0F board
 
 If using ESP32 Devkit V1 DOIT:
  - SDA = GPIO21
  - SCL = GPIO22
  - ADCx = 15
  - ADCy = 2
*/


#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define X_RATE_IN_PIN 15
#define Y_RATE_IN_PIN 2

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET_PIN -1   // -1 if using Arduino reset pin
#define SCREEN_ADDRESS 0x3C // 128x32: 0x3C || 128x64: 0x3D
//#define SCREEN_ADDRESS 0x3D

Adafruit_SSD1306 OLED_LCD(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);

boolean gameover = false;

boolean foodinit  = false;
boolean foodeaten = false;

const int SNAKE_ROWS = 200; // snake segments (rows)
const int SNAKE_COLS = 2;  // x and y for each segment
const int snake_pos_x_pos = 0;
const int snake_pos_y_pos = 1;

int snake_pos[SNAKE_ROWS][SNAKE_COLS];
int snake_length = 25;
int food_pos[] = {0, 0}; // y, x (or row, col) of food

int score = 0;

int game_speed = 100;

int h_dir = 1; // will be either 1 or -1 to control left/right
int v_dir = 0; // will be either 1 or -1 to control up/down

void initSnake();

void checkSerial()
{
  
  if (Serial.available())
  {
    switch(Serial.read())
    {
    case 'w':
      // up
      if (v_dir != 1)
      {
        v_dir = -1;
        h_dir = 0;
        Serial.println("w");
      }
      break;
    case 'a':
      // left
      if (h_dir != 1)
      {
        h_dir = -1;
        v_dir = 0;
        Serial.println("a");
      }
      break;
    case 'd':
      // right
      if (h_dir != -1)
      {
        h_dir = 1;
        v_dir = 0;
        Serial.println("d");
      }
      break;
    case 's':
      // down
      if (v_dir != -1)
      {
        v_dir = 1;
        h_dir = 0;
        Serial.println("s");
      }
      break;
    default:
      break;
    }
  }
}

int x_thresh_low = 1200;
int y_thresh_low = 1200;
int x_thresh_high = 1800; 
int y_thresh_high = 1800;
unsigned long last_gyro_check_time = 0;
unsigned long gyro_debounce_time = 600;
int x_rate, y_rate;
void checkGyroInput() {
  if(millis() - last_gyro_check_time > gyro_debounce_time) {
    x_rate = analogRead(X_RATE_IN_PIN);
    y_rate = analogRead(Y_RATE_IN_PIN);
    
    if (x_rate >= x_thresh_high) { 
      if (h_dir != -1)
      {
        h_dir = 1;
        v_dir = 0;
        Serial.print("RIGHT: "); Serial.println(x_rate);
      }
      last_gyro_check_time = millis(); 
    }
    else if (x_rate <= x_thresh_low)  { 
      if (h_dir != 1)
      {
        h_dir = -1;
        v_dir = 0;
        Serial.print("LEFT: "); Serial.println(x_rate);
      }
      last_gyro_check_time = millis(); 
    }
    else if (y_rate >= y_thresh_high) { 
      if (v_dir != 1)
      {
        v_dir = -1;
        h_dir = 0;
        Serial.print("UP: "); Serial.println(y_rate);
      }
      last_gyro_check_time = millis(); 
    }
    else if (y_rate <= y_thresh_low)  { 
      if (v_dir != -1)
      {
        v_dir = 1;
        h_dir = 0;
        Serial.print("DOWN: "); Serial.println(y_rate);
      }  
      last_gyro_check_time = millis(); 
    }
  }
}

void moveSnake()
{
  OLED_LCD.clearDisplay();

  if (!gameover)
  {
    // let all non-head segments inherit the x,y values of the next
    // segment to give the illusion of movement
    for (int i = 0; i < snake_length-1; ++i)
    {
      snake_pos[i][snake_pos_x_pos] = snake_pos[i+1][snake_pos_x_pos];
      snake_pos[i][snake_pos_y_pos] = snake_pos[i+1][snake_pos_y_pos];
    }

    // now move the head
    snake_pos[snake_length-1][snake_pos_x_pos] += h_dir;
    snake_pos[snake_length-1][snake_pos_y_pos] += v_dir;

    // check if snake head is out of bounds
    if ( (snake_pos[snake_length-1][snake_pos_x_pos] < 0) || (snake_pos[snake_length-1][snake_pos_x_pos] > SCREEN_WIDTH) || 
         (snake_pos[snake_length-1][snake_pos_y_pos] < 0) || (snake_pos[snake_length-1][snake_pos_y_pos] > SCREEN_HEIGHT) )
    {
      gameover = true;
    }

    // check if snake head has hit the body
    int head_x = snake_pos[snake_length-1][snake_pos_x_pos];
    //Serial.print("head_x: "); Serial.println(head_x); // debug
    int head_y = snake_pos[snake_length-1][snake_pos_y_pos];
    //Serial.print("head_y: "); Serial.println(head_y); // debug

    for (int i = 0; i < snake_length-1; ++i)
    {
      if ( snake_pos[i][snake_pos_x_pos] == head_x && 
           snake_pos[i][snake_pos_y_pos] == head_y )
      {
        //Serial.print("fail_x: "); Serial.println(snake_pos[i][snake_pos_x_pos]); // debug
        //Serial.print("fail_y: "); Serial.println(snake_pos[i][snake_pos_y_pos]); // debug
        gameover = true;
        break;
      }
    }
  }

  if (!gameover)
  {
    // draw each segment of the snake
    for(int i = 0; i < snake_length; ++i)
    {
      OLED_LCD.drawPixel(snake_pos[i][snake_pos_x_pos], snake_pos[i][snake_pos_y_pos], SSD1306_WHITE);
    }

    if ( (snake_pos[snake_length-1][snake_pos_x_pos] == food_pos[0]) && 
         (snake_pos[snake_length-1][snake_pos_y_pos] == food_pos[1]) )
    {
      foodeaten = true;
    }
  } 
  else
  {
    OLED_LCD.setTextSize(2);
    OLED_LCD.setTextColor(SSD1306_WHITE);
    OLED_LCD.setCursor(0, 0);
    OLED_LCD.println("GAME OVER");
    OLED_LCD.setTextSize(1);
    OLED_LCD.setCursor(0, 20);
    OLED_LCD.print("SCORE: "); OLED_LCD.println(score);
    OLED_LCD.setCursor(0, 40);
    OLED_LCD.print("(UP x2 = RESTART)");
    OLED_LCD.display();
	
	bool restart = false;
    int up_counter = 0;
    // the below while() loop forces the program into a forever "game over" loop that
	// can only be left by detecting 2 "UP" inputs
	while(!restart) {
      if(millis() - last_gyro_check_time > gyro_debounce_time) {
        x_rate = analogRead(X_RATE_IN_PIN);
        y_rate = analogRead(Y_RATE_IN_PIN);
        
             if (x_rate >= x_thresh_high) { Serial.println("RESTART-RIGHT"); last_gyro_check_time = millis(); }
        else if (x_rate <= x_thresh_low)  { Serial.println("RESTART-LEFT"); last_gyro_check_time = millis(); }
        else if (y_rate >= y_thresh_high) { Serial.println("RESTART-UP"); up_counter+=1; last_gyro_check_time = millis(); }
        else if (y_rate <= y_thresh_low)  { Serial.println("RESTART-DOWN"); last_gyro_check_time = millis(); }
      }
      if(up_counter >= 2) {
        restart = true;
      }
    }
	// if we're here, that means we've detected 2 "UP" motions and have broken out of
	// our "game over" loop and can now reset the game anew
    gameover = false;
    h_dir = 1;
    v_dir = 0;
    initSnake();
  } 
}



void initSnake()
{
  for (int i = 0; i < SNAKE_ROWS; ++i)
  {
    if (i < snake_length)
    {
      snake_pos[i][snake_pos_x_pos] = 0;
      snake_pos[i][snake_pos_y_pos] = 0;
    } 
    else
    {
      // parts of snake that are -99 are not 
      // initialized yet
      snake_pos[i][snake_pos_x_pos] = -99;
      snake_pos[i][snake_pos_y_pos] = -99;
    }
  }

  OLED_LCD.clearDisplay();
  OLED_LCD.setTextSize(2);
  OLED_LCD.setTextColor(SSD1306_WHITE);
  OLED_LCD.setCursor(1, 10);
  OLED_LCD.println("GET READY!");
  OLED_LCD.display();
  delay(2000);

  for (int i = 0; i < snake_length; ++i)
  {
    OLED_LCD.drawPixel( snake_pos[i][snake_pos_x_pos], snake_pos[i][snake_pos_y_pos], SSD1306_WHITE);
  }
}



void checkFood()
{
  if (!foodinit)
  {
    foodinit = true;

    food_pos[0] = random(1,126);
    food_pos[1] = random(1,30);
  }

  if (foodeaten)
  {
    foodeaten = false;

    food_pos[0] = random(1,126);
    food_pos[1] = random(1,30);

    // easy way to add a 5 segment long snake part
    // if the total length <= 94
    if (snake_length <= (SNAKE_ROWS-6))
    {
      for ( int i = 0; i < 10; ++i )
      {
        // head is snake_pos[snake_length-1]
        snake_pos[snake_length+i][snake_pos_x_pos] = snake_pos[snake_length-1][snake_pos_x_pos];
        snake_pos[snake_length+i][snake_pos_y_pos] = snake_pos[snake_length-1][snake_pos_y_pos];
      }
    }

    // if there are < 5 slots available for a segment, just fill up the rest
    if ( (snake_length >= (SNAKE_ROWS-5)) && (snake_length <= SNAKE_ROWS) )
    {
      for ( int i = 0; i < (SNAKE_ROWS-1)-snake_length; ++i)
      {
        snake_pos[snake_length+i][snake_pos_x_pos] = snake_pos[snake_length-1][snake_pos_x_pos];
        snake_pos[snake_length+i][snake_pos_y_pos] = snake_pos[snake_length-1][snake_pos_y_pos];
      }
    }

    snake_length += 10;
    //max length is 199 for safety
    if (snake_length > (SNAKE_ROWS-1)) { snake_length = SNAKE_ROWS-1; }

    game_speed -= 5;
    // 25ms per main loop is fastest game speed
    if (game_speed < 25) { game_speed = 25; }


    // now move the head
    snake_pos[snake_length-1][snake_pos_x_pos] += h_dir;
    snake_pos[snake_length-1][snake_pos_y_pos] += v_dir;
    
    score += 5;
  }

  OLED_LCD.drawPixel(food_pos[0], food_pos[1], SSD1306_WHITE); 
}

void setup() 
{
  Serial.begin(57600);

  pinMode(X_RATE_IN_PIN, INPUT);
  pinMode(Y_RATE_IN_PIN, INPUT);

  if(!OLED_LCD.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 Allocation Failed"));
    for(;;); // could not start LCD error condition, so loop forever
  }

  initSnake();
  OLED_LCD.display();

  // for food generation
  randomSeed(analogRead(32));
}


void loop() 
{
  //checkSerial();
  checkGyroInput();
  moveSnake();
  checkFood();
  OLED_LCD.display();
  delay(game_speed);
}
