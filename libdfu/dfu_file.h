
#ifndef DFU_FILE_H
#define DFU_FILE_H

#include <stdint.h>

struct dfu_file {
    /* Pointer to file loaded into memory */
    uint8_t *firmware;
    /* Different sizes */
    size_t size;
};

// BOOL dfu_load_file(CString filename, struct dfu_file *file);
void *dfu_malloc(size_t size);

#endif /* DFU_FILE_H */
