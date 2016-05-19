#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define INST_LENGTH 50
#define ARGS_NUM 15
#define BLOCK_SIZE 512
#define NUM_INST 50

struct buffer_data
{
    int in_no;
    int out_no;
    int proc_done;
    int out_done;
    char block[BLOCK_SIZE];
};

struct buffer_data *pbuffer, *in;
pthread_mutex_t mutex;
pthread_cond_t startCond, endCond, doneCond;

int i = 0;
char instructions[NUM_INST][INST_LENGTH + 1];


struct thread_args
{
    char s[NUM_INST][INST_LENGTH + 1];
    FILE *fi, *fo;
    int blocksize;
};

/* Reads a line and discards the end of line character */
int getLine(char *str,int max, FILE *fp);

/* Breaks the string pointed by str into words and stores them in the arg array*/
int getArguments(char *str, char *arg[]);

struct buffer_data *check_block_no(int block_no);


void *readdl(void *rdarg)
{
    int j, num;
    char *args[ARGS_NUM];
    struct buffer_data *pb;
    struct thread_args *targ = (struct thread_args *) rdarg;
     
    for (j = 0; j < i; j++)
    {   
        pthread_mutex_lock(&mutex);               
        getArguments(targ->s[j], args); 
        pb = check_block_no(atoi(args[1]));
        
        if (pb == NULL)
        {   /* not find block_no in the memory */
            pb = in;
                     
            fseek(targ->fi, (atoi(args[1]) - 1) * targ->blocksize, SEEK_SET);
            fread(pb->block, 1, targ->blocksize, targ->fi);
            pb->in_no = atoi(args[1]);
            pb->out_no = atoi(args[2]);
            pb->proc_done = 0;
            pb->out_done = 0;
            in++;  
            pthread_cond_broadcast (&startCond);
            
        } 
        pthread_mutex_unlock(&mutex);        
    }    
    
    pthread_exit(NULL);
}

void *processdl(void *parg)
{
    int j, k, n;
    char *args[ARGS_NUM];
    struct thread_args *targ = (struct thread_args *) parg;
    struct buffer_data *pb;
        
    for (j = 0; j < i; j++)
    {
        pthread_mutex_lock(&mutex);
        getArguments(targ->s[j], args);
        pb = check_block_no(atoi(args[1]));
    
        while (pb == NULL)
        {   
            pthread_cond_wait(&startCond, &mutex); 
            pb = check_block_no(atoi(args[1]));
        }
        
        while (pb->proc_done == 1 && pb->out_done == 0)
            pthread_cond_wait(&endCond, &mutex); 
     
        if (!strcmp(args[0], "revert"))
        {   /* revert the block */
            for (k = 0; k < targ->blocksize; k++)
            {   for (n = 3; args[n] != NULL; n++)
                {
                   pb->block[k] = pb->block[k] ^ (1 << atoi(args[n]));
                }
            }
        }
    
        if (!strcmp(args[0], "zero"))
        {   /* zero the block */
            for (k = 0; k < targ->blocksize; k++)
            {   for (n = 3; args[n] != NULL; n++)
                {
                   pb->block[k] = pb->block[k] & (~(1 << atoi(args[n])));
                }
            }
        }
        pb->proc_done = 1;
        if (pb->proc_done ==1 && pb->out_done == 1)
            pb->out_done = 0;
     
        pthread_cond_signal(&doneCond);
        pthread_mutex_unlock(&mutex);
    }          
    pthread_exit(NULL);
}

