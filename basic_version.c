#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#define NUM_SPRINKLES 10000
int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 450;
    InitWindow(screenWidth, screenHeight, "boaties, floaties, and cannons!");
    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 1.5f, 6.0f };
    camera.target = (Vector3){ 0.0f, 2.5f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    Vector2 waterSize = {400.0f, 400.0f};
    Vector3 sprinkles[NUM_SPRINKLES];
    for (int i = 0; i < NUM_SPRINKLES; i++) {
        sprinkles[i].x = (float)rand()/(float)(RAND_MAX/waterSize.x) - waterSize.x / 2;
        sprinkles[i].y = -0.95f;
        sprinkles[i].z = (float)rand()/(float)(RAND_MAX/waterSize.y) - waterSize.y / 2;
    }
    SetTargetFPS(60);
    Model boat = LoadModel("boat.obj");
    Vector3 boatPosition = { 0.0f, 0.0f, 0.0f };
    float boatScale = 0.07f;
    while (!WindowShouldClose()){
        Vector3 direction = Vector3Subtract(camera.target, camera.position);
        direction = Vector3Normalize(direction);
        Vector3 moveStep = Vector3Scale(direction, 0.1f);
        camera.position = Vector3Add(camera.position, moveStep);
        camera.target = Vector3Add(camera.target, moveStep);
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);
        boatPosition.x = camera.position.x;
        boatPosition.y = camera.position.y;
        boatPosition.z = camera.position.z-2.5f;
        BeginDrawing();
            ClearBackground(SKYBLUE);
            BeginMode3D(camera);
                DrawModel(boat, boatPosition, boatScale, BROWN);
                Vector3 waterPosition = {0.0f, -1.0f, 0.0f};
                DrawPlane(waterPosition, waterSize, BLUE);
                for (int i = 0; i < NUM_SPRINKLES; i++) {
                    DrawCube(sprinkles[i], 0.5f, 0.05f, 0.2f, DARKBLUE);
                }
            EndMode3D();
        EndDrawing();
    }
    UnloadModel(boat);
    CloseWindow();
    return 0;
}