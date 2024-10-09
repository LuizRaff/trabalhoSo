// https://www-di.inf.puc-rio.br/~endler/courses/inf1316/
/*
Alunos: Luiz Eduardo Raffaini (2220982) & Isabela Braconi ()
Professor: Endler 
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>

void processo(int pidKernel){
    printf("meu pai: %d\n", pidKernel);
    kill(pidKernel, SIGINT);
    printf("Sou %d\n", getpid());
}

volatile sig_atomic_t syscallFlag = 0;

void kernelHandler(int signal) {
    if (signal == SIGINT) {
        printf("Kernel recebeu um syscall de um processo\n");
        syscallFlag = 1; // Atualiza a flag de syscall
    }
}

int main(void){
    int pid = getpid();
    printf("Kernel criado com pid %d\n\n", pid);
    signal(SIGINT, kernelHandler);

    if (fork() == 0){
        processo(pid);
        exit(0);
    }

    // Loop principal do kernel
    for (;;) {
        if (syscallFlag) {
            printf("Processando syscall\n");
            syscallFlag = 0; // Reseta a flag de syscall após processar
            return 0;
        }
        // Outras operações do kernel
        sleep(1);
    }

    return 0;
}