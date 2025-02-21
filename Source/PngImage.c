#include <stdio.h>
#include <stdlib.h>

#include "PngImage.h"

#define GL_RGB 0x1907
#define GL_RGBA 0x1908

int savePngImage(const char *filename, uint32_t w, uint32_t h, uint8_t *canvas, char *title) {
  FILE *fp = NULL;
  png_structp png_ptr = NULL;
  png_infop info_ptr = NULL;
  png_bytep row = NULL;

  int ret = 1;

  char realfilename[256];
  strcpy(realfilename, filename);
  strcat(realfilename, ".png");

  fp = fopen(realfilename, "wb");

  if (fp == NULL) goto finito;

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_ptr) goto finito;

  info_ptr = png_create_info_struct(png_ptr);

  if (!info_ptr) goto finito;

  if (setjmp(png_jmpbuf(png_ptr))) goto finito;

  png_init_io(png_ptr, fp);

  png_set_IHDR(png_ptr, info_ptr, w, h, 8,
          PNG_COLOR_TYPE_RGB,
          PNG_INTERLACE_NONE,
          PNG_COMPRESSION_TYPE_DEFAULT,
          PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_ptr, info_ptr);

  if (title != NULL && strlen(title)) {
    png_text title_text;
    title_text.compression = PNG_TEXT_COMPRESSION_NONE;
    char titleKey[256];
    strcpy(titleKey, filename);
    strcpy(title_text.key, filename);
    strcpy(title_text.text, title);
    png_set_text(png_ptr, info_ptr, &title_text, 1);
  }

  for (uint32_t i = 1; i <= h; ++i) {
    row = &canvas[3 * ((h - i) * w)];
    png_write_row(png_ptr, row);
  }

  png_write_end(png_ptr, info_ptr);

  ret = 0;

  finito:
  if (fp != NULL) fclose(fp);
  if (info_ptr != NULL) png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
  if (png_ptr != NULL) png_destroy_write_struct(&png_ptr, &info_ptr);

  return ret;
}

void loadPngImage(const char *filename, PngImg* img) {
  memset(img, 0x0, sizeof(PngImg));
  FILE *fp = fopen(filename, "rb");

  if (!fp) {
    return;
  }

  unsigned char sig[8];

  fread(&sig[0], 1, 8, fp);

  if (!png_check_sig(sig,8)) {
    return;
  }

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png) {
    return;
  }

  png_infop info = png_create_info_struct(png);

  if (info == NULL) {
    png_destroy_read_struct(&png, NULL, NULL);
    fclose(fp);
    return;
  }

  if(setjmp(png_jmpbuf(png))) {
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
    return;
  }

  png_init_io(png, fp);
  png_set_sig_bytes(png, sizeof(sig));
  png_read_info(png, info);

  img->w = png_get_image_width(png, info);
  img->h = png_get_image_height(png, info);

  if (png_get_bit_depth(png, info) < 8) {
    png_set_packing(png);
  }

  if (png_get_valid(png, info, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(png);
  }

  switch (png_get_color_type(png, info)) {
    case PNG_COLOR_TYPE_GRAY:
      img->glImgFormat = GL_RGB;
      break;

    case PNG_COLOR_TYPE_GRAY_ALPHA:
      img->glImgFormat = GL_RGBA;
      png_set_gray_to_rgb(png);
      break;

    case PNG_COLOR_TYPE_PALETTE:
      img->glImgFormat = GL_RGB;
      png_set_expand(png);
      break;

    case PNG_COLOR_TYPE_RGB:
      img->glImgFormat = GL_RGB;
      break;

    case PNG_COLOR_TYPE_RGBA:
      img->glImgFormat = GL_RGBA;
      break;

    default:
      img->glImgFormat = 0;
  }

  uint32_t bytes_per_pixel = (uint32_t) (png_get_rowbytes(png, info)) / img->w;

  png_set_interlace_handling(png);
  png_read_update_info(png, info);

  img->size = img->w * img->h * bytes_per_pixel;
  img->img = malloc(sizeof(uint8_t) * (img->w * img->h * bytes_per_pixel));

  png_bytep *rows = malloc(sizeof(png_bytep) * img->h);
  uint8_t *p = img->img;

  for (uint32_t i = img->h - 1; i > 0; --i) {
    rows[i] = p;
    p += img->w * bytes_per_pixel;
  }

  rows[0] = p;

  png_read_image(png, rows);

  fclose(fp);

  png_destroy_read_struct(&png, &info, NULL);
  free(rows);
}

void deallocPngImage(PngImg* img) {
  if (img->img != NULL) {
    free(img->img);
  }
  memset(img, 0x0, sizeof(PngImg));
}
