/*
 * we're reading a 2-byte binary file
 * our goal is to decode/disassemble the bytes into the mov instruction
 * 2-byte 8 bit structure:
 *  - first byte:
 *      - [:6]  opcode
 *      - [6:7] D = destination flag (to register or from register)
 *          - 1 -> REG is the destination register
 *          - 0 -> REG is the source register
 *      - [7]   W = word size flag (word/byte operation)
 *          - 1 -> (word) word size is 16 bits
 *          - 0 -> (byte) word size is 8 bits
 *  - second byte: operand
 *      - [:2] mod (identifies whether one of the operands is in memory or
 * whether both operands are registers)
 *          - 11 -> register operation
 *      - [2:5] reg (register)
 *      - [5:7] r/m (register/memory)
 *
 * we're just decoding one single format: move from register to register = MOV
 * REG, REG
 * furthur, we're just decoding one single CASE: move from bx to cx =
 * MOV cx, bx looking at the Table 4.9 in the 8086 manual:
 *  - cx = 001
 *  - bx = 011
 *  - Both are 16-bit size
 * = 100010 11 - 11 001 011
 * =
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
  FILE *fp = fopen("listing_0037_single_register_mov", "rb");
  if (fp == NULL) {
    perror("fopen");
    return 1;
  }

  uint8_t bytes[2];
  // read 2 elements, each of 1 byte long
  size_t count = fread(bytes, 1, 2, fp);
  if (count != 2) {
    fprintf(stderr, "expected 2 bytes, got %zu\n", count);
    fclose(fp);
    return 1;
  }
  fclose(fp);

  uint8_t first_byte = bytes[0];
  uint8_t opcode = first_byte >> 2;
  uint8_t D = (first_byte & 0x2) >> 1;
  uint8_t W = (first_byte & 0x1);

  if (opcode != 0b100010) {
    printf("Not MOV instrution");
    return 1;
  }

  uint8_t second_byte = bytes[1];
  uint8_t mod = (second_byte & 0b11000000) >> 6;
  uint8_t reg = (second_byte & 0b00111000) >> 3;
  uint8_t rm = (second_byte & 0b00000111);

  if (mod != 0b11) {
    printf("register mode only supported");
    return 1;
  }

  /*
   * the goal is to know which register operands are
   */

  const char *registers_8bit_map[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};

  const char *registers_16bit_map[] = {"ax", "cx", "dx", "bx",
                                 "sp", "bp", "si", "di"};

  const char **registers;
  const char *width;

  if (W == 0) {
    registers = registers_8bit_map;
    width = "8 bits";
  } else {
    registers = registers_16bit_map;
    width = "16 bits";
  }

  const char *reg_name = registers[reg];
  const char *rm_name = registers[rm];

  /*
   * After we know the register we should know which one is the src
   * and which one is the dest
   */

  const char *src, *dest;
  if (D == 0) {
    src = reg_name;
    dest = rm_name;
  } else if (D == 1) {
    src = rm_name;
    dest = reg_name;
  }

  printf("bits %s\n", width);
  printf("mov %s, %s\n", dest, src);
}
