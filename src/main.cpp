#include <Arduino.h>
#include "Audio.h"
#include "SD_Libs.h"
#include "DirPlay.h"

#define   PF Serial.printf
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
DirPlay dplay;
const char *pSong;

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
    return ( strcasecmp(p, ".mp3") == 0 || strcasecmp(p, ".m4a") == 0 );
}

void setup() {
    Serial.begin(115200);
    Serial.println();

    if ( !SD.begin() ) {
        Serial.println("Card Mount Failed");
        return;
    }
    
    //listDir(SD, "/", 10); 
    
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(1); // 0...21

    if ( !dplay.Config("/", "/", 5) )
        Serial.printf("Config fehlgeschlagen!");
    dplay.SetFileFilter(isMusicFile);
    dplay.SetLoopMode(false);    

    if ( dplay.NextFile(&pSong) ) {
        Serial.printf(">>>   %s\n", pSong);
        audio.connecttoFS(SD, pSong);
    }

}
void loop()
{
    audio.loop();
    if (Serial.available()) { 
        String r=Serial.readString(); r.trim();   
        audio.stopSong();
        if ( r == "r" ) {
            dplay.Reset();
            Serial.println(">>>>>> Reset Playlist to root_path");
        }
        if ( dplay.NextFile(&pSong) ) {
            Serial.printf(">>>   %s\n", pSong);
            audio.connecttoFS(SD, pSong);
        }   
        else 
            Serial.println(">>>>>> Ende der Playlist");
    }
}

// optional
// void audio_info(const char *info){
//     Serial.print("info        "); Serial.println(info);
// }
// void audio_id3data(const char *info){  //id3 metadata
//     Serial.print("id3data     ");Serial.println(info);
// }
void audio_eof_mp3(const char *info){  //end of file
    Serial.print("eof_mp3     ");Serial.println(info);
    if ( dplay.NextFile(&pSong) ) {
        Serial.printf(">>>   %s\n", pSong);
        audio.connecttoFS(SD, pSong);
    }
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