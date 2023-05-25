#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <windows.h>

#define ENEMY_TYPE_BASIC 0
#define ENEMY_TYPE_SPACESHIP 1
#define ENEMY_TYPE_ROCKET 2
#define ENEMY_TYPE_SCORPION 3
#define ENEMY_TYPE_MAX 4

#define GAME_STATE_MENU 0
#define GAME_STATE_PLAYING 1
#define GAME_STATE_PAUSED 2
#define GAME_STATE_OVER 3
#define GAME_STATE_WIN 4

struct bullet_t {
    float x;
    float y;
    float bullet_speed;
    int is_missile;
};

struct player_t {
    float x;
    float y;
    int width;
    int height;
    float mx;
    float my;
    float movement_speed;

    struct bullet_t* bullet_array;
    int bullet_array_max;
    int bullet_array_count;
    float bullet_speed;

    int life;
    int score;
    int missiles;

    float blink_duration;
    float blinking_time_elapsed;
    float blink_time_next;
    float blink_rate;
    int blink;
};

struct enemy_t {
    float x;
    float y;
    float spwan_y;
    int width;
    int height;
    int type;
    float movement_speed;
    float time_elapsed;
    int life;
    float movement_range;

    float bullet_speed;
    float bullet_cooldown;

    BITMAP bitmap;
    HBITMAP hbitmap;

    // for scorpion
    int has_target;
    float target_y;
};

struct game_t {
    HWND hwnd;
    wchar_t title[256];
    int height;
    int width;
    int top_margin;
    HBITMAP dbl_buffer;
    HFONT font;
    HFONT font_big;
    struct player_t* player;
    struct enemy_t* enemies;
    int enemy_max;
    int enemy_count;

    double time_elapsed;

    // enemy bullets
    struct bullet_t* bullet_array;
    int bullet_array_max;
    int bullet_array_count;

    float enemy_spawn_time_elapsed;
    float enemy_spawn_time_max;
    int enemy_spawned;

    float bg_1_x;
    float bg_1_speed ;

    float bg_2_x;
    float bg_2_speed;

    int state;
    char keyboard_state[256];

    // loaded resources
    HGDIOBJ bmp_bg;
    HGDIOBJ bmp_bg2;
    HGDIOBJ bmp_heart;
    HGDIOBJ bmp_player;
    HGDIOBJ bmp_player_shield;
    HGDIOBJ bmp_rocket;
    HGDIOBJ bmp_enemies[ENEMY_TYPE_MAX];
};

struct game_t* game;

const float PI = 22.0f/7.0;

static LRESULT CALLBACK main_window_proc(HWND hwnd,
                        UINT message, WPARAM wParam, LPARAM lParam);

float get_randf(float a, float b) {
    float random = ((float) rand()) / (float) RAND_MAX;
    float diff = b - a;
    float r = random * diff;
    return a + r;
}

int get_rand(int min, int max) {
    return rand()%(max-min + 1) + min;
}

HFONT create_font(const wchar_t* name, int size) {
    HDC hDC = GetDC(HWND_DESKTOP);
    int hight = -MulDiv(size, GetDeviceCaps(hDC, LOGPIXELSY), 72);
    ReleaseDC(HWND_DESKTOP, hDC);
    HFONT font = CreateFontW(hight, 0, 0, 0, FW_BOLD, 0, 0, 0,
	   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, name);
    return font;
}

