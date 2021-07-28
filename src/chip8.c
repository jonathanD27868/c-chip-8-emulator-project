#include "chip8.h"
#include <memory.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "SDL2/SDL.h"


// default character set => cf. 2.4 - Display in the doc
const char chip8_default_character_set[] = {
  0xf0, 0x90, 0x90, 0x90, 0xf0, // => 0
  0x20, 0x60, 0x20, 0x20, 0x70, // => 1
  0xf0, 0x10, 0xf0, 0x80, 0xf0, // => 2
  0xf0, 0x10, 0xf0, 0x10, 0xf0, // => 3
  0x90, 0x90, 0xf0, 0x10, 0x10, // => 4
  0xf0, 0x80, 0xf0, 0x10, 0xf0, // => 5
  0xf0, 0x80, 0xf0, 0x90, 0xf0, // => 6
  0xf0, 0x10, 0x20, 0x40, 0x40, // => 7
  0xf0, 0x90, 0xf0, 0x90, 0xf0, // => 8
  0xf0, 0x90, 0xf0, 0x10, 0xf0, // => 9
  0xf0, 0x90, 0xf0, 0x90, 0x90, // => A
  0xe0, 0x90, 0xe0, 0x90, 0xe0, // => B
  0xF0, 0x80, 0x80, 0x80, 0xf0, // => C
  0xe0, 0x90, 0x90, 0x90, 0xe0, // => D
  0xf0, 0x80, 0xf0, 0x80, 0xf0, // => E
  0xf0, 0x80, 0xf0, 0x80, 0x80, // => F
};

void chip8_init(struct chip8* chip8) {
  memset(chip8, 0, sizeof(struct chip8));
  memcpy(&chip8->memory.memory, chip8_default_character_set, sizeof(chip8_default_character_set));
}

void chip8_load(struct chip8* chip8, const char* buf, size_t size) {
  // check that the loaded program is not bigger than the memory capacity of the chip8
  // take into account the offset, the program is stored from address 0x200 not 0x000
  assert(size + CHIP8_PROGRAM_LOAD_ADDRESS < CHIP8_MEMORY_SIZE);

  // copy to the memory chip8
  memcpy(&chip8->memory.memory[CHIP8_PROGRAM_LOAD_ADDRESS], buf, size);

  // change the program counter to point to Ox200 from where the program is loaded
  chip8->registers.PC = CHIP8_PROGRAM_LOAD_ADDRESS;
}

static void chip8_exec_extended_eight(struct chip8* chip8, unsigned short opcode) {
  unsigned char x = (opcode >> 8) & 0x000f;
  unsigned char y = (opcode >> 4) & 0x000f;
  unsigned char final_four_bits = opcode & 0x000f;
  unsigned short tmp = 0;

  switch (final_four_bits) {
    // LD Vx, Vy - 8xy0 - Set Vx = Vy
    case 0x00:
      chip8->registers.V[x] = chip8->registers.V[y];
    break;

    // OR Vx, Vy - 8xy1 - set Vx = Vx OR Vy
    case 0x01:
      chip8->registers.V[x] |= chip8->registers.V[y];
    break;

    // AND Vx, Vy - 8xy1 - set Vx = Vx AND Vy
    case 0x02:
      chip8->registers.V[x] &= chip8->registers.V[y];
    break;

    // XOR Vx, Vy - 8xy1 - set Vx = Vx XOR Vy
    case 0x03:
      chip8->registers.V[x] ^= chip8->registers.V[y];
    break;

    // ADD Vx, Vy - 8xy4 - Set Vx = Vx + Vy, set VF = carry
    case 0x04:
      tmp = chip8->registers.V[x] + chip8->registers.V[y];
      chip8->registers.V[0x0f] = tmp > 0xff ? true : false;
      chip8->registers.V[x] = tmp;
    break;

    // SUB Vx, Vy - 8xy5 - Set Vx = Vx - Vy, set VF NOT borrow
    case 0x05:
    chip8->registers.V[0x0f] = chip8->registers.V[x] > chip8->registers.V[y];
    chip8->registers.V[x] -= chip8->registers.V[y];
    break;

    // SHR Vx {, Vy} - 8xy6 - Set Vx = Vx SHR 1
    case 0x06:
      // chip8->registers.V[0x0f] = chip8->registers.V[x] & 1 == 1 ? true : false;
      // or simpler
      chip8->registers.V[0x0f] = chip8->registers.V[x] & 1;
      chip8->registers.V[x] /= 2;
    break;

    // SUBN Vx, Vy - 8xy7 - Set Vx = Vy - Vx, set VF = NOT borrow
    case 0x07:
      chip8->registers.V[0x0f] = chip8->registers.V[y] > chip8->registers.V[x];
      chip8->registers.V[x] = chip8->registers.V[y] - chip8->registers.V[x];
    break;

    // SHL Vx {, Vy} - 8xyE - Set Vx = Vx SHL 1
    case 0x0E:
      chip8->registers.V[0x0f] =  chip8->registers.V[x] & 0b10000000;
      chip8->registers.V[x] *= 2;
    break;
  }

}

