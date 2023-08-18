#include "kernel/types.h"
#include "user/user.h"

void primes(int *fd){
    close(fd[1]);//关闭写端
    int pipes[2];
    
    int num=0;
    int mid=0;
    read(fd[0],&mid,sizeof(mid));
    
    if(mid==0)
        return;
    if(pipe(pipes)<0){
        fprintf(2,"error: failed pipe");
        exit(1);
    }
    fprintf(1,"prime %d\n",mid);//每次打印第一个读到的数字
    int p=fork();
    if(p==0){
        primes(pipes);
    }
    else if(p>0){
        close(pipes[0]);//关闭读端
        while(read(fd[0],&num,sizeof(num))){
            if(num%mid!=0)
                write(pipes[1],&num,sizeof(num));
        }
        close(pipes[1]);
        close(fd[0]);
        wait((int*)0);
    }
}


int main(int argc, char* argv[]){
    if(argc>1){
        fprintf(2,"error:wrong number");
        exit(1);
    }
    int pipes[2];
    if(pipe(pipes)<0){
        fprintf(2,"error: failed pipe");
        exit(1);
    }

    if(fork() == 0){
        primes(pipes);
    }else {
        close(pipes[0]);
        for(int i = 2; i <= 35; i ++){
            write(pipes[1], &i, sizeof(i));
        }
        close(pipes[1]);
        wait((int*) 0);//等待子进程结束
    }

    exit(0);
}