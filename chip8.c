#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <math.h>
//
#include <SDL2/SDL.h>

typedef uint8_t  U8;
typedef uint16_t U16;
typedef uint32_t U32;

#define MEMORY_SIZE 4096u
#define ROM_START   512u
#define STACK_SIZE  16u

U8 M[MEMORY_SIZE] = {0};
U8 V[16]  = {0};

U16 I  = 0;
U16 PC = ROM_START;

U16 S[STACK_SIZE] = {0};
U8  SP            =  0;

U8 DT = 0;
U8 ST = 0;

#define SCREEN_W 64u
#define SCREEN_H 32u
#define SCREEN_SCALE 16u

#define FILL_COL  0xFFFFFFFFu
#define EMPTY_COL 0x00000000u

bool screen[SCREEN_W * SCREEN_H] = {false};
bool keys[16] = {false};

bool running = true;

#define FONT_START 0x0000u
#define FONT_WIDTH 5u

SDL_Window  *window       = NULL;
SDL_Surface *win_surface  = NULL;
SDL_Surface *buff_surface = NULL;

FILE *file = NULL;

void chip8_stop() {
    running = false;
}

void load_font() {
    const U8 font_set[16 * FONT_WIDTH] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    memcpy(M + FONT_START, font_set, sizeof font_set);
}

U8 *get_mem(U16 addr) {
    if(addr >= MEMORY_SIZE) {
        fprintf(stderr, "address out of bounds %04x/%04x\n", addr, MEMORY_SIZE);
        exit(EXIT_FAILURE);
    }
    return &M[addr];
}

void stack_push(U16 addr) {
    if(++SP >= STACK_SIZE) {
        fprintf(stderr, "stack overflow\n");
        exit(EXIT_FAILURE);
    }
    S[++SP] = PC;
    PC = addr;
}

void stack_pop() {
    if(SP == 0) {
        fprintf(stderr, "stack underflow\n");
        exit(EXIT_FAILURE);
    }
    PC = S[SP--];
}

U8 read_key(U8 key) {
    if(key > 0xF) {
        fprintf(stderr, "invalid key %x\n", key);
        exit(EXIT_FAILURE);
    }
    return keys[key];
}

void mem_store(U8 len) {
    if(I + len >= MEMORY_SIZE) {
        fprintf(stderr, "mem_store address overflow %04x\n", I + len);
        exit(EXIT_FAILURE);
    }
    memcpy(M + I, V, len + 1);
}

void mem_load(U8 len) {
    if(I + len >= MEMORY_SIZE) {
        fprintf(stderr, "mem_load address overflow %04x\n", I + len);
        exit(EXIT_FAILURE);
    }
    memcpy(V, M + I, len + 1);
}

void draw_sprite(U8 s_x, U8 s_y, U8 bytes) {
    V[0xF] = 0;
    SDL_LockSurface(win_surface);
    for(U16 i_y = 0; i_y < bytes; ++i_y) {
        U16 y = s_y + i_y;
        y -= (y / SCREEN_H) * SCREEN_H;
        U8 mem_val = *get_mem(I + i_y);
        for(U16 i_x = 0; i_x < 8; ++i_x) {
            if((mem_val & (1 << (7 - i_x))) >> (7 - i_x)) {
                U16 x = s_x + i_x;
                x -= (x / SCREEN_W) * SCREEN_W;
                V[0xF] |= screen[x + y * SCREEN_W];
                screen[x + y * SCREEN_W] ^= 1;

                U32 col = screen[x + y * SCREEN_W] ? FILL_COL : EMPTY_COL;
                U32 win_x = x * SCREEN_SCALE;
                for(U32 i = 0; i < SCREEN_SCALE; ++i) {
                    U32 win_y = (y * SCREEN_SCALE  * SCREEN_SCALE +
                                 i * SCREEN_SCALE) * SCREEN_W;
                    memset(((U32 *)win_surface->pixels) + win_x + win_y,
                           col,
                           SCREEN_SCALE * 4);
                }
            }
        }
    }
    SDL_UnlockSurface(win_surface);
}

