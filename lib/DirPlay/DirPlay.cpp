#include "DirPlay.h"
const bool DEBUG = false;
#define P if(DEBUG) Serial.printf

void DirPlay::init_dir_stack(int size) {
    if ( size < 0 ) size = 0;
    if ( dir_stack ) {
        if ( dir_stack->size() == size ) {
            dir_stack->clear(); 
            return; 
        }
    }
    delete dir_stack;
    dir_stack = new Stack<dir_info_t>(size); 
}

// 0, wenn kein Eintrag gefunden oder Fehler im Dateinamen aufgetreten sind
// > 0 ist Laenge des Dateinamens. File/Dir-Name ist an cur_path angehängt, cur_path_len aktualisiert, 
// cur_dir_len wird nicht veraendert
size_t DirPlay::next_entry(entry_type_t type, dir_info_t *entry_info, int *entry_pos) {
    size_t entry_name_len;
    int file_name_pos;
    File f;

    if ( entry_info->index == uint16_t(-1) )
        cur_dir.rewind();
    else
        if ( f.open(&cur_dir, entry_info->index, O_RDONLY) )
            f.close();
    f.openNext(&cur_dir, O_RDONLY);
    P("NextEntry in: cur_path=%s, cur_dir_len=%d, type=%s\n", cur_path, cur_dir_path_len, type==FILE_ENTRY?"FILE_ENTRY":"DIR_ENTRY");

    while ( f ) {
        entry_name_len = 0;
        *entry_pos = 0;
        cur_path_len = cur_dir_path_len;
        if ( (f.isDir() ^ type) && !f.isHidden() ) {
            entry_info->init();
            file_name_pos = cur_dir_path_len + cur_dir_offset();
            if ( (entry_name_len = f.getName(cur_path + file_name_pos, sizeof(cur_path) - file_name_pos)) != 0 ) {
                if ( cur_dir_offset() ) 
                    cur_path[cur_path_len++] = '/';
                entry_info->index = f.dirIndex();
                entry_info->pos = cur_path_len;
                cur_path_len += entry_name_len; 
                *entry_pos = file_name_pos;
            }
            f.close();
            P("NextEntry out: cur_path=%s - pos(%d), index(%d), name_len = %d, len=%d/%d\n", 
                              cur_path, entry_info->pos, entry_info->index, entry_name_len, cur_dir_path_len, cur_path_len);
            if ( type == FILE_ENTRY && file_filter != nullptr ) {
                if ( file_filter( cur_path + file_name_pos, entry_name_len) )
                    return entry_name_len;
            }
            else
                return entry_name_len;
        }
        else
            f.close(); 
        f.openNext(&cur_dir, O_RDONLY);
    }
    return 0;
}

int DirPlay::NextFile(const char **file_path_ptr, bool next_dir) {
    bool file_mode = !next_dir;
    *file_path_ptr = "";
    int entry_name_pos;
    while (true) {
        if ( file_mode ) {
            if ( next_entry(FILE_ENTRY, &dinf_file, &entry_name_pos) ) {
                *file_path_ptr = cur_path;
                return entry_name_pos;
            }
            file_mode = false;
        }
        while ( next_entry(DIR_ENTRY, &dinf_dir, &entry_name_pos) ) {
            if ( dir_stack->full() ) 
                continue;
            dir_stack->push(dinf_dir);
            cur_dir_path_len = cur_path_len;
            P("down path-< %s cur_path_len=%d (push info: %d/%d)\n", cur_path, cur_path_len, dinf_dir.pos, dinf_dir.index);
            dinf_file.init();
            dinf_dir.init();
            cur_dir.close();
            cur_dir.open(cur_path, O_RDONLY);
            if ( next_entry(FILE_ENTRY, &dinf_file, &entry_name_pos) ) {
                *file_path_ptr = cur_path;
                return entry_name_pos; 
            }
        }
        if ( dir_stack->empty() ) {
            if ( loop_play ) {
                dinf_dir.init();
                file_mode = true;
                continue;  
            }            
            cur_dir.close();
            return 0; // ende
        }
        else {
            dinf_dir = dir_stack->pop();
            cur_dir_path_len = dinf_dir.pos;
            cur_dir_path_len -= cur_dir_offset();
            cur_path[cur_dir_path_len] = 0;
            P("up path-> %s cur_dir_len=%d\n", cur_path, cur_dir_path_len);
            cur_dir.close();
            cur_dir.open(cur_path, O_RDONLY);
            file_mode = false;
            continue;
        }
    } 
    return 0;
}