void game_load_resources() {
    game->bmp_bg = LoadImageW(0, L"./bg.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    game->bmp_bg2 = LoadImageW(0, L"./bg_2.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    game->bmp_heart = LoadImageW(0, L"./heart.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    game->bmp_player = LoadImageW(0, L"./player.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    game->bmp_player_shield = LoadImageW(0, L"./player_shield.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    game->bmp_rocket = LoadImageW(0, L"./rocket.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    game->bmp_enemies[0] = LoadImageW(0, L"./enemy_basic.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    game->bmp_enemies[1] = LoadImageW(0, L"./enemy_spaceship.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    game->bmp_enemies[2] = LoadImageW(0, L"./enemy_rocket.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
    game->bmp_enemies[3] = LoadImageW(0, L"./enemy_scorpion.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
}

void game_init_dblbuffer() {
    RECT rcClient;
    GetClientRect(game->hwnd, &rcClient);
    if (game->dbl_buffer != 0) {
        DeleteObject(game->dbl_buffer);
    }
    const HDC hdc = GetDC(game->hwnd);
    game->dbl_buffer = CreateCompatibleBitmap(hdc, rcClient.right, rcClient.bottom);
    ReleaseDC(game->hwnd, hdc);
}

int game_init_window() {
    game->height = 720;
    game->width = 1024;
    game->top_margin = 30;
    game->dbl_buffer = 0;

    const wchar_t window_class_name[] = L"main_window_class";

    wcscpy(game->title, L"Space Impact");

    WNDCLASSEXW wincl;

    wincl.hInstance = GetModuleHandleW(0);
    wincl.lpszClassName = window_class_name;
    wincl.lpfnWndProc = main_window_proc;
    wincl.style = CS_DBLCLKS;
    wincl.cbSize = sizeof (WNDCLASSEX);

    wincl.hIcon = LoadIconW(0, IDI_APPLICATION);
    wincl.hIconSm = LoadIconW(0, IDI_APPLICATION);
    wincl.hCursor = LoadCursorW(0, IDC_ARROW);
    wincl.lpszMenuName = 0;
    wincl.cbClsExtra = 0;
    wincl.cbWndExtra = 0;
    wincl.hbrBackground = (HBRUSH) COLOR_BACKGROUND;

    if (RegisterClassExW(&wincl)) {
        game->hwnd = CreateWindowExW(
                    0,
                    window_class_name,
                    game->title,
                    WS_OVERLAPPEDWINDOW,
                    CW_USEDEFAULT,
                    CW_USEDEFAULT,
                    game->width,
                    game->height,
                    HWND_DESKTOP,
                    0,
                    wincl.hInstance,
                    0);
    } else {
        return 0;
    }

    game_init_dblbuffer();

    game->font = create_font(L"Arial", 26);
    game->font_big = create_font(L"Arial", 40);

    game_load_resources();

    ShowWindow(game->hwnd, SW_SHOW);
    return 1;
}

void game_cleanup_resources() {
    for(int i = 0; i < ENEMY_TYPE_MAX;i++) {
        DeleteObject(game->bmp_enemies[i]);
    }
    DeleteObject(game->bmp_rocket);
    DeleteObject(game->bmp_player_shield);
    DeleteObject(game->bmp_player);
    DeleteObject(game->bmp_heart);
    DeleteObject(game->bmp_bg2);
    DeleteObject(game->bmp_bg);
}

void player_init(struct player_t* player) {
    player->x = 100;
    player->y = (game->height / 2) - 100;

    player->mx = 0;
    player->my = 0;

    player->width = 30;
    player->height = 24;

    player->movement_speed = 1.1f;

    player->blink_duration = 100.0f;
    player->blinking_time_elapsed = player->blink_duration;
    player->blink_time_next = player->blink_duration;
    player->blink_rate = 4.5f;
    player->blink = 0;

    player->bullet_array_max = 100;
    player->bullet_array = malloc(sizeof(struct bullet_t) * player->bullet_array_max);
    player->bullet_array_count = 0;

    player->bullet_speed = 1.2f;

    player->life = 3;
    player->score = 0;
    player->missiles = 5;
}

struct game_t* game_initialize() {
    game = (struct game_t*)malloc(sizeof(struct game_t));

    game->enemy_count = 0;
    game->enemy_max = 100;
    game->time_elapsed = 0.0f;
    game->state = GAME_STATE_MENU;

    game->enemy_spawn_time_elapsed = 0.0f;
    game->enemy_spawn_time_max = 100.0f;
    game->enemy_spawned = 0;

    game->bg_1_x = 0.0f;
    game->bg_1_speed = 0.2f;

    game->bg_2_x = 0.0f;
    game->bg_2_speed = 0.4f;

    memset(game->keyboard_state, 0, sizeof(game->keyboard_state));

    if (!game_init_window()) {
        return 0;
    }

    game->player = (struct player_t*)malloc(sizeof(struct player_t));
    player_init(game->player);

    game->enemies = malloc(sizeof(struct enemy_t) * game->enemy_max);

    game->bullet_array_max = 100;
    game->bullet_array = malloc(sizeof(struct bullet_t) * game->bullet_array_max);
    game->bullet_array_count = 0;

    return game;
}

void game_cleanup_enemies() {
    free(game->enemies);
    game->enemy_count = 0;
}

void game_cleanup_player() {
    free(game->player->bullet_array);
    free(game->player);
}

void game_delete() {
    if (game != 0) {
        free(game->bullet_array);
        game_cleanup_enemies();
        game_cleanup_player();
        game_cleanup_resources();
        DeleteObject(game->font_big);
        DeleteObject(game->font);
        DeleteObject(game->dbl_buffer);
        free(game);
    }
}

void enemy_spwan(int x, int y, int type) {
    if (game->enemy_count >= game->enemy_max) {
        return;
    }
    struct enemy_t* new_enemy = &game->enemies[game->enemy_count];
    new_enemy->x = x;
    new_enemy->y = y;
    new_enemy->spwan_y = y;

    new_enemy->has_target = 0;
    new_enemy->target_y = 0.0f;

    new_enemy->type = type;

    new_enemy->hbitmap = game->bmp_enemies[type];
    GetObject(new_enemy->hbitmap, sizeof(BITMAP), &new_enemy->bitmap) ;

    new_enemy->width = 22;
    new_enemy->height = 22;
    new_enemy->movement_speed = get_randf(0.8, 0.4);

    new_enemy->time_elapsed = 0.0f;

    new_enemy->movement_range = get_randf(0.18, 0.05);

    if (new_enemy->type == ENEMY_TYPE_BASIC) {
        new_enemy->width = 28;
        new_enemy->height = 22;
        new_enemy->life = 1;
        new_enemy->movement_speed = get_randf(0.8, 1.1);
    } else if (new_enemy->type == ENEMY_TYPE_SPACESHIP) {
        new_enemy->width = 22;
        new_enemy->height = 22;
        new_enemy->life = 2;
        new_enemy->movement_speed = get_randf(1.0, 1.6);
    } else if (new_enemy->type == ENEMY_TYPE_ROCKET) {
        new_enemy->width = 30;
        new_enemy->height = 20;
        new_enemy->life = 2;
        new_enemy->movement_speed = get_randf(0.6, 0.8);
    } else if (new_enemy->type == ENEMY_TYPE_SCORPION) {
        new_enemy->width = 30;
        new_enemy->height = 20;
        new_enemy->life = 3;
        new_enemy->movement_speed = get_randf(1.6, 2.0);
    }

    new_enemy->bullet_speed = get_randf(1.5f, 1.8f);
    new_enemy->bullet_cooldown = get_randf(50, 100);
    game->enemy_count++;

    game->enemy_spawned++;
}

void enemy_remove(int index) {
    int count = game->enemy_count - 1;
    for(int i = index; i < count; i++) {
        game->enemies[i] = game->enemies[i + 1];
    }
    game->enemy_count--;
}

void bullet_remove(int index) {
    int count = game->bullet_array_count - 1;
    for(int i = index; i < count; i++) {
        game->bullet_array[i] = game->bullet_array[i + 1];
    }
    game->bullet_array_count--;
}

void player_bullet_remove(int index) {
    int count = game->player->bullet_array_count - 1;
    for(int i = index; i < count; i++) {
        game->player->bullet_array[i] = game->player->bullet_array[i + 1];
    }
    game->player->bullet_array_count--;
}

void player_lose_life() {
    struct player_t* player = game->player;
    if (player->blinking_time_elapsed < 1) {
        player->life -= 1;
        player->blinking_time_elapsed = player->blink_duration;
        player->blink_time_next = player->blink_duration;
    }

    if (player->life < 1) {
        game->state = GAME_STATE_OVER;
    }
}

int bullet_collision(struct bullet_t* bullet, float x, float y, float width, float height) {
    if (bullet->x >= x && bullet->x <= (x + width)) {
        if (bullet->y >= y && bullet->y <= (y + height)) {
            return 1;
        }
    }
    return 0;
}

void bullet_update(struct bullet_t* bullet, int index, float dt) {
    if (bullet->x <= 0) {
        bullet_remove(index);
        return;
    }
    if (bullet_collision(bullet, game->player->x, game->player->y,
                         game->player->width, game->player->height)) {
        player_lose_life();
        bullet_remove(index);
        return;
    }
    bullet->x -= bullet->bullet_speed * dt;
}

void enemy_shoot_bullet(struct enemy_t* enemy) {
    if (game->bullet_array_count >= game->bullet_array_max) {
        return;
    }
    struct bullet_t* new_bullet = &game->bullet_array[game->bullet_array_count];
    new_bullet->x = enemy->x;
    new_bullet->y = enemy->y + (enemy->height / 2);
    new_bullet->is_missile = 0;
    new_bullet->bullet_speed = enemy->bullet_speed;
    game->bullet_array_count++;
}

void enemy_update(struct enemy_t* enemy, int index, float dt) {
    if (enemy->x < 0) {
        enemy_remove(index);
        return;
    }
    int old_y = enemy->y;
    enemy->x = enemy->x - enemy->movement_speed * dt;

    if (enemy->type == ENEMY_TYPE_SPACESHIP || enemy->type == ENEMY_TYPE_ROCKET) {
        float posA = enemy->spwan_y - 120;
        float posB = enemy->spwan_y + 120;

        float cosineValue = cos(2.0f * PI * (enemy->time_elapsed));

        enemy->y = posA + (posB - posA) * enemy->movement_range * (1.0f - cosineValue);

        enemy->time_elapsed += dt / 200;

        if (enemy->time_elapsed >= 1.0f) {
            enemy->time_elapsed = 0.0f;

            enemy->movement_range = get_randf(0.18, 0.05);
        }
    } else if (enemy->type == ENEMY_TYPE_SCORPION) {

        if (!enemy->has_target) {
            if (enemy->time_elapsed >= 10.0f) {
                enemy->time_elapsed = 0.0f;

                enemy->has_target = 1;
                int which = get_rand(0, 1);
                if (which) {
                    enemy->target_y = enemy->y - 150;
                } else {
                    enemy->target_y = enemy->y + 150;
                }
            }
            enemy->time_elapsed += dt * 0.1f;
        }

        if (enemy->has_target) {
            float distance = enemy->target_y - enemy->y;
            float n = sqrt(distance*distance);
            float dir = distance / n;
            //printf("%f\n", n);
            if (n > 5.0) {
                enemy->y += dir * (2 * dt);
            } else {
                // reached target
                enemy->has_target = 0;
                enemy->time_elapsed = 0.0f;
            }
        }
    }

    if (enemy->y <= game->top_margin) {
        enemy->y = old_y;
    }

    if (enemy->y > game->height - 100) {
        enemy->y = old_y;
    }

    if (enemy->type == ENEMY_TYPE_SPACESHIP) {
        if (enemy->bullet_cooldown <= 0) {
            enemy_shoot_bullet(enemy);
            enemy->bullet_cooldown = get_randf(30, 80);
        }

        enemy->bullet_cooldown -= dt * 0.5;
    }
}

void player_bullet_update(struct bullet_t* bullet, int index, float dt) {
    if (bullet->x > game->width) {
        player_bullet_remove(index);
        return;
    }
    for(int i = 0;i < game->enemy_count;i++) {
        struct enemy_t* enemy = &game->enemies[i];
        if (bullet_collision(bullet, enemy->x, enemy->y, enemy->width, enemy->height)) {
            enemy->life -= 1;
            if (enemy->life <= 0) {
                enemy_remove(i);
            }
            player_bullet_remove(index);
            if (enemy->type == ENEMY_TYPE_SPACESHIP) {
                game->player->score += 20;
            } else if (enemy->type == ENEMY_TYPE_ROCKET) {
                game->player->score += 30;
            } else if (enemy->type == ENEMY_TYPE_SCORPION) {
                game->player->score += 50;
            } else {
                game->player->score += 10;
            }
            return;
        }
    }
    bullet->x += bullet->bullet_speed * dt;
}

int player_enemy_collision(struct enemy_t* enemy) {
    struct player_t* player = game->player;
    if (player->x < enemy->x + enemy->width &&
        player->x + player->width > enemy->x &&
        player->y < enemy->y + enemy->height &&
        player->height + player->y > enemy->y)
    {
        return 1;
    }
    return 0;
}

void player_update(struct player_t* player, float dt) {
    RECT rcClient;
    GetClientRect(game->hwnd, &rcClient);

    int old_x = player->x;
    int old_y = player->y;

    player->x += player->mx * dt;
    player->y += player->my * dt;

    if (player->x <= 0) {
        player->x = old_x;
    }

    if (player->x + player->width >= rcClient.right) {
        player->x = old_x;
    }

    if (player->y <= game->top_margin) {
        player->y = old_y;
    }

    if (player->y + player->height >= rcClient.bottom) {
        player->y = old_y;
    }

    for(int i = 0;i < player->bullet_array_count;i++) {
        struct bullet_t* bullet = &player->bullet_array[i];
        player_bullet_update(bullet, i, dt);
    }

    if (player->blinking_time_elapsed > 0) {
        player->blinking_time_elapsed -= dt * 0.5f;

        if ( player->blinking_time_elapsed < player->blink_time_next) {
            player->blink = !player->blink;

            player->blink_time_next = player->blinking_time_elapsed - player->blink_rate;
        }
    }
}

void game_update(float dt) {
    if (game->player->life < 1) {
        return;
    }

    if (game->state != GAME_STATE_PLAYING) {
        return;
    }

    game->bg_1_x -= game->bg_1_speed * dt;

    if (game->bg_1_x <= -1024) {
        game->bg_1_x = 0.0f;
    }

    game->bg_2_x -= game->bg_2_speed * dt;

    if (game->bg_2_x <= -1024) {
        game->bg_2_x = 0.0f;
    }

    player_update(game->player, dt);

    for(int i = 0;i < game->enemy_count;i++) {
        struct enemy_t* enemy = &game->enemies[i];
        enemy_update(enemy, i, dt);

        if (player_enemy_collision(enemy)) {
            player_lose_life();
        }
    }

    for(int i = 0;i < game->bullet_array_count;i++) {
        struct bullet_t* bullet = &game->bullet_array[i];
        bullet_update(bullet, i, dt);
    }

    // start spawning enemies
    if (game->enemy_spawn_time_elapsed > game->enemy_spawn_time_max) {
        game->enemy_spawn_time_elapsed = 0;

        if (game->time_elapsed > 2000) {
            game->enemy_spawn_time_max = get_randf(50, 100);
        } else if (game->time_elapsed > 4000) {
            game->enemy_spawn_time_max = get_randf(20, 50);
        } else {
            game->enemy_spawn_time_max = get_randf(150, 200);
        }

        float x = get_randf(game->width, game->width + 50);
        float y = get_randf(200, 500);

        //if (enemy_spawned < 1) {
            if (game->time_elapsed > 6000) {
                int which = get_rand(ENEMY_TYPE_SPACESHIP, ENEMY_TYPE_SCORPION);
                enemy_spwan(x, y, which);
            } else if (game->time_elapsed > 4000) {
                int which = get_rand(ENEMY_TYPE_SPACESHIP, ENEMY_TYPE_ROCKET);
                enemy_spwan(x, y, which);
            } else if (game->time_elapsed > 2000) {
                int which = get_rand(ENEMY_TYPE_BASIC, ENEMY_TYPE_SPACESHIP);
                enemy_spwan(x, y, which);
            } else {
                enemy_spwan(x, y, ENEMY_TYPE_BASIC);
            }
        //}
    }

    game->enemy_spawn_time_elapsed += dt;

    game->time_elapsed += dt;
}

void bullet_draw(struct bullet_t* bullet, HDC hdc) {
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));

    SelectObject(hdc, pen);
    SelectObject(hdc, brush);

    Rectangle(hdc, bullet->x, bullet->y - 1, bullet->x + 5, bullet->y + 2);

    DeleteObject(brush);
    DeleteObject(pen);
}

void enemy_draw(struct enemy_t* enemy, HDC hdc) {
    // draw the player
    HDC hdc_bmp = CreateCompatibleDC(hdc);
    SelectObject(hdc_bmp, enemy->hbitmap);

    TransparentBlt(hdc, enemy->x, enemy->y,
               enemy->width, enemy->height, hdc_bmp, 0, 0, enemy->bitmap.bmWidth, enemy->bitmap.bmHeight, RGB(127, 184, 145));
    DeleteDC(hdc_bmp);
}

void player_draw(struct player_t* player, HDC hdc) {
    if (player->blinking_time_elapsed > 0) {
        if (player->blink) {
            HDC hdc_bmp2 = CreateCompatibleDC(hdc);
            SelectObject(hdc_bmp2, game->bmp_player_shield);
            TransparentBlt(hdc, player->x - 5, player->y - 5,
                           player->width + 10, player->height + 10, hdc_bmp2,
                           0, 0, 54, 42, RGB(127, 184, 145));
            DeleteDC(hdc_bmp2);
        }
    }
    // draw the player
    HDC hdc_bmp = CreateCompatibleDC(hdc);
    SelectObject(hdc_bmp, game->bmp_player);

    TransparentBlt(hdc, player->x, player->y, player->width, player->height, hdc_bmp, 0, 0, 50, 38, RGB(127, 184, 145));
    DeleteDC(hdc_bmp);

    // draw bullets
    for(int i = 0;i < player->bullet_array_count;i++) {
        struct bullet_t* bullet = &player->bullet_array[i];
        bullet_draw(bullet, hdc);
    }
}

void game_draw_score(HDC hdc) {
    wchar_t str_score[256];
    swprintf(str_score, 256, L"%05d", game->player->score);
    TextOutW(hdc, game->width - 250, 5, str_score, wcslen(str_score));
}

void game_draw_missiles_left(HDC hdc, int x) {
    wchar_t str_score[256];
    HDC hdc_bmp2 = CreateCompatibleDC(hdc);

    SelectObject(hdc_bmp2, game->bmp_rocket);
    StretchBlt(hdc, x, 16, 30, 18, hdc_bmp2, 0, 0, 54, 44, SRCCOPY);
    DeleteDC(hdc_bmp2);

    swprintf(str_score, 256, L"%02d", game->player->missiles);
    TextOutW(hdc, x + 50, 5, str_score, wcslen(str_score));
}

void game_draw_background(HDC hdc, RECT* rect) {
    // draw the moving background
    HDC hdc_bg = CreateCompatibleDC(hdc);
    SelectObject(hdc_bg, game->bmp_bg);

    StretchBlt(hdc, game->bg_1_x, 0, rect->right, rect->bottom, hdc_bg, 0, 0, 1024, 720, SRCCOPY);
    StretchBlt(hdc, game->bg_1_x + 1024, 0, rect->right, rect->bottom, hdc_bg, 0, 0, 1024, 720, SRCCOPY);

    DeleteDC(hdc_bg);

    HDC hdc_bg2 = CreateCompatibleDC(hdc);
    SelectObject(hdc_bg2, game->bmp_bg2);

    TransparentBlt(hdc, game->bg_2_x, rect->bottom - 212, rect->right, 212, hdc_bg2, 0, 0, 1024, 212, RGB(127, 184, 145));
    TransparentBlt(hdc, game->bg_2_x + 1024, rect->bottom - 212, rect->right, 212, hdc_bg2, 0, 0, 1024, 212, RGB(127, 184, 145));

    DeleteDC(hdc_bg2);
}

void game_draw_lifes(HDC hdc) {
    HDC hdc_bmp = CreateCompatibleDC(hdc);
    SelectObject(hdc_bmp, game->bmp_heart);

    for(int i = 0; i < game->player->life;i++) {
        StretchBlt(hdc, 10 + (i * 30), 15, 20, 20, hdc_bmp, 0, 0, 20, 20, SRCCOPY);
    }
    DeleteDC(hdc_bmp);
}

void game_draw(HDC hdc, RECT* rect) {
    wchar_t buf[256];

    game_draw_background(hdc, rect);

    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, game->font);

    for(int i = 0;i < game->enemy_count;i++) {
        struct enemy_t* enemy = &game->enemies[i];
        enemy_draw(enemy, hdc);
    }

    if (game->player->life >= 1) {
        player_draw(game->player, hdc);
        // draw how many life(s) left
        game_draw_lifes(hdc);
    }

    // draw bullets (enemy)
    for(int i = 0;i < game->bullet_array_count;i++) {
        struct bullet_t* bullet = &game->bullet_array[i];
        bullet_draw(bullet, hdc);
    }

    // draw how many missiles left
    game_draw_missiles_left(hdc, 400);

    // draw the score
    game_draw_score(hdc);

    RECT rcText = *rect;

    if (game->state == GAME_STATE_MENU) {
        SelectObject(hdc, game->font);
        swprintf(buf, 256, L"Press Enter to Play");
        DrawTextW(hdc, buf, wcslen(buf), &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (game->state == GAME_STATE_OVER) {
        SelectObject(hdc, game->font);
        swprintf(buf, 256, L"Your Score: %d", game->player->score);
        DrawTextW(hdc, buf, wcslen(buf), &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        rcText.top -= 50;
        rcText.bottom -= 50;

        SelectObject(hdc, game->font_big);
        swprintf(buf, 256, L"GAME OVER");
        DrawTextW(hdc, buf, wcslen(buf), &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if(game->state == GAME_STATE_PAUSED) {
        SelectObject(hdc, game->font);
        swprintf(buf, 256, L"Press Enter to Resume");
        DrawTextW(hdc, buf, wcslen(buf), &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        rcText.top -= 50;
        rcText.bottom -= 50;

        SelectObject(hdc, game->font_big);
        swprintf(buf, 256, L"PAUSED");
        DrawTextW(hdc, buf, wcslen(buf), &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void player_shoot_bullet(struct player_t* player) {
    if (player->bullet_array_count >= player->bullet_array_max) {
        return;
    }
    struct bullet_t* new_bullet = &player->bullet_array[player->bullet_array_count];
    new_bullet->x = player->x + player->width;
    new_bullet->y = player->y + (player->height / 2);
    new_bullet->is_missile = 0;
    new_bullet->bullet_speed = player->bullet_speed;
    player->bullet_array_count++;
}

void game_exit() {
    PostQuitMessage(0);
}

void game_check_input() {
    if (game->state == GAME_STATE_PLAYING) {
        if (game->keyboard_state[VK_UP]) {
            game->player->my = -game->player->movement_speed;
        } else if (game->keyboard_state[VK_DOWN]) {
            game->player->my = game->player->movement_speed;
        } else {
            game->player->my = 0.0f;
        }

        if (game->keyboard_state[VK_LEFT]) {
            game->player->mx = -game->player->movement_speed;
        } else if (game->keyboard_state[VK_RIGHT]) {
            game->player->mx = game->player->movement_speed;
        } else {
            game->player->mx = 0.0f;
        }
    }
}

void game_input(int key, int down) {

    if (key == VK_ESCAPE) {
        game_exit();
    }

    if (down) {
        game->keyboard_state[key] = 1;
        if (key == VK_RETURN) {
            if (game->state == GAME_STATE_MENU)
                game->state = GAME_STATE_PLAYING;
            else if (game->state == GAME_STATE_PLAYING)
                game->state = GAME_STATE_PAUSED;
            else if (game->state == GAME_STATE_PAUSED)
                game->state = GAME_STATE_PLAYING;
        } else if (key == VK_SPACE) {
            if (game->state == GAME_STATE_PLAYING) {
                player_shoot_bullet(game->player);
            }
        }
    } else {
        game->keyboard_state[key] = 0;
    }
}

LRESULT CALLBACK main_window_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_KEYDOWN: {
            game_input(wParam, 1);
            break;
        }
        case WM_KEYUP: {
            game_input(wParam, 0);
            break;
        }
        case WM_ERASEBKGND: {
            return 1;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            const HDC hdc = ps.hdc;
            const HDC hdcBuffer = CreateCompatibleDC(hdc);
            SelectObject(hdcBuffer, game->dbl_buffer);

            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            HBRUSH page_bgk_brush = CreateSolidBrush(RGB(127, 184, 145));
            FillRect(hdcBuffer, &rcClient, page_bgk_brush);

            game_draw(hdcBuffer, &rcClient);

            BitBlt(hdc, 0,0, rcClient.right, rcClient.bottom, hdcBuffer, 0,0, SRCCOPY);

            DeleteDC(hdcBuffer);

            DeleteObject(page_bgk_brush);
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            break;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    return 0;
}

void game_run() {
    MSG msg;
    LARGE_INTEGER start_counter, end_counter, frequency;
    double seconds_per_frame;
    QueryPerformanceCounter(&start_counter);
    QueryPerformanceFrequency(&frequency);
    while (1) {
        while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if(msg.message == WM_QUIT)
            break;
        QueryPerformanceCounter(&end_counter);

        seconds_per_frame = (((double)(end_counter.QuadPart -
                                       start_counter.QuadPart) * 100.0f) / (double)frequency.QuadPart);
        start_counter = end_counter;
        game_check_input();
        game_update(seconds_per_frame);
        InvalidateRect(game->hwnd, 0, 1);
	}
}

int main() {
    if (game_initialize()) {
        game_run();
        game_delete(game);
    }
    return 0;
}
