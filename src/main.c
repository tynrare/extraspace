// @tynroar

#include "root.h"
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
//#include <GL/gl.h>
#include "rlgl.h"

#define RLIGHTS_IMPLEMENTATION
#include "external/rlights.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

bool active = false;

static const int VIEWPORT_W = 0x320;
static const int VIEWPORT_H = 0x1c2;
#define MAX_TOUCH_POINTS 10
Vector2 touchPositions[MAX_TOUCH_POINTS] = { 0 };

static int viewport_w = VIEWPORT_W;
static int viewport_h = VIEWPORT_H;

// ---

Vector3 Vector3RotateByMatrix(Vector3 v, Matrix m) {
	v = Vector3Transform(v, m);

	// extract position
	Vector3 pos = Vector3Transform(Vector3Zero(), m);
	v = Vector3Subtract(v, pos);

	return v;
}

Matrix MatrixJustRotate(Vector3 r, Matrix m) {
	Vector3 pos = Vector3Transform(Vector3Zero(), m);
	// 1. discard origin
	m = MatrixMultiply(m, MatrixTranslate(-pos.x, -pos.y, -pos.z));
	// 2. rotate
	m = MatrixMultiply(m, MatrixRotateXYZ(r));
	// 3. translate back
	m = MatrixMultiply(m, MatrixTranslate(pos.x, pos.y, pos.z));

	return m;
}

// ---
static void update_pawn();

// ---

bool _draw_help_enabled = false;
bool _pawn_acceleration_input_enabled = false;
bool _pawn_torque_input_enabled = false;
bool _pawn_torque_input_alt_axis_enabled = false;

// --- app
Camera3D camera = { 0 };
Model model = { 0 };
Model model_sphere = { 0 };
Shader shader = { 0 };
Vector3 torque = { 0 };
Vector3 velocity = { 0 };
Texture texture = { 0 };

static void update() {
	Vector3 cpos = { 0, 0.0f, 10.0f };
	Vector3 up = { 0, 0.1f, 0.0f };
	bool attach_camera = true;
	if (attach_camera) {
		up = Vector3RotateByMatrix((Vector3){ 0, 1.0f, 0.0f }, model.transform);
		camera.position = Vector3Transform(cpos, model.transform);
	} else {
		camera.position = cpos;
	}

	camera.target = Vector3Transform(Vector3Zero(), model.transform);
	camera.up = up;

	update_pawn();
}

static void draw_dbg_3d() {
	Vector3 ra = Vector3Transform((Vector3){ 1.0f, 0.0f, 0.0f }, model.transform);
	Vector3 rb = Vector3Transform(Vector3Zero(), model.transform);
	Vector3 ga = Vector3Transform((Vector3){ 0.0f, 1.0f, 0.0f }, model.transform);
	Vector3 gb = Vector3Transform(Vector3Zero(), model.transform);
	Vector3 ba = Vector3Transform((Vector3){ 0.0f, 0.0f, 2.0f }, model.transform);
	Vector3 bb = Vector3Transform(Vector3Zero(), model.transform);
	DrawLine3D(ra, rb, RED);
	DrawLine3D(ga, gb, GREEN);
	DrawLine3D(ba, bb, BLUE);
}

static void draw_help() {
	if (_draw_help_enabled) {
		DrawRectangleRec((Rectangle){16, 16, 420, 256}, Fade(BLACK, 0.3));
		DrawText("LMB: Accelerate.", 32, 32 + 22 * 0, 20, WHITE);
		DrawText("RMB: Change rotation axis.", 32, 32 + 22 * 1, 20, WHITE);
		DrawText("Space: toggle rotation.", 32, 32 + 22 * 2, 20, WHITE);
	} else {
		Color color = WHITE;
		if (_pawn_torque_input_enabled) {
			color = RED;
		}

		DrawText("SPACE.", 16, 16, 20, color);
		DrawText("'H': toggle help.", viewport_w - 16 - 170, viewport_h - 16 - 20, 20, WHITE);
	}
}

static bool sui_btn_a_pressed = false;

#define SUI_BTN_A (Rectangle){ 16, viewport_h - 16 - 64, 64, 64 }

static void draw_inputs() {
	DrawRectangleLinesEx(SUI_BTN_A, 16, sui_btn_a_pressed ? RED : WHITE);

	int tCount = GetTouchPointCount();
	if(tCount > MAX_TOUCH_POINTS) tCount = MAX_TOUCH_POINTS;
	for (int i = 0; i < tCount; ++i)  {
		Vector2 tp = touchPositions[i];
		if ((tp.x > 0) && (tp.y > 0)) {
			DrawCircle(tp.x, tp.y, 24, Fade(WHITE, 0.4));
			DrawCircle(tp.x, tp.y, 20, Fade(BLACK, 0.1));
			DrawText(TextFormat("%d", i), (int)touchPositions[i].x - 10, (int)touchPositions[i].y - 70, 40, BLACK);
		}
	}
}


