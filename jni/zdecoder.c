#include <dirent.h>
#include <stdio.h>
#include "ICCIMGKIT.h"
#include "parmacro.h"
#include "swdtypes.h"
#include "SDL_Types.h"
#include "list.h"
#include <stdbool.h>


#define LOAD_IMAGE_PATH "/data/images"
#define SAVE_IMAGE_PATH "/data/images"
#define SAVE_IMAGE_PATH2 "/data/dumps"

#define FINDER_BORDER_W_PCT				15	// 7.5% on each side
#define FINDER_BORDER_H_PCT				15	// 7.5% on top and bottom

/*************************************************************************
* Local Routines
*************************************************************************/
static void *ImageKitMalloc(UINT32_ICC nBytes);
static void ImageKitFree(void *ptr);
static HOST_RETURN_VALUES DecCallbackEx(const DEC_CALLBACK *pInfo, void *state);
static BOOL_ICC ImgFlexiExtCallback(UINT32_ICC nDecode);

DEC_RETURN_VALUES DEC_GetDecodeData(DEC_DECODE_DATA *pdecData, int iIndex);

static UINT32_ICC 	multi_decodes	= 0;
static byte_t imgBuffer[(1360 * 960) + 4096];
static bool_t b_PickListValueSetForSession = FALSE;
static bool_t isSwipeSession = FALSE;        /* whether or not we're in hands-free mode */
static DWORD s_dwLoad 		= 0;		//# successful DecLoadImageDataEx calls
static DWORD s_dwNoLoad		= 0;		//# of rejected

/*
 * Linked-list of sensor frames that have been queued into the ImageKit
 * and awaiting processing.
 */
typedef struct {
   list_head_t link;
   void *state;               /* acquisition buffer state variable */
   DEC_IMAGE_DATA imageInfo;  /* ImageKit sensor data, interface record */
} ImageRec;

struct node {
   char str[100];  // image file name
   long key;       // hImage
   int decodeStatus;  // 0->not decoded  1->Decoded
   struct node *next;
};

struct node *head = NULL;  //list of feeded images to IK
struct node *current = NULL;
static char imageFileName[100]; // name of the file inserted in to list
#pragma pack(push, 1)
typedef struct {
    WORD bfType;  //specifies the file type
    DWORD bfSize;  //specifies the size in bytes of the bitmap file
    WORD bfReserved1;  //reserved; must be 0
    WORD bfReserved2;  //reserved; must be 0
    DWORD offset;  //specifies the offset in bytes from the bitmapfileheader to the bitmap bits
} BMP_FILE_HEADER;
#pragma pack(pop)

static HOST_RETURN_VALUES DecCallback(const DEC_CALLBACK *pInfo)
{
  return DecCallbackEx(pInfo, 0);
}

static HOST_RETURN_VALUES DecCallbackEx(const DEC_CALLBACK *pInfo, void *state)
{
   printf("DecCallbackEx pInfo->DecEvent = %d\n", pInfo->DecEvent);
   return HOST_SUCCESS;
}

//-------------------------------------------------------
static BOOL_ICC ImgFlexiExtCallback(UINT32_ICC nDecode)
{
    BOOL_ICC bReturnValue = TRUE;
    printf("ImgFlexiExtCallback nDecode=%d\n", nDecode);

    multi_decodes = nDecode;
    return bReturnValue;
}

static void *ImageKitMalloc(UINT32_ICC nBytes)
{
    void* pv = malloc(nBytes);

    return pv;
}
/**
 * @brief ImageKit utility function for dynamic-memory release
 */
static void ImageKitFree(void *ptr)
{
    free(ptr);
}


static DEC_OPEN decOpenRec =
{
   { sizeof(DEC_OPEN), sizeof(DEC_OPEN) },		/* StructInfo */
   &DecCallback,								/* fnCallback */
   &ImageKitMalloc,
   &ImageKitFree,
   NULL,										/* KickDogCallback */
   NULL,										/* PutStringCallback */
   1,											/* ImageCount */

   /* filled in by the ImageKit on DecOpen */
   NULL, 0,										/* CriticalRam/SizeCriticalRam */

   /* extended callback information */
   &DecCallbackEx,								/* fnCallbackEx */
   NULL,										/* pUserArgEx */
   NULL,										// FlexiScript Callback
   &ImgFlexiExtCallback						// Flexi Extension Callback

};

struct node* find(long key) {

   //start from the first link
   struct node* current = head;

   //if list is empty
   if(head == NULL) {
      return NULL;
   }

   //navigate through list
   while(current->key != key) {

      //if it is last node
      if(current->next == NULL) {
         return NULL;
      } else {
         //go to next link
         current = current->next;
      }
   }

   //if data found, return the current Link
   return current;
}

