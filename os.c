#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fuse.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define MAX_SUBNODES 10

typedef struct __node {
    char* name;
    unsigned short int permission;
    unsigned short int isDir;
    struct timespec ts[2];
    char* content;

    struct __node **subnode;
    struct __node *parentnode;
    int subnode_number;    //number of subnodes
}node;

typedef node* nodeptr;
/*find path. If path cannot be entered, -ENOENT returns.
 *If path can be entered, returns pointer of the node.
 * */
static nodeptr find_path(const char* path)
{
    struct fuse_context *context = fuse_get_context();
    nodeptr root = context->private_data;

    char *p = (char*)malloc(strlen(path)+1);
    strcpy(p, path);

    char* id ;
    id = strtok(p, "/");

    nodeptr now = root;
    while(id != NULL)
    {
        int idx = 0;


        if(now->subnode_number == 0){
            free(p);
            return -ENOENT;
        }
        for(idx = 0; idx < now->subnode_number; idx++)
        {
            if(now->subnode[idx] != NULL && strcmp(id, now->subnode[idx]->name) == 0)
            {
                now = now->subnode[idx];
                break;
            }
            if(idx+1 == now->subnode_number){
                free(p);
                return -ENOENT;
            }
        }
        id = strtok(NULL, "/");
        if(now->isDir == 0 && id != NULL){
            free(p);
            return -ENOTDIR;
        }
        if(!(now->permission & 0444)){
            free(p);
            return -EACCES;
        }
    }
    free(p);
    return now;
}


static void recursive_free(nodeptr node){
    for(int i=0; i<node->subnode_number; i++){
        recursive_free(node->subnode[i]);
    }
    free(node->subnode);
    free(node->content);
    free(node->name);
    free(node);
}

static int os_open(const char* path, struct fuse_file_info* fi)
{
    struct fuse_context *context = fuse_get_context();
    nodeptr root = context->private_data;
    nodeptr walk;
    int pathidx = 0;
    if(fi == 0) fi = (struct fuse_file_info*)malloc(sizeof(struct fuse_file_info));
    //if(fi->fh != NULL) return -EBADFD;
    //if(curnode != NULL) return -EBADFD;

    if(path[0] == '/')  walk = root;
    else                walk = root;
    walk = find_path(path);
    if((long)walk < 0 && (long)walk > -140)
    {
        return walk;
    }
    fi->fh = walk;

    clock_gettime(CLOCK_REALTIME, &walk->ts[0]);
    return 0;
}

static int os_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    size_t len;
    if(!fi || !fi->fh){
        fi = (struct fuse_file_info*)malloc(sizeof(struct fuse_file_info));
        int ecode = os_open(path, fi);
        if(ecode != 0)  return ecode;
    }
    if(((node*)(fi->fh))->content == NULL){
        return 0;
    }
    len = strlen(((node*)(fi->fh))->content) + 1;
    //len = strlen(curnode->content);
    if(offset < len){
        if(offset + size > len)
            size = len - offset;
        memcpy(buf, ((node*)(fi->fh))->content + offset, size);
    }
    else size = 0;

    return size;
}

static int os_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    int len;
    if(!fi || !fi->fh){
        fi = (struct fuse_file_info*)malloc(sizeof(struct fuse_file_info));
        int ecode = os_open(path, fi);
        if(ecode != 0)  return ecode;
    }
    if(!(((node*)(fi->fh))->permission & 0222))
        return -EACCES;

    len = strlen(buf);
    if(offset < len){
        if(offset + size > len)
            size = len - offset;
        ((node*)(fi->fh))->content = realloc(((node*)(fi->fh))->content, size);
        memcpy(((node*)(fi->fh))->content, buf + offset, size);
    }
    else size = 0;

    clock_gettime(CLOCK_REALTIME, &((node*)(fi->fh))->ts[0]);
    clock_gettime(CLOCK_REALTIME, &((node*)(fi->fh))->ts[1]);
    return size;
}
/*
static int os_mknod(const char* path, mode_t mode, dev_t dev){
    char* cptr, *pth;
    int idx = strlen(path);
    nodeptr pos;
    nodeptr npt;
    cptr = strrchr(path, '/');
    if(cptr != 0) idx = strlen(path) - strlen(cptr) + 1;    //get last '/' in path

    if(idx != strlen(path)){
        pth = (char*)malloc(sizeof(char)*(idx+2));
        strncpy(pth, path, idx);
        pth[idx] = 0;
        pos = find_path(pth);           //build path except last filename
        free(pth);
    }

    if((long)pos > -140 && (long)pos < 0)
        return pos;

    if(!(pos->permission | 0222))
        return -EACCES;

    pos->subnode[pos->subnode_number] = (nodeptr)malloc(sizeof(node));
    npt = pos->subnode[pos->subnode_number];
    npt->parentnode = pos;
    npt->permission = mode;
    if(idx == strlen(path)){
        npt->name = (char*)malloc(sizeof(char) * strlen(path));
        strcpy(npt->name, path);
    }
    else{
        npt->name = (char*)malloc(sizeof(char) * strlen(cptr+1));
        strcpy(npt->name, cptr + 1);
    }        
    npt->isDir = 0;
    ++pos->subnode_number;
    return 0;
}
*/
static int os_create(const char* path, mode_t mode, struct fuse_file_info* fi){
    char* cptr, *pth;
    int idx = strlen(path);
    nodeptr pos;
    nodeptr npt;
    cptr = strrchr(path, '/');
    if(cptr != 0) idx = strlen(path) - strlen(cptr) + 1;    //get last '/' in path

    if(idx != strlen(path)){
        pth = (char*)malloc(sizeof(char)*(idx+2));
        strncpy(pth, path, idx);
        pth[idx] = 0;
        pos = find_path(pth);           //build path except last filename
        free(pth);
    }

    if((long)pos > -140 && (long)pos < 0)
        return pos;

    if(!(pos->permission & 0222))
        return -EACCES;

    pos->subnode[pos->subnode_number] = (nodeptr)malloc(sizeof(node));
    npt = pos->subnode[pos->subnode_number];
    npt->parentnode = pos;
    npt->permission = mode & 0777;
    if(idx == strlen(path)){
        npt->name = (char*)malloc(sizeof(char) * strlen(path));
        strcpy(npt->name, path);
    }
    else{
        npt->name = (char*)malloc(sizeof(char) * strlen(cptr+1));
        strcpy(npt->name, cptr + 1);
    }
    npt->isDir = 0;
    ++pos->subnode_number;
    fi = (struct fuse_file_info*)malloc(sizeof(struct fuse_file_info));
    fi->fh = npt;
    
    clock_gettime(CLOCK_REALTIME, &npt->ts[0]);
    clock_gettime(CLOCK_REALTIME, &npt->ts[1]);

    return 0;
}

