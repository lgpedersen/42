#pragma once

#include <png.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

typedef struct {
  uint32_t w;
  uint32_t h;
  uint32_t glImgFormat;
  uint32_t size;
  uint8_t *img;
} PngImg;

#ifdef __cplusplus
extern "C" {
#endif
int savePngImage(const char *filename, uint32_t w, uint32_t h, uint8_t *canvas, char *title);
void loadPngImage(const char *filename, PngImg* img);
void deallocPngImage(PngImg* img);
#ifdef __cplusplus
}
#endif
