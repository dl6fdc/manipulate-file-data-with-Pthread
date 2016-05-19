#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INST_LENGTH 50
#define ARGS_NUM 15
#define BLOCK_SIZE 512

struct buffer_data
{
    int block_no;
    char block[BLOCK_SIZE];
};

/* buffer beging address */
struct buffer_data *begin;

/* Reads a line and discards the end of line character */
int getLine(char *str,int max, FILE *fp);

/* Breaks the string pointed by str into words and stores them in the arg array*/
int getArguments(char *str, char *arg[]);

/* process the instructions: revert and zero */
struct buffer_data *process(char *args[], char *infile, char *outfile, int blockSize,  struct buffer_data *pbuffer);

int main(int argc, char *argv[])
{
	FILE *finst, *fin, *fout;
	char inst[INST_LENGTH + 1];
	char *args[ARGS_NUM];
	
	if (argc != 6)
	{
		fprintf(stderr, "usage: %s instructionFileName inputFileName outputFileName blockSize bufferSize\n", argv[0]);
		exit(1);
	}
	
	/* create the buffer */
	struct buffer_data *pbuffer = (struct buffer_data *) calloc(atoi(argv[5]), sizeof(struct buffer_data));
	if (pbuffer == NULL)
	{
	    perror("memory allocation failed\n");
	    exit(1);
	}
	begin = pbuffer;


    /* copy the inputfile to outputfile */
    fin = fopen(argv[2], "rb");
    fout = fopen(argv[3], "wb");
	    
    while (fread(pbuffer->block, 1, atoi(argv[4]), fin))
        fwrite(pbuffer->block, 1, atoi(argv[4]), fout);
    
    fclose(fin);
    fclose(fout);
	
	/* read instructions from instructionFile */
	finst = fopen(argv[1], "r");
	
	while (getLine(inst, INST_LENGTH + 1, finst))
	{
        getArguments(inst, args);
        pbuffer = process(args, argv[2], argv[3], atoi(argv[4]), pbuffer);
    }
	
	fclose(finst);
	free(begin);
	
	return 0;
}

/* check if the processing block is in the buffer.
 * if in the buffer, return the address; otherwise return NULL. */
struct buffer_data *check_block_no(struct buffer_data *pbuffer, int block_no)
{
    struct buffer_data *temp = begin;

    while (temp != pbuffer)
    {
        if (temp->block_no == block_no)
            break;
        temp++;
    }
    
    if (temp != pbuffer)
        return temp;
    else
        return NULL;
}


struct buffer_data *process(char *args[], char *infile, char *outfile, int blockSize,  struct buffer_data *pbuffer)
{
    FILE *fin, *fout;
    int i, j;
    struct buffer_data *pb;
    
    pb = check_block_no(pbuffer, atoi(args[1]));
    
    if (pb == NULL)
    {   /* not find block_no in the memory */
        pb = pbuffer;
        fin = fopen(infile, "rb");
        fseek(fin, (atoi(args[1]) - 1) * blockSize, SEEK_SET);
        
        pb->block_no = atoi(args[1]);

        fread(pb->block, 1, blockSize, fin);
        fclose(fin);
    }   
    
    if (!strcmp(args[0], "revert"))
    {   /* revert the block */
        for (i = 0; i < blockSize; i++)
        {   for (j = 3; args[j] != NULL; j++)
            {
               pb->block[i] = pb->block[i] ^ (1 << atoi(args[j]));
            }
        }
    }
    
    if (!strcmp(args[0], "zero"))
    {   /* zero the block */
        for (i = 0; i < blockSize; i++)
        {   for (j = 3; args[j] != NULL; j++)
            {
               pb->block[i] = pb->block[i] & (~(1 << atoi(args[j])));
            }
        }
    }
    
    fout = fopen(outfile, "r+b");
    fseek(fout, (atoi(args[2]) - 1) * blockSize, SEEK_SET);

    fwrite(pb->block, 1, blockSize, fout);
    fclose(fout);
    
    if (pb == pbuffer)
        return ++pbuffer;
    else
        return pbuffer;  
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
