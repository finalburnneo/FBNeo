// Archive extract module, added by regret
// use Blargg's File_Extractor (http://www.slack.net/~ant/)

/* changelog:
   update 5: change loadonefile() to use separate fex handle
   update 4: update to Blargg's File_Extractor 1.0.0
   update 3: rewrite interface, use Blargg's great File_Extractor
   update 2: add load one file from archive
   update 1: add 7zip aupport
*/

#include "burner.h"
#include "File_Extractor/fex/fex.h"
#include "archive.h"

static File_Extractor* fex = NULL;
static fex_err_t err = NULL;
static int nCurrFile = 0; // The current file we are pointing to

static inline bool error(const char* error)
{
   if (error)
   {
#ifdef _DEBUG
      printf("fex Error: %s\n", error);
#endif
      return true;
   }
   return false;
}

// check if input name has 7z or zip file, the name will link with extension
// return value: 0:zip; 1:7z; -1:none
int archiveCheck(char* name, int zipOnly)
{
   if (!name)
      return ARC_NONE;

   static char archiveName[MAX_PATH] = "";
   int ret = ARC_NONE;

   // try zip first
   sprintf(archiveName, "%s.zip", name);

   static File_Extractor* fex_scan = NULL;
   static fex_err_t err_scan = NULL;

   err_scan = fex_open(&fex_scan, archiveName);

   if (!error(err_scan)) {
      ret = ARC_ZIP;
      strcat(name, ".zip");
   } else {
      if (!zipOnly) {
         // try 7z
         sprintf(archiveName, "%s.7z", name);

         err_scan = fex_open(&fex_scan, archiveName);
         if (!error(err_scan)) {
            ret = ARC_7Z;
            strcat(name, ".7z");
         }
      }
   }

   if (fex_scan) {
      fex_close(fex_scan);
      fex_scan = NULL;
   }

   return ret;
}

int archiveOpen(const char* archive)
{
   if (!archive)
      return 1;

   err = fex_open(&fex, archive);

   if (error(err))
      return 1;

   nCurrFile = 0;
   return 0;
}

int archiveClose()
{
   if (fex) {
      fex_close(fex);
      fex = NULL;
   }
   return 0;
}

// Get the contents of a archive file into an array of ArcEntry
int archiveGetList(ArcEntry** list, int* count)
{
   if (!fex || !list)
      return 1;

   int nListLen = 0;

   while (!fex_done(fex))
   {
      err = fex_next(fex);
      if (error(err))
      {
         archiveClose();
         return 1;
      }
      nListLen++;
   }

   // Make an array of File Entries
   if (nListLen == 0)
   {
      archiveClose();
      return 1;
   }

   ArcEntry* List = (struct ArcEntry *)malloc(nListLen * sizeof(struct ArcEntry));

   if (List == NULL)
   {
      archiveClose();
      return 1;
   }
   memset(List, 0, nListLen * sizeof(ArcEntry));

   err = fex_rewind(fex);

   if (error(err))
   {
      archiveClose();
      return 1;
   }

   // Step through all of the files, until we get to the end
   for (nCurrFile = 0, err = NULL;
         nCurrFile < nListLen && !error(err);
         nCurrFile++, err = fex_next(fex))
   {
      fex_stat(fex);

      // Allocate space for the filename
      const char* name = fex_name(fex);
      if (name == NULL) continue;
      char* szName = (char *)malloc(strlen(name) + 1);
      if (szName == NULL) continue;
      strcpy(szName, name);

      List[nCurrFile].szName = szName;
      List[nCurrFile].nLen = fex_size(fex);
      List[nCurrFile].nCrc = fex_crc32(fex);
   }

   // return the file list
   *list = List;

   if (count)
      *count = nListLen;

   err = fex_rewind(fex);

   if (error(err))
   {
      archiveClose();
      return 1;
   }

   nCurrFile = 0;
   return 0;
}

int archiveLoadFile(uint8_t* dest, int nLen, int nEntry, int* wrote)
{
   if (!fex || nLen <= 0)
      return 1;

   //	if (nEntry < nCurrFile)
   {
      err = fex_rewind(fex);

      if (error(err))
         return 1;

      nCurrFile = 0;
   }

   // Now step through to the file we need
   while (nCurrFile < nEntry)
   {
      err = fex_next(fex);

      if (error(err) || fex_done(fex))
         return 1;

      nCurrFile++;
   }

   err = fex_read(fex, dest, nLen);

   if (error(err))
      return 1;

   if (wrote != NULL)
      *wrote = nLen;

   return 0;
}

int archiveLoadOneFile(const char* arc, const char* file, void** dest, int* wrote)
{
   File_Extractor* fex_one = NULL;
   fex_err_t err_one = fex_open(&fex_one, arc);

   if (error(err_one))
      return 1;

   int nListLen = 0;
   while (!fex_done(fex_one))
   {
      err_one = fex_next(fex_one);
      if (error(err_one))
      {
         fex_close(fex_one);
         return 1;
      }
      nListLen++;
   }

   if (nListLen <= 0)
   {
      fex_close(fex_one);
      return 1;
   }

   err_one = fex_rewind(fex_one);
   if (error(err_one))
   {
      fex_close(fex_one);
      return 1;
   }

   if (file) {
      const char* filename = file;
      int currentFile = 0;

      for (currentFile = 0, err_one = NULL;
            currentFile < nListLen && !error(err_one);
            currentFile++, err_one = fex_next(fex_one))
      {
         fex_stat(fex_one);

         const char* name = fex_name(fex_one);
         if (name == NULL)
            continue;
         if (!strcmp(name, filename))
            break;
      }

      if (currentFile == nListLen)
      {
         fex_close(fex_one);
         return 1; // didn't find
      }
   }

   fex_stat(fex_one);
   long len = fex_size(fex_one);

   if (*dest == NULL)
   {
      *dest = (unsigned char*)malloc(len);
      if (!*dest)
      {
         fex_close(fex_one);
         return 1;
      }
   }

   // Extract file
   err_one = fex_read(fex_one, *dest, len);
   if (wrote != NULL) *wrote = len;

   fex_close(fex_one);

   if (error(err_one))
   {
      free(*dest);
      return 1;
   }
   return 0;
}
