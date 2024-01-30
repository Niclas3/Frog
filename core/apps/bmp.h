typedef unsigned short int WORD;
typedef unsigned long int DWORD;
typedef signed long int LONG;
typedef unsigned char BYTE;
typedef long int UINT;

/*https://upload.wikimedia.org/wikipedia/commons/7/75/BMPfileFormat.svg*/

typedef struct tagBITMAPFILEHEADER{
  WORD   bfType;           /* "BM" "BA" "CI" "CP" "IC" "PT" */
  DWORD  bfSize;           /* Size of file in bytes */
  WORD   bfReserved1;      /* set to 0 */
  WORD   bfReserved2;      /* set to 0 */
  DWORD  bfOffBits;        /* Byte offset to actual bitmap data (= 54 if RGB) */
}bmp_fileheader_t;

typedef struct tagBITMAPINFOHEADER{
  DWORD  biSize;           /* Size of BITMAPINFOHEADER, in bytes (= 40) */   
  DWORD  biWidth;          /* Width of image, in pixels */
  DWORD  biHeight;         /* Height of images, in pixels */
  WORD   biPlanes;         /* Number of planes in target device (set to 1) */
  WORD   biBitCount;       /* Bits per pixel */  
  DWORD  biCompression;    /* Type of compression (0 if no compression) */
  DWORD  biSizeImage;      /* Image size, in bytes (0 if no compression) */
  DWORD  biXPelsPerMeter;  /* Resolution in pixels/meter of display device */
  DWORD  biYPelsPerMeter;  /* Resolution in pixels/meter of display device */
  DWORD  biClrUsed;        /* Number of colors in the color table (if 0, use 
                              maximum allowed by biBitCount) */
  DWORD  biClrImportant;   /* Number of important colors.  If 0, all colors 
                              are important */
  DWORD  biRedChlBitmask;   /*red channel bitmask*/
  DWORD  biGreenChlBitmask; /*green channel bitmask*/
  DWORD  biBlueChlBitmask;  /*blue channel bitmask*/
  DWORD  biAlphaChlBitmask; /*alpha channel bitmask*/

  DWORD  biClrSpaceType;       /*Color space type*/
  DWORD  biClrSpaceEndpos;     /*Color space endpoints*/
  DWORD  biGammaRedChl;        /*Gamma for Red channel*/
  DWORD  biGammaGreenChl;      /*Gamma for Green channel*/
  DWORD  biGammaBlueChl;       /*Gamma for Blue channel*/

  DWORD  biIntent;
  DWORD  biICCProfileData;
  DWORD  biICCProfileSz;
  DWORD  biReserved;           /* set to 0 */

}bmp_infoheader_t;

int bmp_meta(int fd, int *meta);

int read_bmp(int fd, char *image, int *meta);
