#include "chip8keyboard.h"
#include <assert.h>


// check overflow
static void chip8_keyboard_ensure_in_bounds(int key) {
  assert(key >= 0 && key < CHIP8_TOTAL_KEYS);
}

// set keyboard_map
void chip8_keyboard_set_map(struct chip8_keyboard* keyboard, const char* map) {
  keyboard->keyboard_map = map;
}

// map btw virtual and physical keyboard
// here the "char key“ is our real keyboard
int chip8_keyboard_map(struct chip8_keyboard* keyboard, char key) {
  // loop through the total number of keys available
  for(int i = 0; i < CHIP8_TOTAL_KEYS; i++) {
    // check if the key pressed is in the array of the availabe keys and returns the index
    if (keyboard->keyboard_map[i] == key) {
      return i;
    }
  }
  return -1;
}

// set corresponding key down
void chip8_keyboard_down(struct chip8_keyboard* keyboard, int key) {
  keyboard->keyboard[key] = true;
}

// set corresponding key up
void chip8_keyboard_up(struct chip8_keyboard* keyboard, int key) {
  keyboard->keyboard[key] = false;
}

// return the key state => up or down
bool chip8_keyboard_is_down(struct chip8_keyboard* keyboard, int key) {
  return keyboard->keyboard[key];
}
