#include <stdlib.h>
#include <stdio.h>

int main(){

	int number[200];
	int number2[4][50];
	int lin = 1;
	int col=20;
	int i = 0;
	int j = 0;

	number2[lin][col]=5;
	//matriz [0 a x][0 a y] to array [0 a z]
	int elem=lin*50+col;

	//array[0 a z] to matrix [0 a x][0 a y]
	printf("%d \n",elem);
	printf("%d %d %d \n",70/50,70%50,number2[70/50][(70)%50]);
	printf("%d %d \n",sizeof(number),sizeof(number2));

	return 0;
}
