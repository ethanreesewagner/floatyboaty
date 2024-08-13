#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <time.h>

#define NUM_SPRINKLES 10000
#define MAX_CLIENTS 10
#define MAX_CANNONBALLS 50
#define MAX_HEALTH 100
#define CANNONBALL_DAMAGE 20
#define CANNONBALL_SPEED 0.2f

typedef struct {
    float x, y, z;
} BoatPosition;

typedef struct {
    Vector3 position;
    Vector3 direction;
    bool active;
} Cannonball;

typedef struct {
    BoatPosition position;
    int health;
    bool active;
    Cannonball cannonballs[MAX_CANNONBALLS];
} Player;

typedef struct {
    pthread_mutex_t mutex;
    bool serverRunning;
    int serverPort;
    char serverIp[INET_ADDRSTRLEN];
    Player players[MAX_CLIENTS];
} ServerData;

// Global array for sprinkles
Vector3 sprinkles[NUM_SPRINKLES];

void *ServerMode(void *args);
void ClientMode(const char *ip_address, int port);
void DrawMainMenu(bool *isHosting, bool *isJoining, char *ipAddressBuffer, char *portBuffer, ServerData *serverData, int *focusedInput);
char *GetLocalIPAddress();
void RunGame(Player *player, ServerData *serverData, bool isHost, int clientId);

int main(void) {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "boaties, floaties, and cannons!");

    // Initialize sprinkles array
    Vector2 waterSize = {400.0f, 400.0f};
    for (int i = 0; i < NUM_SPRINKLES; i++) {
        sprinkles[i].x = (float)rand() / (float)(RAND_MAX / waterSize.x) - waterSize.x / 2;
        sprinkles[i].y = -0.95f;
        sprinkles[i].z = (float)rand() / (float)(RAND_MAX / waterSize.y) - waterSize.y / 2;
    }

    bool isHosting = false, isJoining = false;
    char ipAddressBuffer[64] = {0}, portBuffer[6] = {0};
    int focusedInput = 0;  // 0 = IP input, 1 = Port input
    ServerData serverData;
    pthread_mutex_init(&serverData.mutex, NULL);
    serverData.serverRunning = false;
    serverData.serverPort = 0;

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (!isHosting && !isJoining) {
            // Display the main menu
            DrawMainMenu(&isHosting, &isJoining, ipAddressBuffer, portBuffer, &serverData, &focusedInput);
        } else if (isHosting) {
            // Start hosting the game
            char *localIp = GetLocalIPAddress();
            if (localIp) {
                strncpy(serverData.serverIp, localIp, INET_ADDRSTRLEN);
            }

            pthread_mutex_lock(&serverData.mutex);
            serverData.serverRunning = true;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                serverData.players[i].active = false;
                serverData.players[i].health = MAX_HEALTH;
                for (int j = 0; j < MAX_CANNONBALLS; j++) {
                    serverData.players[i].cannonballs[j].active = false;
                }
            }
            pthread_mutex_unlock(&serverData.mutex);

            pthread_t server_thread;
            pthread_create(&server_thread, NULL, ServerMode, &serverData);
            isHosting = false;

            // Run the game for the host
            serverData.players[0].active = true;
            RunGame(&serverData.players[0], &serverData, true, 0);
        } else if (isJoining) {
            if (strlen(portBuffer) > 0) {
                ClientMode(ipAddressBuffer, atoi(portBuffer));

                // Run the game for the client
                RunGame(&serverData.players[1], &serverData, false, 1);
            } else {
                printf("Error: Please enter a valid port number.\n");
            }
            isJoining = false;
        }

        EndDrawing();
    }

    pthread_mutex_lock(&serverData.mutex);
    serverData.serverRunning = false;
    pthread_mutex_unlock(&serverData.mutex);

    CloseWindow();

    return 0;
}

