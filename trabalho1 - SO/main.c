// https://www-di.inf.puc-rio.br/~endler/courses/inf1316/
/*
Alunos: Luiz Eduardo Raffaini (2220982) & Isabela Braconi (2312065)
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

int* terminados;

void controller() {
    int pidKernel = getppid(), auxRand;
    srand(time(NULL) ^ (getpid() << 16));
    
    for(;;) {
        usleep(500000); // 0.5 segundos
        kill(pidKernel, SIGHUP); // envia iqr0
        auxRand = rand() % 100;
        if (auxRand <= 10 && auxRand > 5) {
            kill(pidKernel, SIGUSR1); // envia iqr1
        } else if (auxRand <= 5) {
            kill(pidKernel, SIGUSR2); // envia iqr2
        }
    }
}

void processo(int pidKernel, int memoriaCompartilhadaD, int memoriaCompartilhadaF) {
    raise(SIGSTOP);

    char* accessoMemoriaD = (char *)shmat(memoriaCompartilhadaD, 0, 0);
    char* accessoMemoriaF = (char *)shmat(memoriaCompartilhadaF, 0, 0);
    if (accessoMemoriaD == (char *)-1 || accessoMemoriaF == (char *)-1) {
        perror("Erro ao anexar memória compartilhada");
        exit(1);
    }
    
    srand(time(NULL) ^ (getpid() << 16));

    raise(SIGSTOP);

    int PC = 0, qtdD1 = 0, qtdD2 = 0;
    while (PC < 10) {
        int random = rand() % 100;
        char dispotivo, func;
        if (random < 15) {
            dispotivo = (random % 2);
            if(dispotivo == 1){
                dispotivo = '1';
                qtdD1++;
            }else{
                dispotivo = '2';
                qtdD2++;
            }

            if (random % 2) {
                func = 'r';
            } else if (random % 3) {
                func = 'w';
            } else {
                func = 'x';
            }
            *accessoMemoriaD = dispotivo; 
            *accessoMemoriaF = func;
            kill(pidKernel, SIGINT);
            raise(SIGSTOP);
        }
        PC++;
    }
    kill(pidKernel, SIGTERM);
    printf("Processo %d usou %d o dispositivio D1 e %d o dispositivio D2\n", (getpid()),  qtdD1, qtdD2);
    printf("processo terminado!\n");
    
    terminados[pidKernel - getpid() - 1] = 0;

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
    } else if (signal == SIGHUP) {
        irq0 = 1;
    } else if (signal == SIGUSR1) {
        printf("Kernel recebeu um sinal I/O do D1 do controller...\n");
        irq1 = 1;
    } else if (signal == SIGUSR2) {
        printf("Kernel recebeu um sinal I/O do D2 do controller...\n");
        irq2 = 1;
    }
}

// Estrutura do nó
typedef struct Node {
    int dispositivo;
    int processo;
    struct Node* next;
} Node;

// Estrutura da fila
typedef struct Fila {
    Node* head;
    Node* tail;
} Fila;

// Função para criar um novo nó
Node* create_node(int dispositivo, int processo) {
    Node* new_node = (Node*)malloc(sizeof(Node));
    if (!new_node) {
        perror("Erro ao alocar memória para o nó");
        exit(EXIT_FAILURE);
    }
    new_node->dispositivo = dispositivo;
    new_node->processo = processo;
    new_node->next = NULL;
    return new_node;
}

// Função para verificar se a fila está vazia
int is_empty(Fila* queue) {
    return queue->head == NULL;
}

// Função para inicializar a fila
Fila* create_Fila() {
    Fila* fila = (Fila*)malloc(sizeof(Fila));
    if (!fila) {
        perror("Erro ao alocar memória para a fila");
        exit(EXIT_FAILURE);
    }
    fila->head = NULL;
    fila->tail = NULL;
    return fila;
}

// Função para adicionar um nó ao final da fila
void enFila(Fila* fila, int dispositivo, int processo) {
    Node* new_node = create_node(dispositivo, processo);
    if (is_empty(fila)) {
        fila->head = new_node;
        fila->tail = new_node;
    } else {
        fila->tail->next = new_node;
        fila->tail = new_node;
    }
}

// Função para remover um nó do início da fila
Node* deFila(Fila* fila) {
    if (is_empty(fila)) {
        printf("Fila está vazia\n");
        return NULL;
    }
    Node* temp = fila->head;
    fila->head = fila->head->next;
    if (fila->head == NULL) {
        fila->tail = NULL;
    }
    return temp;
}

// Função para obter o nó no início da fila sem removê-lo
Node* peek(Fila* fila) {
    if (is_empty(fila)) {
        printf("Fila está vazia\n");
        return NULL;
    }
    return fila->head;
}

// Função para liberar a memória da fila
void free_Fila(Fila* fila) {
    while (!is_empty(fila)) {
        Node* temp = deFila(fila);
        free(temp);
    }
    free(fila);
}

int main(void) {
    int pid = getpid();
    printf("Kernel criado com pid %d\n\n", pid);
    signal(SIGINT, kernelHandler);
    signal(SIGTERM, kernelHandler);
    signal(SIGHUP, kernelHandler);
    signal(SIGUSR1, kernelHandler);
    signal(SIGUSR2, kernelHandler);

    int memoriaDispositivo = shmget(IPC_PRIVATE, sizeof(char), IPC_CREAT | S_IRWXU);
    int memoriaFuncao = shmget(IPC_PRIVATE, sizeof(char), IPC_CREAT | S_IRWXU);
    if (memoriaDispositivo == -1 || memoriaFuncao == -1) {
        perror("Erro ao criar memória compartilhada");
        exit(1);
    }

    int nProcessos = 0;
    while (nProcessos < 3 || nProcessos > 6) {
        printf("Insira o numero de processos (3-6): ");
        scanf("%d", &nProcessos);
    }

    int* processos = (int*)malloc(sizeof(int) * nProcessos);
    for (int i = 0; i < nProcessos; i++) {
        processos[i] = fork();
        if (processos[i] == 0) {
            processo(pid, memoriaDispositivo, memoriaFuncao);
            exit(0);
        }
    }

    terminados = (int*)malloc(sizeof(int) * nProcessos);
    for (int i = 0; i < nProcessos; i++)
    {
        terminados[i] = 1;
    }

    int pidController = fork();
    if (pidController == 0) {
        controller();
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
    for (int i = 0; i < nProcessos; i++) {
        estados[i] = 0;
    }

    Fila* fila = create_Fila();

    // Loop principal do kernel
    for (;;) {
        if (sysCallProcessFlag) {
            printf("\nProcessando sysCallProcess %d\n", processoAtual);
            printf("Syscall: %c%c\n", *accessoMemoriaD, *accessoMemoriaF);
            estados[processoAtual] = 1;
            enFila(fila, *accessoMemoriaD - '0', processoAtual);
            sysCallProcessFlag = 0; // Reseta a flag de sysCallProcess após processar
        }

        if (irq1) {
            Node* node = peek(fila);
            if (node != NULL && node->dispositivo == 1) {
                estados[node->processo] = 0;
                kill(processos[node->processo], SIGCONT);
                free(deFila(fila));
                printf("Liberando fila...\n");
            }
            irq1 = 0;
        }

        if (irq2) {
            Node* node = peek(fila);
            if (node != NULL && node->dispositivo == 2) {
                estados[node->processo] = 0;
                kill(processos[node->processo], SIGCONT);
                free(deFila(fila));
                printf("Liberando fila...\n");
            }
            irq2 = 0;
        }
        
        if (irq0) {
            kill(processos[processoAtual], SIGSTOP);

            processoAtual = (processoAtual + 1) % nProcessos;
            for (int i = 0; i < 3; i++)
            {
                if (estados[processoAtual] == 0) {
                    if (terminados[processoAtual]){
                        kill(processos[processoAtual], SIGCONT);
                    break;
                    }
                }
            }

            irq0 = 0;
        }
        if (processEndFlag == nProcessos) {
            printf("Todos os processos terminaram!\nTerminando kernel...\n");
            kill(pidController, SIGKILL);
            break;
        }
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

    free(processos);
    free(estados);
    free_Fila(fila);

    return 0;
}