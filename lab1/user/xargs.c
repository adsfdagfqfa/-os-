
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"

int main(int argc, char* argv[]){
    // 检查参数个数
    if(argc < 3){
        fprintf(2, "error: too few arguments!");
        exit(1);
    }

    char** arguments = (char**)malloc(MAXARG * sizeof(char*));
    for (int i = 0; i < MAXARG; ++i) {
        arguments[i] = (char*)malloc(128);
    };//参数个数为MAXARG，缓冲区为128
    int  i= 0;
    for(i = 1;i < argc; i++){
        arguments[i-1] = argv[i];//复制原有的参数
        if(argv[i]==0)
            break;
    }
    int argnum = i-1;//记录此时的参数个数

    int index = 0;//每一个缓冲区的索引
    int s=0;//标志位
    while(read(0, arguments[argnum]+index, 1) != 0){
        if(arguments[argnum][index] == ' '){
            if(s==1){
                arguments[argnum][index] =0;
                argnum++;//指向下一个位置
            }
            index=0;
            s=0;
            continue;
        }
        if(arguments[argnum][index]== '\n'){
            
            arguments[argnum][index]=0;
            argnum++;
            arguments[argnum]=0;//根据exec代码可知，参数表列最后一项需要等于0
            
            if(fork() == 0){
                exec(arguments[0], arguments);
                exit(1);
            }else {
                wait((int*)0);
            }
        }
        index ++;
        s=1;
    }
    free(arguments);
    exit(0);
}