//insert link at the first location, if exist-> update / else-> add
void insertFirst(int key, char *data1) {
    struct node *link = find(key);

    if(link == NULL){
    //create a link
      link = (struct node*) malloc(sizeof(struct node));
      link->key = key;
      strcpy(link->str,data1);
      link->decodeStatus = -1;

      //MTRACE(TRDBG, (_T("[INSERTED IMAGE NAME %s -> "),link->str));
      //point it to old first node
      link->next = head;

      //point first to new first node
      head = link;
    }else{
      strcpy(link->str,data1);
    }

}

//find a link with given key and update with the updatevalue
struct node* findAndPrintStat(int key) {

   //start from the first link
   struct node* current = head;

   //if list is empty
   if(head == NULL) {
      return NULL;
   }

   //navigate through list
   while(current->key != key) {

      //if it is last node
      if(current->next == NULL) {
         return NULL;
      } else {
         //go to next link
         current = current->next;
      }
   }

    if(current->decodeStatus == 1){
        DEC_DECODE_DATA decDataA;
        SDL_U8 *pBuffer = malloc(150);
        decDataA.StructInfo.Allocated = decDataA.StructInfo.Used = sizeof(decDataA);
        decDataA.DecodeBuffer = ((byte_t *) pBuffer) + 1;
        decDataA.BufferLength = 150-1;

        DEC_RETURN_VALUES	stat ;

        if ((stat = DEC_GetDecodeData(&decDataA, -1)) == DEC_SUCCESS)
        {
              char *data ;
              data = malloc(  decDataA.DataLength);
              for(int i = 1 ; i< decDataA.DataLength; i++){
                 data[i-1] = decDataA.DecodeBuffer[i];
              }
              printf("[IMAGE_STACK] %s -> DECODED --> Decode Type = %d  --> Decoded Data = %s decDataA.DataLength=%d", current->str,decDataA.DecodeBuffer[0], data, decDataA.DataLength );
        }

    }
    else{
        printf("[IMAGE_STACK] %s -> NOT DECODED", current->str);
   }
   return current;
}

//find a link with given key and update with the updatevalue
struct node* findAndUpdate(int key, int updateVal) {

   //start from the first link
   struct node* current = head;

   //if list is empty
   if(head == NULL) {
      return NULL;
   }

   //navigate through list
   while(current->key != key) {

      //if it is last node
      if(current->next == NULL) {
         return NULL;
      } else {
         //go to next link
         current = current->next;
      }
   }

   //if data found, return the current Link

   current->decodeStatus =updateVal;
   return current;
}

void  printImageStack() {
   struct node *ptr = head;

   //start from the beginning
   while(ptr != NULL) {
       if(ptr->decodeStatus == 1){
           printf("[IMAGE_STACK] s% -> DECODED", ptr->str);
       }
       else{
            printf("[IMAGE_STACK] s% -> NOT DECODED", ptr->str);
       }

//      printf("(%d,%d) ",ptr->key,ptr->data);
      ptr = ptr->next;
   }

}

void deleteList(struct node** head)
{
    struct node* prev = *head;

    while (*head)
    {
        *head = (*head)->next;
        free(prev);
        prev = *head;
    }
}

int loadBitMapFile(char *filename , unsigned char *image){

    FILE *filePtr;  //our file pointer
    BMP_FILE_HEADER bitmapFileHeader;

    //open file in read binary mode
    filePtr = fopen(filename,"rb");
    if (filePtr == NULL){
        printf("ERROR : Can not open %s \n", filename);
        return -1;
    }

    //read the bitmap file header
    fread(&bitmapFileHeader, 1,sizeof(BMP_FILE_HEADER),filePtr);

    //verify that this is a .BMP file by checking bitmap id
    if (bitmapFileHeader.bfType !=0x4D42)
    {
        printf("ERROR : Not a BMP  %s \n", filename);
        fclose(filePtr);
        return -1;
    }
    //move file pointer to the beginning of bitmap data
    fseek(filePtr, bitmapFileHeader.offset, SEEK_SET);

    //read in the bitmap image data
    fread(image, 1, 1360*800  , filePtr);

    if (image == NULL)
    {
        printf("ERROR : NULL IMAGE  \n");
        fclose(filePtr);
        return -1;
    }

    fclose(filePtr);
    return 1;
}
void countNumberOfFiles(int *file_count){
    DIR * dirp;
    struct dirent * entry1;

    dirp = opendir("data/images/");

    if(dirp != NULL){
        while ((entry1 = readdir(dirp)) != NULL) {
            if(entry1->d_type == DT_REG) { /* only deal with regular file */
                const char *ext = strrchr(entry1->d_name,'.');
                if((!ext) || (ext == entry1->d_name)){
                    // do nothing
                }
                else {
                    if(strcmp(ext, ".bmp") == 0){
                        *file_count = *file_count +1;
                    }

                }
            }
            //              if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")){
            //                continue;    /* skip self and parent */
            //              }
            //              *file_count = *file_count +1;
        }
    }
    closedir(dirp);
}

