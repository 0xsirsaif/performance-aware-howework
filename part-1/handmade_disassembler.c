#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
    return 1;
  }

  FILE *fp = fopen(argv[1], "rb");
  if (fp == NULL) {
    perror("fopen");
    return 1;
  }

  uint8_t bytes[2];
  // read 2 bytes
  size_t count = fread(bytes, 1, 2, fp);
  if (count != 2) {
    fprintf(stderr, "expected 2 bytes, got %zu\n", count);
    fclose(fp);
    return 1;
  }

  uint8_t first_byte = bytes[0];
  uint8_t opcode = first_byte >> 2;
  uint8_t D = (first_byte & 0x2) >> 1;
  uint8_t W = (first_byte & 0x1);

  if (opcode != 0b100010) {
    printf("Not MOV instrution");
    return 1;
  }

  uint8_t second_byte = bytes[1];
  uint8_t mod = (second_byte & 0xc0) >> 6;
  uint8_t reg = (second_byte & 0x38) >> 3;
  uint8_t rm = (second_byte & 0x7);

  const char *registers_8bit_map[] = {"al", "cl", "dl", "bl",
                                      "ah", "ch", "dh", "bh"};

  const char *registers_16bit_map[] = {"ax", "cx", "dx", "bx",
                                       "sp", "bp", "si", "di"};

  const char *rm_address_expression[] = {
      "[BX + SI]", "[BX + DI]", "[BP + SI]", "[BP + DI]",
      "[SI]",      "[DI]",      "[BP]",      "[BX]",
  };

  const char **registers;
  const char *width;

  if (W == 0) {
    registers = registers_8bit_map;
    width = "8";
  } else {
    registers = registers_16bit_map;
    width = "16";
  }

  const char *reg_name = registers[reg];

  const char *rm_address_exp;
  int read_extra_bytes;
  int is_direct_access = 0;
  switch (mod) {
  case 0b00:
    if (rm == 0b110) {
      is_direct_access = 1;
    } else {
      rm_address_exp = rm_address_expression[rm];
      read_extra_bytes = 0;
    }
    break;
  case 0b01:
    rm_address_exp = rm_address_expression[rm];
    read_extra_bytes = 1;
    break;
  case 0b10:
    rm_address_exp = rm_address_expression[rm];
    read_extra_bytes = 2;
    break;
  case 0b11:
    rm_address_exp = registers[rm];
    read_extra_bytes = 0;
    break;
  }

  int total_num_of_bytes = 2 + read_extra_bytes;
  uint8_t bytes2[total_num_of_bytes];
  if (total_num_of_bytes > 2) {
    int n = fread(bytes2, 1, total_num_of_bytes, fp);
    if (n != 2) {
      fprintf(stderr, "expected %i bytes, got %i\n", total_num_of_bytes, n);
      fclose(fp);
      return 1;
    }
    fclose(fp);

    uint8_t third_byte = bytes2[2];
    uint16_t the_full_extra_byte;
    if (total_num_of_bytes == 3) {
      the_full_extra_byte = third_byte;
    } else if (total_num_of_bytes == 4) {
      uint8_t fourth_byte = bytes2[3];
      the_full_extra_byte = third_byte | (fourth_byte << 8);
    }
  }

  /*
   * After we know the register we should know which one is the src
   * and which one is the dest
   */

  const char *src, *dest;
  if (D == 0) {
    src = reg_name;
    dest = rm_address_exp;
  } else if (D == 1) {
    src = rm_address_exp;
    dest = reg_name;
  }

  printf("bits %s\n", width);
  printf("mov %s, %s\n", dest, src);
}
