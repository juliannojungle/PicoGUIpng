extern "C" {
    #include "ff.h" // FS functions and declarations.
    #include <setjmp.h>
    #include <png.h>
    #define PNG_READ_SUPPORTED

    #include "DEV_Config.c"
    #include "LCD_1in28.c"
    #include "font20.c"
    #include "GUI_Paint.h"
}

typedef struct {
    FIL *file;
} custom_file;

void custom_read_data(png_structrp png_ptr, png_bytep data, size_t length) {
#if _DEBUG
    printf("Custom read data...\n");
#endif
    UINT bytesRead;
    custom_file *filep = (custom_file*)png_get_io_ptr(png_ptr);
    f_read(filep->file, data, length, &bytesRead);
}

static void error(png_structp png_ptr, const char *message)
{
    printf("Error from libpng: %s\n", message);
}

void DisplayPng(FIL &file) {
#if _DEBUG
    printf("Creating read structure...\n");
#endif
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, error, NULL);

    if (png_ptr == NULL) {
        printf("png_create_read_struct error\n");
        return;
    }

#if _DEBUG
    printf("Allocating memory for image information...\n");
#endif
    png_infop info_ptr = png_create_info_struct(png_ptr);

    if (info_ptr == NULL) {
        printf("png_create_info_struct error\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return;
    }

#if _DEBUG
    printf("Setting up the custom read function...\n");
#endif
    custom_file filep;
    filep.file = &file;
    png_set_read_fn(png_ptr, &filep, custom_read_data);

#if _DEBUG
    printf("Setting up LongJump...\n");
#endif

    if (setjmp(png_jmpbuf(png_ptr)) == 0) {
#if _DEBUG
        printf("LongJump set...\n");
#endif
    } else {
        printf("We got a LongJump, destroying read struct...\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return;
    }

    // The call to png_read_info() gives us all of the information from the
    // PNG file before the first IDAT (image data chunk). REQUIRED.
#if _DEBUG
    printf("Reading info...\n");
#endif
    png_read_info(png_ptr, info_ptr);

#if _DEBUG
    printf("Parsing image info...\n");
#endif
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, &interlace_type, NULL, NULL);
#if _DEBUG
    printf("PNG info: width: %d, height: %d, bit_depth: %d\n", width, height, bit_depth);

    printf("Initialize display...\n");
#endif
    if (DEV_Module_Init() != 0) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return;
    }

    /* LCD Init */
    LCD_1IN28_Init(HORIZONTAL);
    LCD_1IN28_Clear(BLACK);

    png_bytep row_pointers(NULL);
    int col, row;
    int maxCol(width > LCD_1IN28.WIDTH ? LCD_1IN28.WIDTH : width);     // won't print outside display.
    int maxRow(height > LCD_1IN28.HEIGHT ? LCD_1IN28.HEIGHT : height); // won't print outside display.

    // ####### LCD_1IN28_Display #######
    LCD_1IN28_SetWindows(0, 0, maxCol, maxRow);
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_PIN, 0);

    int num_palette = 0;
    png_colorp palette = NULL;

    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);
    }

    for (row = 0; row < maxRow; row++) {
        row_pointers = (png_bytep)png_malloc(png_ptr, png_get_rowbytes(png_ptr, info_ptr));
        png_read_rows(png_ptr, &row_pointers, NULL, 1);

        for (col = 0; col < maxCol; col++) {
            png_byte red, green, blue;

            if ((color_type == PNG_COLOR_TYPE_PALETTE) && (palette != NULL)) {
                red = palette[row_pointers[col]].red;
                green = palette[row_pointers[col]].green;
                blue = palette[row_pointers[col]].blue;
            } else {
                // if the image is paletted but we don't have a palette, display as grayscale using palette index.
                png_bytep pixel = &row_pointers[col];
                red = *(pixel++);
                green = *(pixel++);
                blue = *(pixel++);
            }

            /* The LCD uses RGB565 16-bits format: RRRRRGGG GGGBBBBB */
            DEV_SPI_WriteByte((red & 0b11111000) | ((green & 0b11100000) >> 5));
            DEV_SPI_WriteByte(((green & 0b00011100) << 3) | ((blue & 0b11111000) >> 3));
        }

        free(row_pointers);
        row_pointers = NULL;
    }

    DEV_Digital_Write(EPD_CS_PIN, 1);
    LCD_1IN28_SendCommand(0x29);
    // ####### LCD_1IN28_Display #######

    /* Turn backlight on */
#if _DEBUG
    printf("Turning on backlight...\n");
#endif
    EPD_BL_PIN = 25;
    DEV_GPIO_Mode(EPD_BL_PIN, GPIO_OUT);
    DEV_Digital_Write(EPD_CS_PIN, 1);
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_BL_PIN, 1);

#if _DEBUG
    printf("DEV_Module_Exit...\n");
#endif
    DEV_Module_Exit();

#if _DEBUG
    printf("Done! Destroying read struct...\n");
#endif
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
}