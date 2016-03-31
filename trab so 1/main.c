//Jefferson Boldrin Cardozo - 7591671

#include <pthread.h>	//Para pthreads. Compilar com "gcc -pthreads -o 'output' 'arquivo.c'"
#include <unistd.h>		//Para sleep() da animação
#include <semaphore.h>  //Para o semáforo
#include <time.h>       //Para contagem de tempo

#include <stdio.h>
#include <stdlib.h>

//Structs e Typedefs
	//TAD Queue
	typedef struct Node {
		int item;
		struct Node* next;
	} Node;

	typedef struct Queue {
		Node* head;
		Node* tail;
		int size;
	} Queue;

	//Structs de Parada, Ônibus e Passageiro
	typedef struct Stop {
		pthread_t thread;		//A thread processando a parada atual

        int nStop;              //Número do ponto atual
		int carro;				//Valor negativo indica que não há nenhum carro atualmente nesta parada
		Queue passEsperando;	//Fila de passageiros esperando no ponto de ônibus
	} Stop;

	typedef struct Carro {
		pthread_t thread;

		int estado;			//0 = parado, passageiros descendo; 1 = parado, passageiros subindo; 2 = na estrada
		int stop;			//Se parado, mostra parada atual. Se em movimento, mostra próxima parada
		int nCarro;			//Numero do carro atual

		int nPass;		    //Numero de passageiros no ônibus
        int nPassNDesc;     //Número de passageiros que confirmaram que não descem no ponto atual

        time_t horaSaida;   //Tempo que o onibus saiu do ultimo ponto
		int tempoProxStop;	//Tempo necessário para a próxima parada (em segundos)
	} Carro;

	typedef struct Passageiro {
		pthread_t thread;
		int nPass;              //Número do passageiro

		int viajando;			// 1 = passageiro ainda não chegou no destino; 0 = já chegou
		int onibus;				//-1 = não está num onibus; n = no do onibus atual
		int ponto;              //-1 = não está num ponto;  n = no do ponto atual

		int ptoPartida;
		int ptoChegada;

		time_t horaChegada;
        int tempoEspera;

		enum estado {ptoPart = 0, indoDest = 1, Dest = 2, voltDest = 3, fim = 4} estado;
	}Passageiro;



//Variáveis globais
	int S, C, P, A;         //S = bus stops, C = carros, P = passageiros, A = assentos

	sem_t mutex;            //Semáforo
	int terminou = 0;       //Variável que controla se a simulação já terminou ou não
	int passAindaViajando;  //Nº de passageiros ainda viajando
	time_t tempo;           //Variável utilizada com a função time() para dizer o tempo atual

    pthread_t renderer; 	//Thread que cuida da animação

	Stop 		*stops;
	Carro 		*carros;
	Passageiro 	*passageiros;



//Declaração de funções
	void Entrada();			//Recebe e trata a entrada das variáveis S, C, P e A
	void Inicializacao();	//Inicializa as structs e a thread de cada membro
    void uAnimacao();       //Mostra apenas uma tela da animação

	void *Animacao(void *argAnimacao);			//Função da thread de animação
	void *funcStop(void *argStop);				//Função das threads de Paradas
	void *funcCarro(void *argCarro);			//Função das threads de Carros
	void *funcPassageiro(void *argPassageiro);	//Função das threads de Passageiros

	//TAD Queue
	Queue createQueue ();
	void  push(Queue* queue, int item);
	int   pop (Queue* queue);

//Main
	int main (){

		Entrada();
		Inicializacao();

		while (passAindaViajando > 0) { }   //Mantém o programa rodando enquanto a simulação não terminar
        terminou = 1;                       //Sinaliza as threads para terminarem sua execução

        uAnimacao();

        sem_destroy(&mutex);

		return 0;
	}