void RunGame(Player *player, ServerData *serverData, bool isHost, int clientId) {
    Vector2 waterSize = {400.0f, 400.0f};
    Model boat = LoadModel("boat.obj");
    float boatScale = 0.07f;

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 1.5f, 6.0f };
    camera.target = (Vector3){ 0.0f, 2.5f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        // If the window should close, break out of the loop
        if (WindowShouldClose()) break;

        // Update camera and boat position
        Vector3 direction = Vector3Subtract(camera.target, camera.position);
        direction = Vector3Normalize(direction);
        Vector3 moveStep = Vector3Scale(direction, 0.1f);
        camera.position = Vector3Add(camera.position, moveStep);
        camera.target = Vector3Add(camera.target, moveStep);
        UpdateCamera(&camera, CAMERA_FIRST_PERSON);

        player->position.x = camera.position.x;
        player->position.y = camera.position.y;
        player->position.z = camera.position.z - 2.5f;

        if (IsKeyPressed(KEY_SPACE)) {
            for (int i = 0; i < MAX_CANNONBALLS; i++) {
                if (!player->cannonballs[i].active) {
                    player->cannonballs[i].position = (Vector3){player->position.x, player->position.y, player->position.z};
                    player->cannonballs[i].direction = direction;
                    player->cannonballs[i].active = true;
                    break;
                }
            }
        }

        // Update cannonballs
        for (int i = 0; i < MAX_CANNONBALLS; i++) {
            if (player->cannonballs[i].active) {
                player->cannonballs[i].position = Vector3Add(player->cannonballs[i].position,
                                                             Vector3Scale(player->cannonballs[i].direction, CANNONBALL_SPEED));
                if (player->cannonballs[i].position.z < -waterSize.y / 2 ||
                    player->cannonballs[i].position.z > waterSize.y / 2 ||
                    player->cannonballs[i].position.x < -waterSize.x / 2 ||
                    player->cannonballs[i].position.x > waterSize.x / 2) {
                    player->cannonballs[i].active = false;
                }
            }
        }

        // Check for hits
        if (isHost) {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (serverData->players[i].active && i != clientId) {
                    for (int j = 0; j < MAX_CANNONBALLS; j++) {
                        if (player->cannonballs[j].active &&
                            Vector3Distance((Vector3){serverData->players[i].position.x, serverData->players[i].position.y, serverData->players[i].position.z},
                                            player->cannonballs[j].position) < 1.0f) {
                            serverData->players[i].health -= CANNONBALL_DAMAGE;
                            player->cannonballs[j].active = false;
                            if (serverData->players[i].health <= 0) {
                                serverData->players[i].health = 0;
                                serverData->players[i].active = false;
                            }
                        }
                    }
                }
            }
        }

        // Regenerate health
        if (player->health < MAX_HEALTH) {
            player->health += 1;
        }

        BeginDrawing();
        ClearBackground(SKYBLUE);
        BeginMode3D(camera);
        DrawModel(boat, (Vector3){player->position.x, player->position.y, player->position.z}, boatScale, BROWN);
        Vector3 waterPosition = {0.0f, -1.0f, 0.0f};
        DrawPlane(waterPosition, waterSize, BLUE);

        for (int i = 0; i < NUM_SPRINKLES; i++) {
            DrawCube(sprinkles[i], 0.5f, 0.05f, 0.2f, DARKBLUE);
        }

        // Draw cannonballs
        for (int i = 0; i < MAX_CANNONBALLS; i++) {
            if (player->cannonballs[i].active) {
                DrawSphere(player->cannonballs[i].position, 0.2f, BLACK);
            }
        }

        // Draw other players
        if (isHost) {
            for (int i = 1; i < MAX_CLIENTS; i++) {
                if (serverData->players[i].active) {
                    DrawModel(boat, (Vector3){serverData->players[i].position.x, serverData->players[i].position.y, serverData->players[i].position.z}, boatScale, DARKGRAY);
                    DrawRectangle((int)(serverData->players[i].position.x - 0.5f), (int)(serverData->players[i].position.z - 2.5f),
                                  (int)(MAX_HEALTH * 0.1f), 5, RED);
                    DrawRectangle((int)(serverData->players[i].position.x - 0.5f), (int)(serverData->players[i].position.z - 2.5f),
                                  (int)(serverData->players[i].health * 0.1f), 5, GREEN);
                }
            }
        }
        EndMode3D();

        // Draw health bar for the player
        DrawRectangle(10, GetScreenHeight() - 40, MAX_HEALTH, 20, RED);
        DrawRectangle(10, GetScreenHeight() - 40, player->health, 20, GREEN);
        DrawText("Health", 10, GetScreenHeight() - 60, 20, DARKGRAY);

        EndDrawing();
    }

    UnloadModel(boat);
}

