// https://www-di.inf.puc-rio.br/~endler/courses/inf1316/
/*
Alunos: Luiz Eduardo Raffaini (2220982) & Isabela Braconi ()
Professor: Endler 
*/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <sys/shm.h>

void controller(){
    int pidKernel = getppid(), auxRand;
    srand(time(NULL) ^ (getpid() << 16));
    
    for(;;){
        sleep(0.5);
        kill(pidKernel, SIGHUP); // envia iqr0
        auxRand = rand() % 10000;
        if (auxRand <= 10 && auxRand > 5){
            kill(pidKernel, SIGUSR1); // envia iqr1
        }else if (auxRand <= 5){
            kill(pidKernel, SIGUSR2); // envia iqr2
        }
    }
}

#define MAX 100;

void processo(int pidKernel, int memoriaCompartilhadaD, int memoriaCompartilhadaF){
    raise(SIGSTOP);

    char* accessoMemoriaD = (char *)shmat(memoriaCompartilhadaD, 0, 0);
    char* accessoMemoriaF = (char *)shmat(memoriaCompartilhadaF, 0, 0);
    
    srand(time(NULL) ^ (getpid() << 16));

    raise(SIGSTOP);

    int PC = 0;
    while (PC < 100) {
        int random = rand() % 100;
        char dispotivo, func;
        if (random < 15){
            dispotivo = (random % 2) ? '1': '2';
            if (random % 2){
                func = 'r';
            }else if (random % 3){
                func = 'w';
            }else{
                func = 'x';
            }
            *accessoMemoriaD = dispotivo; *accessoMemoriaF = func;
            printf("Dispositivo: %c : Func %c : sysCallProcess %c%c\n", dispotivo, func, *accessoMemoriaD, *accessoMemoriaF);
            kill(pidKernel, SIGINT);
            raise(SIGSTOP);
        }
        PC++;
    }
    printf("processo terminado!\n");
    kill(pidKernel, SIGTERM);

    // Desanexar a memória compartilhada
    if (shmdt(accessoMemoriaD) == -1 || shmdt(accessoMemoriaF) == -1) {
        perror("Erro ao desanexar memória compartilhada");
        exit(1);
    }
}

volatile sig_atomic_t sysCallProcessFlag = 0;
volatile sig_atomic_t processEndFlag = 0;
volatile sig_atomic_t irq0 = 0;
volatile sig_atomic_t irq1 = 0;
volatile sig_atomic_t irq2 = 0;

void kernelHandler(int signal) {
    if (signal == SIGINT) {
        printf("Kernel recebeu um sysCallProcess de um processo...\n");
        sysCallProcessFlag = 1; // Atualiza a flag de sysCallProcess
    } else if (signal == SIGTERM) {
        printf("Kernel recebeu um end of process de um processo...\n");
        processEndFlag++;
    } else if (signal == SIGHUP){
        irq0 = 1;
    } else if (signal == SIGUSR1){
        printf("Kernel recebeu um sinal IRQ1 do controller...\n");
        irq1 = 1;
    } else if (signal == SIGUSR2){
        printf("Kernel recebeu um sinal IRQ2 do controller...\n");
        irq2 = 1;
    }
}

int main(void){
    int pid = getpid();
    printf("Kernel criado com pid %d\n\n", pid);
    signal(SIGINT, kernelHandler);
    signal(SIGTERM, kernelHandler);
    signal(SIGHUP, kernelHandler);
    signal(SIGUSR1, kernelHandler);
    signal(SIGUSR2, kernelHandler);

    int memoriaDispositivo = shmget(IPC_PRIVATE, sizeof(char), IPC_CREAT | 0666);
    int memoriaFuncao = shmget(IPC_PRIVATE, sizeof(char), IPC_CREAT | 0666);
    if (memoriaDispositivo == -1 || memoriaFuncao == -1) {
        perror("Erro ao criar memória compartilhada");
        exit(1);
    }

    int nProcessos = 0;
    while (nProcessos < 3 || nProcessos > 6){
        printf("Insira o numero de processos (3-6): ");
        scanf("%d", &nProcessos);
    }

    int* processos = (int*)malloc(sizeof(int) * nProcessos);
    for (int i = 0; i < nProcessos; i++)
    {
        processos[i] = fork();
        if (processos[i] == 0){
            processo(pid, memoriaDispositivo, memoriaFuncao);
            exit(0);
        }
    }

    int pidController = fork();
    if(pidController == 0){
        controller(pid);
        exit(0);
    }

    char* accessoMemoriaD = (char *)shmat(memoriaDispositivo, 0, 0);
    char* accessoMemoriaF = (char *)shmat(memoriaFuncao, 0, 0);
    if (accessoMemoriaD == (char *)-1 || accessoMemoriaF == (char *)-1) {
        perror("Erro ao anexar memória compartilhada");
        exit(1);
    }
    
    int processoAtual = 0;
    int* estados = (int*)malloc(sizeof(int) * nProcessos);
    for (int i = 0; i < nProcessos; i++)
    {
        estados[i] = 0;
    }

    // Loop principal do kernel
    for (;;) {
        if (sysCallProcessFlag) {
            printf("Processando sysCallProcess\n");
            sysCallProcessFlag = 0; // Reseta a flag de sysCallProcess após processar
            printf("Syscall: %c%c\n", *accessoMemoriaD, *accessoMemoriaF);
            estados[processoAtual] = 1;

        }
        if(irq0){
            kill(processos[processoAtual], SIGSTOP);
            processoAtual = (processoAtual + 1) % nProcessos;
            kill(processos[processoAtual], SIGCONT);
            irq0 = 0;
        }
        // if(irq1){
            
        // }
        // if(rq2){

        // }
        if (processEndFlag == nProcessos) {
            printf("Todos os processos terminaram!\nTerminando kernel...\n");
            kill(pidController, SIGKILL);
            break;
        }
        // Outras operações do kernel
        sleep(1);
    }

    // Desanexar a memória compartilhada
    if (shmdt(accessoMemoriaD) == -1 || shmdt(accessoMemoriaF) == -1) {
        perror("Erro ao desanexar memória compartilhada");
        exit(1);
    }

    // Remover a memória compartilhada
    if (shmctl(memoriaDispositivo, IPC_RMID, NULL) == -1 || shmctl(memoriaFuncao, IPC_RMID, NULL) == -1) {
        perror("Erro ao remover memória compartilhada");
        exit(1);
    }

    return 0;
}