# Linker
Linker is used to combine multiple files into a single executable by resolving symbol references. Here is a 2-pass implementation of the Linker program, where the first pass creates the symbole table, and the second pass corrects the address of each symbol based on the memory instruction.

## Running the Program
1. Compile using the MakeFile
2. ./linker [inpufile]

## Input file Format
- Contains modules that are represented by 3 lines
- First line defines the *definition list*, defined symbols
- Second line defines the *use list*, used symbols
- Third line defines the *program text*, the instructions