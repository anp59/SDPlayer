#ifndef __DIR_PLAY__
#define __DIR_PLAY__

#include "Arduino.h"
#include "SD_Libs.h"

// The idea was to design an SD player that could play all the music tracks on an SD in an endless loop.
// Both the directory ranges and a file name filter can be specified (Config()). 
// The method NextFile() returns the next file (including file path), 
// which can then be played back with ESP32-AudioI2S, for example.
// Bill Greiman's library SdFat is needed because the solution works with working directories and relative paths. 

typedef Print print_t; // Use Arduino Print. 

template <typename T>
class Stack {
    T   *data = nullptr; //[elements];
    int count = 0;
    int elements = 0;
    public:
        Stack(int size);
        ~Stack() { delete[] data; }
        void clear() { count = 0; }
        bool empty() const { return (count == 0); }
        bool full() const { return (count == elements); }
        int  size() const { return elements; }
        int status() const { return count; }
        bool push(T const& el); 
        T    top() const;
        T    pop();
        void print(print_t* pr);    // only for test, T must be printable 
};
template<typename T>
Stack<T>::Stack(int size) { 
    if ( size > 0 ) {
        data = new T[size];
        if ( data ) 
            elements = size;
    }
    else
        data = nullptr;
}
template<typename T>
bool Stack<T>::push(T const& el)
{
    if ( full() ) 
        return false;
    data[count++] = el;
    return true;
}
template<typename T>
T Stack<T>::top() const
{
    if ( empty() ) return T();
    return data[count-1];
}
template<typename T>
T Stack<T>::pop()
{
    if ( empty() ) return T();
    return data[--count];
}
template<typename T>
void  Stack<T>::print(print_t* pr) {
    pr->printf("* stack: %d of %d *\n", status(), size()); 
    for ( int i = count; i > 0; i-- ) {
        pr->printf("stack %d: ", i);
        data[i-1].print(pr);
    }
}

/******************************************************************************/

enum entry_type_t { DIR_ENTRY = 0, FILE_ENTRY = 1 };
typedef bool (*filter_func)(const char *, int);


class DirPlay {
public:
    DirPlay() { Config("/"); }
    DirPlay(const char *path, const char *root_path = nullptr, int max_dir_depth = 0) {
        Config(path, root_path, max_dir_depth);
    }
    ~DirPlay() { delete dir_stack; }
    bool Config(const char *path, const char *root_path = nullptr, int max_dir_depth = 0); 
    int NextFile(const char **name_ptr, bool next_dir = false);
    bool Reset(); // reset to root_path
    bool Restart();
    void SetLoopMode(bool mode) { loop_play = mode; }
    void SetFileFilter(filter_func f = nullptr) { file_filter = f; }
    uint8_t GetError() { return read_error; }
    unsigned int GetPlayedFiles() { return file_count; }
private:
    typedef struct dir_info {
        int pos;
        uint16_t index;
        dir_info() { init(); };
        dir_info(int p, int i) : pos(p), index(i) {}
        void print(print_t* pr) { pr->printf("(pos=%d / index=%u)\n", pos, index); }
        void init() { pos = 0; index = uint16_t(-1); }
    } dir_info_t;
    
    bool loop_play = false;

    char cur_path[256];
    int root_path_len = 0; // must be 0 for correct initialization
    int cur_path_len;
    int cur_dir_path_len;
    unsigned int file_count;
    File cur_dir_file;
    uint8_t read_error = 0;

    void init(int size);
    void init_dir_stack(unsigned int size);
    Stack<dir_info_t> *dir_stack = nullptr;
    dir_info_t dinf_file;
    dir_info_t dinf_dir;
    size_t next_entry(entry_type_t type, dir_info_t *entry_info, int *entry_pos);
    bool is_dir_sep(char c) { return (c=='/'); }
    int cur_dir_offset() { return (cur_dir_path_len == 1 ? 0 : 1); }
    filter_func file_filter = nullptr;
};


#endif // __DIR_PLAY__