#include "kernel/types.h"
#include "user/user.h"


int main(int argc, char* argv[]){
    //只接受一个参数
    if(argc != 2){
        fprintf(2, "sleep: wrong numbers of arguments.");
    }
    sleep(atoi(argv[1]));
    exit(0);//释放内存
}