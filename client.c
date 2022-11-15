#include <stdio.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#define MAX 1000

struct mesg_buffer
{
	long mesg_type;
	char mesg_text[100];
} message;

typedef struct memory
{
	char buff[100];
	int status, pid1, pid2;
	int times;
	int pos[100];
	int attempts;
	int found;

} memory;

typedef struct info
{
	int size, attempts;
	char first, last;
} info;

typedef struct game
{
	char *word;
	char *letter;
	int exist;
	int *pos;
	int found;
	int times;
} game;

void clrscr(void);
void handler(int signum);
void win(void);
void initialize_struct(game *g);
void initialize_game(info *in, game *g);
void get_input(game *g);
void update_word(info *in, game *g);
void not_exist_update(memory *shmptr, info *in, game *g);
void exist_update(memory *shmptr, info *in, game *g);
void game_over(void);
void free_struct(game *g);

int main(int argc, char *argv[])
{
	memory *shmptr;
	key_t key;
	int msgid, shmid, i, pid;
	info in;
	game g;
	sigset_t s, s2;

	pid = getpid();
	if ((key = ftok("server.c", 65)) == (key_t)-1)
	{
		perror("IPC error: ftok");
		exit(-1);
	}

	msgid = msgget(key, 0666 | IPC_CREAT);
	if (msgid < 0)
	{
		printf("error creating message queue\n");
		perror("msgget");
		exit(-1);
	}
	message.mesg_type = 1;

	strcpy(message.mesg_text, "hi");

	if ((msgsnd(msgid, &message, sizeof(message), 0)) < 0)
	{

		printf("error sending message\n");
		perror("msgsnd");
		exit(-1);
	}

	if ((msgrcv(msgid, &message, sizeof(message), 1, 0)) < 0)
	{
		printf("error receiving length\n");
		perror("msgrcv");
		exit(-1);
	}
	else
		printf("length received\n");

	in.size = atoi(message.mesg_text);

	if ((msgrcv(msgid, &message, sizeof(message), 1, 0)) < 0)
	{
		printf("error receiving first letter\n");
		perror("receive msgrcv");
		exit(-1);
	}
	else
		printf("first letter received\n");

	i = atoi(message.mesg_text);
	in.first = (char)i;

	if ((msgrcv(msgid, &message, sizeof(message), 1, 0)) < 0)
	{
		printf("error receiving last letter\n");
		perror("msgrcv");
		exit(-1);
	}
	else
		printf("last letter received\n");

	i = atoi(message.mesg_text);
	in.last = (char)i;

	if ((msgrcv(msgid, &message, sizeof(message), 1, 0)) < 0)
	{
		printf("error receiving number of attempts\n");
		perror("msgrcv");
		exit(-1);
	}
	else
		printf("number of attempts received\n");

	in.attempts = atoi(message.mesg_text);

	if ((msgctl(msgid, IPC_RMID, NULL)) < 0)
	{
		printf("error destrying message queue\n");
		perror("msgctl");
		exit(-1);
	}

	shmid = shmget(key, sizeof(struct memory), IPC_CREAT | 0666);
	if (shmid < 0)
	{
		printf("failed creating shared memory \n");
		perror("shmget");
		exit(-1);
	}
	else
		printf("shared memory successfully created\n");

	if ((shmptr = (struct memory *)shmat(shmid, NULL, 0)) < 0)
	{
		printf("error attaching to shared memory\n");
		perror("shmat");
		exit(-1);
	}
	shmptr->pid2 = pid;

	struct sigaction action;
	action.sa_handler = &handler;
	action.sa_flags = SA_RESTART;
	if (sigaction(SIGUSR2, &action, NULL) < 0)
	{
		printf("error signal\n");
		perror("sigaction");
		exit(-1);
	}
	sigemptyset(&s);
	sigaddset(&s, SIGUSR2);
	sigprocmask(SIG_BLOCK, &s, &s2);

	for (i = 0; i < 80; i++)
	{
		shmptr->pos[i] = 0;
	}

	initialize_struct(&g);
	initialize_game(&in, &g);

	while (1)
	{
		printf("enter a letter: ");
		scanf("%s", shmptr->buff);
		strcpy(g.letter, shmptr->buff);

		kill(shmptr->pid1, SIGUSR1);

		sigsuspend(&s2);

		if (strcmp(shmptr->buff, "yes") == 0)
		{
			exist_update(shmptr, &in, &g);
		}
		else if (strcmp(shmptr->buff, "win") == 0)
		{
			win();
			break;
		}
		else if (strcmp(shmptr->buff, "no") == 0)
		{
			not_exist_update(shmptr, &in, &g);
		}
		else if (strcmp(shmptr->buff, "lost") == 0)
		{

			game_over();
			break;
		}
	}

	if ((shmdt((void *)shmptr)) < 0)
	{
		printf("error detaching shared memory segment\n");
		perror("shmdt");
		exit(-1);
	}

	free_struct(&g);

	return 0;
}

