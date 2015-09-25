#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>
#include<malloc.h>
#include<errno.h>
#define DATA_LEN 2
#define GPIO_NUM_LEN 4
#define file_len 126
#define DEFAULT_GPIO_NUM  "47"
#define GPIO_CLASS_NAME "/sys/class/gpio/export"
int main(int argc,char **argv)
{
char gpio_direction_name[file_len];
char gpio_value_name[file_len];
char *gpio_name;
char gpio_old_num[GPIO_NUM_LEN];
int gpio_num;
int fd_class,fd_direction,fd_value,fd_num;
char  buf[DATA_LEN];
char *gpio_dir[2]={"out","in"};
gpio_name=DEFAULT_GPIO_NUM;

if(argc==2){
gpio_name=malloc(GPIO_NUM_LEN);
gpio_name=argv[1];
}

gpio_num=atoi(gpio_name);
if(gpio_num>192||gpio_num<0){
	printf("inval gpio number\n");
	return EINVAL;
}

sprintf(gpio_direction_name,"/sys/class/gpio/gpio%s/direction",gpio_name);
sprintf(gpio_value_name,"/sys/class/gpio/gpio%s/value",gpio_name);

/*you should make sure the gpio num is not exit*/
if((access(gpio_direction_name,F_OK)==0)||(access(gpio_value_name,F_OK)==0))
goto run;

/*  only gpio direction name not exit!*/
/*first step open gpio class  you should need root and open only 

 *write 

*/
if((fd_class=open(GPIO_CLASS_NAME,O_WRONLY))<0){
	perror("failed open gpio class\n");
	goto err_open_class;
}


/*write  you contrl gpio number */
if(write(fd_class,gpio_name,strlen(gpio_name))<0){
	printf("falied write gpio name (%s)\n",gpio_name);
	goto err_write;
	}
	
/*second  step open gpio direction also need root */
if((fd_direction=open(gpio_direction_name,O_WRONLY))<0){
	perror("failed open gpio direction\n");
	goto err_open_direction;
	}
	
/*write you select gpio direction in or out*/
if(write(fd_direction,gpio_dir[0],strlen(gpio_dir[0]))<0){
	printf("falied write gpio (%s) direction \n",gpio_name);
	goto err_write;
}

run:
/* third setp open gpio value also need root */
if((fd_value=open(gpio_value_name,O_RDWR))<0){
	perror("falied open gpio value\n");
	goto err_open_value;
}
printf("             now you can enter value: \n");
/* write gpio value */
for(;;){
	scanf("%c",buf);
	write(fd_value,buf,1);
	printf("%s\n",buf);
}
err_open_class:
err_open_direction:
err_open_value:
err_write:
if(fd_class)
close(fd_class);
if(fd_direction)
close(fd_direction);
if(fd_value)
close(fd_value);
if(gpio_name)
free(gpio_name);
return 0;

}
