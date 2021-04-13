#ifndef __DIR_PLAY__
#define __DIR_PLAY__

#include "Arduino.h"
#include "SD_Libs.h"

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

enum entry_type_t { DIR_ENTRY = 0, FILE_ENTRY = 1 }; // als const bool definieren
typedef bool (*filter_func)(const char *, int);

class DirPlay {
public:
    DirPlay() { init_dir_stack(0); }
    DirPlay(const char *path, const char *root_path = nullptr, int max_dir_depth = 0) {
        Config(path, root_path, max_dir_depth);
    }
    ~DirPlay() { delete dir_stack; }
    bool Config(const char *path, const char *root_path = nullptr, int max_dir_depth = 0); 
    int NextFile(const char **name_ptr, bool next_dir = false);
    bool Reset(); // reset to root_path
    void SetLoopMode(bool mode) { loop_play = mode; }
    void SetFileFilter(filter_func f = nullptr) { file_filter = f; }
private:
    bool loop_play = false;
    char cur_path[256];
    int root_path_len;
    int cur_path_len;
    int cur_dir_path_len;
    File cur_dir;
    typedef struct dir_info {
        int pos;
        uint16_t index;
        dir_info() { init(); };
        dir_info(int p, int i) : pos(p), index(i) {}
        void print(print_t* pr) { pr->printf("(pos=%d / index=%u)\n", pos, index); }
        void init() { pos = 0; index = uint16_t(-1); }
    } dir_info_t;
    void init_dir_stack(int size);
    Stack<dir_info_t> *dir_stack = nullptr;
    dir_info_t dinf_file;
    dir_info_t dinf_dir;
    size_t next_entry(entry_type_t type, dir_info_t *entry_info, int *entry_pos);
    int m_strcat(char* dest, const char* src, int n = -1);
    bool is_dir_sep(char c) { return (c=='/'); }
    int cur_dir_offset() { return (cur_dir_path_len == 1 ? 0 : 1); }
    filter_func file_filter = nullptr;
};


#endif // __DIR_PLAY__