#include "kernel/types.h"
#include "user/user.h"


int main(int argc, char* argv[]){
    if(argc>1){
        fprintf(2,"error:wrong number");
        exit(1);
    }
    int parent_c[2];//由父进程发送给子进程
    int child_p[2];
    if(pipe(parent_c)<0){
        fprintf(2,"error: failed pipe");
        exit(1);
    }
    if(pipe(child_p)<0){
        fprintf(2,"error: failed pipe");
        exit(1);
    }
    int p=fork();
    char content[32];
    if(p==0){
        close(parent_c[1]);//关闭写端
        read(parent_c[0],content,7);
        close(parent_c[0]);
        printf("%d: received ping\n", getpid());//使用getpid来获取进程号
        close(child_p[0]);
        write(child_p[1],content,sizeof(content));
        close(child_p[1]);
    }
    else if(p>0){
        close(parent_c[0]);//关闭读端
        write(parent_c[1],"hello world!",13);
        close(parent_c[1]);
        wait((int*)0);
        
        close(child_p[1]);
        read(child_p[0],content,sizeof(content));
        printf("%d: received pong\n", getpid());
        close(child_p[0]);
    }
    else
        exit(1);
    exit(0);
}