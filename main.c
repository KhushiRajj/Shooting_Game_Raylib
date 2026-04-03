#include "raylib.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define MAX_BULLETS 20
#define MAX_TARGETS 20
#define MAX_NAME_LEN 20
#define MAX_SCORES 10
#define SCOREBOARD_FILE "scoreboard.txt"

typedef struct {
    Vector2 position;
    Vector2 direction;
    bool active;
} Bullet;

typedef struct {
    Vector2 position;
    float speed;
    bool active;
    int size;
} Target;

typedef struct {
    Vector2 position;
    float radius;
    float alpha;
    bool active;
} Explosion;

typedef struct {
    char name[MAX_NAME_LEN + 1];
    int score;
    int wave;
} ScoreEntry;

// ---- Scoreboard I/O ----

int LoadScoreboard(ScoreEntry entries[MAX_SCORES]) {
    FILE *f = fopen(SCOREBOARD_FILE, "r");
    int count = 0;
    if (!f) return 0;
    while (count < MAX_SCORES && fscanf(f, "%20s %d %d", entries[count].name, &entries[count].score, &entries[count].wave) == 3) {
        count++;
    }
    fclose(f);
    return count;
}

void SaveScoreboard(ScoreEntry entries[MAX_SCORES], int count) {
    FILE *f = fopen(SCOREBOARD_FILE, "w");
    if (!f) return;
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s %d %d\n", entries[i].name, entries[i].score, entries[i].wave);
    }
    fclose(f);
}

void AddScore(const char *name, int score, int wave) {
    ScoreEntry entries[MAX_SCORES];
    int count = LoadScoreboard(entries);

    // Insert new entry sorted by score descending
    ScoreEntry newEntry;
    strncpy(newEntry.name, name, MAX_NAME_LEN);
    newEntry.name[MAX_NAME_LEN] = '\0';
    newEntry.score = score;
    newEntry.wave = wave;

    // Find insertion point
    int insertAt = count;
    for (int i = 0; i < count; i++) {
        if (score > entries[i].score) {
            insertAt = i;
            break;
        }
    }

    // Shift entries down
    int newCount = (count < MAX_SCORES) ? count + 1 : MAX_SCORES;
    for (int i = newCount - 1; i > insertAt; i--) {
        entries[i] = entries[i - 1];
    }
    if (insertAt < MAX_SCORES) {
        entries[insertAt] = newEntry;
    }

    SaveScoreboard(entries, newCount);
}

// ---- Helpers ----

Vector2 GetOutsidePosition(int margin) {
    int side = GetRandomValue(0, 3);
    switch(side) {
        case 0: return (Vector2){GetRandomValue(0, SCREEN_WIDTH), -margin};
        case 1: return (Vector2){GetRandomValue(0, SCREEN_WIDTH), SCREEN_HEIGHT + margin};
        case 2: return (Vector2){-margin, GetRandomValue(0, SCREEN_HEIGHT)};
        case 3: return (Vector2){SCREEN_WIDTH + margin, GetRandomValue(0, SCREEN_HEIGHT)};
    }
    return (Vector2){0, 0};
}

void Draw3DButton(Rectangle rect, const char *text, Color topColor, Color sideColor, Color borderColor, Color textColor) {
    DrawRectangle(rect.x + 3, rect.y + 3, rect.width, rect.height, sideColor);
    DrawRectangleRec(rect, topColor);
    DrawRectangleLinesEx(rect, 2, borderColor);
    int fontSize = 20;
    int textWidth = MeasureText(text, fontSize);
    DrawText(text, rect.x + rect.width / 2 - textWidth / 2, rect.y + rect.height / 2 - fontSize / 2, fontSize, textColor);
}

bool IsButtonHovered(Rectangle rect) {
    return CheckCollisionPointRec(GetMousePosition(), rect);
}

