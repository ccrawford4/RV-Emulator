#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bits.h"
#include "rv_emu.h"

#define DEBUG 0

static void unsupported(char *s, uint32_t n) {
  printf("unsupported %s 0x%x\n", s, n);
  exit(-1);
}

// Emulates R-Type Instructions
void emu_r_type(rv_state *rsp, uint32_t iw) {
  uint32_t rd = get_bits(iw, 7, 5);
  uint32_t rs1 = get_bits(iw, 15, 5);
  uint32_t rs2 = get_bits(iw, 20, 5);
  uint32_t funct3 = get_bits(iw, 12, 3);
  uint32_t funct7 = get_bits(iw, 25, 7);

  if (funct3 == 0b000 && funct7 == 0b0000000) {
    rsp->regs[rd] = rsp->regs[rs1] + rsp->regs[rs2];
  } else if (funct3 == 0b000 && funct7 == 0b0100000) {
    rsp->regs[rd] = rsp->regs[rs1] - rsp->regs[rs2];
  } else if (funct3 == 0b000 && funct7 == 0b0000001) {
    rsp->regs[rd] = rsp->regs[rs1] * rsp->regs[rs2];
  } else if (funct3 == 0b001 && funct7 == 0b0000000) {
    rsp->regs[rd] = rsp->regs[rs1] << rsp->regs[rs2];
  } else if (funct3 == 0b100 && funct7 == 0b0000001) {
    rsp->regs[rd] = rsp->regs[rs1] / rsp->regs[rs2];
  } else if (funct3 == 0b110 && funct7 == 0b0000001) {
    rsp->regs[rd] = rsp->regs[rs1] % rsp->regs[rs2];
  } else if (funct3 == 0b101 && funct7 == 0b0000000) {
    rsp->regs[rd] = rsp->regs[rs1] >> rsp->regs[rs2];
  } else if (funct3 == 0b111 && funct7 == 0b0000000) {
    rsp->regs[rd] = rsp->regs[rs1] & rsp->regs[rs2];
  } else {
    unsupported("R-type funct3", funct3);
  }
  // Update IR Count
  rsp->analysis.ir_count += 1;
  rsp->pc += 4; // Next instruction
}

// Emulates I-type Instructions
void emu_i_type(rv_state *rsp, uint32_t iw) {
  uint32_t rd = get_bits(iw, 7, 5);
  uint32_t rs1 = get_bits(iw, 15, 5);
  uint32_t shamt = get_bits(iw, 20, 5);
  uint32_t funct3 = get_bits(iw, 12, 3);
  uint32_t funct7 = get_bits(iw, 25, 7);
  uint32_t imm = get_bits(iw, 20, 12);
  int64_t signed_offset = sign_extend(imm, 11);

  if (funct3 == 0b101 && funct7 == 0b0000000) {
    rsp->regs[rd] = rsp->regs[rs1] >> shamt;
  } else if (funct3 == 0b000) {
    rsp->regs[rd] = rsp->regs[rs1] + signed_offset;
  } else if (funct3 == 0b001 && funct7 == 0b0000000) {
    rsp->regs[rd] = rsp->regs[rs1] << shamt;
  } else {
    unsupported("I-type funct3", funct3);
  }
  rsp->analysis.ir_count += 1;
  rsp->pc += 4; // Next instruction
}

// Emulates SB-Type Instructions
void emu_sb_type(rv_state *rsp, uint32_t iw) {
  uint32_t rs1 = get_bits(iw, 15, 5);
  uint32_t rs2 = get_bits(iw, 20, 5);
  uint32_t funct3 = get_bits(iw, 12, 3);
  uint32_t imm4_1 = get_bits(iw, 8, 4);
  uint32_t imm10_5 = get_bits(iw, 25, 4);
  uint32_t imm11 = get_bit(iw, 7);
  uint32_t imm12 = get_bit(iw, 31);

  uint32_t offset =
      (imm12 << 12) | (imm11 << 11) | (imm10_5 << 5) | (imm4_1 << 1);
  int64_t signed_offset = sign_extend(offset, 12);

  uint32_t val_one = rsp->regs[rs1];
  uint32_t val_two = rsp->regs[rs2];

  bool b_taken = false;
  switch (funct3) {
  case 0b100:
    if ((int)val_one < (int)val_two)
      b_taken = true;
    break;
  case 0b101:
    if ((int)val_one >= (int)val_two)
      b_taken = true;
    break;
  case 0b000:
    if (val_one == val_two)
      b_taken = true;
    break;
  case 0b001:
    if (val_one != val_two)
      b_taken = true;
    break;
  default:
    unsupported("SB-Type funct3", funct3);
  }

  if (b_taken) {
    rsp->analysis.b_taken += 1;
    rsp->pc += signed_offset;
  } else {
    rsp->analysis.b_not_taken += 1;
    rsp->pc += 4;
  }
}

// Emulates the JALR (or RET) function for RISC-V Architecture
void emu_jalr(rv_state *rsp, uint32_t iw) {
  uint32_t rs1 = get_bits(iw, 15, 5);
  uint64_t val = rsp->regs[rs1]; // Value of RA

  rsp->analysis.j_count += 1;
  rsp->pc = val;
}

// Emulates the JAL (or CALL) function for RISC-V Architecture
void emu_jal(rv_state *rsp, uint32_t iw) {
  uint32_t rd = get_bits(iw, 7, 5);
  uint32_t imm19_12 = get_bits(iw, 12, 8);
  uint32_t imm11 = get_bit(iw, 20);
  uint32_t imm10_1 = get_bits(iw, 21, 10);
  uint32_t imm20 = get_bit(iw, 31);

  uint32_t offset =
      (imm20 << 20) | (imm19_12 << 12) | (imm11 << 11) | (imm10_1 << 1);
  int64_t signed_offset = sign_extend(offset, 20);

  // Make sure the rd is the RA register before populating it with next instruction
  if (rd == 1)
    rsp->regs[rd] = rsp->pc + 4;

  rsp->analysis.j_count += 1;
  rsp->pc += signed_offset;
}

