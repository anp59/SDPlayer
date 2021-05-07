#include <Arduino.h>
#include "Preferences.h"
#include "Audio.h"
#include "SD_Libs.h"
#include "DirPlay.h"
#include "InputButton.h"
#include "ESP32Encoder.h"

// Version 1.0  /   07.05.2021

// Digital I/O used
#ifndef SS
    #define SS      5
#endif
#define SD_CS       5
#define SPI_SCK     18
#define SPI_MISO    19
#define SPI_MOSI    23

#define I2S_DOUT    25
#define I2S_BCLK    27
#define I2S_LRC     26

#define ENC_A       32
#define ENC_B       33
#define NEXT_BTN    4  

Audio audio;
DirPlay dplay;          // default without Config(): "/", dir_depth = 0
Preferences prefs;
ESP32Encoder encoder;
InputButton enc_button(NEXT_BTN, true, ACTIVE_LOW);

int8_t old_enc_val = -2, enc_val;   // -2 guarantees that setVolume is called at the beginning

const char *prefs_key_volume = "volume";
const char *prefs_key_path   = "filepath";

const int maxDirDepth = 10;
const char *ptrCurrentFile;
bool playNextFile = false;
bool readError = false;

const unsigned int errorCheckInterval = 2000;
unsigned last_time = 0;
bool tick = false;


// for directory listing over Serial (optional)
void listDir(fs::FS &fs, const char * dirname, uint8_t levels);
const char *name(File& f);

//###############################################################

// file filter for PlayNextFile() (set with Dirplay::SetFileFilter(...))
bool isMusicFile(const char *filename, int len) {
    const char *p;
    if ( !len ) len = strlen(filename);
    while ( len )
        if ( filename[--len] == '.' ) break;
    p = filename + len;
    Serial.printf("isMusicFile: %s\n", p);
    return  (  strcasecmp(p, ".mp3") == 0 
            || strcasecmp(p, ".m4a") == 0 
            );
}

size_t PlayNextFile(const char** p, bool next_dir = false) {
    size_t file_name_pos;
    while ( true ) {
        if ( (file_name_pos = dplay.NextFile(p, next_dir)) ) {
            Serial.printf(">>> Play %s%s\n", next_dir ? "next dir " : "", *p);
            if ( !audio.connecttoFS(SD, *p) )
                continue; 
        }
        else {
            Serial.println(">>> End of playlist, no loop-mode or no file found!");  
        }
        return file_name_pos; 
    }   
}

//###############################################################

void setup() {
    char last_filepath [256];
    const char rootpath[] = "/";    // All music files are played from this directory.

    Serial.begin(115200);
    while (!Serial) {       // Wait for USB Serial
        SysCall::yield();
    }
    Serial.println();

    if ( !SD.begin() ) {
        SD.initErrorHalt(); // SdFat-lib helper function
    }
    
    // listDir(SD, "/", 10); 
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);    
      
    ESP32Encoder::useInternalWeakPullResistors = DOWN;
    encoder.attachSingleEdge(ENC_A, ENC_B);
    
    prefs.begin("lastFile", false);    
    enc_val = prefs.getInt(prefs_key_volume, 1);
    encoder.setCount(enc_val ? enc_val : 1);
    encoder.setFilter(1023); // debouncing filter (0...1023)    
    
    if ( !prefs.getString(prefs_key_path, last_filepath, sizeof(last_filepath)) )
        strcpy(last_filepath, rootpath);

    if ( !dplay.Config(last_filepath, rootpath, maxDirDepth) ) {   // dirdepth = 1, all files from rootpath plus one subdir will be selected
        Serial.printf("Config failed! Ceck the path '%s' / root_path '%s'\nUsing rootpath instead of path!\n", last_filepath, rootpath);
        if ( !dplay.Config(rootpath, rootpath, maxDirDepth) ) { 
            Serial.printf("Config failed! Ceck the rootpath!\n");
            SysCall::halt();    // SysCall from SdFat-Lib
        }
    } 
    
    dplay.SetFileFilter(isMusicFile);   // select only music files
    dplay.SetLoopMode(true); 
    Serial.println(last_filepath);
    PlayNextFile(&ptrCurrentFile);
}

//###############################################################

void loop()
{
    bool nextDir = false;
    bool saveCurrentFile = true;

    if ( tick ) {
        tick = false;
        last_time = millis();    
    }
    else tick = ( millis() > (last_time + errorCheckInterval) );

    if ( (enc_val = (int8_t)(encoder.getCount())) != old_enc_val )
    {
        // Check volume level and adjust if necassary
        if ( enc_val < 0 ) 
            enc_val = 0;
        else
            if ( enc_val > 21 )
                enc_val = 21;
        Serial.printf(">>> Volume = %d\n", enc_val);
        old_enc_val = enc_val;
        encoder.setCount(enc_val);
        audio.setVolume(enc_val);
        prefs.putInt(prefs_key_volume, enc_val);
    } 
    
    audio.loop();
    
    if ( Serial.available() ) { 
        String r = Serial.readString(); r.trim();   
        if ( r == "d" ) {
            nextDir = true;
            Serial.println(">>> Next directory");
        }
        if ( r == "r" ) {
            if ( SD.begin() )
                if ( !dplay.Reset() )
                    Serial.println("Error Reset!");
            Serial.println(">>> Reset playlist to root_path");
        }
        playNextFile = true;
    }
    
    if ( enc_button.longPress() && audio.isRunning() ) 
    {
        nextDir = true;
        playNextFile = true;
    }
    if ( enc_button.shortPress() && audio.isRunning() ) 
    {
        playNextFile = true;
    }
    if ( tick && (readError || SD.card()->errorCode() || dplay.GetError()) ) {
        last_time = millis();
        Serial.print('.');   
        if ( SD.begin() ) {
            if ( dplay.Restart() ) {
                Serial.println();                 
                readError = false;   
                saveCurrentFile = false;
                playNextFile = true;
            }
        }
    }
    
    if ( playNextFile ) {
        int file_name_pos;
        audio.stopSong();
        if ( saveCurrentFile && !nextDir ) 
            prefs.putString(prefs_key_path, ptrCurrentFile);
        file_name_pos = PlayNextFile(&ptrCurrentFile, nextDir);
        if ( nextDir ) {
            char buf[256];
            strcpy(buf, ptrCurrentFile);
            buf[file_name_pos] = 0; // save directory only
            prefs.putString(prefs_key_path, buf);
        }
        playNextFile = false;
    }
}

//###############################################################
//optional

void audio_eof_mp3(const char *info) {  //end of file
    Serial.print("eof_mp3     "); Serial.println(info);
    playNextFile = true;
}
void audio_error_mp3(const char *info) {
    Serial.print("error_mp3   "); Serial.println(info);
    readError = true;
}

// void audio_info(const char *info) {
//     Serial.print("info        "); Serial.println(info);
// }

// void audio_id3data(const char *info) {  //id3 metadata
//     Serial.print("id3data     "); Serial.println(info);
// }

//###############################################################
// optional functions for listing directory

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