// handle key press via SDL2, it holds until a key is pressed
static char chip8_wait_for_key_press(struct chip8* chip8) {
  SDL_Event event;

  // Wait indefinitely for the next available event.
  while(SDL_WaitEvent(&event)) {
    // if the event is not a key pressed we continue
    if (event.type != SDL_KEYDOWN) continue;

    char c = event.key.keysym.sym;
    char chip8_key = chip8_keyboard_map(&chip8->keyboard, c);
    if (chip8_key != -1) {
      return chip8_key;
    }
  }
  return -1;
}

static void chip8_exec_extended_F(struct chip8* chip8, unsigned short opcode) {
  unsigned char x = (opcode >> 8) & 0x000f;
  unsigned char y = (opcode >> 4) & 0x000f;

  switch (opcode & 0x00ff) {
    // LD Vx, DT - Fx07 - Set Vx = delay timer value
    case 0x07:
      chip8->registers.V[x] = chip8->registers.delay_timer;
    break;

    // LD Vx, K - Fx0A - "WAIT" for a key press, store the value of the in Vx
    // key pressing is detected using SDL2, we then create a function to handle that
    case 0x0A:
    {
      char pressed_key = chip8_wait_for_key_press(chip8);
      chip8->registers.V[x] = pressed_key;
    }
    break;

    // LD DT, Vx - Fx15 - Set delay timer = Vx
    case 0x15:
      chip8->registers.delay_timer = chip8->registers.V[x];
    break;

    // LD ST, Vx - Fx18 - Set sound timer = Vx
    case 0x18:
      chip8->registers.sound_timer = chip8->registers.V[x];
    break;

    // ADD I, Vx - Fx1E - Set I = I + Vx
    case 0x1E:
      chip8->registers.I += chip8->registers.V[x];
    break;

    // LD F, Vx - Fx29 - Set I = location of sprite for digit Vx => default character set
    // if V[x] == 0 => 0 * 5 = 0 | if V[x] = 1 => 1 * 5 = 5 | etc.
    case 0x29:
      chip8->registers.I = chip8->registers.V[x] * CHIP8_DEFAULT_SPRITE_HEIGHT;
    break;

    // LD B, Vx - Fx33 - Store BCD representation of Vx in mimory locations, I, I+1 and I+2
    // 15 in BCD = 0001 0101 => 1 5
    case 0x33:
    {
      unsigned char hundreds = chip8->registers.V[x]/100;
      unsigned char tens = chip8->registers.V[x] / 10 % 10;
      unsigned char units = chip8->registers.V[x] % 10;
      chip8_memory_set(&chip8->memory, chip8->registers.I, hundreds);
      chip8_memory_set(&chip8->memory, chip8->registers.I + 1, tens);
      chip8_memory_set(&chip8->memory, chip8->registers.I + 2, units);
    }
    break;

    // LD [I], Vx - Fx55 - Store registers V0 through Vx in memory starting at location I
    case 0x55:
    {
      for(int i = 0; i <= x; i++) {
        chip8_memory_set(&chip8->memory, chip8->registers.I + i, chip8->registers.V[i]);
      }
    }
    break;

    // LD Vx, [I] - Fx65 - Read registers V0 through Vx from memory starting at location I
    case 0x65:
    {
      for(int i = 0; i <= x; i++) {
        chip8->registers.V[i] = chip8_memory_get(&chip8->memory, chip8->registers.I + i);
      }
    }
    break;
  }
}

