#include <string>
#include <cstring>

// The static parts of the generated file
#define BEGINNING "uint32_t rom_code[] = {"
#define MIDDLE "};\nuint32_t ram_code[] = {"
#define END "};"

// The file that will be generated
FILE *mem_fp;

FILE *open_file(const char *pathname, const char *mode) {
    FILE *fp = fopen(pathname, mode);
    if (fp == NULL) {
        printf("Could not open file: %s\n", strerror(errno));
        exit(-1);
    }
    return fp;
}

/**
 * Reads a binary file, reorders the instructions correctly and writes them to the file that is generated.
 */
void read_instructions(const char *pathname) {
    FILE *fp = open_file(pathname, "rb");

    fseek(fp, 0L, SEEK_END);
    long n_instructions = ftell(fp) / 4; // One instruction is 4 bytes long
    rewind(fp);

    uint8_t instruction[4];
    uint32_t reordered_instruction;
    // Read one instruction after another
    for (int i = 0; i < n_instructions; i++) {
        fread(instruction, sizeof(instruction[0]), 4, fp);
        // The bytes of the instructions need to be reordered
        reordered_instruction = instruction[3]<<3*8 | instruction[2]<<2*8 | instruction[1]<<8 | instruction[0];
        fprintf(mem_fp, "0x%08X, ", reordered_instruction);
    }

    fclose(fp);
}

/**
 * Reads the files rom.dat and ram.dat, reorders the instructions correctly
 * and then generates a header file memory.h that contains the instructions in an array.
 */
int main() {
    mem_fp = open_file("memory.h", "w+");
    fprintf(mem_fp, BEGINNING);

    printf("Loading ROM\n");
    read_instructions("rom.dat");

    fprintf(mem_fp, MIDDLE);

    printf("Loading RAM\n");
    read_instructions("ram.dat");

    fprintf(mem_fp, END);
    fclose(mem_fp);
    printf("Created memory.h\n");

    return 0;
}