// Draw a star icon for rank decoration
void DrawRankStar(int x, int y, int rank) {
    Color c = (rank == 1) ? GOLD : (rank == 2) ? LIGHTGRAY : (rank == 3) ? (Color){205,127,50,255} : GRAY;
    DrawCircle(x, y, 10, c);
    DrawText(TextFormat("%d", rank), x - MeasureText(TextFormat("%d", rank), 14)/2, y - 7, 14, BLACK);
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Deeksha and Khushi Shooting Game");
    SetTargetFPS(60);
    srand(time(0));

    Texture2D playerTexture = LoadTexture("shooting.png");
    Texture2D slimeTexture = LoadTexture("cat.png");

    if (playerTexture.id == 0) {
        Image img = GenImageColor(64, 64, BLUE);
        playerTexture = LoadTextureFromImage(img);
        UnloadImage(img);
    }
    if (slimeTexture.id == 0) {
        Image img = GenImageColor(32, 32, RED);
        slimeTexture = LoadTextureFromImage(img);
        UnloadImage(img);
    }

    float playerScale = 0.04f;
    float playerWidth = playerTexture.width * playerScale;
    float playerHeight = playerTexture.height * playerScale;

    float slimeScale = 0.1f;
    int slimeSize = (int)(slimeTexture.height * slimeScale);

    // Game states
    typedef enum { STATE_NAME_ENTRY, STATE_START, STATE_PLAYING, STATE_GAMEOVER, STATE_NEXT_WAVE, STATE_SCOREBOARD } GameState;
    GameState gameState = STATE_NAME_ENTRY;

    // Player name input
    char playerName[MAX_NAME_LEN + 1] = {0};
    int nameLen = 0;
    float nameCursorBlink = 0.0f;

    Vector2 playerPos = {0};
    float playerSpeed = 5.0f;
    float playerRotation = 0.0f;

    Bullet bullets[MAX_BULLETS] = {0};
    float bulletSpeed = 10.0f;
    float bulletRadius = 5;

    Target targets[MAX_TARGETS] = {0};
    int maxTargetsThisWave = 5;
    float baseTargetSpeed = 0.5f;

    Explosion explosions[MAX_BULLETS] = {0};

    int currentWave = 1;
    int score = 0;
    int lives = 3;

    // Scoreboard view state
    ScoreEntry scoreEntries[MAX_SCORES];
    int scoreCount = 0;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        nameCursorBlink += dt;
        if (nameCursorBlink > 1.0f) nameCursorBlink = 0.0f;

        BeginDrawing();
        ClearBackground(RAYWHITE);

        // ============================================================
        // STATE: NAME ENTRY
        // ============================================================
        if (gameState == STATE_NAME_ENTRY) {
            // Background
            ClearBackground((Color){20, 20, 40, 255});

            // Decorative stars
            for (int i = 0; i < 60; i++) {
                int sx = (i * 137 + 50) % SCREEN_WIDTH;
                int sy = (i * 97 + 30) % SCREEN_HEIGHT;
                DrawCircle(sx, sy, 1 + (i % 2), (Color){255, 255, 255, 80 + (i % 5) * 30});
            }

            // Title
            const char *title = "SHOOTING GAME";
            int tw = MeasureText(title, 48);
            DrawText(title, SCREEN_WIDTH/2 - tw/2 + 2, 82, 48, (Color){255,140,0,200});
            DrawText(title, SCREEN_WIDTH/2 - tw/2, 80, 48, GOLD);

            const char *sub = "by Deeksha & Khushi";
            int sw = MeasureText(sub, 18);
            DrawText(sub, SCREEN_WIDTH/2 - sw/2, 138, 18, (Color){200,200,200,200});

            // Card background
            Rectangle card = {SCREEN_WIDTH/2 - 180, 190, 360, 220};
            DrawRectangleRounded(card, 0.2f, 20, (Color){40, 40, 70, 240});
            DrawRectangleRoundedLines(card, 0.2f, 20, 2, (Color){255,180,0,200});

            const char *prompt = "Enter Your Name:";
            int pw = MeasureText(prompt, 22);
            DrawText(prompt, SCREEN_WIDTH/2 - pw/2, 220, 22, (Color){255,220,100,255});

            // Name input box
            Rectangle inputBox = {SCREEN_WIDTH/2 - 140, 265, 280, 45};
            DrawRectangleRounded(inputBox, 0.3f, 10, (Color){255,255,255,20});
            DrawRectangleRoundedLines(inputBox, 0.3f, 10, 2, (nameLen > 0) ? GOLD : (Color){150,150,150,200});

            // Draw typed name + cursor
            char displayName[MAX_NAME_LEN + 2];
            strncpy(displayName, playerName, MAX_NAME_LEN);
            displayName[nameLen] = '\0';
            if (nameCursorBlink < 0.5f) {
                strncat(displayName, "|", 1);
            }
            int dnw = MeasureText(displayName, 24);
            DrawText(displayName, SCREEN_WIDTH/2 - dnw/2, 275, 24, WHITE);

            // Hint
            const char *hint = "Type and press ENTER to start";
            int hw = MeasureText(hint, 15);
            DrawText(hint, SCREEN_WIDTH/2 - hw/2, 325, 15, (Color){150,150,200,200});

            // Score button
            Rectangle scoreBoardBtn = {SCREEN_WIDTH/2 - 90, 365, 180, 38};
            Color sbColor = IsButtonHovered(scoreBoardBtn) ? (Color){70,130,200,255} : (Color){50,100,180,255};
            DrawRectangleRounded(scoreBoardBtn, 0.3f, 10, sbColor);
            DrawRectangleRoundedLines(scoreBoardBtn, 0.3f, 10, 2, (Color){120,180,255,255});
            const char *sbText = "VIEW SCOREBOARD";
            int sbtw = MeasureText(sbText, 16);
            DrawText(sbText, SCREEN_WIDTH/2 - sbtw/2, 374, 16, WHITE);

            // Handle keyboard input for name
            int key = GetCharPressed();
            while (key > 0) {
                if (key >= 32 && key <= 125 && nameLen < MAX_NAME_LEN) {
                    playerName[nameLen] = (char)key;
                    nameLen++;
                    playerName[nameLen] = '\0';
                }
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) && nameLen > 0) {
                nameLen--;
                playerName[nameLen] = '\0';
            }
            if (IsKeyPressed(KEY_ENTER) && nameLen > 0) {
                gameState = STATE_START;
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), scoreBoardBtn)) {
                scoreCount = LoadScoreboard(scoreEntries);
                gameState = STATE_SCOREBOARD;
            }
        }
        // ============================================================
        // STATE: START MENU
        // ============================================================
        else if (gameState == STATE_START) {
            ClearBackground((Color){120, 200, 150, 255});
            DrawRectangleRounded((Rectangle){SCREEN_WIDTH / 2 - 150, 80, 300, 400}, 0.2f, 20, (Color){139, 69, 19, 255});

            const char *welcome = TextFormat("Welcome, %s!", playerName);
            int ww = MeasureText(welcome, 22);
            DrawText(welcome, SCREEN_WIDTH/2 - ww/2, 100, 22, GOLD);
            DrawText("MENU", SCREEN_WIDTH / 2 - 50, 135, 40, YELLOW);

            Rectangle playButton     = {SCREEN_WIDTH / 2 - 75, 200, 150, 40};
            Rectangle scoreButton    = {SCREEN_WIDTH / 2 - 75, 260, 150, 40};
            Rectangle changeName     = {SCREEN_WIDTH / 2 - 75, 320, 150, 40};
            Rectangle exitButton     = {SCREEN_WIDTH / 2 - 75, 390, 150, 40};

            Draw3DButton(playButton, "PLAY", GREEN, DARKGREEN, BLACK, WHITE);
            Draw3DButton(scoreButton, "SCORES", BLUE, DARKBLUE, BLACK, WHITE);
            Draw3DButton(changeName, "CHANGE NAME", ORANGE, (Color){180,80,0,255}, BLACK, WHITE);
            Draw3DButton(exitButton, "EXIT", RED, (Color){139, 0, 0, 255}, BLACK, WHITE);

            Vector2 mouse = GetMousePosition();
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mouse, playButton)) {
                    playerPos = (Vector2){SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f};
                    playerRotation = 0.0f;
                    currentWave = 1;
                    score = 0;
                    lives = 3;
                    maxTargetsThisWave = 5;
                    baseTargetSpeed = 0.5f;

                    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
                    for (int i = 0; i < MAX_TARGETS; i++) targets[i].active = false;
                    for (int i = 0; i < MAX_BULLETS; i++) explosions[i].active = false;

                    for (int i = 0; i < maxTargetsThisWave; i++) {
                        targets[i].position = GetOutsidePosition(slimeSize);
                        targets[i].speed = baseTargetSpeed;
                        targets[i].size = slimeSize;
                        targets[i].active = true;
                    }
                    gameState = STATE_PLAYING;
                } else if (CheckCollisionPointRec(mouse, scoreButton)) {
                    scoreCount = LoadScoreboard(scoreEntries);
                    gameState = STATE_SCOREBOARD;
                } else if (CheckCollisionPointRec(mouse, changeName)) {
                    nameLen = 0;
                    playerName[0] = '\0';
                    gameState = STATE_NAME_ENTRY;
                } else if (CheckCollisionPointRec(mouse, exitButton)) {
                    CloseWindow();
                    return 0;
                }
            }
        }
        // ============================================================
        // STATE: PLAYING
        // ============================================================
        else if (gameState == STATE_PLAYING) {
            if (IsKeyDown(KEY_W)) playerPos.y -= playerSpeed;
            if (IsKeyDown(KEY_S)) playerPos.y += playerSpeed;
            if (IsKeyDown(KEY_A)) playerPos.x -= playerSpeed;
            if (IsKeyDown(KEY_D)) playerPos.x += playerSpeed;

            if (playerPos.x < 0) playerPos.x = 0;
            if (playerPos.y < 0) playerPos.y = 0;
            if (playerPos.x > SCREEN_WIDTH - playerWidth) playerPos.x = SCREEN_WIDTH - playerWidth;
            if (playerPos.y > SCREEN_HEIGHT - playerHeight) playerPos.y = SCREEN_HEIGHT - playerHeight;

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                Vector2 dir = {mouse.x - (playerPos.x + playerWidth / 2), mouse.y - (playerPos.y + playerHeight / 2)};
                float len = sqrt(dir.x * dir.x + dir.y * dir.y);
                if (len != 0) { dir.x /= len; dir.y /= len; }
                playerRotation = atan2f(dir.y, dir.x) * RAD2DEG;

                for (int i = 0; i < MAX_BULLETS; i++) {
                    if (!bullets[i].active) {
                        bullets[i].position = (Vector2){playerPos.x + playerWidth / 2, playerPos.y + playerHeight / 2};
                        bullets[i].direction = dir;
                        bullets[i].active = true;
                        break;
                    }
                }
            }

            for (int i = 0; i < MAX_BULLETS; i++) {
                if (bullets[i].active) {
                    bullets[i].position.x += bullets[i].direction.x * bulletSpeed;
                    bullets[i].position.y += bullets[i].direction.y * bulletSpeed;

                    if (bullets[i].position.x < 0 || bullets[i].position.x > SCREEN_WIDTH ||
                        bullets[i].position.y < 0 || bullets[i].position.y > SCREEN_HEIGHT) {
                        bullets[i].active = false;
                    } else {
                        for (int j = 0; j < MAX_TARGETS; j++) {
                            if (targets[j].active) {
                                Rectangle targetRect = {targets[j].position.x, targets[j].position.y, targets[j].size, targets[j].size};
                                if (CheckCollisionCircleRec(bullets[i].position, bulletRadius, targetRect)) {
                                    bullets[i].active = false;
                                    targets[j].active = false;
                                    score++;
                                    for (int e = 0; e < MAX_BULLETS; e++) {
                                        if (!explosions[e].active) {
                                            explosions[e].active = true;
                                            explosions[e].position = (Vector2){targets[j].position.x + targets[j].size / 2, targets[j].position.y + targets[j].size / 2};
                                            explosions[e].radius = 10.0f;
                                            explosions[e].alpha = 1.0f;
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            for (int i = 0; i < MAX_TARGETS; i++) {
                if (targets[i].active) {
                    Vector2 dir = {playerPos.x + playerWidth / 2 - targets[i].position.x,
                                   playerPos.y + playerHeight / 2 - targets[i].position.y};
                    float len = sqrt(dir.x * dir.x + dir.y * dir.y);
                    if (len != 0) { dir.x /= len; dir.y /= len; }
                    targets[i].position.x += dir.x * targets[i].speed;
                    targets[i].position.y += dir.y * targets[i].speed;

                    Rectangle playerRect = {playerPos.x, playerPos.y, playerWidth, playerHeight};
                    Rectangle targetRect = {targets[i].position.x, targets[i].position.y, targets[i].size, targets[i].size};
                    if (CheckCollisionRecs(playerRect, targetRect)) {
                        targets[i].active = false;
                        lives--;
                        if (lives <= 0) {
                            // Save score on game over
                            AddScore(playerName, score, currentWave);
                            gameState = STATE_GAMEOVER;
                        }
                    }
                }
            }

            for (int i = 0; i < MAX_BULLETS; i++) {
                if (explosions[i].active) {
                    explosions[i].radius += 2.0f;
                    explosions[i].alpha -= 0.02f;
                    if (explosions[i].alpha <= 0) explosions[i].active = false;
                }
            }

            bool allDestroyed = true;
            for (int i = 0; i < MAX_TARGETS; i++) {
                if (targets[i].active) { allDestroyed = false; break; }
            }
            if (allDestroyed) gameState = STATE_NEXT_WAVE;

            // DRAW
            ClearBackground((Color){50, 50, 80, 255});

            Rectangle sourceRec = {0, 0, playerTexture.width, playerTexture.height};
            Rectangle destRec = {playerPos.x + playerWidth / 2, playerPos.y + playerHeight / 2, playerWidth, playerHeight};
            Vector2 origin = {playerWidth / 2, playerHeight / 2};
            DrawTexturePro(playerTexture, sourceRec, destRec, origin, playerRotation, WHITE);

            for (int i = 0; i < MAX_BULLETS; i++) {
                if (bullets[i].active) DrawCircleV(bullets[i].position, bulletRadius, YELLOW);
            }
            for (int i = 0; i < MAX_TARGETS; i++) {
                if (targets[i].active) {
                    Rectangle srcRec = {0, 0, slimeTexture.width, slimeTexture.height};
                    Rectangle dstRec = {targets[i].position.x, targets[i].position.y, targets[i].size, targets[i].size};
                    DrawTexturePro(slimeTexture, srcRec, dstRec, (Vector2){0, 0}, 0, WHITE);
                }
            }
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (explosions[i].active) {
                    Color ec = (Color){255, 100, 0, (unsigned char)(explosions[i].alpha * 255)};
                    DrawCircleV(explosions[i].position, explosions[i].radius, ec);
                }
            }

            // HUD
            DrawText(TextFormat("Player: %s", playerName), 10, 10, 18, GOLD);
            DrawText(TextFormat("Score: %d", score), 10, 32, 18, WHITE);
            DrawText(TextFormat("Wave: %d", currentWave), 10, 54, 18, WHITE);
            DrawText(TextFormat("Lives: %d", lives), 10, 76, 18, RED);
        }
        // ============================================================
        // STATE: NEXT WAVE
        // ============================================================
        else if (gameState == STATE_NEXT_WAVE) {
            ClearBackground((Color){30, 80, 30, 255});

            const char *wc = "WAVE COMPLETE!";
            int wcw = MeasureText(wc, 48);
            DrawText(wc, SCREEN_WIDTH/2 - wcw/2, SCREEN_HEIGHT/2 - 90, 48, YELLOW);

            DrawText(TextFormat("Score: %d", score), SCREEN_WIDTH/2 - MeasureText(TextFormat("Score: %d", score), 28)/2, SCREEN_HEIGHT/2 - 20, 28, WHITE);
            DrawText(TextFormat("Wave %d cleared!", currentWave), SCREEN_WIDTH/2 - MeasureText(TextFormat("Wave %d cleared!", currentWave), 22)/2, SCREEN_HEIGHT/2 + 20, 22, LIGHTGRAY);
            DrawText("Press SPACE to continue to next wave", SCREEN_WIDTH/2 - MeasureText("Press SPACE to continue to next wave", 18)/2, SCREEN_HEIGHT/2 + 60, 18, WHITE);

            if (IsKeyPressed(KEY_SPACE)) {
                currentWave++;
                maxTargetsThisWave = 5 + currentWave * 2;
                if (maxTargetsThisWave > MAX_TARGETS) maxTargetsThisWave = MAX_TARGETS;
                baseTargetSpeed += 0.2f;

                for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
                for (int i = 0; i < MAX_TARGETS; i++) targets[i].active = false;
                for (int i = 0; i < MAX_BULLETS; i++) explosions[i].active = false;

                for (int i = 0; i < maxTargetsThisWave; i++) {
                    targets[i].position = GetOutsidePosition(slimeSize);
                    targets[i].speed = baseTargetSpeed;
                    targets[i].size = slimeSize;
                    targets[i].active = true;
                }
                gameState = STATE_PLAYING;
            }
        }
        // ============================================================
        // STATE: GAME OVER
        // ============================================================
        else if (gameState == STATE_GAMEOVER) {
            ClearBackground((Color){60, 10, 10, 255});

            // Decorative lines
            for (int i = 0; i < 10; i++) {
                DrawRectangle(0, i * 65, SCREEN_WIDTH, 1, (Color){255,50,50,30});
            }

            const char *go = "GAME OVER";
            int gow = MeasureText(go, 56);
            DrawText(go, SCREEN_WIDTH/2 - gow/2 + 3, 83, 56, (Color){100,0,0,255});
            DrawText(go, SCREEN_WIDTH/2 - gow/2, 80, 56, RED);

            // Player result card
            Rectangle card = {SCREEN_WIDTH/2 - 200, 155, 400, 210};
            DrawRectangleRounded(card, 0.15f, 20, (Color){30,10,10,230});
            DrawRectangleRoundedLines(card, 0.15f, 20, 2, (Color){255,80,80,200});

            const char *pname = TextFormat("Player: %s", playerName);
            DrawText(pname, SCREEN_WIDTH/2 - MeasureText(pname,22)/2, 175, 22, GOLD);
            DrawText(TextFormat("Final Score: %d", score), SCREEN_WIDTH/2 - MeasureText(TextFormat("Final Score: %d", score), 26)/2, 210, 26, WHITE);
            DrawText(TextFormat("Wave Reached: %d", currentWave), SCREEN_WIDTH/2 - MeasureText(TextFormat("Wave Reached: %d", currentWave), 20)/2, 248, 20, LIGHTGRAY);

            // Rank message
            ScoreEntry tmp[MAX_SCORES];
            int tc = LoadScoreboard(tmp);
            int rank = tc;
            for (int i = 0; i < tc; i++) {
                if (strcmp(tmp[i].name, playerName) == 0 && tmp[i].score == score) {
                    rank = i + 1;
                    break;
                }
            }
            if (rank == 1) DrawText("NEW HIGH SCORE!", SCREEN_WIDTH/2 - MeasureText("NEW HIGH SCORE!", 22)/2, 285, 22, GOLD);
            else DrawText(TextFormat("Your Rank: #%d", rank), SCREEN_WIDTH/2 - MeasureText(TextFormat("Your Rank: #%d", rank), 20)/2, 285, 20, YELLOW);

            // Buttons
            Rectangle menuBtn  = {SCREEN_WIDTH/2 - 160, 390, 140, 42};
            Rectangle scoreBtn = {SCREEN_WIDTH/2 + 20,  390, 140, 42};

            Draw3DButton(menuBtn,  "MAIN MENU",  GREEN,  DARKGREEN,  BLACK, WHITE);
            Draw3DButton(scoreBtn, "SCOREBOARD", BLUE,   DARKBLUE,   BLACK, WHITE);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 m = GetMousePosition();
                if (CheckCollisionPointRec(m, menuBtn)) {
                    gameState = STATE_START;
                } else if (CheckCollisionPointRec(m, scoreBtn)) {
                    scoreCount = LoadScoreboard(scoreEntries);
                    gameState = STATE_SCOREBOARD;
                }
            }

            DrawText("Press ENTER for Main Menu", SCREEN_WIDTH/2 - MeasureText("Press ENTER for Main Menu", 16)/2, 445, 16, (Color){180,180,180,200});
            if (IsKeyPressed(KEY_ENTER)) gameState = STATE_START;
        }
        // ============================================================
        // STATE: SCOREBOARD
        // ============================================================
        else if (gameState == STATE_SCOREBOARD) {
            ClearBackground((Color){10, 15, 35, 255});

            // Star background
            for (int i = 0; i < 80; i++) {
                int sx = (i * 173 + 20) % SCREEN_WIDTH;
                int sy = (i * 113 + 10) % SCREEN_HEIGHT;
                DrawCircle(sx, sy, 1, (Color){255,255,255,60 + (i%5)*20});
            }

            // Title
            const char *title = "SCOREBOARD";
            int tw = MeasureText(title, 44);
            DrawText(title, SCREEN_WIDTH/2 - tw/2 + 2, 32, 44, (Color){255,140,0,180});
            DrawText(title, SCREEN_WIDTH/2 - tw/2, 30, 44, GOLD);

            // Trophy icon area
            DrawText("🏆", 10, 30, 44, GOLD); // May not render on all systems, fallback below

            // Table header
            Rectangle headerRect = {60, 90, SCREEN_WIDTH - 120, 36};
            DrawRectangleRounded(headerRect, 0.3f, 10, (Color){60,60,100,220});
            DrawText("RANK", 80, 99, 18, YELLOW);
            DrawText("NAME", 180, 99, 18, YELLOW);
            DrawText("SCORE", 380, 99, 18, YELLOW);
            DrawText("WAVE", 520, 99, 18, YELLOW);
            DrawText("DATE", 630, 99, 18, YELLOW);

            // Rows
            if (scoreCount == 0) {
                DrawText("No scores yet. Play a game!", SCREEN_WIDTH/2 - MeasureText("No scores yet. Play a game!", 22)/2, 200, 22, LIGHTGRAY);
            }

            for (int i = 0; i < scoreCount && i < MAX_SCORES; i++) {
                int rowY = 140 + i * 38;
                Color rowBg = (i % 2 == 0) ? (Color){30,30,60,200} : (Color){20,20,45,200};

                // Highlight top 3
                if (i == 0) rowBg = (Color){80,60,0,220};
                else if (i == 1) rowBg = (Color){50,50,60,220};
                else if (i == 2) rowBg = (Color){60,35,10,220};

                Rectangle rowRect = {60, rowY, SCREEN_WIDTH - 120, 34};
                DrawRectangleRounded(rowRect, 0.2f, 10, rowBg);

                // Rank medal
                DrawRankStar(102, rowY + 17, i + 1);

                // Highlight current player's row
                bool isCurrentPlayer = (strcmp(scoreEntries[i].name, playerName) == 0);
                Color nameColor = isCurrentPlayer ? GOLD : WHITE;
                Color scoreColor = isCurrentPlayer ? YELLOW : LIGHTGRAY;

                DrawText(scoreEntries[i].name, 180, rowY + 8, 18, nameColor);
                DrawText(TextFormat("%d", scoreEntries[i].score), 380, rowY + 8, 18, scoreColor);
                DrawText(TextFormat("Wave %d", scoreEntries[i].wave), 520, rowY + 8, 18, LIGHTGRAY);

                // Show crown for #1
                if (i == 0) {
                    int nw = MeasureText(scoreEntries[i].name, 18);
                    DrawText(" <-- BEST!", 180 + nw, rowY + 8, 14, GOLD);
                }
            }

            // Back button
            Rectangle backBtn = {SCREEN_WIDTH/2 - 80, SCREEN_HEIGHT - 60, 160, 40};
            Draw3DButton(backBtn, "BACK", (Color){60,60,100,255}, (Color){30,30,60,255}, (Color){100,100,180,255}, WHITE);

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), backBtn)) {
                gameState = (nameLen > 0) ? STATE_START : STATE_NAME_ENTRY;
            }
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE)) {
                gameState = (nameLen > 0) ? STATE_START : STATE_NAME_ENTRY;
            }
        }

        EndDrawing();
    }

    UnloadTexture(playerTexture);
    UnloadTexture(slimeTexture);
    CloseWindow();

    return 0;
}