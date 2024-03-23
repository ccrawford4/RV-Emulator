# Risc-V Emulator

This is a highly specialized emulator built in C, designed to emulate Risc-V instructions within a Risc-V environment itself.

## Features

- **Direct Mapped and Set Associative Cache**: Implemented in `cache.c`, the emulator enhances instruction word lookup efficiency by incorporating both direct mapped and set associative cache mechanisms. This feature significantly reduces latency and improves the execution speed of the emulated instructions.

- **Bitwise Manipulation for Instruction Extraction**: The core functionality, found in `rv_emu.c`, utilizes bitwise manipulation techniques to extract information from Risc-V assembly instruction words (presented in machine code). This extraction is the first step in translating these instructions into equivalent operations in C, enabling the emulation of Risc-V instructions with high fidelity.

- **Execution in Risc-V Environment**: Uniquely, this emulator is designed to run in a Risc-V environment, providing an authentic context for emulation. This peculiarity ensures that users can test and debug Risc-V code on a platform that closely mirrors the target environment.
