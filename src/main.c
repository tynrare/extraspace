// @tynroar

#include "root.h"
#include <raylib.h>
#include <stdio.h>

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

bool active = false;

static const int VIEWPORT_W = 0x320;
static const int VIEWPORT_H = 0x1c2;

static int viewport_w = VIEWPORT_W;
static int viewport_h = VIEWPORT_H;

static void draw() {
  ClearBackground(BLACK);

  DrawText("tynbox", 16, 16, 20, WHITE);
  DrawText("tynbox", viewport_w - 16 - 72, viewport_h - 16 - 20, 20, WHITE);
}

static void dispose() {}

static void init() { dispose(); }

static void inputs() {}

static long resize_timestamp = -1;
static const float resize_threshold = 0.3;
static Vector2 requested_viewport = {VIEWPORT_W, VIEWPORT_H};
static void equilizer() {
  const int vw = GetScreenWidth();
  const int vh = GetScreenHeight();

  const long now = GetTime();

  // thresholds resizing
  if (requested_viewport.x != vw || requested_viewport.y != vh) {
    requested_viewport.x = vw;
    requested_viewport.y = vh;

    // first resize triggers intantly (important in web build)
    if (resize_timestamp > 0) {
      resize_timestamp = now;
      return;
    }
  }

  // reinits after riseze stops
  const bool resized =
      requested_viewport.x != viewport_w || requested_viewport.y != viewport_h;
  if (resized && now - resize_timestamp > resize_threshold) {
    resize_timestamp = now;
    viewport_w = vw;
    viewport_h = vh;
    // init();
  }
}

void step(void) {
  equilizer();

  inputs();

  BeginDrawing();
  draw();
  EndDrawing();
}

void loop() {
#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(step, 0, 1);
#else

  while (!WindowShouldClose()) {
    step();
  }
#endif
}

int main(void) {
  const int seed = 2;

  InitWindow(viewport_w, viewport_h, "extraspace");
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  SetTargetFPS(60);
  SetRandomSeed(seed);

  init();

  active = true;
  loop();

  dispose();
  CloseWindow();

  return 0;
}