void chip8_exec_extended(struct chip8* chip8, unsigned short opcode) {
  unsigned short nnn = opcode & 0x0fff;
  unsigned char x = (opcode >> 8) & 0x000f;
  unsigned char y = (opcode >> 4) & 0x000f;
  unsigned char kk = opcode & 0x00ff;
  unsigned char n = opcode & 0x000f;

  switch (opcode & 0xf000) {
    // JP addr - 1nnn - Jump to location nnn's
    case 0x1000:
      chip8->registers.PC = nnn;
    break;

    // CALL addr - 2nnn - Call subroutine at location nnn
    case 0x2000:
      // put the current PC on the top of the stack
      chip8_stack_push(chip8, chip8->registers.PC);
      chip8->registers.PC = nnn;
    break;

    // SE Vx, byte - 3xkk - Skip next instruction if Vx = kk
    case 0x3000:
      if (chip8->registers.V[x] == kk) {
        chip8->registers.PC += 2;
      }
    break;

    // SNE Vx, byte - 4xkk - Skip next instruction if vx != kk
    case 0x4000:
      if (chip8->registers.V[x] != kk) {
        chip8->registers.PC += 2;
      }
    break;

    // SE Vx, Vy - 5xy0 - Skip next instruction if vx = vy
    case 0x5000:
      if (chip8->registers.V[x] == chip8->registers.V[y]) {
        chip8->registers.PC += 2;
      }
    break;

    // LD Vx, byte - 6xkk - Set Vx = kk
    case 0x6000:
      chip8->registers.V[x] = kk;
    break;

    // ADD vx, byte - 7xkk - Set Vx = Vx + kk
    case 0x7000:
      chip8->registers.V[x] += kk;
    break;

    // handles instruction starting by 8xy
    case 0x8000:
      chip8_exec_extended_eight(chip8, opcode);
    break;

    // SNE Vx, Vy - 9xy0 - Skip next instruction if Vx != Vy
    case 0x9000:
      chip8->registers.PC += chip8->registers.V[x] != chip8->registers.V[y] ? 2 : 0;
    break;

    // LD I, addr - Annn - Set I = nnn (I register is bound to the draw instruction)
    // cf. DRW Vx, Vy, nibble - Dxyn
    case 0xA000:
      chip8->registers.I = nnn;
    break;

    // JP V0, addr - Bnnn - Jump to location nnn + V0
    case 0xB000:
      chip8->registers.PC = nnn + chip8->registers.V[0];
    break;

    // RND Vx, byte - Cxkk - Set Vx = random byte AND kk
    case 0xC000:
      srand(clock());
      char rand_number = rand() % 256;
      chip8->registers.V[x] = rand_number & kk;
    break;

    // DRW Vx, Vy, nibble - Dxyn - Display n-byte sprite starting at memory location I at (Vx, Vy),
    // set VF = collision
    case 0xD000:
    {
      // get the sprite from the ram
      unsigned char* sprite = (unsigned char*) &chip8->memory.memory[chip8->registers.I];
      chip8->registers.V[0x0f] = chip8_screen_draw_sprite(&chip8->screen, chip8->registers.V[x], 
                                  chip8->registers.V[y], sprite, n);
    }
    break;

    // handle instructions starting by 0xE
    case 0xE000:
    {
      switch (kk){
        // SKP Vx - Ex9E - Skip next instruction if key with the value of Vx is pressed
        case 0x9E:
          if (chip8_keyboard_is_down(&chip8->keyboard, chip8->registers.V[x])) {
            chip8->registers.PC += 2;
          }
        break;

        // SKNP Vx - ExA1 - Skip next instruction if key with the value of Vx is not pressed
        case 0xA1:
          if (!chip8_keyboard_is_down(&chip8->keyboard, chip8->registers.V[x])) {
            chip8->registers.PC += 2;
          }
        break;
      }
    }
    break;

    // handle instructions startin by 0xF
    case 0xF000:
      chip8_exec_extended_F(chip8, opcode);
    break;
  }
}

void chip8_exec(struct chip8* chip8, unsigned short opcode) {
  switch(opcode) {
    // CLS: clear chip8 screen
    case 0x00E0:
      chip8_screen_clear(&chip8->screen);
    break;

    // RET: return from subroutine, we set the Program Counter to the address of the next 
    // instruction to be executed from the memory
    case 0x00EE:
      chip8->registers.PC = chip8_stack_pop(chip8);
    break;

    // default handles the opcodes that need an random address (1nnn where "nnn" is the address)
    default:
      chip8_exec_extended(chip8, opcode);
  }
}