char ** getListOfImages(int *file_count){
    int count_insert = 0;
    DIR * dirp;
    struct dirent * entry2;
    static char **arr;

    arr =  malloc(*file_count* sizeof *arr);
    if(arr == NULL){ //check for allocation errors
        return 0;
    }
    //allocate memory for each individual char array
    for(int i = 0; i < *file_count; i++){
        arr[i] = malloc(100); //char size is always 1 byte

        if(arr == NULL){ //check for allocation errors
            return 0;
        }
    }
    dirp = opendir("data/images/");
    while ((entry2 = readdir(dirp)) != NULL) {
        if(entry2->d_type == DT_REG) { /* only deal with regular file */
            const char *ext = strrchr(entry2->d_name,'.');
            if((!ext) || (ext == entry2->d_name)){
                // do nothing
            }
            else {
                if(strcmp(ext, ".bmp") == 0){

                    strcpy(arr[count_insert],entry2->d_name);
                    printf("Inserted File Name=%s  into Index = %d\n  ",entry2->d_name, count_insert );
                    count_insert++;
                }

            }
        }
    }
    printf("Inserted File count=%d  \n",count_insert  );
    closedir(dirp);
    return  arr;
}

DEC_RETURN_VALUES DEC_GetDecodeData(DEC_DECODE_DATA *pdecData, int iIndex)
{
    DEC_RETURN_VALUES Result;
    //MTRACE(TRDBG, (_T("[DEC] GetDecodeData at %d"),iIndex));

    if ( (pdecData == NULL) || (iIndex >= MAX_SUPPORTED_BARCODES))
        return DEC_ERROR_NULLPTR;


    Result = DecGetDecodeData(pdecData);

    return Result;
}

static void FillDecImageData(DEC_IMAGE_DATA *pRec)
{
    printf("FillDecImageData + \n");
   memset(pRec, 0, sizeof(*pRec));

   pRec->StructInfo.Allocated = pRec->StructInfo.Used = sizeof(*pRec);

   pRec->image_row_pitch = 1360;
   pRec->Width        = 1360 - 80;
   pRec->Height       = 800;
   pRec->RowsAcquired = 800;
   pRec->ImageData    = (PIXEL_ICC*) malloc(sizeof(PIXEL_ICC) * 1360*800);
   pRec->PixelFormat  = PF_8BPP;
   pRec->UsedAim      = 0;
   pRec->UsedIllumination = 1;
   pRec->hImage		  = 0;
   pRec->MaxProcessingTime = PARAM_MAX_DECODE_TIME;


    static BOOL init = FALSE;
    static BOOL isCanproceed = FALSE;
    static int fileCount = 0;
    static int tempN = 0;
    static char **fileNameArray;

    static unsigned char *image;
    char filename[256] = {};
    int success = -1;

    if(init == FALSE)
    {
        countNumberOfFiles(&fileCount);
        if(fileCount > 0){
            isCanproceed = TRUE;

            fileNameArray = getListOfImages(&fileCount);
            printf("FileCount=%d",fileCount );
            tempN = fileCount;
            image = malloc(1360*800);
            init = TRUE;

            fileCount--;
        } else{
            isCanproceed = FALSE;
            init = FALSE;
            printf("IMAGE_STACK -> NO BMP FILES in /data/images");

        }

    }else{
        if(fileCount==0){
            fileCount=tempN;
        }
        fileCount--;
    }

    if(isCanproceed){
        sprintf(filename, "%s/%s", LOAD_IMAGE_PATH, fileNameArray[fileCount]);
        sprintf(imageFileName, "%s", fileNameArray[fileCount]);

        success =  loadBitMapFile(filename, image);

        if(success == -1){ // failed
            printf("Load Failed image fileNAme=%s", filename);
            free(image);
            image = NULL;
            init = FALSE;
            fileCount = 0;
            tempN = 0;

        }else{ // succeeded
            //f_idx++;
            printf("Fed image   fileNAme=%s", filename);
            memcpy(pRec->ImageData, image, 1360*800);
        }
    }else{

    }

   if ( 1 )
   {
       pRec->hwassist_info.stats_in_image =1;
       pRec->hwassist_info.ranking_in_image =1;
       pRec->hwassist_info.stats_block_size=4;;
       pRec->hwassist_info.stats_blocks_per_row=20;
       pRec->hwassist_info.stats_image_offset=8;
       pRec->hwassist_info.spare[0]=0;
       pRec->hwassist_info.spare[1]=0;
       pRec->hwassist_info.tag_line_Y_offset=0;
       pRec->hwassist_info.tag_line_X_offset=428;
       pRec->hwassist_info.version=0;
   }

   printf("[DEC]_FillDecImageData: w=%d, h=%d", pRec->Width, pRec->Height);
}


