#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)//该函数作用为获取path路径的最后一项，即文件或者目录的名称
{
  static char buf[DIRSIZ+1];
  char *p;
 
  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;
 
  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  //memset(buf+strlen(p), ' ', DIRSIZ-strlen(p)); 用空格填充，不过在本问无需如此
  buf[strlen(p)]='\0';
  return buf;
}


void find(char* path, char* name){//递归寻找
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;
  
  if((fd = open(path, 0)) < 0){
    fprintf(2, "error: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "error: cannot stat %s\n", path);
    close(fd);
    return;
  }
  
  switch(st.type){
  case T_FILE:
    
    if(strcmp(fmtname(path),name)==0)//返回值为0表示相同
    {
      printf("%s\n", path);
    }
    break;

  case T_DIR:
    
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
      printf("error: path too long\n");
      break;
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      
      if(strcmp(de.name,".")==0||strcmp(de.name,"..")==0)
        continue;
      if(de.inum == 0)
        continue;
      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;
      
      if(stat(buf, &st) < 0){
        printf("erroe: cannot stat %s\n", buf);
        continue;
      }
      
      find(buf,name);
    }
    break;
  }
  close(fd);
}

int main(int argc, char* argv[]){
    if(argc != 3){
        fprintf(2, "error: wrong numbers of arguments!");
        exit(1);
    }

    char* dir = argv[1];
    char* name= argv[2];
    find(dir,name );
    
    exit(0);
    
}
