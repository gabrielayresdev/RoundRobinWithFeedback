#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define QUANTUM 4
#define MAX_PROCESSOS 5
#define PRINTER 8
#define DISC 4
#define TAPE 6

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
  int priority;
  int arrival_time;
  State state;
  IOtype io;
  int IOfullfilled;
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


void dequeue(Queue *queue)
{
  if (queue->front == NULL)
  {
    printf("Queue is empty, nothing to dequeue.\n");
    return;
  }

  Node *temp = queue->front;
  queue->front = queue->front->next;

  if (queue->front == NULL)
    queue->final = NULL; // Queue is now empty

  free(temp->process); // Free the process memory
  free(temp);          // Free the node memory
}

void dequeue_without_free(Queue *queue) {
    if (queue->front == NULL)
  {
    printf("Queue is empty, nothing to dequeue.\n");
    return;
  }

  queue->front = queue->front->next;
  if(queue->front == NULL) 
    queue->final = NULL;
}

void read_file_and_delegate_queue(Queue *high_priority_queue, Queue *low_priority_queue, FILE *file, int current_time)
{
  char row[256];
  char name[50];
  int pid, execution_time, io_time, priority, arrival_time, state, io_type;

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

    if (sscanf(row, "%s %d %d %d %d %d %d",
               name, &pid, &execution_time,
               &priority, &arrival_time, &state, &io_type) == 7)
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
        process->priority = priority;
        process->arrival_time = arrival_time;
        process->state = (State)state;
        process->io = (IOtype)io_type;
        switch(process->io) {
          case NONE:
            process->io_time = 0; // Nenhum I/O
            break;
          case DISCO:
            process->io_time = DISC;
            break;
          case FITA: 
            process->io_time = TAPE;
            break;
          case IMPRESSORA:
            process->io_time = PRINTER;
            break;
        }

        printf("Processo %d criado com tempo de execução %d, I/O %d, dispositivo %d, prioridade %d, tempo de chegada %d\n",
               pid, execution_time, io_time, io_type, priority, arrival_time);

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

void execute_process(Process *process, int *quantum_time)
{
    process->state = EXECUTANDO;
    process->execution_time--;
    printf("Executando processo %d por 1 unidade de tempo (tempo restante: %d)\n", process->pid, process->execution_time);
    (*quantum_time)++;
}

void end_process(Process *process, Queue *queue) {
// Processo finalizado
        process->state = FINALIZADO;
        printf("Processo %d finalizado\n", process->pid);
        dequeue(queue);
}

void realocate_process(Process *process, Queue *high_priority_queue, Queue *low_priority_queue, Queue *io_queue) {
  if (process->io == NONE) {
    if(high_priority_queue->front != NULL && process == high_priority_queue->front->process) {
      printf("O processo %d foi movido para a fila de baixa prioridadea\n", process->pid);
      enqueue(low_priority_queue, process);
      dequeue_without_free(high_priority_queue);
    } else if(low_priority_queue != NULL && process == low_priority_queue->front->process) {
      printf("O processo %d foi movido para a fila de alta prioridadea\n", process->pid);
      enqueue(high_priority_queue, process);
      dequeue_without_free(low_priority_queue);
    }
  } else {
    printf("O processo %d foi movido para a fila de io para realizar um acesso ao disco \n", process->pid);
    enqueue(io_queue, process);
    dequeue_without_free(process == high_priority_queue->front->process ? high_priority_queue : low_priority_queue);
  }
}

void execute_io_queue(Queue *high_priority_queue, Queue *low_priority_queue, Queue *io_queue) {
  if (io_queue->front == NULL) {
    printf("Fila de I/O está vazia, nada para executar.\n");
    return;
  }

  Node *current = io_queue->front;
  while (current != NULL) {
    Process *process = current->process;
    if (process->io_time > 0) {
      printf("Processo %d está realizando I/O no dispositivo %d por 1 unidade de tempo (tempo restante: %d)\n", process->pid, process->io, process->io_time);
      process->io_time--;
    } else {
      printf("Processo %d finalizou o I/O e está pronto para ser reencaminhado.\n", process->pid);
      switch(process->io) {
        case DISCO:
          process->io = NONE; // Resetando o tipo de I/O
          enqueue(low_priority_queue, process); // Reenfileira na fila correta
          break;
        case FITA:
          process->io = NONE; // Resetando o tipo de I/O
          enqueue(high_priority_queue, process); // Reenfileira na fila de alta prioridade
          break;
        case IMPRESSORA:
          process->io = NONE; // Resetando o tipo de I/O
          enqueue(high_priority_queue, process); // Reenfileira na fila de alta prioridade
          break;
        default:
          printf("Processo %d não está realizando I/O.\n", process->pid);
          break;
      }
      
      dequeue_without_free(io_queue); // Remove o processo da fila de I/O
    }
    current = current->next;
  }
}

void print_queue(Queue *queue) {
  if (queue->front == NULL) {
    printf("Fila vazia.\n");
    return;
  }

  Node *current = queue->front;
  printf("Fila: ");
  while (current != NULL) {
    printf("%d ", current->process->pid);
    current = current->next;
  }
  printf("\n");
}

void RoundRobinWithFeedback()
{
  Queue high_priority_queue = {NULL, NULL};
  Queue low_priority_queue = {NULL, NULL};
  Queue io_queue = {NULL, NULL};

  FILE *file = fopen("process.txt", "r");
  if (file == NULL)
  {
    printf("Erro: Não foi possível abrir o arquivo process.txt\n");
    return;
  }

  int time = 0;
  int quantum_time = 0;
  Process *current_process = NULL;
  Queue *current_queue = NULL;

  while (time < 60)
  {
    printf("\n--- Tempo %d ---\n", time);
    read_file_and_delegate_queue(&high_priority_queue, &low_priority_queue, file, time);

    if (current_process == NULL) {
      printf("pegando novo processo");
      if(high_priority_queue.front != NULL) {
        printf("pegando processo da fila de alta prioridade\n");
        print_queue(&high_priority_queue);
        current_process = high_priority_queue.front->process;
        current_queue = &high_priority_queue;
      } else if(low_priority_queue.front != NULL) {
        printf("pegando processo da fila de baixa prioridade\n");
        print_queue(&low_priority_queue);
        current_process = low_priority_queue.front->process;
        current_queue = &low_priority_queue;
      } else {
        printf("Nenhum processo disponível para executar\n");
        current_process = NULL;
        current_queue = NULL;
      }
    }

    if(current_process != NULL) {
      // Simula a execução do processo
      execute_process(current_process, &quantum_time);
      
      if (current_process->execution_time <= 0)
      {
        end_process(current_process, current_queue);
        quantum_time = 0; // Reseta o quantum
        current_process = NULL; current_queue = NULL;
      } else if (current_process->io != NONE) {
        // Se o processo precisa de I/O, realoca para a fila de I/O
        printf("Processo %d requer I/O, movendo para a fila de I/O\n", current_process->pid);
        realocate_process(current_process, &high_priority_queue, &low_priority_queue, &io_queue);
        quantum_time = 0; // Reseta o quantum
        current_process = NULL; current_queue = NULL;
      }else if (quantum_time >= QUANTUM)
      {
        // Se o quantum for atingido, move o processo para a fila de baixa prioridade
        printf("Quantum de %d atingido\n", QUANTUM);
        realocate_process(current_process, &high_priority_queue, &low_priority_queue, &io_queue);
        quantum_time = 0; // Reseta o quantum
        current_process = NULL; current_queue = NULL;
      }
    }


    /*     if (high_priority_queue.front != NULL)
    {
      // Executa processo da fila de alta prioridade
      current_process = high_priority_queue.front->process;
      printf("Executando processo %d da fila de alta prioridade\n", current_process->pid);

      // Simula a execução do processo
      execute_process(current_process, &quantum_time);

      if (current_process->execution_time <= 0)
      {
        end_process(current_process, &high_priority_queue);
        free(current_process);
        quantum_time = 0; // Reseta o quantum
      }
      if(current_process->io != NONE) {
        // Se o processo precisa de I/O, realoca para a fila de I/O
        printf("Processo %d requer I/O, movendo para a fila de I/O\n", current_process->pid);
        realocate_process(current_process, &high_priority_queue, &low_priority_queue, &io_queue);
        quantum_time = 0; // Reseta o quantum
      }
      if (quantum_time >= QUANTUM)
      {
        // Se o quantum for atingido, move o processo para a fila de baixa prioridade
        printf("Quantum de %d atingido\n", QUANTUM);
        realocate_process(current_process, &high_priority_queue, &low_priority_queue, &io_queue);
        quantum_time = 0; // Reseta o quantum
      }
    }
    else if (low_priority_queue.front != NULL)
    {
      // Executa processo da fila de baixa prioridade
      current_process = low_priority_queue.front->process;
      printf("Executando processo %d da fila de baixa prioridade\n", current_process->pid);

      
      // Simula a execução do processo
      execute_process(current_process, &quantum_time);

      if (current_process->execution_time <= 0)
      {
        end_process(current_process, &low_priority_queue);
        free(current_process);
        quantum_time = 0; // Reseta o quantum
      }
      if(current_process->io != NONE) {
        // Se o processo precisa de I/O, realoca para a fila de I/O
        printf("Processo %d requer I/O, movendo para a fila de I/O\n", current_process->pid);
        realocate_process(current_process, &high_priority_queue, &low_priority_queue, &io_queue);
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
    } */
    else
    {
      printf("Nenhum processo para executar no momento.\n");
    }
    execute_io_queue(&high_priority_queue, &low_priority_queue, &io_queue);
    time++;
  }
}

int main()
{
  RoundRobinWithFeedback();
  return 0;
}