static int os_rename(const char* path, const char* buf, unsigned int flags){
    nodeptr ptr = find_path(path);
    char *cptr;
    cptr = strrchr(buf, '/');
    ++cptr;
    if((long)ptr < 0 && (long)ptr > -140)      return ptr;
    if(!(ptr->permission & 0222))
        return -EACCES;
    ptr->name = realloc(ptr->name, strlen(cptr)+1);
    strcpy(ptr->name, cptr);

    clock_gettime(CLOCK_REALTIME, &ptr->ts[0]);
    clock_gettime(CLOCK_REALTIME, &ptr->ts[1]);
    return 0;
}

static int os_chmod(const char* path, mode_t mode, struct fuse_file_info* fi){
    if(!fi || !fi->fh){
        fi = (struct fuse_file_info*)malloc(sizeof(struct fuse_file_info));
        int ecode = os_open(path, fi);
        if(ecode != 0)  return ecode;
    }
    ((node*)(fi->fh))->permission = mode;

    clock_gettime(CLOCK_REALTIME, &((node*)(fi->fh))->ts[0]);
    clock_gettime(CLOCK_REALTIME, &((node*)(fi->fh))->ts[1]);
    return 0;
}

static int os_getattr(const char* path, struct stat* attr, struct fuse_file_info* fi){
    nodeptr ptr;
    if(fi == 0) ptr = find_path(path);
    else        ptr = fi->fh;
    if((long)ptr < 0 && (long)ptr > -140)      return ptr;

    memset(attr, 0, sizeof(struct stat));

    if(ptr->isDir == 0){
        if(ptr->content != 0){
            attr->st_size = strlen(ptr->content);
        }
        else attr->st_size = 0;
        attr->st_nlink = 1;
        attr->st_mode = ptr->permission | __S_IFREG;
    }
    else{
        attr->st_mode = ptr->permission | __S_IFDIR;
        attr->st_nlink = 2;
    }
    attr->st_atime = ptr->ts[0].tv_sec;
    attr->st_mtime = ptr->ts[1].tv_sec;
    //attr->st_atimensec = ptr->ts[0].tv_nsec;
    //attr->st_mtimensec = ptr->ts[1].tv_nsec;
    return 0;
}

static int os_flush(const char* path, struct fuse_file_info* fi)
{
    if(fi == NULL)return 0;
    (fi->fh) = NULL;
    return 0;
}

static int os_truncate(const char* path, off_t offset, struct fuse_file_info* fi){
    if(!fi || !fi->fh){
        fi = (struct fuse_file_info*)malloc(sizeof(struct fuse_file_info));
        int ecode = os_open(path, fi);
        if(ecode != 0)  return ecode;
    }

    if(!(((node*)(fi->fh))->permission & 0222))
        return -EACCES;

    ((node*)(fi->fh))->content = realloc(((node*)(fi->fh))->content, offset);
    clock_gettime(CLOCK_REALTIME, &((node*)(fi->fh))->ts[0]);
    clock_gettime(CLOCK_REALTIME, &((node*)(fi->fh))->ts[1]);
    return 0;
}

static int os_opendir(const char* path, struct fuse_file_info* fi){
    fi = (struct fuse_file_info*)malloc(sizeof(struct fuse_file_info));
    int ecode = os_open(path, fi);
    if(ecode != 0)     return ecode;                  //When error occurs
    return 0;
}

