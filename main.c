#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "commands.h"
#include "buffer.h"
#include "defs.h"

int main(void)
{
    srand((unsigned int)time(NULL));

    SystemState sys;
    sys.status_reg = 0x00;
    sys.tick       = 0;

    /* Dynamic allocation: the circular log buffer lives on the heap */
    if (cb_init(&sys.log, BUFFER_SIZE) != 0) {
        fprintf(stderr, "FATAL: could not allocate log buffer\n");
        return 1;
    }

    printf("==============================================\n");
    printf("   Micro-Kernel Environmental Simulator\n");
#ifdef DEBUG
    printf("   Build : DEBUG\n");
#else
    printf("   Build : PRODUCTION\n");
#endif
    printf("   Log   : %d slots  (%zu bytes on heap)\n",
           BUFFER_SIZE, (size_t)BUFFER_SIZE * sizeof(PacketView));
    printf("==============================================\n");
    printf("Type 'help' for a list of commands.\n\n");

    char line[MAX_CMD_LEN];
    while (1) {
        printf("sim> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        line[strcspn(line, "\n")] = '\0';   /* strip newline */

        if (!dispatch(&sys, line))
            break;
    }

    /* Release the heap-allocated log buffer */
    cb_free(&sys.log);
    printf("Simulator shutdown. Memory freed.\n");
    return 0;
}
