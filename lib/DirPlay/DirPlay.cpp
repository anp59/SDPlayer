#include "DirPlay.h"

// helper macro for debug purposes
const bool ENABLE_DEBUG_LOG = false;
#define DBG(...) do if (ENABLE_DEBUG_LOG) printf("D# " __VA_ARGS__); while (0)


void DirPlay::init_dir_stack(unsigned int size) {
    if ( size > 255 ) size = 0;
    if ( dir_stack ) {
        if ( dir_stack->size() == size ) {
            dir_stack->clear(); 
            return; 
        }
    }
    delete dir_stack;
    dir_stack = new Stack<dir_info_t>(size); 
}

// returns len of filename, 0 if no next file or error
size_t DirPlay::next_entry(entry_type_t type, dir_info_t *entry_info, int *entry_pos) {
    size_t entry_name_len;
    int file_name_pos;
    File f;

    if ( entry_info->index == uint16_t(-1) )
        cur_dir_file.rewind();
    else
        if ( f.open(&cur_dir_file, entry_info->index, O_RDONLY) )
            f.close();
    f.openNext(&cur_dir_file, O_RDONLY);
    
    DBG("NextEntry in: cur_path=%s, cur_dir_len=%d, type=%s, cur_dir_file %sopen, openNext %sfound\n", cur_path, cur_dir_path_len, type==FILE_ENTRY?"FILE_ENTRY":"DIR_ENTRY", cur_dir_file?"":" not", f?"":"not ");

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
            DBG("NextEntry out: cur_path=%s - pos(%d), index(%d), name_len = %d, len=%d/%d\n", 
                              cur_path, entry_info->pos, entry_info->index, entry_name_len, cur_dir_path_len, cur_path_len);
            if ( type == FILE_ENTRY && file_filter != nullptr ) {
                if ( file_filter( cur_path + file_name_pos, entry_name_len) ) {
                    file_count++; 
                    return entry_name_len;
                }
            }
            else
                return entry_name_len;  // no file_filter or DIR_ENTRY
        }
        else
            f.close(); 
        f.openNext(&cur_dir_file, O_RDONLY);
    }
    read_error = cur_dir_file.getError();
    return 0;
}

// returns pos of filename in *file_path_ptr, 0 if not exist or error
int DirPlay::NextFile(const char **file_path_ptr, bool next_dir) {
    bool file_mode = !next_dir;
    *file_path_ptr = "";
    int entry_name_pos;
    while ( !read_error) {
        DBG("NextFile: file_mode=%d\n", (int)file_mode);
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
            DBG("Down path-< %s cur_path_len=%d (push info: %d/%d)\n", cur_path, cur_path_len, dinf_dir.pos, dinf_dir.index);
            dinf_file.init();
            dinf_dir.init();
            cur_dir_file.close();
            if ( cur_dir_file.open(cur_path, O_RDONLY) ) {
                if ( next_entry(FILE_ENTRY, &dinf_file, &entry_name_pos) ) {
                    *file_path_ptr = cur_path;
                    return entry_name_pos; 
                }
            }
            else {
                read_error = cur_dir_file.getError();
                break;
            }
        }
        DBG("NextFile: file_count=%d, loop_count=%d, entry_name_pos=%d\n", file_count, loop_count, entry_name_pos);
        if ( dir_stack->empty() ) {
            if ( loop_play && (file_count || !loop_count) ) {
                dinf_dir.init();
                dinf_file.init();
                file_mode = true;
                loop_count++;
                continue;  
            }            
            cur_dir_file.close();
            return 0; // end
        }
        else {
            dinf_dir = dir_stack->pop();
            cur_dir_path_len = dinf_dir.pos;
            cur_dir_path_len -= cur_dir_offset();
            cur_path[cur_dir_path_len] = 0;
            DBG("Up path-> %s cur_dir_len=%d\n", cur_path, cur_dir_path_len);
            cur_dir_file.close();
            if ( cur_dir_file.open(cur_path, O_RDONLY) ) {
                file_mode = false;
                continue;
            }
            read_error = cur_dir_file.getError();
        }
    } 
    return 0;
}