static void os_destroy(void* private_data)
{
    nodeptr walk = private_data;
    recursive_free(walk);
    return 0;
}

static int os_readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi)
{
    if(!fi || !fi->fh){
        fi = (struct fuse_file_info*)malloc(sizeof(struct fuse_file_info));
        int ecode = os_open(path, fi);
        if(ecode != 0)  return ecode;
    }
    filler(buffer, ".", NULL, 0);
    filler(buffer, "..", NULL, 0);

    for (int i = 0; i < ((node*)fi->fh)->subnode_number; i++)
    {
        if(filler(buffer, ((node*)fi->fh)->subnode[i]->name, NULL, 0))return 0;
    }
    return 0;
}

static int os_mkdir(const char * path, mode_t mode)
{
    char* cptr, *pth;
    int idx = strlen(path);
    nodeptr pos;
    nodeptr npt;
    cptr = strrchr(path, '/');
    if(cptr != 0) idx = strlen(path) - strlen(cptr) + 1;    //get last '/' in path

    if(idx != strlen(path)){
        pth = (char*)malloc(sizeof(char)*(idx+2));
        strncpy(pth, path, idx);
        pth[idx] = 0;
        pos = find_path(pth);           //build path except last filename
        free(pth);
    }

    if((long)pos > -140 && (long)pos < 0)
        return pos;

    if(!(pos->permission & 0222))
        return -EACCES;

    pos->subnode[pos->subnode_number] = (nodeptr)malloc(sizeof(node));
    npt = pos->subnode[pos->subnode_number];
    npt->parentnode = pos;
    npt->permission = mode;
    npt->subnode = (nodeptr*)malloc(sizeof(node)*MAX_SUBNODES);
    if(idx == strlen(path)){
        npt->name = (char*)malloc(sizeof(char) * strlen(path));
        strcpy(npt->name, path);
    }
    else{
        npt->name = (char*)malloc(sizeof(char) * strlen(cptr+1));
        strcpy(npt->name, cptr + 1);
    }
    npt->isDir = 1;
    ++pos->subnode_number;
    clock_gettime(CLOCK_REALTIME, &npt->ts[0]);
    clock_gettime(CLOCK_REALTIME, &npt->ts[1]);
    return 0;
}

static int os_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info *fi){
    if(1){
        fi = (struct fuse_file_info*)malloc(sizeof(struct fuse_file_info));
        int ecode = os_open(path, fi);
        if(ecode != 0)  return ecode;
    }
    if(!(((node*)(fi->fh))->permission & 0222))
        return -EACCES;
    ((node*)(fi->fh))->ts[0] = tv[0];
    ((node*)(fi->fh))->ts[1] = tv[1];
    return 0;
}

static void* os_init(struct fuse_conn_info* conn, struct fuse_config *cfg){
    nodeptr root = (nodeptr)malloc(sizeof(node));
    root->isDir = 1;
    root->name = (char*)malloc(sizeof(char)*2);
    strcpy(root->name, "/");
    root->permission = 0775;
    root->subnode = (nodeptr*)malloc(sizeof(nodeptr)*MAX_SUBNODES);
    root->subnode_number = 0;
    return root;
}


static int os_unlink(const char* path){
    nodeptr ptr = find_path(path);
    if((long)ptr < 0 && (long)ptr > -140)   return ptr;
    if(ptr->isDir)                          return -EISDIR;
    nodeptr parent = ptr->parentnode;
    for(int i=0; i<parent->subnode_number; i++){
        if(parent->subnode[i] == ptr){
            parent->subnode[i] = parent->subnode[parent->subnode_number - 1];
        }
    }
    --parent->subnode_number;
    free(ptr->content);
    free(ptr->name);
    free(ptr);
    return 0;
}

static int os_rmdir(const char* path){
    nodeptr ptr = find_path(path);
    if((long)ptr < 0 && (long)ptr > -140)   return ptr;
    if(!(ptr->isDir))                       return -ENOTDIR;
    nodeptr parent = ptr->parentnode;
    for(int i=0; i<parent->subnode_number; i++){
        if(parent->subnode[i] == ptr){
            parent->subnode[i] = parent->subnode[parent->subnode_number - 1];
        }
    }
    --parent->subnode_number;
    recursive_free(ptr);
    return 0;
}

static struct fuse_operations os_oper = {
    .open       = os_open,
    .read       = os_read,
    .rename     = os_rename,
    .chmod      = os_chmod,
    .truncate   = os_truncate,
    .opendir    = os_opendir,
    .readdir    = os_readdir,
    .destroy    = os_destroy,
    .create     = os_create,
    .write      = os_write,
    .flush      = os_flush,
//    .mknod      = os_mknod,
    .destroy    = os_destroy,
    .mkdir      = os_mkdir,
    .getattr    = os_getattr,
    .init       = os_init,
    .utimens    = os_utimens,
    .unlink     = os_unlink,
    .rmdir      = os_rmdir,
    .flag_nullpath_ok = 0,
};

int main(int argc, char* argv[])
{
    fuse_main(argc, argv, &os_oper, NULL);
    return 0;
}