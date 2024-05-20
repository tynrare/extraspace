// @tynroar

#include "root.h"
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>

#define RLIGHTS_IMPLEMENTATION
#include "external/rlights.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

bool active = false;

static const int VIEWPORT_W = 0x320;
static const int VIEWPORT_H = 0x1c2;

static int viewport_w = VIEWPORT_W;
static int viewport_h = VIEWPORT_H;

// --- app
Camera camera = { 0 };
Model model = { 0 };
Shader shader = { 0 };
Vector3 torque = { 0 };

static void update() {
}

static void draw() {
  ClearBackground(BLACK);

	SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], &camera.position.x, SHADER_UNIFORM_VEC3);

	BeginMode3D(camera);
		DrawModel(model, (Vector3){ 0, 0, 0}, 1.0f, WHITE);
	EndMode3D();

	Color color = WHITE;
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
		color = RED;
	}
  DrawText("Left Mouse Button.", 16, 16, 20, color);
  DrawText("tynbox", viewport_w - 16 - 72, viewport_h - 16 - 20, 20, color);
}

static void inputs() {
	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
		float factor = 1e-4;
		Vector2 ppos = GetMouseDelta();
		// conversion from 2d xz flipped
		torque.y += ppos.x * factor;
		torque.x += ppos.y * factor;
	}

  // do not apply this transform if you want to rotate in gloval coorditanes
	//Vector3 t =  Vector3Transform(torque, model.transform);
	Vector3 t = torque;

	Matrix mx = MatrixRotateX(t.x);
	Matrix my = MatrixRotateY(t.y);
	Matrix m1 = MatrixMultiply(mx, my);
	model.transform = MatrixMultiply(model.transform, m1);
}

// --- flow

static void dispose() {
	UnloadModel(model);
	UnloadShader(shader);
}

static void init() { 
	dispose(); 

	shader = LoadShader(RES_PATH "lighting.vs", RES_PATH "lighting.fs");
	// Get some required shader locations
	shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
	int ambientLoc = GetShaderLocation(shader, "ambient");
	SetShaderValue(shader, ambientLoc, (float[4]){ 0.1f, 0.1f, 0.1f, 1.0f }, SHADER_UNIFORM_VEC4);
	CreateLight(LIGHT_DIRECTIONAL, (Vector3){ -2, 1, -2 }, Vector3Zero(), WHITE, shader);

	model = LoadModelFromMesh(GenMeshCube(1, 1, 2));
	model.materials[0].shader = shader;

	camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };
	camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
	camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;

}

// --- system

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
	update();

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