void DrawMainMenu(bool *isHosting, bool *isJoining, char *ipAddressBuffer, char *portBuffer, ServerData *serverData, int *focusedInput) {
    int screenWidth = GetScreenWidth(), screenHeight = GetScreenHeight();

    DrawText("boaties, floaties, and cannons!", screenWidth / 2 - MeasureText("boaties, floaties, and cannons!", 40) / 2, screenHeight / 4 - 40, 40, DARKBLUE);

    Rectangle hostButton = {screenWidth / 2 - 100, screenHeight / 2 - 90, 200, 50};
    if (CheckCollisionPointRec(GetMousePosition(), hostButton)) {
        DrawRectangleRec(hostButton, LIGHTGRAY);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            *isHosting = true;
        }
    } else {
        DrawRectangleRec(hostButton, GRAY);
    }
    DrawText("Host Game", screenWidth / 2 - MeasureText("Host Game", 20) / 2, screenHeight / 2 - 75, 20, DARKGRAY);

    // Display the server IP and port after hosting
    pthread_mutex_lock(&serverData->mutex);
    if (serverData->serverIp[0] && serverData->serverPort > 0) {
        char serverAddress[64];
        snprintf(serverAddress, sizeof(serverAddress), "Server: %s:%d", serverData->serverIp, serverData->serverPort);
        DrawText(serverAddress, screenWidth / 2 - MeasureText(serverAddress, 20) / 2, screenHeight / 2 - 20, 20, DARKGRAY);
    }
    pthread_mutex_unlock(&serverData->mutex);

    // IP Address Input
    DrawText("Enter IP:", screenWidth / 2 - 150, screenHeight / 2 + 10, 20, DARKGRAY);
    Rectangle ipBox = {screenWidth / 2 - 60, screenHeight / 2 + 35, 200, 30};
    DrawRectangleRec(ipBox, (*focusedInput == 0) ? LIGHTGRAY : GRAY);
    DrawText(ipAddressBuffer, screenWidth / 2 - 55, screenHeight / 2 + 40, 20, DARKGRAY);

    if (CheckCollisionPointRec(GetMousePosition(), ipBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *focusedInput = 0;
    }

    // Port Number Input
    DrawText("Enter Port:", screenWidth / 2 - 150, screenHeight / 2 + 75, 20, DARKGRAY);
    Rectangle portBox = {screenWidth / 2 - 60, screenHeight / 2 + 100, 200, 30};
    DrawRectangleRec(portBox, (*focusedInput == 1) ? LIGHTGRAY : GRAY);
    DrawText(portBuffer, screenWidth / 2 - 55, screenHeight / 2 + 105, 20, DARKGRAY);

    if (CheckCollisionPointRec(GetMousePosition(), portBox) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        *focusedInput = 1;
    }

    int key = GetCharPressed();
    if (*focusedInput == 0) {
        // Handle IP Address Input
        if (IsKeyPressed(KEY_BACKSPACE) && strlen(ipAddressBuffer)) {
            ipAddressBuffer[strlen(ipAddressBuffer) - 1] = '\0';
        }
        while (key > 0 && key != ':') {
            if ((key >= 32) && (key <= 125) && strlen(ipAddressBuffer) < 63) {
                ipAddressBuffer[strlen(ipAddressBuffer)] = (char)key;
                ipAddressBuffer[strlen(ipAddressBuffer) + 1] = '\0';
            }
            key = GetCharPressed();
        }
    } else if (*focusedInput == 1) {
        // Handle Port Input
        if (IsKeyPressed(KEY_BACKSPACE) && strlen(portBuffer)) {
            portBuffer[strlen(portBuffer) - 1] = '\0';
        }
        while (key > 0 && (key >= '0' && key <= '9')) {
            if (strlen(portBuffer) < 5) {
                portBuffer[strlen(portBuffer)] = (char)key;
                portBuffer[strlen(portBuffer) + 1] = '\0';
            }
            key = GetCharPressed();
        }
    }

    Rectangle joinButton = {screenWidth / 2 - 100, screenHeight / 2 + 150, 200, 50};
    if (CheckCollisionPointRec(GetMousePosition(), joinButton)) {
        DrawRectangleRec(joinButton, LIGHTGRAY);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) *isJoining = true;
    } else {
        DrawRectangleRec(joinButton, GRAY);
    }
    DrawText("Join Game", screenWidth / 2 - MeasureText("Join Game", 20) / 2, screenHeight / 2 + 165, 20, DARKGRAY);
}

char *GetLocalIPAddress() {
    static char ip[INET_ADDRSTRLEN];
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr) == -1) return NULL;
    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            if (sa->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                inet_ntop(AF_INET, &(sa->sin_addr), ip, INET_ADDRSTRLEN);
                break;
            }
        }
    }
    freeifaddrs(ifaddr);
    return ip;
}

