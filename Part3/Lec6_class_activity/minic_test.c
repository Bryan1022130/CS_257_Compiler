/*Create a miniC program to do the following:

1. Define a function that takes 1 parameter named p.

2. The function body should include the following:

A call to the read function to read input from the user
An if-else statement to check if the value read in the step 
above is greater than the parameter, and do the following:
If the value entered by the user is greater than p, then return p.
 Otherwise, return the value entered by the user. 
*/

extern void print(int);
extern int read();

int func(int p){
	int x = read(); 
    if (x > p){
        return p; 
    }
    else {
        return x; 
    }
}