bool_t processNextImage()
{
    printf("processNextImage +\n");
    DEC_RETURN_VALUES	decRet;
    bool_t				processedFrame = TRUE;
    bool_t				useFinderRegion = FALSE;
    static RECT_ICC borderRect;
    UINT32_ICC	uiHBorder;
    UINT32_ICC	uiVBorder;

    if ( 1 )
    {
        ImageRec pRec;

         FillDecImageData(&pRec.imageInfo);

         // Default finder strategy
         pRec.imageInfo.FinderStrategy = FINDER_STRATEGY_DIAMOND;

         useFinderRegion = (isSwipeSession || PARAM_DECODE_MULTI) ? TRUE : FALSE;

         if (useFinderRegion) // handsfree and multidecode
         {
            uiHBorder = (1360  * FINDER_BORDER_W_PCT) / 200;
            uiVBorder = (800 * FINDER_BORDER_H_PCT) / 200;

            // Check for hardware assist
            pRec.imageInfo.FinderStrategy = pRec.imageInfo.hwassist_info.stats_in_image ? FINDER_STRATEGY_RANK_N_DIAMOND :FINDER_STRATEGY_RANDOM ;
         }
        else // level triggered
        {
            uiHBorder = 0;//paul (acqBuffer->width ) / 4; //25%
            uiVBorder = 0;//paul (acqBuffer->height) / 4; //25%

            // Check for hardware assist
            if(pRec.imageInfo.hwassist_info.stats_in_image)
                pRec.imageInfo.FinderStrategy = FINDER_STRATEGY_RANK_N_DIAMOND;
        }

        // Setup the border for the finder
        borderRect.Left		= uiHBorder;
        borderRect.Top		= uiVBorder;
        borderRect.Right	= uiHBorder;
        borderRect.Bottom	= uiVBorder;
        pRec.imageInfo.FinderBorder   = borderRect;


         multi_decodes = 0;
         printf("DecLoadImageDataEx + \n");
         decRet = DecLoadImageDataEx(&pRec.imageInfo, TRUE, TRUE);
         printf("DecLoadImageDataEx - \n");
         if (decRet == DEC_SUCCESS)
         {
            ++s_dwLoad;
            printf("[DEC]_FrameHandler: DecLoadImgDataEx Success! (hit=%d, miss=%d)\n",s_dwLoad,s_dwNoLoad);

            processedFrame = FALSE;

            DecAttemptDecode();

            if(multi_decodes > 0)
            {
                DEC_DECODE_DATA decDataA;
                SDL_U8 *pBuffer = malloc(150);
                decDataA.StructInfo.Allocated = decDataA.StructInfo.Used = sizeof(decDataA);
                decDataA.DecodeBuffer = ((byte_t *) pBuffer) + 1;
                decDataA.BufferLength = 150-1;

                DEC_RETURN_VALUES	stat ;

                if ((stat = DEC_GetDecodeData(&decDataA, -1)) == DEC_SUCCESS)
                {
                      char *data ;
                      data = malloc(  decDataA.DataLength);
                      for(int i = 1 ; i< decDataA.DataLength; i++){
                         data[i-1] = decDataA.DecodeBuffer[i];
                      }
                      printf("[IMAGE_STACK]  -> DECODED --> Decode Type = %d  --> Decoded Data = %s decDataA.DataLength=%d\n",decDataA.DecodeBuffer[0], data, decDataA.DataLength );
                }
            }
            else
            {
                printf("[IMAGE_STACK]  -> NOT DECODED \n" );
            }
         }
         else
         {
            ++s_dwNoLoad;
            if (isSwipeSession)	//warn for swipe-mode performance
                 printf("[DEC]_FrameHandler: Swipe session dropped frame");
         }
      }

     printf("processNextImage - processedFrame=%d\n", processedFrame);
   return processedFrame;
}

int main()
{
   printf("Hello World +\n");


   PAR_Init();

   DEC_RETURN_VALUES decRet = DEC_IMAGEIGNORED;

   decRet = DecOpen(&decOpenRec);
   printf("DecOpen decRet=%d\n", decRet);

    DecSetPicklistEnabled(0);
   decRet = DecStartSession();
   printf("DecStartSession decRet=%d\n", decRet);

   for(int i= 0; i<50; i++)
   {
       processNextImage();
   }


   bool gDecoded = FALSE;

   multi_decodes = 0;

   decRet = DecClose();
   printf("DecClose decRet=%d\n", decRet);

   printf("Hello World -\n");

   return 0;
}
