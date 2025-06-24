#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define QUANTUM 4
#define MAX_PROCESSOS 5

typedef enum
{
  PRONTO,
  EXECUTANDO,
  FINALIZADO
} State;
typedef enum
{
  NONE,
  DISCO,
  FITA,
  IMPRESSORA
} IOtype;

typedef struct
{
  int pid;
  int execution_time;
  int io_time;
  int io_device;
  int priority;
  int arrival_time;
  State state;
  IOtype io;
} Process;

typedef struct Node
{
  Process *process;
  struct Node *next;
} Node;

typedef struct
{
  Node *front;
  Node *final;
} Queue;

void enqueue(Queue *queue, Process *process)
{
  Node *new_node = (Node *)malloc(sizeof(Node));
  if (new_node == NULL)
  {
    perror("Failed to allocate memory for new node");
    exit(EXIT_FAILURE);
  }

  new_node->process = process;
  new_node->next = NULL;

  if (queue->final == NULL)
  {
    // Queue is empty
    queue->front = new_node;
    queue->final = new_node;
  }
  else
  {
    // Queue has elements
    queue->final->next = new_node;
    queue->final = new_node;
  }
}

void read_file_and_delegate_queue(Queue *high_priority_queue, Queue *low_priority_queue, FILE *file, int current_time)
{
  char row[256];
  char name[50];
  int pid, execution_time, io_time, io_device, priority, arrival_time, state, io_type;

  while (1)
  {
    long pos = ftell(file); // remember position before reading

    if (fgets(row, sizeof(row), file) == NULL)
      break; // EOF or error

    if (row[0] == '#' || row[0] == '\n')
    {
      printf("Linha ignorada: %s", row);
      continue; // skip comments and empty lines
    }

    if (sscanf(row, "%s %d %d %d %d %d %d %d %d",
               name, &pid, &execution_time, &io_time, &io_device,
               &priority, &arrival_time, &state, &io_type) == 9)
    {
      if (arrival_time == current_time)
      {
        // Aloca memória para o processo
        Process *process = (Process *)malloc(sizeof(Process));
        if (process == NULL)
        {
          printf("Erro: Falha na alocação de memória para processo\n");
          continue;
        }

        // Preenche os dados do processo
        process->pid = pid;
        process->execution_time = execution_time;
        process->io_time = io_time;
        process->io_device = io_device;
        process->priority = priority;
        process->arrival_time = arrival_time;
        process->state = (State)state;
        process->io = (IOtype)io_type;

        // Enfileira baseado na prioridade
        switch (process->io)
        {
        case NONE:
          if (priority == 1)
          {
            // Alta prioridade
            enqueue(high_priority_queue, process);
            printf("Processo %d enfileirado na fila de alta prioridade (tempo %d)\n", pid, current_time);
          }
          else
          {
            // Baixa prioridade (prioridade 2 e 3)
            enqueue(low_priority_queue, process);
            printf("Processo %d enfileirado na fila de baixa prioridade (tempo %d)\n", pid, current_time);
          }
          break;
        case DISCO:
          enqueue(low_priority_queue, process);
          printf("Processo %d enfileirado na fila de baixa prioridade (tempo %d)\n", pid, current_time);
          break;
        case FITA:
          enqueue(high_priority_queue, process);
          printf("Processo %d enfileirado na fila de alta prioridade (tempo %d)\n", pid, current_time);
          break;
        case IMPRESSORA:
          enqueue(high_priority_queue, process);
          printf("Processo %d enfileirado na fila de alta prioridade (tempo %d)\n", pid, current_time);
          break;
        }
      }
      else
      {
        // arrival_time diferente: volta o ponteiro e sai
        fseek(file, pos, SEEK_SET);
        return;
      }
    }
  }
}

void MLQF()
{
  Queue high_priority_queue = {NULL, NULL};
  Queue low_priority_queue = {NULL, NULL};

  FILE *file = fopen("process.txt", "r");
  if (file == NULL)
  {
    printf("Erro: Não foi possível abrir o arquivo proccess.txt\n");
    return;
  }

  int time = 0;
  int quantum_time = 0;
  Process *current_process = NULL;

  while (time < 20)
  {
    printf("\n--- Tempo %d ---\n", time);
    read_file_and_delegate_queue(&high_priority_queue, &low_priority_queue, file, time);

    if (high_priority_queue.front != NULL)
    {
      // Executa processo da fila de alta prioridade
      current_process = high_priority_queue.front->process;
      current_process->state = EXECUTANDO;
      printf("Executando processo %d da fila de alta prioridade\n", current_process->pid);

      // Simula a execução do processo

      current_process->execution_time--;
      quantum_time++;
      printf("Processo %d executado por 1 unidade de tempo (tempo restante: %d)\n", current_process->pid, current_process->execution_time);

      if (current_process->execution_time <= 0)
      {
        // Processo finalizado
        current_process->state = FINALIZADO;
        printf("Processo %d finalizado\n", current_process->pid);
        free(current_process);
        high_priority_queue.front = high_priority_queue.front->next;
        if (high_priority_queue.front == NULL)
          high_priority_queue.final = NULL;
        quantum_time = 0; // Reseta o quantum
      }
      if (quantum_time >= QUANTUM)
      {
        // Se o quantum for atingido, move o processo para a fila de baixa prioridade
        printf("Quantum de %d atingido, movendo processo %d para a fila de baixa prioridade\n", QUANTUM, current_process->pid);
        enqueue(&high_priority_queue, current_process);
        high_priority_queue.front = high_priority_queue.front->next;
        if (high_priority_queue.front == NULL)
          high_priority_queue.final = NULL;
        quantum_time = 0; // Reseta o quantum
      }
    }
    else if (low_priority_queue.front != NULL)
    {
      // Executa processo da fila de baixa prioridade
      current_process = low_priority_queue.front->process;
      current_process->state = EXECUTANDO;
      printf("Executando processo %d da fila de baixa prioridade\n", current_process->pid);

      current_process->execution_time--;
      quantum_time++;
      printf("Processo %d executado por 1 unidade de tempo (tempo restante: %d)\n", current_process->pid, current_process->execution_time);

      if (current_process->execution_time <= 0)
      {
        // Processo finalizado
        current_process->state = FINALIZADO;
        printf("Processo %d finalizado\n", current_process->pid);
        free(current_process);
        low_priority_queue.front = low_priority_queue.front->next;
        if (low_priority_queue.front == NULL)
          low_priority_queue.final = NULL;
        quantum_time = 0; // Reseta o quantum
      }
      if (quantum_time >= QUANTUM)
      {
        // Se o quantum for atingido, move o processo para a fila de baixa prioridade
        printf("Quantum de %d atingido, movendo processo %d para a fila de baixa prioridade\n", QUANTUM, current_process->pid);
        enqueue(&high_priority_queue, current_process);
        high_priority_queue.front = high_priority_queue.front->next;
        if (high_priority_queue.front == NULL)
          high_priority_queue.final = NULL;
        quantum_time = 0; // Reseta o quantum
      }
    }
    else
    {
      printf("Nenhum processo para executar no momento.\n");
    }
    time++;
  }
}

int main()
{
  MLQF();
  return 0;
}
