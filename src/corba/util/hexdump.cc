#include <cstdio>

using namespace std;

void _hexdump(const unsigned char *buffer, std::size_t nbytes) {
    size_t pos = 0;
    while (pos < nbytes) {
        printf("%04zx ", pos);
        for (int x = 0; x < 16; x++) {
            if (pos < nbytes)
                printf("%02x ", (unsigned)buffer[pos]);
            else
                printf("   ");
            pos++;
        }
        pos -= 16;
        for (int x = 0; x < 16; x++) {
            if (pos < nbytes)
                printf("%c", buffer[pos] >= 32 && buffer[pos] <= 127 ? buffer[pos] : '.');
            else
                printf(" ");
            pos++;
        }
        printf("\n");
    }
}