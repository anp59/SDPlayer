#include <Arduino.h>
#include "Preferences.h"
#include "Audio.h"
#include "SD_Libs.h"
#include "DirPlay.h"


// Digital I/O used
#ifndef SS
    #define SS         5
#endif
#define SD_CS          5
#define SPI_SCK       18
#define SPI_MISO      19
#define SPI_MOSI      23

#define I2S_DOUT      25
#define I2S_BCLK      27
#define I2S_LRC       26

Audio audio;
DirPlay dplay;  // default without Config(): "/", dir_depth = 0
Preferences prefs;

const char *pCurrentSong;

void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
const char *name(File& f);

//###############################################################

bool isMusicFile(const char *filename, int len) {
    const char *p;
    if ( !len ) len = strlen(filename);
    while ( len )
        if ( filename[--len] == '.' ) break;
    p = filename + len;
    //Serial.printf("%s ", p);
    return  (  strcasecmp(p, ".mp3") == 0 
            || strcasecmp(p, ".m4a") == 0 
            );
}

bool PlayNextFile(const char** p, bool next_dir = false) {
    while ( true ) {
        if ( dplay.NextFile(p, next_dir) ) {
            Serial.printf(">>> play %s\n", *p);
            if ( !audio.connecttoFS(SD, *p) )
                continue; 
            return true;
        }
        else {
            Serial.println(">>>>>> End of playlist, no loop-mode or no file found!");
            return false;  
        }
    }
}

void setup() {
    char filepath [256];
    const char rootpath[] = "/";

    Serial.begin(115200);
    Serial.println();

    if ( !SD.begin() ) {
        //Serial.println("Card Mount Failed");
        SD.initErrorHalt(); // SdFat-lib helper function
    }
    
    // listDir(SD, "/", 10); 
    
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(1); // 0...21

    prefs.begin("lastFile", false); 
    if ( !prefs.getString("filepath", filepath, sizeof(filepath)) )
        strcpy(filepath, rootpath);

    if ( !dplay.Config(filepath, rootpath, 5) ) {   // dirdepth = 1, all files from rootpath plus one subdir will be selected
        Serial.printf("Config failed! Ceck the path '%s' / root_path '%s'\nUsing rootpath instead of path!\n", filepath, rootpath);
        if ( !dplay.Config(rootpath, rootpath, 5) ) { 
            Serial.printf("Config failed! Ceck the rootpath!\n");
            SysCall::halt();    // SysCall from SdFat-Lib
        }
    } 
    
    dplay.SetFileFilter(isMusicFile);
    dplay.SetLoopMode(true);    

    PlayNextFile(&pCurrentSong);
}


void loop()
{
    bool nextDir = false;
    audio.loop();
    if ( Serial.available() ) { // enter for next file, r to restart
        String r = Serial.readString(); r.trim();   
        audio.stopSong();
        prefs.putString("filepath", pCurrentSong);
        if ( r == "d" ) {
            nextDir = true;
            Serial.println(">>>>>> Next directory");
        }
        if ( r == "r" ) {
            if ( SD.begin() )
                if ( !dplay.Reset() )
                    Serial.println("Fehler Reset!");
            Serial.println(">>>>>> Reset playlist to root_path");
            //nextDir = true;
        }
        PlayNextFile(&pCurrentSong, nextDir);
    }
}

//optional
void audio_info(const char *info){
    Serial.print("info        "); Serial.println(info);
}
void audio_id3data(const char *info){  //id3 metadata
    Serial.print("id3data     ");Serial.println(info);
}
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
    prefs.putString("filepath", pCurrentSong);
    PlayNextFile(&pCurrentSong);
}
// void audio_showstation(const char *info){
//     Serial.print("station     ");Serial.println(info);
// }
// void audio_showstreamtitle(const char *info){
//     Serial.print("streamtitle ");Serial.println(info);
// }
// void audio_bitrate(const char *info){
//     Serial.print("bitrate     ");Serial.println(info);
// }
// void audio_commercial(const char *info){  //duration in sec
//     Serial.print("commercial  ");Serial.println(info);
// }
// void audio_icyurl(const char *info){  //homepage
//     Serial.print("icyurl      ");Serial.println(info);
// }
// void audio_lasthost(const char *info){  //stream URL played
//     Serial.print("lasthost    ");Serial.println(info);
// }
// void audio_eof_speech(const char *info){
//     Serial.print("eof_speech  ");Serial.println(info);
// }

//####################################################################################

const char *name(File& f)
{
#ifdef SDFATFS_USED
    static char buf[64];
    buf[0] = 0;
    if ( f ) 
        f.getName(buf, sizeof(buf));
    return (const char *)buf;
#else
    return f.name();
#endif
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {
    char path[256] = "";    // muss mit 0 initialisiert sein
    int len = 0;            // muss für SD-Lib mit 0 initialisiert sein
 
    
    File root;
    
    if (true) {
        int mode = 2;            
        root = fs.open(dirname);    
        if(!root){
            Serial.println("Failed to open directory");
            return;
        }
        if(!root.isDirectory()){
            Serial.println("Not a directory");
            return;
        }
        Serial.println("----------------------------------------------");
        Serial.printf("Listing directory: %s (I%d)\n", dirname, root.dirIndex());
        while (true) {
            File file = root.openNextFile();
            while(file){
                if( file.isDirectory() && mode == 1 ){
                    Serial.print("DIR : ");
                    Serial.printf("%s (L%d - I%d)\n", name(file), levels, file.dirIndex());
                    if(levels){
                        // nur bei SdFat - kompletten pfad für file rekursiv weitergeben 
                        if ( (name(file))[0] != '/' ) {
                            strcpy(path, dirname); 
                            len = strlen(path);
                            if ( !(len == 1 && path[0] == '/') )    // not root (/) 
                                path[len++] = '/'; // ohne abschliessende 0
                        }
                        strcpy(path+len, name(file));
                        listDir(fs, path, levels -1);
                    }
                } 
                if ( !file.isDirectory() && mode == 2 ) {

                    Serial.print("  FILE: ");
                    Serial.printf("%s (L%d - I%d)\n", name(file), levels, file.dirIndex());
                    //Serial.print("  SIZE: ");
                    //Serial.println(file.size());
                }
                file = root.openNextFile();
            }
            mode--;
            if ( !mode )
                // 
                break;
            root.rewindDirectory();
        }
    }
}