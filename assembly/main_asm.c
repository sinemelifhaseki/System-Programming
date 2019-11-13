#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int cross_correlation_asm_full(int* arr_1, int size_1, int* arr_2, int size_2, int* output, int output_size);


int main()
{
	int numberOfArrays;
	char buffer[100] = {0};
	
	int** arrays;
	int* arraySizes;
	int lineCount = 0;
	int i, j;
	
	const char arrayFileName[] = "arrays.txt";
	FILE* arrayFile = fopen(arrayFileName, "r");
	if (arrayFile == NULL)
	{
		fprintf(stderr, "Unable to open file %s!\n", arrayFileName);
		return 1;
	}
	
	
	//  read number of arrays
	fgets(buffer, 100, arrayFile);
	sscanf(buffer, "%d", &numberOfArrays);
	printf("%s contains %d arrays\n", arrayFileName, numberOfArrays);
	
    
    //  allocate memory for arrays
    arrays = malloc(numberOfArrays * sizeof(int*));
    if (arrays == NULL)
    {
        fprintf(stderr, "Unable to allocate memory for arrays!\n");
		return 3;
    }
    //  and array sizes
	arraySizes = malloc(numberOfArrays * sizeof(int));
	if (arraySizes == NULL)
	{
		fprintf(stderr, "Unable to allocate memory for arrays!\n");
		return 4;
	}
	
	
	//  read array file line by line
	for (lineCount = 0; lineCount < numberOfArrays; ++lineCount)
	{
		char* tok;
		int numberCount = 0;
		char tokenBuffer[100] = {0};
		int j;
		
		
		//  read line into buffer
		fgets(buffer, 100, arrayFile);
		
		//  count amount of numbers in the line string
		strcpy(tokenBuffer, buffer);
		tok = strtok(tokenBuffer, " \t\n");
		while (tok != NULL)
		{
			++numberCount;
			tok = strtok(NULL, " \t\n");
		}
		//printf("Found %d numbers in line %d\n", numberCount, lineCount);
		arraySizes[lineCount] = numberCount;
		
		
		//  allocate memory for arrays[lineCount]
		arrays[lineCount] = malloc(numberCount * sizeof(int));
		if (arrays[lineCount] == NULL)
		{
			fprintf(stderr, "Unable to allocate memory for array %d\n", lineCount);
			return 2;
		}
		
		//  read numbers
		strcpy(tokenBuffer, buffer);
		tok = strtok(tokenBuffer, " \t\n");
		j = 0;
		while (tok != NULL)
		{
			sscanf(tok, "%d", &(arrays[lineCount][j]));
			tok = strtok(NULL, " \t\n");
			++j;
		}
		
	}
	fclose(arrayFile);
	
	
	
	//  print all readed arrays
	printf("\n\n");
	printf("--- Arrays ---\n");
	for (i = 0; i < numberOfArrays; ++i)
	{
		int j = 0;
		for (j = 0; j < arraySizes[i]; ++j)
		{
			printf("%d ", arrays[i][j]);
		}
		printf("\n");
	}
    
	
	
	
	
	//  open an output file
	FILE* outputFile = fopen("cross_correlation_output_c.txt", "w");
	if (outputFile == NULL)
	{
		fprintf(stderr, "Unable to open output file for correlations!\n");
		return 4;
	}
	
	//  calculate result of all cross correlations
	//and write to output file
	for (i = 0; i < numberOfArrays-1; ++i)
	{
		for (j = i+1; j < numberOfArrays; ++j)
		{
            int output_size = arraySizes[i] + arraySizes[j] - 1;
            int* output = malloc(output_size * sizeof(int));
			int nonzeroCount = cross_correlation_asm_full(arrays[i], arraySizes[i], arrays[j], arraySizes[j], output, output_size);
			
			
			//  write output to file
			int k = 0;
			for (k = 0; k < output_size; ++k)
			{
				fprintf(outputFile, "%d ", output[k]);
			}
			fprintf(outputFile, "\n");
            
            
            //  write number of nonzero elements
            fprintf(outputFile, "%d\n", nonzeroCount);
            
            
            //  deallocate output
            free(output);
		}
	}
    fclose(outputFile);
    
    
    //  deallocate 
    for (i = 0; i < numberOfArrays; ++i)
    {
        free(arrays[i]);
    }
    free(arrays);
    free(arraySizes);
			
	
	return 0;
}
