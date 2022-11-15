#include <stdio.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <string.h>

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

typedef struct dictionary
{
	char **dict_arr;
	char *word;
	int *pos;
	char letter;
	int times;
} dictionary;

typedef struct info
{
	long mesg_type;
	int size, attempts;
	char first, last;
} info;

void handler(int signum);
size_t my_strlen(const char *s);
char *my_strcpy(char *destination, char *source);
int my_strcmp(const char *a, const char *b);
void initialize_struct(dictionary *d);
void read_dictionary(char *s, dictionary *d);
void get_random_word(dictionary *d, info *in);
void reverse(char s[]);
void itoa(int n, char s[]);
void free_struct(dictionary *d);

int main(int argc, char *argv[])
{
	dictionary d;
	info in;
	memory *shmptr;
	key_t key;
	int flag = 0, cnt = 0;
	int msgid, shmid, i, f, l, length, pid;
	char *s;
	sigset_t s1, s2;

	if (argc < 2)
	{
		printf("Give the name of the dictionary file\n");
		exit(-1);
	}

	if (argc > 2)
	{
		printf("Too many arguments supplied.\n");
		exit(-1);
	}

	pid = getpid();
	s = malloc(80);
	if (s == NULL)
	{
		printf("Out of memory");
		exit(-1);
	}

	initialize_struct(&d);
	read_dictionary(argv[1], &d);

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

	shmid = shmget(key, 1024, 0666 | IPC_CREAT);

	if (shmid < 0)
	{
		printf("error creating shared memory segment\n");
		perror("shared memory creation");
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

	if ((msgrcv(msgid, &message, sizeof(message), 1, 0)) < 0)
	{

		printf("error receiving message\n");
		perror("receive message");
		exit(-1);
	}
	if (strcmp(message.mesg_text, "hi") == 0)
	{
		get_random_word(&d, &in);
	}

	length = strlen(d.word);
	in.size = length;
	itoa(length, s);
	strcpy(message.mesg_text, s);

	if ((msgsnd(msgid, &message, sizeof(message), 0)) < 0)
	{

		printf("error sending length\n");
		perror("msgsnd");
		exit(-1);
	}

	f = (int)d.word[0];

	itoa(f, s);

	strcpy(message.mesg_text, s);

	sleep(1);

	if ((msgsnd(msgid, &message, sizeof(message), 0)) < 0)
	{

		printf("error sending first letter\n");
		perror("msgsnd");
		exit(-1);
	}

	l = (int)d.word[length - 1];

	itoa(l, s);

	strcpy(message.mesg_text, s);

	sleep(1);

	if ((msgsnd(msgid, &message, sizeof(message), 0)) < 0)
	{

		printf("error sending last letter\n");
		perror("msgsnd");
		exit(-1);
	}

	in.attempts = 3;
	itoa(in.attempts, s);
	strcpy(message.mesg_text, s);

	sleep(1);

	if ((msgsnd(msgid, &message, sizeof(message), 0)) < 0)
	{

		printf("error sending number of attempts\n");
		perror("msgsnd");
		exit(-1);
	}

	shmptr->attempts = 3;
	shmptr->found = 0;

	shmptr->pid1 = pid;

	struct sigaction action;
	action.sa_handler = &handler;
	action.sa_flags = SA_RESTART;
	if ((sigaction(SIGUSR1, &action, NULL)) < 0)
	{
		printf("error signal\n");
		perror("sigaction");
		exit(-1);
	}
	sigemptyset(&s1);
	sigaddset(&s1, SIGUSR1);
	sigprocmask(SIG_BLOCK, &s1, &s2);

	while (1)
	{

		sigsuspend(&s2);
		cnt = 0;

		for (i = 1; i < (in.size - 1); i++)
		{

			if (shmptr->buff[0] == d.word[i])
			{

				shmptr->pos[cnt] = i;
				d.pos[cnt] = i;
				cnt++;
			}
		}

		if (cnt > 0)
		{

			shmptr->found += cnt;
			if ((shmptr->found) == (strlen(d.word) - 2))
			{
				flag = 1;
				strcpy(shmptr->buff, "win");
				printf("client won\n");
			}
			else
				strcpy(shmptr->buff, "yes");

			shmptr->times = cnt;
		}
		else
		{

			shmptr->attempts--;
			if (shmptr->attempts == 0)
			{
				flag = 1;
				strcpy(shmptr->buff, "lost");
				printf("client lost\n");
			}
			else
				strcpy(shmptr->buff, "no");
		}

		kill(shmptr->pid2, SIGUSR2);
		if (flag == 1)
			break;
	}

	free_struct(&d);
	free(s);

	if ((shmdt((void *)shmptr)) < 0)
	{
		printf("error detaching shared memory segment\n");
		perror("shmdt");
		exit(-1);
	}

	if ((shmctl(shmid, IPC_RMID, NULL)) < 0)
	{
		printf("error destroyng shared memory\n");
		perror("shmctl");
		exit(-1);
	}

	return 0;
}

void handler(int signum)
{

	if (signum == SIGUSR1)
	{

		write(STDOUT_FILENO, "letter received\n", 16);
	}
}

void initialize_struct(dictionary *d)
{

	int i, j;

	d->dict_arr = malloc(150 * sizeof(char *));
	if (d->dict_arr == NULL)
	{
		printf("Out of memory");
		exit(-1);
	}

	d->word = malloc(50);
	if (d->word == NULL)
	{
		printf("Out of memory");
		exit(-1);
	}

	d->pos = malloc(80 * sizeof(int));
	if (d->pos == NULL)
	{
		printf("Out of memory");
		exit(-1);
	}
}

void read_dictionary(char *s, dictionary *d)
{

	int fd, readret;
	int i = 0, j = 0;
	int l = 0, n = 0;
	char x;
	char buf[200][200];

	char *m = malloc(1000);
	if (m == NULL)
	{
		printf("Out of memory");
		exit(-1);
	}

	if ((fd = open(s, O_RDONLY)) < 0)
	{
		printf("error opening file\n");
		perror("open failed");
		exit(-1);
	}

	do
	{

		readret = read(fd, &x, sizeof(char));

		if (x == '\n')
		{
			x = '\0';
			m[l] = x;
			strcpy(buf[n], m);
			n++;
			l = 0;
		}
		else
		{
			m[l] = x;
			l++;
		}
	} while (readret > 0);

	for (int i = 0; i < 100; i++)
	{
		d->dict_arr[i] = buf[i];
	}

	free(m);
}

void get_random_word(dictionary *d, info *in)
{
	int num;
	time_t t;

	srand((unsigned)time(&t));
	num = (rand() % 100);
	strcpy(d->word, d->dict_arr[num]);
	printf("word is : %s\n", d->word);
	in->size = strlen(d->word);
}

void reverse(char s[])
{
	int i, j;
	char c;

	for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
	{
		c = s[i];
		s[i] = s[j];
		s[j] = c;
	}
}

void itoa(int n, char s[])
{
	int i, sign;

	if ((sign = n) < 0)
		n = -n;
	i = 0;
	do
	{
		s[i++] = n % 10 + '0';
	} while ((n /= 10) > 0);
	if (sign < 0)
		s[i++] = '-';
	s[i] = '\0';
	reverse(s);
}

void free_struct(dictionary *d)
{
	free(d->dict_arr);
	free(d->pos);
	free(d->word);
}