U8 await_keypress() {
    SDL_Event e;
    while(true) {
        SDL_WaitEvent(&e);
        if(e.type == SDL_QUIT) chip8_stop();
        if(e.type == SDL_KEYDOWN) {
            switch(e.key.keysym.sym) {
                case SDLK_1 : return 0x1;
                case SDLK_2 : return 0x2;
                case SDLK_3 : return 0x3;
                case SDLK_4 : return 0xC;
                case SDLK_q : return 0x4;
                case SDLK_w : return 0x5;
                case SDLK_e : return 0x6;
                case SDLK_r : return 0xD;
                case SDLK_a : return 0x7;
                case SDLK_s : return 0x8;
                case SDLK_d : return 0x9;
                case SDLK_f : return 0xE;
                case SDLK_z : return 0xA;
                case SDLK_x : return 0x0;
                case SDLK_c : return 0xB;
                case SDLK_v : return 0xF;
            }
        }
    }
}

void exec_opcode() {
    U16 opcode = ((U16)*get_mem(PC) << 8) | *get_mem(PC + 1);
    PC += 2;
    U16 addr   = opcode &  0x0FFF;
    U8  byte   = opcode &  0x00FF;
    U8  n      = opcode &  0x000F;
    U8  x      = (opcode & (0x0F00)) >> 8;
    U8  y      = (opcode & (0x00F0)) >> 4;
    U8 *Vx     = &V[x];
    U8 *Vy     = &V[y];
    switch((opcode & 0xF000) >> 12) {
        case 0x0: switch(opcode & 0x00FF) {
            case 0x00: chip8_stop();                               break;
            case 0xE0: memset(screen, EMPTY_COL, sizeof screen);
                       SDL_FillRect(win_surface, NULL, EMPTY_COL); break;
            case 0xEE: stack_pop();                                break;
            default: goto error_opcode;
        } break;
        case 0x1: PC = addr;                     break;
        case 0x2: stack_push(addr)       ;       break;
        case 0x3: *Vx == byte ? PC += 2 : 0;     break;
        case 0x4: *Vx != byte ? PC += 2 : 0;     break;
        case 0x5: *Vx == *Vy  ? PC += 2 : 0;     break;
        case 0x6: *Vx  = byte;                   break;
        case 0x7: *Vx += byte;                   break;
        case 0x8: switch(opcode & 0x000F) {
            case 0x0: *Vx  = *Vy;                break;
            case 0x1: *Vx |= *Vy;                break;
            case 0x2: *Vx &= *Vy;                break;
            case 0x3: *Vx ^= *Vy;                break;
            case 0x4: V[0xF] = *Vx > 0xFF - *Vy;
                      *Vx += *Vy;                break;
            case 0x5: V[0xF] = *Vx > *Vy;
                      *Vx -= *Vy;                break;
            case 0x6: V[0xF] = *Vx & 0x1;
                      *Vx >>= 1;                 break;
            case 0x7: V[0xF] = *Vy > *Vx;
                      *Vx = *Vy - *Vx;           break;
            case 0xE: V[0xF] = (*Vy & 0x80) >> 7; 
                      *Vx <<= 1;                 break;
            default: goto error_opcode;
        } break;
        case 0x9: *Vx != *Vy ? PC += 2 : 0;           break;
        case 0xA: I = addr;                           break;
        case 0xB: PC = V[0x0] + addr;                 break;
        case 0xC: *Vx = (rand() % 256) & byte;        break;
        case 0xD: draw_sprite(*Vx, *Vy, n);           break;
        case 0xE: switch(opcode & 0x00FF) {
            case 0x9E:  read_key(*Vx) ? PC += 2 : 0;  break;
            case 0xA1: !read_key(*Vx) ? PC += 2 : 0;  break;
            default: goto error_opcode;
        } break;
        case 0xF: switch(opcode & 0x00FF) {
            case 0x07: *Vx = DT;                          break;
            case 0x0A: *Vx = await_keypress();            break;
            case 0x15: DT = *Vx;                          break;
            case 0x18: ST = *Vx;                          break;
            case 0x1E: I += *Vx;                          break;
            case 0x29: I = *Vx * FONT_WIDTH + FONT_START; break;
            case 0x33: *get_mem(I + 0) =  *Vx / 100;
                       *get_mem(I + 1) = (*Vx % 100) / 10;
                       *get_mem(I + 2) = (*Vx % 10);      break;
            case 0x55: mem_store(x);                      break;
            case 0x65: mem_load (x);                      break;
            default: goto error_opcode;
        } break;
        default: goto error_opcode;
    }
    return;

error_opcode:
    fprintf(stderr, "invalid opcode: %x\n", opcode);
    exit(EXIT_FAILURE);
}

