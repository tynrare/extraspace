// @tynroar

#include "root.h"
#include "dust.h"
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <math.h>
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

TouchPoint touch_points[MAX_TOUCH_POINTS] = { 0 };

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

float PLAYSPACE_SIZE = 101.0f;

bool _draw_help_enabled = false;
bool _pawn_acceleration_input_enabled = false;
bool _pawn_torque_input_enabled = false;
bool _pawn_torque_input_alt_axis_enabled = false;

Vector2 input_delta = { 0 };
Vector2 input_pos = { 0 };

// --- app
Camera3D camera = { 0 };
Model model = { 0 };
Model model_sphere = { 0 };
Shader shader = { 0 };
Vector3 torque = { 0 };
Vector3 velocity = { 0 };
Texture texture = { 0 };

// --- res
Texture pic_forwards = { 0 };
Texture pic_rotate = { 0 };

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
		//DrawText(TextFormat("%d:%d", (int)input_delta.x, (int)input_delta.y), 16, 36, 20, RED);
		//DrawText("'H': toggle help.", viewport_w - 16 - 170, viewport_h - 16 - 20, 20, WHITE);
	}
}

static bool sui_btn_a_pressed = false;
static bool sui_btn_b_pressed = false;
static bool sui_screen_pressed = false;

#define SUI_BTN_A (Rectangle){ 16, viewport_h - 16 - 64, 64, 64 }
#define SUI_BTN_B (Rectangle){ 16, viewport_h - 16 - 64 * 2 - 8 * 1, 64, 64 }

static void draw_inputs() {
	Rectangle sba = SUI_BTN_A;
	Rectangle sbb = SUI_BTN_B;

	Color sba_color = sui_btn_a_pressed ? RED : WHITE;
	Color sbb_color = sui_btn_b_pressed ? RED : WHITE;

	DrawRectangleLinesEx(sba, 4, sba_color);
	DrawRectangleLinesEx(sbb, 4, sbb_color);

	DrawTexturePro(pic_forwards,
			(Rectangle){0, 0, pic_forwards.width, pic_forwards.height}, sba,
			Vector2Zero(), 0.0, sba_color);
	DrawText("LMB.", sba.x + sba.width - 8 + 2, sba.y + sba.height - 16 + 2, 20, BLACK);
	DrawText("LMB.", sba.x + sba.width - 8, sba.y + sba.height - 16, 20, sba_color);

	DrawTexturePro(pic_rotate,
			(Rectangle){0, 0, pic_rotate.width, pic_rotate.height}, sbb,
			Vector2Zero(), 0.0, sbb_color);
	DrawText("RMB.", sbb.x + sbb.width - 8 + 2, sbb.y + sbb.height - 16 + 2, 20, BLACK);
	DrawText("RMB.", sbb.x + sbb.width - 8, sbb.y + sbb.height - 16, 20, sbb_color);

	/*
	for (int i = 0; i < MAX_TOUCH_POINTS; ++i)  {
		TouchPoint tp = touch_points[i];
		if (!tp.active) {
			break;
		}

		DrawCircle(tp.pos.x, tp.pos.y, 24, Fade(WHITE, 0.4));
		DrawCircle(tp.pos.x, tp.pos.y, 20, Fade(BLACK, 0.1));
		DrawText(TextFormat("%d:%d", i, tp.id), (int)tp.pos.x - 10, (int)tp.pos.y - 70, 40, RED);
	}
	*/
}


static void draw() {
  ClearBackground(BLACK);

	SetShaderValue(shader, shader.locs[SHADER_LOC_VECTOR_VIEW], &camera.position.x, SHADER_UNIFORM_VEC3);

	BeginMode3D(camera);
		DrawModel(model, (Vector3){ 0, 0, 0}, 1.0f, WHITE);

		draw_dbg_3d();

		rlDisableBackfaceCulling();
		DrawModel(model_sphere, (Vector3){ 0, 0, 0 }, 1.0f, WHITE);
		rlEnableBackfaceCulling();
	EndMode3D();

	draw_help();
	draw_inputs();
}

static RayCollision get_pawn_playspace_bounds_collision() {
	Ray ray = { 0 };
	Vector3 model_pos = Vector3Transform(Vector3Zero(), model.transform);
	if (!Vector3Length(model_pos)) {
		model_pos.z = 1;
	}
	Vector3 ray_pos = Vector3Scale(Vector3Normalize(model_pos), PLAYSPACE_SIZE);
	ray.position = ray_pos;
	ray.direction = Vector3Negate(ray_pos);
	RayCollision collision = GetRayCollisionMesh(ray, model.meshes[0], model.transform);

	return collision;
}

