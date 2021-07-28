#include <stdio.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "chip8.h"
#include "chip8keyboard.h"
#include "toot.h"


// keyboard map (btw physical and virtual keyboard)
const char keyboard_map[CHIP8_TOTAL_KEYS] = { 
  SDLK_0, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5,
  SDLK_6, SDLK_7, SDLK_8, SDLK_9, SDLK_a, SDLK_b,
  SDLK_c, SDLK_d, SDLK_e, SDLK_f
};

int main(int argc, char** argv) {
  // get program from cmd line
  if (argc < 2) {
    printf("You must provide a file to load\n");
    return -1;
  }

  // get filename
  const char* filename = argv[1];
  printf("The filename to load is: %s\n", filename);

  // load the file into memory
  FILE* f = fopen(filename, "rb");
  if (!f) {
    printf("Failed to open the file\n");
    fclose(f);
    return -1;
  }

  // determine the size of the buffer
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  // create a buffer with the defined size
  char buf[size];

  // fread() reads data from the given stream into the array pointed to => from f to buf
  int res = fread(buf, size, 1, f);
  if (res != 1) {
    printf("Failed to read from file\n");
    return -1;
  }

  // instantiate a chip8
  struct chip8 chip8;
  chip8_init(&chip8);

  // call the method to load the program into the chip8 memory
  chip8_load(&chip8, buf, size);

  // set the keyboard to the map
  chip8_keyboard_set_map(&chip8.keyboard, keyboard_map);


  // init sdl2
  SDL_Init(SDL_INIT_EVERYTHING);

  // create sdl window
  SDL_Window* window = SDL_CreateWindow(
    EMULATOR_WINDOW_TITLE, 
    SDL_WINDOWPOS_UNDEFINED,
    SDL_WINDOWPOS_UNDEFINED,
    CHIP8_WIDTH * CHIP8_WINDOW_MULTIPLIER,
    CHIP8_HEIGHT * CHIP8_WINDOW_MULTIPLIER,
    SDL_WINDOW_SHOWN
  );

  // create a renderer
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_TEXTUREACCESS_TARGET);

  uint32_t start_tick;
  uint32_t frame_speed;

  // the SDL_Event structure to be filled with the next event from the queue, or NULL
  SDL_Event event;
  while(1) {
    start_tick = SDL_GetTicks();

    // check events in the queue
    while(SDL_PollEvent(&event)) {
        // SDL_QUIT is an event => https://wiki.libsdl.org/SDL_Event (click to quit program)
        switch (event.type) {
          case SDL_QUIT:
            return -1;
          break;

          case SDL_KEYDOWN:
          {
            // get the key pressed
            char key = event.key.keysym.sym;
            // get the corresponding virtual key
            int vkey = chip8_keyboard_map(&chip8.keyboard, key);
            // if the key is in keyboard_map we change its state
            if (vkey != -1) {
              chip8_keyboard_down(&chip8.keyboard, vkey);
            }
          }
          break;

          case SDL_KEYUP:
          {
            // get the key released
            char key = event.key.keysym.sym;
            // get the corresponding virtual key
            int vkey = chip8_keyboard_map(&chip8.keyboard, key);
            // if the key is in keyboard_map we change its state
            if (vkey != -1) {
              chip8_keyboard_up(&chip8.keyboard, vkey);
            }
          }
          break;
        }
    }


    /********** Set color for background **********/

    // Set the color used for drawing operations
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    // Clear the current rendering target with the drawing color
    SDL_RenderClear(renderer);

    /********** Set color for rect **********/

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 0);

    for(int y = 0; y < CHIP8_HEIGHT; y++) {
      for(int x = 0; x < CHIP8_WIDTH; x++) {
        if(chip8_screen_is_set(&chip8.screen, x, y)) {
          SDL_Rect r;
          r.x = x * CHIP8_WINDOW_MULTIPLIER;
          r.y = y * CHIP8_WINDOW_MULTIPLIER;
          r.w = CHIP8_WINDOW_MULTIPLIER;
          r.h = CHIP8_WINDOW_MULTIPLIER;

          // draw rect filled by the specified color
          SDL_RenderFillRect(renderer, &r);
        }
      }
    }

    // update the screen with any rendering performed since the previous call.
    SDL_RenderPresent(renderer);

    // delay timer
    if (chip8.registers.delay_timer > 0) {
      printf("%d\n", chip8.registers.delay_timer);
      // usleep(10000);
      SDL_Delay(1);
      chip8.registers.delay_timer -=1;
    }

    // sound timer
    if(chip8.registers.sound_timer > 0) {
      toot(500, 10 * chip8.registers.sound_timer); // args => frequence and duration in ms
      chip8.registers.sound_timer = 0;
    }

    // get the opcode from the program loaded into the chip8 memory
    unsigned short opcode = chip8_memory_get_short(&chip8.memory, chip8.registers.PC);
    // increment the PC BEFORE executing the opcode => infinite loop with chip8_stack_push() => 2nnn
    chip8.registers.PC += 2;
    // execute the opcode
    chip8_exec(&chip8, opcode);

    // handle the frame rate
    frame_speed = SDL_GetTicks() - start_tick;
    if (frame_speed < MILLISECONDS_PER_CYCLE) {
      SDL_Delay(MILLISECONDS_PER_CYCLE - frame_speed);
    }
  }

  // destroy the window and free memory
  SDL_DestroyWindow(window);

  // close the file resource
  fclose(f);

  return 0;
}