//Funções Sequenciais
    //Recebe e trata a entrada das variáveis S, C, P e A
	void Entrada(){
		printf("Numero de paradas (S): ");
		scanf("%d", &S);
		while (S <= 0){
			printf("S deve ser um numero positivo e maior que zero, digite novamente: ");
			scanf("%d", &S);
		}

		printf("Numero de carros (C): ");
		scanf("%d", &C);
			while (C <= 0 || C > S){
			printf("C deve ser um numero positivo, maior que zero e menor ou igual a S, digite novamente: ");
			scanf("%d", &C);
		}

		printf("Numero de passageiros (P): ");
		scanf("%d", &P);
		while (P <= 0 || P <= C){
			printf("P deve ser um numero positivo, maior que zero e maior ou igual C, digite novamente: ");
			scanf("%d", &P);
		}

		printf("Numero de assentos (A): ");
		scanf("%d", &A);
			while (A <= 0 || A <= C || A >= P){
			printf("A deve ser um numero positivo, maior que zero, maior ou igual C e menor ou igual a P, digite novamente: ");
			scanf("%d", &A);
		}

		passAindaViajando = P;
	}

    //Inicializa as structs e a thread de cada membro
	void Inicializacao(){
	//Inicializa semáforo e arrays das structs Stop, Carro, Passageiro
		sem_init(&mutex, 0, 1);

		stops 		= malloc(sizeof(Stop) 		* S);
		carros 		= malloc(sizeof(Carro) 		* C);
		passageiros = malloc(sizeof(Passageiro) * P);

		int i;
		//Inicializa Stops
		for (i = 0; i < S; ++i){
			stops[i].carro = -1; 			//Valor neg indica nenhum carro
			stops[i].passEsperando = createQueue();
			stops[i].nStop = i;
		}
		//Inicialize Passageiros
		srand(time(NULL));
		for (i = 0; i < P; ++i){
			passageiros[i].viajando = 1;
			passageiros[i].estado = 0;
			passageiros[i].onibus = -1;
			passageiros[i].nPass = i;

			passageiros[i].ptoPartida = rand() % (S-1);
			passageiros[i].ptoChegada = rand() % (S-1);

			if (passageiros[i].ptoPartida == passageiros[i].ptoChegada){
				if (passageiros[i].ptoPartida == S-1) passageiros[i].ptoChegada = 0;
				else passageiros[i].ptoChegada++;
			}
			passageiros[i].ponto = passageiros[i].ptoPartida;

			push(&stops[passageiros[i].ptoPartida].passEsperando, i);
		}
		//Inicializa Carros
		for (i = 0; i < C; ++i){
			carros[i].estado = 1;	//Inicialmente nenhum passageiro está dentro de um ônibus, então estado inicial é de passageiros embarcando
			carros[i].nCarro = i;

			carros[i].stop = rand() % (S-1);

			while (stops[carros[i].stop].carro != -1) {
				carros[i].stop++;
				if (carros[i].stop >= S) carros[i].stop = 0;
			}
			stops[carros[i].stop].carro = i;
		}

		uAnimacao();

	//Inicializa threads
		//Thread de Animação
		if(pthread_create(&renderer, NULL, Animacao, NULL)) {
			fprintf(stderr, "Erro ao criar a thread de animacao, interrompendo programa.\n");
			terminou = 1;
			return;
		}
		//Threads de Paradas
		for (i = 0; i < S; ++i){
			if(pthread_create(&stops[i].thread, NULL, funcStop, (void *) &stops[i] )) {
				fprintf(stderr, "Erro ao criar uma thread de parada de onibus, interrompendo programa.\n");
				terminou = 1;
				return;
			}
		}
		//Threads de Passageiros
		for (i = 0; i < P; ++i){
			if(pthread_create(&passageiros[i].thread, NULL, funcPassageiro, (void *) &passageiros[i] )) {
				fprintf(stderr, "Erro ao criar uma thread de passageiro, interrompendo programa.\n");
				terminou = 1;
				return;
			}
		}
		//Threads de Carros
		for (i = 0; i < C; ++i){
			if(pthread_create(&carros[i].thread, NULL, funcCarro, (void *) &carros[i] )) {
				fprintf(stderr, "Erro ao criar uma thread de parada de carro, interrompendo programa.\n");
				terminou = 1;
				return;
			}
		}
	}

    //Mostra apenas uma tela da animação. Usada antes de iniciar as threads e depois que a simulação terminou,
    //para se certificar que independente da ordem de execução da thread de animação, será mostrada a tela de antes de a
    //animação começar e depois que ela terminar
    void uAnimacao(){
        printf("\033[H\033[J");
            printf("\nPassageiros ainda em viagem: %d\n", passAindaViajando);
            printf("Paradas (S): \n\n");
            int i;
            for (i = 0; i < S; i++){
                printf("(S%d - P em espera: %d)\t", i, stops[i].passEsperando.size);
                if (stops[i].carro >= 0) printf("(C%d - P a bordo: %d - Est: %d)\n", stops[i].carro, carros[stops[i].carro].nPass, carros[stops[i].carro].estado);
                else printf("(S/ Carro)\n");
            }

            printf("\nOnibus na estrada(C): \n\n");
            for (i = 0; i < C; i++){
                    printf("C%d (S: %d, P a bordo: %d, ", i, carros[i].stop, carros[i].nPass);

                    if (carros[i].tempoProxStop - (time(&tempo) - carros[i].horaSaida) >= 0 && carros[i].estado == 2)
                        printf("ETA: %d, ", carros[i].tempoProxStop - (time(&tempo) - carros[i].horaSaida) );

                    if      (carros[i].estado == 0) printf("Desembacando)\n");
                    else if (carros[i].estado == 1) printf("Embarcando)\n");
                    else if (carros[i].estado == 2) printf("Na estrada)\n");
                //}
            }

            printf("\nPassageiros:\n\n");
            for (i = 0; i < P; i++){
                printf("P%d (%d -> %d) ",  i, passageiros[i].ptoPartida, passageiros[i].ptoChegada);
                if (passageiros[i].estado == ptoPart || passageiros[i].estado == Dest)
                    printf("S: %d, ", passageiros[i].ponto);
                else if (passageiros[i].estado == indoDest || passageiros[i].estado == voltDest)
                    printf("C: %d, ", passageiros[i].onibus);

                if (passageiros[i].estado == ptoPart)           printf("No Pto de Partida\n", (int)passageiros[i].estado);
                else if (passageiros[i].estado == indoDest)     printf("Indo para o Pto de Destino\n");
                else if (passageiros[i].estado == Dest){
                    if (time(&tempo) - passageiros[i].horaChegada < passageiros[i].tempoEspera)
                        printf("Passeando no Pto de Destino\n");
                    else printf("Pronto para voltar do Pto de Destino\n");
                }
                else if (passageiros[i].estado == voltDest)     printf("Voltando para o Pto de Partida\n");
                else if (passageiros[i].estado == fim)          printf("Terminou a Viagem\n");
            }
            sleep(1);
    }