static void update_pawn() {
	// rotation
	model.transform = MatrixJustRotate(torque, model.transform);

	RayCollision bounds_collision = get_pawn_playspace_bounds_collision();

	if (bounds_collision.distance * PLAYSPACE_SIZE <= 20) {
		Vector3 model_pos = Vector3Transform(Vector3Zero(), model.transform);
		float speed = fmaxf(1e-2, Vector3LengthSqr(velocity) * 1e-1);
		velocity = Vector3Scale(Vector3Normalize(Vector3Negate(model_pos)), speed);
	}

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

	Vector2 ppos = input_delta;
	ppos.x = fminf(ppos.x, 77);
	ppos.y = fminf(ppos.y, 77);

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
	// need this flag to prevent wrong delta
	bool had_screen_press = sui_screen_pressed;

	sui_btn_a_pressed = false;
	sui_btn_b_pressed = false;
	sui_screen_pressed = false;

	for (int i = 0; i < MAX_TOUCH_POINTS; ++i)  {
		TouchPoint tp = touch_points[i];
		if (!tp.active) {
			break;
		}

		bool collide_a = CheckCollisionPointRec(tp.pos, SUI_BTN_A);
		bool collide_b = CheckCollisionPointRec(tp.pos, SUI_BTN_B);

		sui_btn_a_pressed = sui_btn_a_pressed || collide_a;
		sui_btn_b_pressed = sui_btn_b_pressed || collide_b;
		if (!sui_screen_pressed && !(collide_a || collide_b)) {
			sui_screen_pressed = true;
			if (had_screen_press) {
				input_delta = Vector2Subtract(input_pos, tp.pos);
			}
			input_pos = tp.pos;
		}
	}
}

static void inputs() {
	int tCount = GetTouchPointCount();
	for (int i = 0; i < MAX_TOUCH_POINTS; ++i) { 
		bool active = i < tCount;
		touch_points[i].active = active;

		if (!active) {
			continue;
		}

		touch_points[i].pos = GetTouchPosition(i);
		touch_points[i].id = GetTouchPointId(i);
	}

	inputs_sui_buttons();

	if (IsKeyPressed(KEY_H)) {
		_draw_help_enabled = !_draw_help_enabled;
	}
	
	bool mode_mouse = !tCount;

	sui_screen_pressed = sui_screen_pressed || IsKeyDown(KEY_SPACE);
	sui_btn_a_pressed = sui_btn_a_pressed || (mode_mouse && IsMouseButtonDown(MOUSE_BUTTON_LEFT));
	sui_btn_b_pressed = sui_btn_b_pressed || (mode_mouse && IsMouseButtonDown(MOUSE_BUTTON_RIGHT));

	_pawn_torque_input_enabled = sui_screen_pressed;
	_pawn_acceleration_input_enabled = sui_btn_a_pressed;
	_pawn_torque_input_alt_axis_enabled = sui_btn_b_pressed;

	if (_pawn_torque_input_enabled && !IsCursorHidden()) {
		HideCursor();
		DisableCursor();
	} else if (!_pawn_torque_input_enabled && IsCursorHidden()){
		ShowCursor();
		EnableCursor();
	}

	if (mode_mouse) {
		//input_delta = GetMouseDelta(); // does not work in web
		Vector2 p = GetMousePosition();
		input_delta = Vector2Subtract(p, input_pos);
		input_pos = p;
	} else {
		// updated in inputs_sui_buttons
	}

	inputs_pawn();
}

// --- flow

static void dispose() {
	UnloadModel(model);
	UnloadModel(model_sphere);
	UnloadShader(shader);
	UnloadTexture(texture);
	UnloadTexture(pic_forwards);
	UnloadTexture(pic_rotate);
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
	pic_forwards = LoadTexture(RES_PATH "pic/forwards.png");
	pic_rotate = LoadTexture(RES_PATH "pic/rotate.png");

	model = LoadModelFromMesh(GenMeshCube(1, 1, 2));
	model.materials[0].shader = shader;
	model_sphere = LoadModelFromMesh(GenMeshSphere(PLAYSPACE_SIZE, 16, 16));
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