void *ServerMode(void *args) {
    ServerData *serverData = (ServerData *)args;
    int server_fd, new_socket, client_sockets[MAX_CLIENTS] = {0};
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY};
    int addrlen = sizeof(address);

    srand(time(NULL));
    pthread_mutex_lock(&serverData->mutex);
    serverData->serverPort = rand() % (65535 - 1024) + 1024;
    int serverPort = serverData->serverPort;
    pthread_mutex_unlock(&serverData->mutex);

    address.sin_port = htons(serverPort);
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0 || bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0 || listen(server_fd, 3) < 0) {
        pthread_mutex_lock(&serverData->mutex);
        serverData->serverRunning = false;
        pthread_mutex_unlock(&serverData->mutex);
        return NULL;
    }

    while (1) {
        pthread_mutex_lock(&serverData->mutex);
        if (!serverData->serverRunning) {
            pthread_mutex_unlock(&serverData->mutex);
            break;
        }
        pthread_mutex_unlock(&serverData->mutex);

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        if (select(max_sd + 1, &readfds, NULL, NULL, NULL) < 0) continue;
        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) continue;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    serverData->players[i].active = true;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = client_sockets[i];
            if (FD_ISSET(sd, &readfds)) {
                BoatPosition boatPos;
                int valread = read(sd, &boatPos, sizeof(BoatPosition));
                if (valread == 0) {
                    close(sd);
                    client_sockets[i] = 0;
                    serverData->players[i].active = false;
                } else {
                    pthread_mutex_lock(&serverData->mutex);
                    serverData->players[i].position = boatPos;
                    pthread_mutex_unlock(&serverData->mutex);
                }
            }
        }
    }

    close(server_fd);
    return NULL;
}

void ClientMode(const char *ip_address, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr = {.sin_family = AF_INET, .sin_port = htons(port)};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0 || inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0 || connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Error: Connection to server failed.\n");
        return;
    }

    Player player = {0};
    player.health = MAX_HEALTH;
    fd_set readfds;

    while (!WindowShouldClose()) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};

        if (select(sock + 1, &readfds, NULL, NULL, &timeout) < 0) {
            printf("Error: Select failed.\n");
            break;
        }

        if (FD_ISSET(sock, &readfds)) {
            BoatPosition receivedPos;
            int valread = recv(sock, &receivedPos, sizeof(BoatPosition), 0);
            if (valread <= 0) {
                printf("Error: Failed to receive data from server or server closed connection.\n");
                break;
            }

            player.position = receivedPos;
        }

        send(sock, &player.position, sizeof(BoatPosition), 0);

        // Simulate boat movement
        Vector3 direction = Vector3Subtract((Vector3){player.position.x, player.position.y, player.position.z}, (Vector3){0, 0, 0});
        direction = Vector3Normalize(direction);
        Vector3 moveStep = Vector3Scale(direction, 0.1f);
        Vector3 newPosition = Vector3Add((Vector3){player.position.x, player.position.y, player.position.z}, moveStep);
        player.position.x = newPosition.x;
        player.position.y = newPosition.y;
        player.position.z = newPosition.z;

        // Run the same gameplay loop as the host
        RunGame(&player, NULL, false, 0); // Pass `NULL` for `serverData` as client doesn't control other players

        BeginDrawing();
        ClearBackground(SKYBLUE);

        // Camera and drawing logic
        Camera3D camera = { 0 };
        camera.position = (Vector3){ player.position.x, player.position.y + 1.5f, player.position.z + 6.0f };
        camera.target = (Vector3){ player.position.x, player.position.y + 2.5f, player.position.z };
        camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
        camera.fovy = 45.0f;
        camera.projection = CAMERA_PERSPECTIVE;

        BeginMode3D(camera);
            DrawModelEx(LoadModel("boat.obj"), (Vector3){player.position.x, player.position.y, player.position.z}, (Vector3){0, 1, 0}, 0, (Vector3){0.07f, 0.07f, 0.07f}, BROWN);
            Vector3 waterPosition = {0.0f, -1.0f, 0.0f};
            DrawPlane(waterPosition, (Vector2){400.0f, 400.0f}, BLUE);
            
            // Draw the sprinkles 
            for (int i = 0; i < NUM_SPRINKLES; i++) {
                DrawCube(sprinkles[i], 0.5f, 0.05f, 0.2f, DARKBLUE);
            }
            
            DrawGrid(10, 1.0f);
        EndMode3D();

        EndDrawing();
    }

    close(sock);
}
