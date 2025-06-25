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
    return;
  }

  queue->front = queue->front->next;
  if(queue->front == NULL) 
    queue->final = NULL;
}

void print_queue_status(Queue *queue, const char *queue_name) {
  if (queue->front == NULL) {
    printf("  %s: vazia\n", queue_name);
    return;
  }

  Node *current = queue->front;
  printf("  %s: ", queue_name);
  while (current != NULL) {
    printf("P%d(%d) ", current->process->pid, current->process->execution_time);
    current = current->next;
  }
  printf("\n");
}

void print_system_status(Queue *high_priority_queue, Queue *low_priority_queue, Queue *io_queue, Process *current_process, int time, int quantum_time) {
  printf("\n=== TEMPO %d ===\n", time);
  printf("Status do Sistema:\n");
  print_queue_status(high_priority_queue, "Fila Alta Prioridade");
  print_queue_status(low_priority_queue, "Fila Baixa Prioridade");
  print_queue_status(io_queue, "Fila I/O");
  
  if (current_process != NULL) {
    printf("Processo em Execução: P%d (tempo restante: %d, quantum: %d/%d)\n", 
           current_process->pid, current_process->execution_time, quantum_time, QUANTUM);
  } else {
    printf("Processo em Execução: nenhum\n");
  }
  printf("----------------------------------------\n");
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
      continue; // skip comments and empty lines
    }

    if (sscanf(row, "%s %d %d %d %d %d %d",
               name, &pid, &execution_time,
               &priority, &arrival_time, &state, &io_type) == 7)
    {
      if (arrival_time == current_time)
      {
        // Reserves memory for the new process
        Process *process = (Process *)malloc(sizeof(Process));
        if (process == NULL)
        {
          printf("Erro: Falha na alocação de memória para processo\n");
          continue;
        }

        // Assigns values to the process
        process->pid = pid;
        process->execution_time = execution_time;
        process->priority = priority;
        process->arrival_time = arrival_time;
        process->state = (State)state;
        process->io = (IOtype)io_type;
        switch(process->io) {
          case NONE:
            process->io_time = 0; // No I/O
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

        printf(">> Novo processo P%d criado (execução: %d, I/O: %d, prioridade: %d)\n",
               pid, execution_time, io_type, priority);

        if (priority == 1)
        {
          // Hhigher priority
          enqueue(high_priority_queue, process);
          printf(">> P%d enfileirado na fila de alta prioridade\n", pid);
        }
        else
        {
          // lower priority	
          enqueue(low_priority_queue, process);
          printf(">> P%d enfileirado na fila de baixa prioridade\n", pid);
        }
        break;
      
      }
      else
      {
        // different arrival_time: pointer go bback to the position before reading
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
    printf(">> Executando P%d (tempo restante: %d, quantum: %d/%d)\n", 
           process->pid, process->execution_time, *quantum_time + 1, QUANTUM);
    (*quantum_time)++;
}

void end_process(Process *process, Queue *queue) {
        process->state = FINALIZADO;
        printf(">> P%d FINALIZADO\n", process->pid);
        dequeue(queue);
}

void realocate_process(Process *process, Queue *high_priority_queue, Queue *low_priority_queue, Queue *io_queue) {
  if (process->io == NONE) {
    if(high_priority_queue->front != NULL && process == high_priority_queue->front->process) {
      printf(">> P%d movido para fila de baixa prioridade (quantum esgotado)\n", process->pid);
      enqueue(low_priority_queue, process);
      dequeue_without_free(high_priority_queue);
    } else if(low_priority_queue->front != NULL && process == low_priority_queue->front->process) {
      printf(">> P%d movido para fila de alta prioridade\n", process->pid);
      enqueue(high_priority_queue, process);
      dequeue_without_free(low_priority_queue);
    }
  } else {
    printf(">> P%d movido para fila de I/O (dispositivo %d)\n", process->pid, process->io);
    enqueue(io_queue, process);
    if (high_priority_queue->front != NULL && process == high_priority_queue->front->process) {
      dequeue_without_free(high_priority_queue);
    } else if (low_priority_queue->front != NULL && process == low_priority_queue->front->process) {
      dequeue_without_free(low_priority_queue);
    }
  }
}

void execute_io_queue(Queue *high_priority_queue, Queue *low_priority_queue, Queue *io_queue) {
  if (io_queue->front == NULL) {
    return;
  }

  Node *current = io_queue->front;
  while (current != NULL) {
    Process *process = current->process;
    if (process->io_time > 0) {
      printf(">> P%d realizando I/O no dispositivo %d (tempo restante: %d)\n", 
             process->pid, process->io, process->io_time);
      process->io_time--;
    } else {
      printf(">> P%d finalizou I/O e foi reenfileirado\n", process->pid);
      switch(process->io) {
        case DISCO:
          process->io = NONE;
          enqueue(low_priority_queue, process);
          break;
        case FITA:
          process->io = NONE;
          enqueue(high_priority_queue, process);
          break;
        case IMPRESSORA:
          process->io = NONE;
          enqueue(high_priority_queue, process);
          break;
        default:
          break;
      }
      
      dequeue_without_free(io_queue);
    }
    current = current->next;
  }
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
    print_system_status(&high_priority_queue, &low_priority_queue, &io_queue, current_process, time, quantum_time);
    
    read_file_and_delegate_queue(&high_priority_queue, &low_priority_queue, file, time);

    if (current_process == NULL) {
      if(high_priority_queue.front != NULL) {
        current_process = high_priority_queue.front->process;
        current_queue = &high_priority_queue;
        printf(">> Selecionando P%d da fila de alta prioridade\n", current_process->pid);
      } else if(low_priority_queue.front != NULL) {
        current_process = low_priority_queue.front->process;
        current_queue = &low_priority_queue;
        printf(">> Selecionando P%d da fila de baixa prioridade\n", current_process->pid);
      } else {
        printf(">> Nenhum processo disponível para executar\n");
        current_process = NULL;
        current_queue = NULL;
      }
    }

    if(current_process != NULL) {
      execute_process(current_process, &quantum_time);
      
      if (current_process->execution_time <= 0)
      {
        end_process(current_process, current_queue);
        quantum_time = 0;
        current_process = NULL; 
        current_queue = NULL;
      } else if (current_process->io != NONE) {
        realocate_process(current_process, &high_priority_queue, &low_priority_queue, &io_queue);
        quantum_time = 0;
        current_process = NULL; 
        current_queue = NULL;
      } else if (quantum_time >= QUANTUM)
      {
        printf(">> Quantum de %d atingido\n", QUANTUM);
        realocate_process(current_process, &high_priority_queue, &low_priority_queue, &io_queue);
        quantum_time = 0;
        current_process = NULL; 
        current_queue = NULL;
      }
    }

    execute_io_queue(&high_priority_queue, &low_priority_queue, &io_queue);
    time++;
  }
  
  fclose(file);
}

int main()
{
  printf("=== SIMULADOR DE ESCALONAMENTO ROUND ROBIN COM FEEDBACK ===\n");
  printf("Quantum: %d unidades de tempo\n", QUANTUM);
  printf("Dispositivos I/O: Disco(%d), Fita(%d), Impressora(%d)\n", DISC, TAPE, PRINTER);
  printf("========================================================\n");
  
  RoundRobinWithFeedback();
  
  printf("\n=== SIMULAÇÃO FINALIZADA ===\n");
  return 0;
}