// Emulates S-Type Instructions
void emu_s_type(rv_state *rsp, uint32_t iw) {
  uint32_t rs1 = get_bits(iw, 15, 5);
  uint32_t rs2 = get_bits(iw, 20, 5);
  uint32_t funct3 = get_bits(iw, 12, 3);
  uint32_t imm4_0 = get_bits(iw, 7, 5);
  uint32_t imm11_5 = get_bits(iw, 25, 7);

  uint64_t offset = (imm11_5 << 5) | (imm4_0);

  int64_t signed_offset = sign_extend(offset, 12);

  int64_t target = rsp->regs[rs1] + signed_offset;

  switch (funct3) {
  case 0b000:
    *(uint8_t *)target = rsp->regs[rs2];        // For Bytes
    break;
  case 0b010:
    *(uint32_t *)target = rsp->regs[rs2];       // For Words (4 Bytes Each)
    break;
  case 0b11:
    *(uint64_t *)target = rsp->regs[rs2];       // For Doubles
    break;
  default:
    unsupported("S-Type funct3", funct3);
  }

  rsp->analysis.st_count += 1;
  rsp->pc += 4;
}

// Emulates L-Type Instructions
void emu_l_type(rv_state *rsp, uint32_t iw) {
  uint32_t rd = get_bits(iw, 7, 5);
  uint32_t rs1 = get_bits(iw, 15, 5);
  uint32_t funct3 = get_bits(iw, 12, 3);

  uint32_t offset = get_bits(iw, 20, 12);
  int64_t signed_offset = sign_extend(offset, 12);
  int64_t target = rsp->regs[rs1] + signed_offset;

  switch (funct3) {
  case 0b000:
    uint8_t byte = *(uint8_t *)target;
    rsp->regs[rd] = byte;
    break;
  case 0b010:
    uint32_t word = *(uint32_t *)target;
    rsp->regs[rd] = word;
    break;
  case 0b11:
    uint64_t d = *(uint64_t *)target;
    rsp->regs[rd] = d;
    break;
  default:
    unsupported("S-Type funct3", funct3);
  }

  rsp->analysis.ld_count += 1;
  rsp->pc += 4;
}

static void rv_one(rv_state *state) {
  uint32_t iw = cache_lookup(&state->i_cache, (uint64_t)state->pc);

  uint32_t opcode = get_bits(iw, 0, 7);

#if DEBUG
  printf("iw: %x\n", iw);
//    printf("RA: %x\n", (int64_t)state->regs[RV_RA]);
//    printf("A0: %ld\n", state->regs[10]);
#endif

  switch (opcode) {
  case 0b0110011:
    // R-Type instructions have two register operands
    emu_r_type(state, iw);
    break;
  case 0b0010011:
    // I-type instructions
    emu_i_type(state, iw);
    break;
  case 0b1100011:
    // SB-type instructions
    emu_sb_type(state, iw);
    break;
  case 0b0000011:
    emu_l_type(state, iw);
    break;
  case 0b0100011:
    emu_s_type(state, iw);
    break;
  case 0b1101111:
    emu_jal(state, iw);
    break;
  case 0b1100111:
    // JALR (aka RET) is a variant of I-type instructions
    emu_jalr(state, iw);
    break;
  default:
    unsupported("Unknown opcode: ", opcode);
  }
  state->analysis.i_count += 1;
}

void rv_init(rv_state *state, uint32_t *target, uint64_t a0, uint64_t a1,
             uint64_t a2, uint64_t a3) {
  state->pc = (uint64_t)target;
  state->regs[RV_A0] = a0;
  state->regs[RV_A1] = a1;
  state->regs[RV_A2] = a2;
  state->regs[RV_A3] = a3;

  state->regs[RV_ZERO] = 0; // zero is always 0  (:
  state->regs[RV_RA] = RV_STOP;
  state->regs[RV_SP] = (uint64_t)&state->stack[STACK_SIZE];

  memset(&state->analysis, 0, sizeof(rv_analysis));
  cache_init(&state->i_cache);
}

uint64_t rv_emulate(rv_state *state) {
  while (state->pc != RV_STOP) {
    rv_one(state);
  }
  return state->regs[RV_A0];
}

static void print_pct(char *fmt, int numer, int denom) {
  double pct = 0.0;

  if (denom)
    pct = (double)numer / (double)denom * 100.0;
  printf(fmt, numer, pct);
}

void rv_print(rv_analysis *a) {
  int b_total = a->b_taken + a->b_not_taken;

  printf("=== Analysis\n");
  print_pct("Instructions Executed  = %d\n", a->i_count, a->i_count);
  print_pct("R-type + I-type        = %d (%.2f%%)\n", a->ir_count, a->i_count);
  print_pct("Loads                  = %d (%.2f%%)\n", a->ld_count, a->i_count);
  print_pct("Stores                 = %d (%.2f%%)\n", a->st_count, a->i_count);
  print_pct("Jumps/JAL/JALR         = %d (%.2f%%)\n", a->j_count, a->i_count);
  print_pct("Conditional branches   = %d (%.2f%%)\n", b_total, a->i_count);
  print_pct("  Branches taken       = %d (%.2f%%)\n", a->b_taken, b_total);
  print_pct("  Branches not taken   = %d (%.2f%%)\n", a->b_not_taken, b_total);
}