void poll_keys() {
    SDL_Event e;
    while(SDL_PollEvent(&e) != 0) {
        if(e.type == SDL_QUIT) chip8_stop();
        if(e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            bool val = e.type == SDL_KEYDOWN;
            switch(e.key.keysym.sym) {
                case SDLK_1 : keys[0x1] = val; break;
                case SDLK_2 : keys[0x2] = val; break;
                case SDLK_3 : keys[0x3] = val; break;
                case SDLK_4 : keys[0xC] = val; break;
                case SDLK_q : keys[0x4] = val; break;
                case SDLK_w : keys[0x5] = val; break;
                case SDLK_e : keys[0x6] = val; break;
                case SDLK_r : keys[0xD] = val; break;
                case SDLK_a : keys[0x7] = val; break;
                case SDLK_s : keys[0x8] = val; break;
                case SDLK_d : keys[0x9] = val; break;
                case SDLK_f : keys[0xE] = val; break;
                case SDLK_z : keys[0xA] = val; break;
                case SDLK_x : keys[0x0] = val; break;
                case SDLK_c : keys[0xB] = val; break;
                case SDLK_v : keys[0xF] = val; break;
            }
        }
    }
}

void beep() {
    printf("BEEP!\n");
}

void start() {
    clock_t t;
    double dt_cycle = 0.0;
    double st_cycle = 0.0;
    while(running) {
        t = clock();
        poll_keys();
        exec_opcode();
        SDL_UpdateWindowSurface(window);
        t = clock() - t;
        double delta = (double)t / CLOCKS_PER_SEC;
        double tick = (1.0 / 540.0) - delta;
        if(tick > 0)
        {
            SDL_Delay(tick * 1000);
            if(DT > 0) {
                dt_cycle += (2.0 * delta + tick);
                while(dt_cycle > 1.0 / 60.0) {
                    --DT;
                    dt_cycle -= 1.0 / 60.0;
                    if(DT == 0) {
                        dt_cycle = 0.0;
                        break;
                    }
                }
            }
            if(ST > 0) {
                st_cycle += (2.0 * delta + tick);
                while(st_cycle > 1.0 / 60.0) {
                    --ST;
                    st_cycle -= 1.0 / 60.0;
                    if(ST == 0) {
                        st_cycle = 0.0;
                        beep();
                        break;
                    }
                }
            }
        }
    }
}

void close_file() {
    if(fclose(file) == EOF) fprintf(stderr, "failed to close file\n");
}

void load_file(const char *path) {
#define BUFF_SIZE (MEMORY_SIZE - ROM_START)
    U8 buff[BUFF_SIZE] = {0};
    file = fopen(path, "rb");
    if(file == NULL) {
        fprintf(stderr, "couldn't open input file\n");
        exit(EXIT_FAILURE);
    }
    atexit(&close_file);
    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    if(len == 0) {
        fprintf(stderr, "file is empty\n");
        exit(EXIT_FAILURE);
    }
    if(len > sizeof buff) {
        fprintf(stderr, "file is too long %zx/%zx\n", len, sizeof buff);
        exit(EXIT_FAILURE);
    }
    fseek(file, 0, SEEK_SET);

    if(len != fread(buff, 1, BUFF_SIZE, file)) {
        fprintf(stderr, "failed to read input file\n");
        exit(EXIT_FAILURE);
    }
    memcpy(M + ROM_START, buff, sizeof buff);
}

void deinit_sdl() {
    SDL_FreeSurface(buff_surface);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void init_sdl() {
    if(SDL_Init(SDL_INIT_VIDEO) < 0) goto error;
    window = SDL_CreateWindow("chip8",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        SCREEN_W * SCREEN_SCALE, SCREEN_H * SCREEN_SCALE, SDL_WINDOW_SHOWN);
    if(window == NULL) goto error_sdl;
    win_surface = SDL_GetWindowSurface(window);
    if(win_surface == NULL) goto error_sdl_window;
    buff_surface = SDL_CreateRGBSurface(0, SCREEN_W, SCREEN_H, 32, 0, 0, 0, 0);
    if(buff_surface == NULL) goto error_sdl_window;
    atexit(&deinit_sdl);
    return;

error_sdl_window:
    SDL_DestroyWindow(window);
error_sdl:
    fprintf(stderr, "SDL error: %s\n", SDL_GetError());
    SDL_Quit();
error:
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if(argc != 2) {
        fprintf(stderr, "usage: chip8 <input_file>\n");
        return EXIT_FAILURE;
    }
    load_file(argv[1]);
    init_sdl();

    srand(time(NULL));
    load_font();
    signal(SIGINT, &chip8_stop);
    start();

    return EXIT_SUCCESS;
}