void *writedl(void *wrarg)
{
    int j;
    char *args[ARGS_NUM];
    struct thread_args *targ = (struct thread_args *) wrarg;
    struct buffer_data *pb;
    
    for (j = 0; j < i; j++)
    {
        pthread_mutex_lock(&mutex);
        getArguments(instructions[j], args);
        pb = check_block_no(atoi(args[1]));
        
        while (pb == NULL)
        {   pthread_cond_wait(&startCond, &mutex); 
            pb = check_block_no(atoi(args[1]));
        }
        
        while (pb->proc_done == 0)
            pthread_cond_wait(&doneCond, &mutex); 
                       
        fseek(targ->fo, (atoi(args[2]) - 1) * targ->blocksize, SEEK_SET);
        fwrite(pb->block, 1, targ->blocksize, targ->fo);
        pb->out_done = 1;
        
        if (pb->proc_done == 1 && pb->out_done == 1)
            pb->proc_done = 0;
        pthread_cond_signal(&endCond);
        
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
	FILE *finst, *fin, *fout;
	char inst[INST_LENGTH + 1];
    pthread_t reader, processor, writer;
    int failed;
    void *status1, *status2, *status3;

	if (argc != 6)
	{
		fprintf(stderr, "usage: %s instructionFileName inputFileName outputFileName blockSize bufferSize\n", argv[0]);
		exit(1);
	}
	
	/* create the buffer */
	pbuffer = (struct buffer_data *) calloc(atoi(argv[5]), sizeof(struct buffer_data));
	if (pbuffer == NULL)
	{
	    perror("memory allocation failed\n");
	    exit(1);
	}
	in = pbuffer;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&startCond, NULL);
    pthread_cond_init(&doneCond, NULL);
    pthread_cond_init(&endCond, NULL);

    fin = fopen(argv[2], "rb");
    fout = fopen(argv[3], "wb");
    finst = fopen(argv[1], "r");
	    
	/* copy the inputfile to outputfile */
    while (fread(pbuffer->block, 1, atoi(argv[4]), fin))
        fwrite(pbuffer->block, 1, atoi(argv[4]), fout);
    fclose(fout);
    fout = fopen(argv[3], "r+b");
    
	/* read instructions from instructionFile */
	while (getLine(inst, INST_LENGTH + 1, finst))
        strcpy(instructions[i++], inst);
   
    int d, m;
    struct thread_args targ[3];
    for (d = 0; d < 3; d++) 
    {   targ[d].fi = fin;
        targ[d].fo = fout;
        targ[d].blocksize = atoi(argv[4]); 
        for (m = 0; m < i; m++)
            strcpy(targ[d].s[m], instructions[m]);
    }
    
        failed = pthread_create(&reader, NULL, readdl, (void*) &targ[0]);
        if (failed) {
            printf("thread_create failed!\n");
            return -1;
        }
         
        failed = pthread_create(&processor, NULL, processdl, (void*) &targ[1]);
        if (failed) {
            printf("thread_create failed!\n");
            return -1;
        }
    
        failed = pthread_create(&writer, NULL, writedl, (void*) &targ[2]);
        if (failed) {
            printf("thread_create failed!\n");
            return -1;
        }
    
    
    pthread_join(reader, &status1);
    pthread_join(writer, &status2);
    pthread_join(processor, &status3);

	fclose(fin);
    fclose(fout);
	fclose(finst);
	free(pbuffer);
	printf("done\n");
	pthread_exit(NULL);

}

/* Breaks the string pointed by str into words and stores them in the arg array*/
int getArguments(char *str, char *arg[])
{
     char delimeter[] = " ";
     char *temp = NULL;
     int i=0;
     temp = strtok(str,delimeter);
     while (temp != NULL)
     {
           arg[i++] = temp;                   
           temp = strtok(NULL,delimeter);
     }
     arg[i] = NULL;     
     return i;
}

/* Reads a line and discards the end of line character */
int getLine(char *str,int max, FILE *fp)
{
    char *temp;
    if ((temp = fgets(str,max,fp)) != NULL)
    {
       int len = strlen(str);
       str[len-1] = '\0';
       return len-1;
    }   
    else return 0;
}


/* check if the processing block is in the buffer.
 * if in the buffer, return the address; otherwise return NULL. */
struct buffer_data *check_block_no(int block_no)
{
    struct buffer_data *temp = pbuffer;

    while (temp != in)
    {
        if (temp->in_no == block_no)
            break;
        temp++;
    }
    
    if (temp != in)
        return temp;
    else
        return NULL;
}
