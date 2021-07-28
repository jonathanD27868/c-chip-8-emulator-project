#include "chip8stack.h"
#include "chip8.h"
#include <assert.h>


// check if the value passed is in the chip8 bounds
static void chip8_stack_in_bounds(struct chip8* chip8) {
  assert(chip8->registers.SP < CHIP8_TOTAL_STACK_DEPTH);
}

void chip8_stack_push(struct chip8* chip8, unsigned short val){
  chip8_stack_in_bounds(chip8);

  // set SP (stack pointer)
  chip8->stack.stack[chip8->registers.SP] = val;

  // increment the stack pointer, SP=8 bits and is of type char then +=1 does the trick
  // so that the next call to chip8_stack_push() will go to the next value in the stack
  chip8->registers.SP += 1;
}

unsigned short chip8_stack_pop(struct chip8* chip8){
  // decrement the stack pointer, SP=8 bits and is of type char then -=1 does the trick
  chip8->registers.SP -= 1;

  chip8_stack_in_bounds(chip8);

  // get the last frame in the stack to return it
  unsigned short val = chip8->stack.stack[chip8->registers.SP];
  return val;
}