//Funções paralelas
    //Função da thread de animação
	void *Animacao(void *argAnimacao){
	    while(terminou == 0){
            sem_wait(&mutex);

            printf("\033[H\033[J");
            printf("\nPassageiros ainda em viagem: %d\n", passAindaViajando);
            printf("Paradas (S): \n\n");
            int i;
            for (i = 0; i < S; i++){
                printf("(S%d - P em espera: %d)\t", i, stops[i].passEsperando.size);
                if (stops[i].carro >= 0) printf("(C%d - P a bordo: %d - Est: %d)\n", stops[i].carro, carros[stops[i].carro].nPass, carros[stops[i].carro].estado);
                else printf("(S/ Carro)\n");
            }

            printf("\nOnibus na estrada(C): \n\n");
            for (i = 0; i < C; i++){
                    printf("C%d (S: %d, P a bordo: %d, ", i, carros[i].stop, carros[i].nPass);

                    if (carros[i].tempoProxStop - (time(&tempo) - carros[i].horaSaida) >= 0 && carros[i].estado == 2)
                        printf("ETA: %d, ", carros[i].tempoProxStop - (time(&tempo) - carros[i].horaSaida) );

                    if      (carros[i].estado == 0) printf("Desembacando)\n");
                    else if (carros[i].estado == 1) printf("Embarcando)\n");
                    else if (carros[i].estado == 2) printf("Na estrada)\n");
                //}
            }

            printf("\nPassageiros:\n\n");
            for (i = 0; i < P; i++){
                printf("P%d (%d -> %d) ",  i, passageiros[i].ptoPartida, passageiros[i].ptoChegada);
                if (passageiros[i].estado == ptoPart || passageiros[i].estado == Dest)
                    printf("S: %d, ", passageiros[i].ponto);
                else if (passageiros[i].estado == indoDest || passageiros[i].estado == voltDest)
                    printf("C: %d, ", passageiros[i].onibus);

                if (passageiros[i].estado == ptoPart)           printf("No Pto de Partida\n", (int)passageiros[i].estado);
                else if (passageiros[i].estado == indoDest)     printf("Indo para o Pto de Destino\n");
                else if (passageiros[i].estado == Dest){
                    if (time(&tempo) - passageiros[i].horaChegada < passageiros[i].tempoEspera)
                        printf("Passeando no Pto de Destino\n");
                    else printf("Pronto para voltar do Pto de Destino\n");
                }
                else if (passageiros[i].estado == voltDest)     printf("Voltando para o Pto de Partida\n");
                else if (passageiros[i].estado == fim)          printf("Terminou a Viagem\n");
            }
            sem_post(&mutex);

            sleep(1);
        }
	}

    //Função das threads de parada
	void *funcStop(void *objStop){
        Stop *tStop = (Stop *) objStop;

        while (!terminou){
            if (tStop->carro != -1 && carros[tStop->carro].estado == 1){
                sem_wait(&mutex);

                int tamFila = tStop->passEsperando.size;
                int i, p;

                for (i = 0; i < tamFila && carros[tStop->carro].nPass < A; i++){
                    p = pop(&(tStop->passEsperando));

                    if (passageiros[p].estado == ptoPart){
                        passageiros[p].estado = indoDest;
                        passageiros[p].onibus = tStop->carro;
                        carros[tStop->carro].nPass++;

                        passageiros[p].ponto = tStop->nStop;
                    }

                    else if (passageiros[p].estado == Dest && time(&tempo) - passageiros[p].horaChegada >= passageiros[p].tempoEspera){
                        passageiros[p].estado = voltDest;
                        passageiros[p].onibus = tStop->carro;
                        carros[tStop->carro].nPass++;

                        passageiros[p].ponto = tStop->nStop;
                    }
                    else{
                        push(&(tStop->passEsperando), p);
                    }
                }

                sem_post(&mutex);
            }
        }

        pthread_exit(0);
	}

    //Função das threads de carro
	void *funcCarro(void *objCarro){
		Carro *tCarro = (Carro *) objCarro;

		while (!terminou){
			//Se o carro está parado, espera passageiros (des)embarcarem e então parte para a próxima parada.
			if (tCarro->estado == 0){   //Desembarque
                sem_wait(&mutex);

                if (tCarro->nPassNDesc == tCarro->nPass){
                    tCarro->estado = 1;
                }

                sem_post(&mutex);
			}

			else if (tCarro->estado == 1){  //Embarque
                tCarro->nPassNDesc = 0;
				if (stops[tCarro->stop].passEsperando.size == 0 || tCarro->nPass >= A){
					sem_wait(&mutex);

					tCarro->estado = 2;
					stops[tCarro->stop].carro = -1;

					tCarro->stop++;
					if(tCarro->stop >= S) tCarro->stop = 0;

					time(&tCarro->horaSaida);
                    tCarro->tempoProxStop = (rand()%4) + 4;

					sem_post(&mutex);
				}
			}

			//Se o carro está em movimento, diminui gradualmente o tempo para a próxima parada.
			//Ao chegar lá, se a parada estiver vaga, estaciona.
			else if ((tCarro->estado == 2)){    //Viagem
				sem_wait(&mutex);

				if (time(&tempo) - tCarro->horaSaida >= tCarro->tempoProxStop){
                    if (stops[tCarro->stop].carro = -1){
                        tCarro->estado = 0;
                        stops[tCarro->stop].carro = tCarro->nCarro;
                    }
                    else {
                        tCarro-> stop++;
                        if (tCarro->stop >= S) tCarro->stop = 0;

                        time(&tCarro->horaSaida);
                        tCarro->tempoProxStop = (rand()%4) + 4;
                    }
				}

				sem_post(&mutex);
			}
		}

        pthread_exit(0);
	}

    //Função das threads de passageiro
	void *funcPassageiro(void *objPass){
		Passageiro *tPass = (Passageiro *) objPass;

        while (!terminou){
            //Se passageiro está num onibus que está num ponto esperando desembarque
            if (tPass->estado == indoDest){
                if (carros[tPass->onibus].estado == 0 && tPass->ponto != carros[tPass->onibus].stop){
                    sem_wait(&mutex);
                    //Se onibus parou no ponto de destino, desce
                    if (carros[tPass->onibus].stop == tPass->ptoChegada){
                        push(&stops[tPass->ptoChegada].passEsperando, tPass->nPass);

                        carros[tPass->onibus].nPass--;

                        tPass->onibus   = -1;
                        tPass->ponto    = tPass->ptoChegada;
                        tPass->estado   = Dest;

                        time(&(tPass->horaChegada));
                        tPass->tempoEspera = (rand()%5) + 5;
                    }
                    //Senão, espera
                    else{
                        carros[tPass->onibus].nPassNDesc++;
                        tPass->ponto = carros[tPass->onibus].stop;
                    }
                    sem_post(&mutex);
                }
            }

            else if (tPass->estado == voltDest){
                if (carros[tPass->onibus].estado == 0 && tPass->ponto != carros[tPass->onibus].stop){
                    sem_wait(&mutex);
                    //Se onibus voltou pro ponto de partida, desce
                    if (carros[tPass->onibus].stop == tPass->ptoPartida){
                        carros[tPass->onibus].nPass--;

                        tPass->onibus   = -1;
                        tPass->ponto    = tPass->ptoPartida;
                        tPass->estado   = fim;

                        passAindaViajando--;
                    }
                    //Senão, espera
                    else{
                        carros[tPass->onibus].nPassNDesc++;
                        tPass->ponto = carros[tPass->onibus].stop;
                    }
                    sem_post(&mutex);
                }
            }
        }

        pthread_exit(0);
	}



//Funções do TAD Queue
	void push (Queue* queue, int item) {
		Node* n = (Node*) malloc (sizeof(Node));
		n->item = item;
		n->next = NULL;

		if (queue->head == NULL) {
			queue->head = n;
		}
		else{
			queue->tail->next = n;
		}
		queue->tail = n;
		queue->size++;
	}

	int pop (Queue* queue) {
		Node* head = queue->head;
		int item = head->item;
		queue->head = head->next;
		queue->size--;
		free(head);
		return item;
	}

	Queue createQueue () {
		Queue queue;
		queue.size = 0;
		queue.head = NULL;
		queue.tail = NULL;
		return queue;
}
