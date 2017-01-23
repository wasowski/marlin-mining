#ifndef CARDREADER_H
#define CARDREADER_H

#ifdef SDSUPPORT

#define MAX_DIR_DEPTH 10          // Maximum folder depth

#ifdef SDCARD_SORT_ALPHA
  #define SORT_USES_RAM false      // Buffer while sorting, else re-read from SD
  #define SORT_USES_MORE_RAM false // Always keep the directory in RAM
  #define SORT_LIMIT 256           // Maximum number of sorted items
  #define FOLDER_SORTING -1        // -1=above  0=none  1=below
#endif

#include "SdFile.h"
enum LsAction {LS_SerialPrint,LS_Count,LS_GetFilename};
class CardReader
{
public:
  CardReader();
  
  void initsd();
  void write_command(char *buf);
  //files auto[0-9].g on the sd card are performed in a row
  //this is to delay autostart and hence the initialisaiton of the sd card to some seconds after the normal init, so the device is available quick after a reset

  void checkautostart(bool x); 
  void openFile(char* name,bool read,bool replace_current=true);
  void openLogFile(char* name);
  void removeFile(char* name);
  void closefile(bool store_location=false);
  void release();
  void startFileprint();
  void pauseSDPrint();
  void getStatus();
  void printingHasFinished();

  void getfilename(const uint16_t nr);
  uint16_t getnrfilenames();
  
  void getAbsFilename(char *t);
  

  void ls();
  void chdir(const char * relpath);
  void updir();
  void setroot();

#ifdef SDCARD_SORT_ALPHA
  void presort();
  void flush_presort();
  void getfilename_sorted(const uint16_t nr);
#endif


  FORCE_INLINE bool isFileOpen() { return file.isOpen(); }
  FORCE_INLINE bool eof() { return sdpos>=filesize ;};
  FORCE_INLINE int16_t get() {  sdpos = file.curPosition();return (int16_t)file.read();};
  FORCE_INLINE void setIndex(long index) {sdpos = index;file.seekSet(index);};
  FORCE_INLINE uint8_t percentDone(){if(!isFileOpen()) return 0; if(filesize) return sdpos/((filesize+99)/100); else return 0;};
  FORCE_INLINE char* getWorkDirName(){workDir.getFilename(filename);return filename;};

public:
  bool saving;
  bool logging;
<<<<<<< HEAD
  bool sdprinting;
  bool cardOK;
=======
  bool sdprinting ;  
  bool cardOK ;
>>>>>>> f074706d2e34e4b91e600b0809be5392db49c828
  char filename[FILENAME_LENGTH];
  char longFilename[LONG_FILENAME_LENGTH];
  bool filenameIsDir;
  int lastnr; //last number of the autostart;
private:
  SdFile root,*curDir,workDir,workDirParents[MAX_DIR_DEPTH];
  uint16_t workDirDepth;
#ifdef SDCARD_SORT_ALPHA
  uint16_t sort_count;
  uint8_t *sort_order;
  #if SORT_USES_MORE_RAM
    char **sortshort;
    char **sortnames;
    uint8_t *isDir;
  #endif
#endif
  Sd2Card card;
  SdVolume volume;
  SdFile file;
  #define SD_PROCEDURE_DEPTH 1
  #define MAXPATHNAMELENGTH (FILENAME_LENGTH*MAX_DIR_DEPTH+MAX_DIR_DEPTH+1)
  uint8_t file_subcall_ctr;
  uint32_t filespos[SD_PROCEDURE_DEPTH];
  char filenames[SD_PROCEDURE_DEPTH][MAXPATHNAMELENGTH];
  uint32_t filesize;
  //int16_t n;
  unsigned long autostart_atmillis;
  uint32_t sdpos ;

  bool autostart_stilltocheck; //the sd start is delayed, because otherwise the serial cannot answer fast enought to make contact with the hostsoftware.
  
  LsAction lsAction; //stored for recursion.
  uint16_t nrFiles; //counter for the files in the current directory and recycled as position counter for getting the nrFiles'th name in the directory.
  char* diveDirName;
  void lsDive(const char *prepend,SdFile parent);
};
extern CardReader card;
#define IS_SD_PRINTING (card.sdprinting)

#if (SDCARDDETECT > -1)
# ifdef SDCARDDETECTINVERTED 
#  define IS_SD_INSERTED (READ(SDCARDDETECT)!=0)
# else
#  define IS_SD_INSERTED (READ(SDCARDDETECT)==0)
# endif //SDCARDTETECTINVERTED
#else
//If we don't have a card detect line, aways asume the card is inserted
# define IS_SD_INSERTED true
#endif

#else

#define IS_SD_PRINTING (false)

#endif //SDSUPPORT
#endif