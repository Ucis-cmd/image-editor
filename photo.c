#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <math.h>

#define GRID_START_3x3 1, 1 //For grid traversal in 3x3 grid of each pixel
#define GRID_START_1x1 0, 0 //For regular traversal of each pixel

#pragma pack(push, 1)
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;

typedef struct{
  WORD  bfType;
  DWORD bfSize;
  WORD  bfReserved1;
  WORD  bfReserved2;
  DWORD bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    DWORD bV5Size;               // The size of this header (124 bytes)
    LONG  bV5Width;              // The width of the bitmap in pixels
    LONG  bV5Height;             // The height of the bitmap in pixels
    WORD  bV5Planes;             // The number of planes (must be 1)
    WORD  bV5BitCount;           // The number of bits per pixel
    DWORD bV5Compression;        // The type of compression (0 = BI_RGB, no compression)
    DWORD bV5SizeImage;          // The size of the image in bytes (can be 0 if no compression)
    LONG  bV5XPelsPerMeter;      // Horizontal resolution in pixels per meter
    LONG  bV5YPelsPerMeter;      // Vertical resolution in pixels per meter
    DWORD bV5ClrUsed;            // The number of colors used in the bitmap (0 = all colors)
    DWORD bV5ClrImportant;       // The number of important colors (0 = all colors)
    DWORD bV5RedMask;            // Mask for red channel (used for bitfields compression)
    DWORD bV5GreenMask;          // Mask for green channel (used for bitfields compression)
    DWORD bV5BlueMask;           // Mask for blue channel (used for bitfields compression)
    DWORD bV5AlphaMask;          // Mask for alpha channel (used for bitfields compression)
    DWORD bV5CSType;             // Color space type (e.g., LCS_sRGB)
    struct {
        LONG ciexyzX;
        LONG ciexyzY;
        LONG ciexyzZ;
    } bV5Endpoints[3];           // CIEXYZTRIPLE endpoints (red, green, blue color space endpoints)
    DWORD bV5GammaRed;           // Gamma red correction
    DWORD bV5GammaGreen;         // Gamma green correction
    DWORD bV5GammaBlue;          // Gamma blue correction
    DWORD bV5Intent;             // Rendering intent (e.g., LCS_GM_BUSINESS)
    DWORD bV5ProfileData;        // Profile data offset
    DWORD bV5ProfileSize;        // Size of embedded ICC profile
    DWORD bV5Reserved;           // Reserved, must be 0
} BITMAPV5HEADER;

typedef struct __attribute__((packed)) Pixel {
	uint8_t blue;
	uint8_t green;
	uint8_t red;
} Pixel;

typedef struct BMPImage {
    BITMAPFILEHEADER bitmapfileheader;
	BITMAPV5HEADER bitmapv5header;
	Pixel ** pixels;
} BMPImage;

typedef struct{
    int i;
    int j;
} PixelPosition;

typedef struct {
    int blue_sum;
    int green_sum;
    int red_sum;
} ColorSums;

typedef struct {
    int blue_avg;
    int green_avg;
    int red_avg;
} AvgColor;

typedef struct {
    int gx;
    int gy;
} EdgeValues;

typedef struct{
    PixelPosition pixel_position;
    AvgColor avg_color;
} BlurData; 

typedef void (*PixelOperation)(Pixel* pixel, void* data);
typedef void (*ImageOperation)(BMPImage *ptr, PixelPosition* pixel_position);
#pragma pack(pop)

void process_image_grid(BMPImage* ptr, int start_i, int start_j, ImageOperation operation){
    int width = ptr->bitmapv5header.bV5Width;
    int height = ptr->bitmapv5header.bV5Height;

    for (int i = start_i; i < height - start_i; i++) {
        for (int j = start_j; j < width - start_j; j++) {
            PixelPosition pixel_position;
            pixel_position.i = i;
            pixel_position.j = j;
            operation(ptr, &pixel_position);
        }
    }
}

void process_3x3_grid(BMPImage* ptr, int center_i, int center_j, PixelOperation operation, void* data) {
    int height = ptr->bitmapv5header.bV5Height;
    int width = ptr->bitmapv5header.bV5Width;

    for (int i = center_i - 1; i <= center_i + 1; i++) {
        for (int j = center_j - 1; j <= center_j + 1; j++) {
            if (i >= 0 && i < height && j >= 0 && j < width) {
                operation(&ptr->pixels[i][j], data);
            }
        }
    }
}

BMPImage *read_image(char* file_name){
    BMPImage *img = malloc(sizeof(BMPImage));
    BITMAPFILEHEADER *bitmapheader = &img->bitmapfileheader;
    FILE *input_ptr = fopen("sample1.bmp", "rb");
    if(input_ptr == NULL){
        perror("error");
    }
    fread(bitmapheader, sizeof(*bitmapheader), 1, input_ptr);

    BITMAPV5HEADER *bitv5header = &img->bitmapv5header;
    fread(bitv5header, sizeof(*bitv5header), 1, input_ptr);

    int height = bitv5header->bV5Height;
    int width = bitv5header->bV5Width;

    img->pixels = malloc(sizeof(Pixel *) * height); //Allocate memory for columns
    if (img->pixels == NULL) {
        perror("Error allocating memory for pixel rows");
        fclose(input_ptr);
        free(img);
        return NULL;
    }

    for(int i=0; i< height; i++){ //Allocate memory for rows
        img->pixels[i] = malloc(sizeof(Pixel) * width);

        if(img->pixels[i] == NULL){
             perror("Error allocating memory for pixel columns");
            // Free previously allocated rows
            for (int j = 0; j < i; j++) {
                free(img->pixels[j]);
            }
            free(img->pixels);
            fclose(input_ptr);
            free(img);
            return NULL;
        }
    }

    int row_size = ((width * sizeof(Pixel) + 3) / 4) * 4;
    uint8_t *row_buffer = malloc(row_size);
    
    for(int i=0; i < height; i++){
        fread(row_buffer, row_size, 1, input_ptr);
        memcpy(img->pixels[i], row_buffer, width * sizeof(Pixel));
    } 
    
    free(row_buffer);
    fclose(input_ptr);
    return img;
}