bool DirPlay::Config(const char *path, const char *root_path, int max_dir_depth) {
    char *p;
    int i, pos;
    File tmp;
    dir_info_t dinf;
    bool is_lastfile = false;

    P("path=%s root=%s\n", path, root_path ? root_path : "<>");
    cur_dir.close();
    init_dir_stack(max_dir_depth);
    if ( !is_dir_sep(*path) )
        return false; 
    if ( root_path ) 
        if ( !is_dir_sep(*root_path) )
            return false;
    // file / dir oeffnen, um path zu pruefen
    if ( !tmp.open(path, O_RDONLY) )
        return false;
    if ( tmp.isFile() ) { // file name separieren
        P("isFile\n");
        is_lastfile = true;
        dinf_file.index = tmp.dirIndex();
        tmp.close();
    }
    *cur_path = 0;
    cur_path_len = m_strcat(cur_path, path, sizeof(cur_path));
    while ( cur_path_len > 1 && cur_path[cur_path_len-1] == '/' )   
        cur_path[--cur_path_len] = 0; // letztes '/' entfernen
    if ( is_lastfile ) {  // filename abtrennen   
        if ( (p = strrchr(cur_path, '/' )) ) {
            if ( p == cur_path ) *(p+1) = 0; else *p = 0;
            cur_path_len = p - cur_path;
        }
        P("file=%s, index=%d\n", p+1, dinf_file.index);
    }
    if ( root_path ) {
        root_path_len = strlen(root_path);
        while ( root_path_len>1 && is_dir_sep(root_path[root_path_len-1]) ) root_path_len--;
        if ( !strncmp(cur_path, root_path, root_path_len) == 0 )
            return false;
    }
    else
        root_path_len = 1;  // root_path is "/"           

    pos = root_path_len > 1 ? root_path_len+1 : root_path_len; // pos steht auf ersten Zeichen nach root_path (Start rel);
    P("1-cur_path=%s, pos=%d\n", cur_path, pos);    
    for ( i = pos; pos < cur_path_len && !dir_stack->full(); pos = ++i ) {
        while ( cur_path[i] && cur_path[i] != '/' ) i++; // i begrenzt Ende des pos folgenden directory, steht auf 0 (i==cur_path_len) oder /
        if ( pos == 1 ) {
            cur_dir.open("/", O_RDONLY);
            P("* cur_dir=/\n");
        }
        else {
            cur_path[pos-1] = 0;    // cur_dir path begrenzen 
            P("* cur_dir=%s\n", cur_path);
            cur_dir.open(cur_path, O_RDONLY);
            cur_path[pos-1] = '/';
        }
        if ( cur_dir ) {
            cur_path[i] = 0;
            P("* open %s\n", cur_path+pos);
            tmp.open(&cur_dir, cur_path + pos, O_RDONLY);
            if ( i != cur_path_len )
                cur_path[i] = '/';
            if ( tmp ) {
                dinf = { pos, tmp.dirIndex() };
                tmp.close();
            }
            else
                dinf = { pos, (uint16_t)-1 };
            cur_dir.close();
            dir_stack->push(dinf);
            P("stack %d: ", dir_stack->status()); dir_stack->top().print(&Serial);
        }
        else {
            P("* cur_dir open failed\n");
            return false;
        }
    }
    P("2-cur_path=%s| rpl=%d, cpl=%d\n-------------------\n", cur_path, root_path_len, cur_path_len);
    cur_dir_path_len = cur_path_len;
    return cur_dir.open(cur_path, O_RDONLY);    
}

bool DirPlay::Reset() {
    cur_path_len = cur_dir_path_len = root_path_len;
    cur_path[cur_path_len = cur_dir_path_len = root_path_len] = 0;
    dir_stack->clear();
    dinf_dir.init();
    dinf_file.init();
    cur_dir.close();
    return cur_dir.open(cur_path, O_RDONLY);  
}

// https://www.joelonsoftware.com/2001/12/11/back-to-basics/ ... extended
// return Zeiger auf abschliessende 0
int DirPlay::m_strcat(char* dest, const char* src, int n) {
    char *start = dest;
    while ( *dest ) dest++;
    if ( n < 1 )
        while ( (*dest++ = *src++) );
    else {
        while ( --n && *src) 
            *dest++ = *src++;
        *dest++ = 0;
    }
    return --dest - start;
}