void clrscr(void)
{
	system("clear");
	return;
}

void handler(int signum)
{
	if (signum == SIGUSR2)
	{
		write(STDOUT_FILENO, "Signal received\n", 16);
	}
}

void win(void)
{
	clrscr();
	printf("CONGRATULATIONS, YOU FOUND THE WORD\n");
}

void initialize_struct(game *g)
{
	g->word = malloc(80);
	if (g->word == NULL)
	{
		printf("Out of memory");
		exit(-1);
	}
	g->letter = malloc(80);
	if (g->letter == NULL)
	{
		printf("Out of memory");
		exit(-1);
	}
	g->pos = malloc(80);
	if (g->pos == NULL)
	{
		printf("Out of memory");
		exit(-1);
	}
}

void initialize_word(info *in, game *g)
{
	int i;

	g->word = malloc(in->size);
	g->word[0] = in->first;
	g->word[in->size - 1] = in->last;

	for (i = 1; i < (in->size) - 1; i++)
	{
		g->word[i] = '_';
	}

	for (i = 0; i < (in->size); i++)
	{
		printf("%c  ", g->word[i]);
	}
}

void initialize_game(info *in, game *g)
{
	g->found = 0;
	clrscr();
	printf("Welcome!!\n\n\n\n");
	initialize_word(in, g);
	printf("\n\n\nletters found: %d\t\t\t\t", g->found);
	printf("%d attempts left\n", in->attempts);
}

void get_input(game *g)
{
	printf("\n\nenter a letter:\n");
	scanf("%s", g->letter);

	if ((strlen(g->letter) < 1) || (strlen(g->letter) > 1))
		do
		{
			printf("Please enter only one character\n");
			scanf("%s", g->letter);
		} while ((strlen(g->letter) < 1) || (strlen(g->letter) > 1));
}

void update_word(info *in, game *g)
{
	int i;

	for (i = 0; i < (in->size); i++)
	{
		printf("%c  ", g->word[i]);
	}
}

void not_exist_update(memory *shmptr, info *in, game *g)
{
	sleep(1);
	clrscr();
	printf("try again\n");
	update_word(in, g);
	printf("\n\n\nletters found: %d\t\t\t\t", shmptr->found);
	printf("%d attempts left\n", shmptr->attempts);
}

void exist_update(memory *shmptr, info *in, game *g)
{
	int i;

	sleep(1);
	clrscr();
	for (i = 0; i < shmptr->times; i++)
	{
		g->word[shmptr->pos[i]] = g->letter[0];
	}
	printf("letter is: %s\n", g->letter);
	update_word(in, g);
	printf("\n\n\nletters found: %d\t\t\t\t", shmptr->found);
	printf("%d attempts left\n", shmptr->attempts);
}

void game_over(void)
{
	clrscr();
	printf("GAMEOVER!!!\n");
}

void free_struct(game *g)
{
	free(g->word);
	free(g->letter);
	free(g->pos);
}
