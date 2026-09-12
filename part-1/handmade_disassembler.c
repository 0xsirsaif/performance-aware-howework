#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

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

  printf("bits 16\n");

  while (1) {

    uint8_t bytes[2];
    // read 2 bytes
    size_t count = fread(bytes, 1, 1, fp);
    if (count == 0) {
      if (feof(fp)) {
        break; // clean end of file
      }

      perror("fread");
      fclose(fp);
      return 1;
    }

    // Here we need to decide if we're dealing with the
    // third form of ADD/SUB/CMP
    // if yes, don't read the second byte
    // decode the first, print and continue
    uint8_t first_byte = bytes[0];
    uint8_t opcode = first_byte >> 2;
    uint8_t D = (first_byte & 0x2) >> 1;
    uint8_t W = (first_byte & 0x1);

    uint8_t first_2bits = first_byte >> 6;
    char *instruction_name = NULL;
    if (opcode == 0b100010){
        instruction_name = "Mov";
    }
    if (first_2bits == 0b00) {
      uint8_t potentially_op_bits = (first_byte & 0b00111000) >> 3;
      switch (potentially_op_bits) {
      case 0b000:
        instruction_name = "Add";
        break;
      case 0b101:
        instruction_name = "Sub";
        break;
      case 0b111:
        instruction_name = "Cmp";
        break;
      }
    }

    int third_form = (first_byte & 0b11000110) == 0b00000100;
    int immediate = 0;
    if (third_form) {
      uint8_t op = (first_byte & 0b00111000) >> 3;
      uint8_t w = (first_byte & 0b00000001);

      if (w == 0) {
        // AL, imm8
        uint8_t imm8;
        fread(&imm8, 1, 1, fp);

        immediate = imm8;

      } else {
        // AX, imm16
        uint8_t imm_bytes[2];
        fread(imm_bytes, 1, 2, fp);

        immediate = (uint16_t)imm_bytes[0] | ((uint16_t)imm_bytes[1] << 8);
      }

      const char *accumulator = W ? "ax" : "al";

      printf("%s %s, %d\n", instruction_name, accumulator, immediate);

      continue;
    }

    count = fread(&bytes[1], 1, 1, fp);

    if (count != 1) {
      break; // ignore trailing byte
    }

    // let's determine the encoding form for the ADD/SUB/CMP
    // we've 3 encoding forms:
    // 1. reg/mem with register: (00 [OP] 0 D W)  . (mod reg r/m)
    // 2. immediate to reg/mem:  (100000 S W)     . (mod [OP] r/m)
    // 3. register to accumlator (00 [OP] 10 w)
    // we need to determine if it's the first/third case

    uint8_t second_byte = bytes[1];
    uint8_t mod = (second_byte & 0xc0) >> 6;
    uint8_t reg = (second_byte & 0x38) >> 3;
    uint8_t rm = (second_byte & 0x7);

    int second_form = 0;
    uint8_t second_form_flag = (first_byte & 0b11111100) >> 2;

    if (second_form_flag == 0b100000) {
      second_form = 1;
      uint8_t opcode_extension = (second_byte & 0b00111000) >> 3;
      switch (opcode_extension) {
      case 0b000:
        instruction_name = "Add";
        break;
      case 0b101:
        instruction_name = "Sub";
        break;
      case 0b111:
        instruction_name = "Cmp";
        break;
      }
    }

    const char *registers_8bit_map[] = {"al", "cl", "dl", "bl",
                                        "ah", "ch", "dh", "bh"};

    const char *registers_16bit_map[] = {"ax", "cx", "dx", "bx",
                                         "sp", "bp", "si", "di"};

    const char *rm_address_expression[] = {
        "BX + SI", "BX + DI", "BP + SI", "BP + DI", "SI", "DI", "BP", "BX",
    };

    const char **registers;
    if (W == 0) {
      registers = registers_8bit_map;
    } else {
      registers = registers_16bit_map;
    }

    const char *reg_name = registers[reg];

    char rm_operand[64];
    const char *base;
    int read_extra_bytes;
    int is_direct_access = 0;
    int is_r_r = 0;
    switch (mod) {
    case 0b00:
      if (rm == 0b110) {
        // [16-bit direct address]
        is_direct_access = 1;
        read_extra_bytes = 2;
      } else {
        base = rm_address_expression[rm];
        read_extra_bytes = 0;
      }
      break;
    case 0b01:
      base = rm_address_expression[rm];
      read_extra_bytes = 1;
      break;
    case 0b10:
      base = rm_address_expression[rm];
      read_extra_bytes = 2;
      break;
    case 0b11:
      base = registers[rm];
      read_extra_bytes = 0;
      is_r_r = 1;
      break;
    }

    uint8_t extra_bytes[2];
    int n = fread(extra_bytes, 1, read_extra_bytes, fp);
    if (n != read_extra_bytes) {
      fprintf(stderr, "expected %i bytes, got %i\n", read_extra_bytes, n);
      fclose(fp);
      return 1;
    }

    int displacement;
    uint16_t direct_address;
    if (read_extra_bytes == 1) {
      uint8_t low_byte = extra_bytes[0];
      displacement = (int8_t)low_byte;
    } else if (read_extra_bytes == 2) {
      uint8_t low_byte = extra_bytes[0];
      uint8_t high_byte = extra_bytes[1];
      uint16_t raw_displacement =
          (uint16_t)low_byte | ((uint16_t)high_byte << 8);
      displacement = (int16_t)(raw_displacement);
      if (is_direct_access) {
        direct_address = raw_displacement;
      }
    }

    if (is_direct_access == 0) {
      // register to register
      if (is_r_r) {
        snprintf(rm_operand, sizeof(rm_operand), "%s", base);
      }
      // with displacement
      else if (read_extra_bytes > 0) {
        if (displacement > 0) {
          snprintf(rm_operand, sizeof(rm_operand), "[%s + %d]", base,
                   displacement);
        } else if (displacement < 0) {
          snprintf(rm_operand, sizeof(rm_operand), "[%s - %d]", base,
                   -displacement);
        } else {
          snprintf(rm_operand, sizeof(rm_operand), "[%s]", base);
        }
      } else if (read_extra_bytes == 0) {
        // without displacement
        snprintf(rm_operand, sizeof(rm_operand), "[%s]", base);
      }
    } else if (is_direct_access == 1) {
      // direct access
      snprintf(rm_operand, sizeof(rm_operand), "[%u]",
               (unsigned)direct_address);
    }

    /*
     * After we know the register we should know which one is the src
     * and which one is the dest
     */

    const char *src, *dest;
    if (D == 0) {
      src = reg_name;
      dest = rm_operand;
    } else if (D == 1) {
      src = rm_operand;
      dest = reg_name;
    }

    uint8_t S = D;

    if (second_form) {
      if (W == 0) {
        uint8_t imm8;
        fread(&imm8, 1, 1, fp);
        immediate = imm8;

      } else if (S == 0) {
        uint8_t imm_bytes[2];
        fread(imm_bytes, 1, 2, fp);

        immediate = (uint16_t)imm_bytes[0] | ((uint16_t)imm_bytes[1] << 8);

      } else {
        int8_t imm8;
        fread(&imm8, 1, 1, fp);

        immediate = imm8;
      }
    }

    if (second_form) {
      printf("%s %s, %d\n", instruction_name, rm_operand, immediate);

    } else {
      printf("%s %s, %s\n", instruction_name, dest, src);
    }

    fclose(fp);
    return 0;
  }
}
