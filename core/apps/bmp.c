#include "bmp.h"
#include <sys/syscall.h>


// meta[0] 1 represents bmp file
// meta[1] image width
// meta[2] image height
// meta[3] image size

int bmp_meta(int fd, int *meta)
{
    bmp_fileheader_t bmp_file_header;
    bmp_infoheader_t bmp_info_header;
    /* char* filename = "/b.bmp"; */
    /* int fd = open(filename, O_RDONLY); */
    /* if (fd == -1) { */
    /*     return 0; */
    /* } */
    //Read bmp file header
    read(fd, &bmp_file_header.bfType, sizeof(bmp_file_header.bfType));
    read(fd, &bmp_file_header.bfSize, sizeof(bmp_file_header.bfSize));
    read(fd, &bmp_file_header.bfReserved1, sizeof(bmp_file_header.bfReserved1));
    read(fd, &bmp_file_header.bfReserved2, sizeof(bmp_file_header.bfReserved2));
    read(fd, &bmp_file_header.bfOffBits, sizeof(bmp_file_header.bfOffBits));

    //Read bmp info header
    read(fd, &bmp_info_header.biSize, sizeof(bmp_info_header.biSize));
    read(fd, &bmp_info_header.biWidth, sizeof(bmp_info_header.biWidth));
    read(fd, &bmp_info_header.biHeight, sizeof(bmp_info_header.biHeight));
    read(fd, &bmp_info_header.biPlanes, sizeof(bmp_info_header.biPlanes));
    read(fd, &bmp_info_header.biBitCount, sizeof(bmp_info_header.biBitCount));
    read(fd, &bmp_info_header.biCompression, sizeof(bmp_info_header.biCompression));
    read(fd, &bmp_info_header.biSizeImage, sizeof(bmp_info_header.biSizeImage));
    read(fd, &bmp_info_header.biXPelsPerMeter, sizeof(bmp_info_header.biXPelsPerMeter));
    read(fd, &bmp_info_header.biYPelsPerMeter, sizeof(bmp_info_header.biYPelsPerMeter));
    read(fd, &bmp_info_header.biClrUsed, sizeof(bmp_info_header.biClrUsed));
    read(fd, &bmp_info_header.biClrImportant, sizeof(bmp_info_header.biClrImportant));
    read(fd, &bmp_info_header.biRedChlBitmask, sizeof(bmp_info_header.biRedChlBitmask));
    read(fd, &bmp_info_header.biGreenChlBitmask, sizeof(bmp_info_header.biGreenChlBitmask));
    read(fd, &bmp_info_header.biBlueChlBitmask, sizeof(bmp_info_header.biBlueChlBitmask));
    read(fd, &bmp_info_header.biAlphaChlBitmask, sizeof(bmp_info_header.biAlphaChlBitmask));
    read(fd, &bmp_info_header.biClrSpaceType, sizeof(bmp_info_header.biClrSpaceType));
    read(fd, &bmp_info_header.biClrSpaceEndpos, sizeof(bmp_info_header.biClrSpaceEndpos));
    read(fd, &bmp_info_header.biGammaRedChl, sizeof(bmp_info_header.biGammaRedChl));
    read(fd, &bmp_info_header.biGammaGreenChl, sizeof(bmp_info_header.biGammaGreenChl));
    read(fd, &bmp_info_header.biGammaBlueChl, sizeof(bmp_info_header.biGammaBlueChl));
    read(fd, &bmp_info_header.biIntent, sizeof(bmp_info_header.biIntent));
    read(fd, &bmp_info_header.biICCProfileData, sizeof(bmp_info_header.biICCProfileData));
    read(fd, &bmp_info_header.biICCProfileSz, sizeof(bmp_info_header.biICCProfileSz));
    read(fd, &bmp_info_header.biReserved, sizeof(bmp_info_header.biReserved));

    int width = bmp_info_header.biWidth;
    int height = bmp_info_header.biHeight;
    if (height < 0){
        height = ~height;
    }
    int depth = bmp_info_header.biBitCount/8;

    int img_offset = bmp_file_header.bfOffBits;

    int biimagesz = bmp_info_header.biSizeImage; // maybe the end of image
    int imagesz = biimagesz - bmp_file_header.bfOffBits;

    /* if(bmp_file_header.bfType[0] == 'B' &&  */
    /*    bmp_file_header.bfType[1] == 'M') */
    meta[0] = 1;
    meta[1] = imagesz;
    meta[2] = img_offset;
    meta[3] = width;
    meta[4] = height;
    /* char* image = malloc(imagesz); */
    /* lseek(fd, img_offset, SEEK_SET); */
    /* read(fd, image, imagesz); */

    return 1;
}

int read_bmp(int fd, char *image, int *meta)
{
    int imagesz = meta[1];
    int img_offset = meta[2];

    lseek(fd, img_offset, SEEK_SET);
    read(fd, image, imagesz);

    return 0;
}
