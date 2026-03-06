#ifndef TREX_GAME_H
#define TREX_GAME_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>



#define PLAYER_SAFE_ZONE_WIDTH 32
#define CACTI_RESPAWN_RATE 50
#define GROUND_CACTI_SCROLL_SPEED 3
#define PTERODACTY_SPEED 5
#define PTERODACTY_RESPAWN_RATE 255
#define INCREASE_FPS_EVERY_N_SCORE_POINTS 256
#define LIVES_START 3
#define LIVES_MAX 5
#define SPAWN_NEW_LIVE_MIN_CYCLES 800
#define DAY_NIGHT_SWITCH_CYCLES 1024
#define TARGET_FPS_START 23
#define TARGET_FPS_MAX 48

#ifndef LCD_HEIGHT
#define LCD_HEIGHT 64U
#define LCD_WIDTH 128U
#endif

#define LCD_PART_BUFF_WIDTH LCD_WIDTH
#define LCD_PART_BUFF_HEIGHT LCD_HEIGHT
#define LCD_PART_BUFF_SZ ((LCD_PART_BUFF_HEIGHT/8)*LCD_PART_BUFF_WIDTH)

#define LCD_IF_VIRTUAL_WIDTH(TRUE_COND, FALSE_COND) FALSE_COND

#include "trex/array.h"
#include "trex/TrexPlayer.h"
#include "trex/Ground.h"
#include "trex/Cactus.h"
#include "trex/Pterodactyl.h"
#include "trex/HeartLive.h"

static uint16_t trexHiScore = 0;

inline bool isPressedJump() {
  return digitalRead(35) == LOW; // BTN1 (pin35) for jumping dino and start/restarting
}

inline bool isPressedDuck() {
  return false; 
}

inline bool isPressedScreenChange() {
  return digitalRead(34) == LOW; // Button 2(pin 34) for screen change
}

void renderNumber(BitCanvas& canvas, Point2Di8 point, const uint16_t number) {
  uint16_t base = 10000;
  while(base) {
    const uint8_t digit = (number/base)%10;
    canvas.render(numbers.getSprite(digit, point));
    base /= 10;
    point.x += numbers.getWidth() + 1;
  }
}

void runTrexGameLoop() {
  VirtualBitCanvas bitCanvas(
    VirtualBitCanvas::VIRTUAL_HEIGHT, 
    display.getBuffer(), 
    LCD_PART_BUFF_HEIGHT,
    LCD_PART_BUFF_WIDTH,
    LCD_HEIGHT
  );

  SpawnHold spawnHolder;

  TrexPlayer trex;
  Ground ground1(-1);
  Ground ground2(63);
  Ground ground3(127);
  Cactus cactus1(spawnHolder);
  Cactus cactus2(spawnHolder);
  Pterodactyl pterodactyl1(spawnHolder);
  HeartLive heartLive;

  const array<SpriteAnimated*, 8> sprites{{&ground1, &ground2, &ground3, &cactus1, &cactus2, &pterodactyl1, &heartLive, &trex}};
  const array<SpriteAnimated*, 3> enemies{{&cactus1, &cactus2, &pterodactyl1}};

  const Sprite gameOverSprite(&game_overver_bm, {15, 12});
  const Sprite restartIconSprite(&restart_icon_bm, {55, 25});
  const Sprite hiSprite(&hi_score, {44, 0});
  Sprite heartsSprite(&hearts_5x_bm, {95, 8});

  uint32_t prvT = millis();
  bool gameOver = false;
  uint16_t score = 0;
  uint8_t targetFPS = TARGET_FPS_START;
  uint8_t lives = LIVES_START;
  bool night = false;

  display.invertDisplay(false);

  while(1) {
    if (isPressedScreenChange()) {
        while(isPressedScreenChange()) delay(10);
        delay(50);
        display.invertDisplay(false);
        // Switch to next mode on exit
        currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
        return;
    }

    bitCanvas.clear();
    bitCanvas.render(hiSprite);
    renderNumber(bitCanvas, {60, 0}, trexHiScore);
    renderNumber(bitCanvas, {95, 0}, score);
    bitCanvas.render(heartsSprite);
    
    for(uint8_t i = 0; i < sprites.size(); ++i) {
      bitCanvas.render(*sprites[i]);
    }
    
    if(gameOver) {
      bitCanvas.render(gameOverSprite);
      bitCanvas.render(restartIconSprite);
    }
    
    display.display();

    if(gameOver) {
      if(score > trexHiScore) trexHiScore = score;
      
      while(isPressedJump()) {
          if (isPressedScreenChange()) break;
          delay(10);
      }
      
      while(1) {
          if (isPressedJump()) {
              display.invertDisplay(false);
              return runTrexGameLoop();
          }
          if (isPressedScreenChange()) break;
          delay(10);
      }
      
      while(isPressedScreenChange()) delay(10);
      delay(50);
      display.invertDisplay(false);
      currentMode = (DisplayMode)((currentMode + 1) % MODE_COUNT);
      return;
    }

    if(!trex.isBlinking() && CollisionDetector::check(trex, enemies.data, enemies.size())) {
      if(lives) {
        trex.blink();
        --lives;
      } else {
        trex.die();
        gameOver = true;
        continue;
      }
    }
    if(lives < LIVES_MAX && CollisionDetector::check(trex, heartLive)) {
      ++lives;
      heartLive.eat();
    }

    if(isPressedJump()) {
       trex.jump();
    }
    trex.duck(isPressedDuck());

    for(uint8_t i = 0; i < sprites.size(); ++i) {
      sprites[i]->step();
    }
    
    if(score < 0xFFFE) ++score;
    if(!(score%INCREASE_FPS_EVERY_N_SCORE_POINTS) && targetFPS < TARGET_FPS_MAX) ++targetFPS;
    heartsSprite.limitRenderWidthTo = 6*lives + 1;
    
    if(!(score%DAY_NIGHT_SWITCH_CYCLES)) {
        night = !night;
        display.invertDisplay(night);
    }

    const uint8_t frameTime = 1000/targetFPS;

    while(millis() - prvT < frameTime) {
        if (isPressedScreenChange()) break;
        delay(1);
    }
    prvT = millis();
  } 
}

void drawTrexScreen() {
    display.clearDisplay();
    display.setTextSize(1);
    
    const char* title = "T-REX GAME";
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds(title, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((128 - bw) / 2, 16);
    display.print(title);

    const char* startTxt = "Press Button1 to Start";
    display.getTextBounds(startTxt, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((128 - bw) / 2, 36);
    display.print(startTxt);
    
    const char* exitTxt = "Press Button2 to Exit";
    display.getTextBounds(exitTxt, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((128 - bw) / 2, 48);
    display.print(exitTxt);

    display.display();

    // Check if we start the game
    if (isPressedJump()) {
        // Wait until button is released
        while(isPressedJump()) delay(10);
        delay(50);
        runTrexGameLoop();
    }
}

#endif
