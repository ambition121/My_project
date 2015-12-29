#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
 between_character(char buffer[], char strout[], char strin[] ,char *out)  //we get the character that between strout and strin,the out is the character that return main
{  
 	char *str1, *str2, *token, *subtoken;  
    char *saveptr1, *saveptr2;  
    int j;      
    for (j = 1, str1 = buffer;  ; j++, str1 = NULL) 
    {  
		token = strtok_r(str1, strout, &saveptr1);  
      	if (token == NULL)  
        	break;  
          	printf("%d: %s\n", j, token);   
      		if(j == 3)
      		{
            	for (str2 = token; ; str2 = NULL) 
            	{  
            		subtoken = strtok_r(str2, strin, &saveptr2);  
                	if (subtoken == NULL)  
                		break;  
               		printf(" --> %s\n", subtoken); 
                	memcpy(out,subtoken,strlen(subtoken));
             	}  
         	}
     }  
    // return subtoken;
}  