bool DirPlay::Config(const char *path, const char *root_path, int max_dir_depth) {
    char *p;
    int i, pos;
    File tmp_file;
    dir_info_t dinf;

    DBG("Config in: path=%s root=%s\n", path, root_path ? root_path : "<>");
    init(max_dir_depth);
    if ( !is_dir_sep(*path) )
        return false; 
    if ( root_path ) 
        if ( !is_dir_sep(*root_path) )
            return false;
    cur_path_len = strlen(path);
    if ( cur_path_len > sizeof(cur_path) )
        return false;
    strcpy(cur_path, path);

    while ( cur_path_len > 1 && is_dir_sep(cur_path[cur_path_len-1]) )
        cur_path[--cur_path_len] = 0; // cut last separator '/'    
    if ( root_path ) {
        root_path_len = strlen(root_path);
        while ( root_path_len>1 && is_dir_sep(root_path[root_path_len-1]) ) root_path_len--;
        if ( !strncmp(cur_path, root_path, root_path_len) == 0 ) {
            // root_path is not part of path, replace cur_path with root_path
            strncpy(cur_path, root_path, root_path_len);
            cur_path[cur_path_len = root_path_len] = 0;
            DBG("cur_path=%s len=%d (replaced with root_path)\n", cur_path, cur_path_len);
        }
    }
    else
        root_path_len = 1;  // set root_path to "/"           
    // open file/dir to check path
    if ( !tmp_file.open(cur_path, O_RDONLY) ) {
        DBG("cur_path=%s not found!\n", cur_path);
        return false;
    }
    if ( tmp_file.isFile() ) {
        dinf_file.index = tmp_file.dirIndex();
        tmp_file.close();
        // cut filename from cur_path 
        if ( (p = strrchr(cur_path, '/' )) ) {
            if ( p == cur_path ) *(p+1) = 0; else *p = 0;
            cur_path_len = p - cur_path;
        }
        DBG("last_file=%s, index=%d\n", p+1, dinf_file.index);
    }
    // pos at first char after root_path, save details (dir_info_t) of all directories on the stack
    pos = root_path_len > 1 ? root_path_len+1 : root_path_len; // pos at first char after root_path
    for ( i = pos; pos < cur_path_len && !dir_stack->full(); pos = ++i ) {
        while ( cur_path[i] && cur_path[i] != '/' ) i++; 
        if ( pos == 1 ) {
            cur_dir_file.open("/", O_RDONLY);
            DBG("* cur_dir=/\n");
        }
        else {
            cur_path[pos-1] = 0; 
            DBG("* cur_dir=%s\n", cur_path);
            cur_dir_file.open(cur_path, O_RDONLY);
            cur_path[pos-1] = '/';
        }
        if ( cur_dir_file ) {
            cur_path[i] = 0;
            DBG("* open %s\n", cur_path+pos);
            tmp_file.open(&cur_dir_file, cur_path + pos, O_RDONLY);
            if ( i != cur_path_len )
                cur_path[i] = '/';
            if ( tmp_file ) {
                dinf = { pos, tmp_file.dirIndex() };
                tmp_file.close();
            }
            else
                dinf = { pos, (uint16_t)-1 };
            cur_dir_file.close();
            dir_stack->push(dinf);
            DBG("* pushed on stack %d pos:%d / index%d\n", dir_stack->status(), dinf.pos, dinf.index); 
        }
        else {
            DBG("* cur_dir open failed\n");
            return false;
        }
    }
    DBG("Config out: cur_path=%s| rpl=%d, cpl=%d\n-------------------\n", cur_path, root_path_len, cur_path_len);
    cur_dir_path_len = cur_path_len;
    return cur_dir_file.open(cur_path, O_RDONLY);    
}

void DirPlay::init(int size) {
    if ( size >= 0 ) 
        init_dir_stack(size);
    else
        dir_stack->clear();
    read_error = 0;
    //dir_stack->clear();
    dinf_dir.init();
    dinf_file.init();
    cur_dir_file.close();
    file_count = loop_count = 0;
    if ( root_path_len == 0 ) {
        cur_path[0] = '/';
        root_path_len = 1;
    }
    cur_path[cur_dir_path_len = cur_path_len = root_path_len] = 0;
}

bool DirPlay::Reset() {
    init(-1);
    if ( cur_dir_file.open(cur_path, O_RDONLY) ) {
        cur_dir_file.rewind();
        return true;
    }
    return false;
}

bool DirPlay::Restart() {
    read_error = 0;
    file_count = loop_count = 0;
    cur_dir_file.close();
    cur_path[cur_dir_path_len] = 0;
    return cur_dir_file.open(cur_path, O_RDONLY);
}