static void draw() {
  ClearBackground(BLACK);

	SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], &camera.position.x, SHADER_UNIFORM_VEC3);

	BeginMode3D(camera);
		DrawModel(model, (Vector3){ 0, 0, 0}, 1.0f, WHITE);

		draw_dbg_3d();

		rlDisableBackfaceCulling();
		DrawModel(model_sphere, (Vector3){ 0, 0, 0}, 3.3f, WHITE);
		rlEnableBackfaceCulling();
	EndMode3D();

	draw_help();
	draw_inputs();
}

static void update_pawn() {
	// rotation
	model.transform = MatrixJustRotate(torque, model.transform);

	// acceleration
	Matrix mvel = MatrixTranslate(velocity.x, velocity.y, velocity.z);
	model.transform = MatrixMultiply(model.transform, mvel);

	// dumping down movement (not very spacish)
	torque = Vector3Lerp(torque, Vector3Zero(), 1e-2);
	velocity = Vector3Lerp(velocity, Vector3Zero(), 1e-2);
}

static void inputs_pawn_torque() {
	if (!_pawn_torque_input_enabled) {
		return;
	}

	Vector3 ltorque = { 0 };
	float factor = 1e-4;

	Vector2 ppos = GetMouseDelta();
	ltorque.y += ppos.x * factor;
	ltorque.x += ppos.y * factor;

	if (_pawn_torque_input_alt_axis_enabled) {
		ltorque.z = ltorque.y;
		ltorque.y = 0;
		// rotating on both xz axis wobbly. disabling it here
		ltorque.x = 0;
	}

  // do not apply this transform if you want to rotate in global coorditanes:
	ltorque = Vector3RotateByMatrix(ltorque, model.transform);

	torque = Vector3Add(torque, ltorque);
}

static void inputs_pawn_acceleration() {
	if (!_pawn_acceleration_input_enabled) {
		return;
	}
	
	float factor = 1e-2;
	Vector3 acceleration = { 0.0f, 0.0f, -factor };

	acceleration = Vector3RotateByMatrix(acceleration, model.transform);

	velocity = Vector3Add(velocity, acceleration);
}

static void inputs_pawn() {
	inputs_pawn_torque();
	inputs_pawn_acceleration();
}

static void inputs_sui_buttons() {
	int tCount = GetTouchPointCount();
	if(tCount > MAX_TOUCH_POINTS) tCount = MAX_TOUCH_POINTS;

	sui_btn_a_pressed = false;

	for (int i = 0; i < tCount; ++i)  {
		Vector2 tp = touchPositions[i];
		sui_btn_a_pressed = sui_btn_a_pressed || CheckCollisionPointRec(tp, SUI_BTN_A);
	}
}

static void inputs() {
	int tCount = GetTouchPointCount();
	// Clamp touch points available ( set the maximum touch points allowed )
	if(tCount > MAX_TOUCH_POINTS) tCount = MAX_TOUCH_POINTS;
	// Get touch points positions
	
	for (int i = 0; i < tCount; ++i) touchPositions[i] = GetTouchPosition(i);

	inputs_sui_buttons();

	if (IsKeyPressed(KEY_H)) {
		_draw_help_enabled = !_draw_help_enabled;
	}

	_pawn_torque_input_enabled = IsKeyDown(KEY_SPACE) || sui_btn_a_pressed;
	_pawn_acceleration_input_enabled = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
	_pawn_torque_input_alt_axis_enabled = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

	if (_pawn_torque_input_enabled && !IsCursorHidden()) {
		HideCursor();
		DisableCursor();
	} else if (!_pawn_torque_input_enabled && IsCursorHidden()){
		ShowCursor();
		EnableCursor();
	}

	inputs_pawn();
}

// --- flow

static void dispose() {
	UnloadModel(model);
	UnloadModel(model_sphere);
	UnloadShader(shader);
	UnloadTexture(texture);
}

static void init() { 
	dispose(); 

	shader = LoadShader(RES_PATH "lighting.vs", RES_PATH "lighting.fs");
	// Get some required shader locations
	shader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(shader, "viewPos");
	int ambientLoc = GetShaderLocation(shader, "ambient");
	SetShaderValue(shader, ambientLoc, (float[4]){ 0.1f, 0.1f, 0.1f, 1.0f }, SHADER_UNIFORM_VEC4);
	CreateLight(LIGHT_DIRECTIONAL, (Vector3){ -2, 1, -2 }, Vector3Zero(), WHITE, shader);

	texture = LoadTexture(RES_PATH "tex1.png");

	model = LoadModelFromMesh(GenMeshCube(1, 1, 2));
	model.materials[0].shader = shader;
	model_sphere = LoadModelFromMesh(GenMeshSphere(10, 16, 16));
	//model_sphere.materials[0].shader = shader;
	model_sphere.materials[0].maps->texture = texture;

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

  BeginDrawing();
  draw();
  EndDrawing();

  inputs();
	update();
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