void write_image(BMPImage *ptr) {
    FILE *output_ptr = fopen("output.bmp", "wb");
    if (output_ptr == NULL) {
        perror("Error opening output file");
        return;
    }

    // Calculate total size needed for headers and pixel data
    size_t header_size = sizeof(ptr->bitmapfileheader) + sizeof(ptr->bitmapv5header);
    size_t pixel_data_size = ptr->bitmapv5header.bV5Width * ptr->bitmapv5header.bV5Height * sizeof(Pixel);
    size_t total_size = header_size + pixel_data_size;

    // Allocate memory for the combined data
    uint8_t *buffer = malloc(total_size);
    if (buffer == NULL) {
        perror("Error allocating memory for buffer");
        fclose(output_ptr);
        return;
    }

    // Copy headers into the buffer
    memcpy(buffer, &ptr->bitmapfileheader, sizeof(ptr->bitmapfileheader));
    memcpy(buffer + sizeof(ptr->bitmapfileheader), &ptr->bitmapv5header, sizeof(ptr->bitmapv5header));

    int width = ptr->bitmapv5header.bV5Width;
    int height = ptr->bitmapv5header.bV5Height;
    int row_size = ((width * sizeof(Pixel) + 3) / 4) * 4;

    // Copy pixel data into the buffer
    for (int i = 0; i < height; ++i) { 
        memcpy(buffer + header_size + i * row_size, ptr->pixels[i], width * sizeof(Pixel));
        memset(buffer + header_size + i * row_size + width * sizeof(Pixel), 0, row_size - width * sizeof(Pixel)); // Add padding
    }

    // Write the combined data to the file
    fwrite(buffer, total_size, 1, output_ptr);

    // Clean up
    free(buffer);
    fclose(output_ptr);
}

void sum_colors_pixel_operation(Pixel* pixel, void* data) {
    ColorSums* sums = (ColorSums*)data;
    sums->blue_sum += pixel->blue;
    sums->green_sum += pixel->green;
    sums->red_sum += pixel->red;
}

void calculate_avg_color(BMPImage* ptr, PixelPosition* pixel_position, AvgColor* avg_color) {
    ColorSums sums = {0, 0, 0};
    process_3x3_grid(ptr, pixel_position->i, pixel_position->j, sum_colors_pixel_operation, &sums);
    
    avg_color->blue_avg = sums.blue_sum / 9;
    avg_color->green_avg = sums.green_sum / 9;
    avg_color->red_avg = sums.red_sum / 9;
}

void apply_blur_pixel_operation(Pixel* pixel, void* data) {
    AvgColor* avg = (AvgColor*)data;
    pixel->blue = avg->blue_avg;
    pixel->green = avg->green_avg;
    pixel->red = avg->red_avg;
}

void blur_image_operation(BMPImage* ptr, PixelPosition* pixel_position) {
    AvgColor avg_color;
    calculate_avg_color(ptr, pixel_position, &avg_color);
    process_3x3_grid(ptr, pixel_position->i, pixel_position->j, apply_blur_pixel_operation, &avg_color);
}

void detect_edges_image_operation(BMPImage* ptr, PixelPosition* pixel_position){
    EdgeValues edgevalues;
    // process_3x3_grid(); 
    // process_3x3_grid();
}

void grayscale_image(BMPImage *ptr, PixelPosition* pixel_position){
    int i = pixel_position->i;
    int j = pixel_position->j;
    int avg_pixel_color = (ptr->pixels[i][j].blue + ptr->pixels[i][j].green + ptr->pixels[i][j].red) / 3;
    ptr->pixels[i][j].blue = avg_pixel_color;
    ptr->pixels[i][j].green = avg_pixel_color;
    ptr->pixels[i][j].red = avg_pixel_color;
}

void mirror_image(BMPImage* ptr){
    int width = ptr->bitmapv5header.bV5Width; 
    int height = ptr->bitmapv5header.bV5Height;
    for(int i = 0; i < height; i++){
        for(int j = 0; j < width/2; j++){
            Pixel temp = ptr->pixels[i][j];              
            ptr->pixels[i][j] = ptr->pixels[i][width-j-1];             
            ptr->pixels[i][width-j-1] = temp;
        }
    }
}

void manipulate_image(BMPImage *ptr){
    int image_mode = 0;
    if(image_mode == 0){
        process_image_grid(ptr, GRID_START_3x3, blur_image_operation);
    }
    else if(image_mode == 1){
        process_image_grid(ptr, GRID_START_3x3, detect_edges_image_operation);
    }
    else if(image_mode == 2){
        mirror_image(ptr);
    }
    else if(image_mode == 3){
        process_image_grid(ptr, GRID_START_1x1, grayscale_image);
    }
}

int main(int argc, char* argv[]){
    BMPImage *ptr = read_image("sample1.bmp");
    manipulate_image(ptr);
    if (ptr != NULL) {
        write_image(ptr);

        // Free the allocated memory
        for (int i = 0; i < ptr->bitmapv5header.bV5Height; i++) {
            free(ptr->pixels[i]);
        }
        free(ptr->pixels);
        free(ptr);
    }
    return 0;
}

