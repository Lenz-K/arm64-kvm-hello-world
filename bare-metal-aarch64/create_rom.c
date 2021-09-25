#include <string>
#include <cstring>

#define BEGINNING "uint32_t rom_code[] = {"
#define MIDDLE "};\nuint32_t ram_code[] = {"
#define END "};"

int main() {
    printf("Loading ROM\n");
    FILE *fp = fopen("rom.dat", "rb");
    if (fp == NULL) {
        printf("Could not open file: %s\n", strerror(errno));
        return -1;
    }
    fseek(fp, 0L, SEEK_END);
    long n_instructions = ftell(fp) / 4; // One instruction is 4 Bytes long
    rewind(fp);

    FILE *mem_fp = fopen("memory.h", "w+");
    if (mem_fp == NULL) {
        printf("Could not open file: %s\n", strerror(errno));
        return -1;
    }

    fprintf(mem_fp, BEGINNING);
    uint8_t instruction[4];
    uint32_t rom_code[n_instructions];
    // Read one instruction after another
    for (int i = 0; i < n_instructions; i++) {
        fread(instruction, sizeof(instruction[0]), 4, fp);
        rom_code[i] = instruction[3]<<3*8 | instruction[2]<<2*8 | instruction[1]<<8 | instruction[0]; // We need to reorder the Bytes
        fprintf(mem_fp, "0x%08X, ", rom_code[i]);
    }
    fclose(fp);
    fprintf(mem_fp, MIDDLE);

    printf("Loading RAM\n");
    fp = fopen("ram.dat", "rb");
    if (fp == NULL) {
        printf("Could not open file: %s\n", strerror(errno));
        return -1;
    }
    fseek(fp, 0L, SEEK_END);
    n_instructions = ftell(fp) / 4;
    rewind(fp);

    uint32_t ram_code[n_instructions];
    for (int i = 0; i < n_instructions; i++) {
        fread(instruction, sizeof(instruction[0]), 4, fp);
        ram_code[i] = instruction[3]<<3*8 | instruction[2]<<2*8 | instruction[1]<<8 | instruction[0];
        fprintf(mem_fp, "0x%08X, ", ram_code[i]);
    }
    fclose(fp);
    fprintf(mem_fp, END);
    fclose(mem_fp);
    return 0;
}
