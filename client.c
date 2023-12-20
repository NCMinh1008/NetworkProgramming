/*
Client side:
socket->connect->send/recv->close
*/

#include <string.h>
#include <sys/socket.h> 
#include <unistd.h> 
#include <arpa/inet.h> 
#include <stdio.h>
#include <stdlib.h> 
	 
#define PORT 8000

//------------------------------Function declarations---------------------------------------//

int client(int sock);
int menu(int sock,int type);
int user_function(int sock,int choice);
int crud_flight(int sock,int choice);
int crud_user(int sock,int choice);

int main(int argc, char const *argv[]) { 
	int sock; 
    	struct sockaddr_in server; 
    	char server_reply[50],*server_ip;
		//server_ip = "127.0.0.1"; 
     
    	sock = socket(AF_INET, SOCK_STREAM, 0); 
    	if (sock == -1) { 
       	printf("Could not create socket"); 
    	} 
    
    	server.sin_addr.s_addr = inet_addr(argv[1]); 
    	server.sin_family = AF_INET; 
    	server.sin_port = htons(atoi(argv[2])); 
   
    	if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0)
       	perror("connect failed. Error"); 
    
	while(client(sock)!=3);
    	close(sock); 
    	
	return 0; 
} 

//-------------------- First function which is called-----------------------------//

int client(int sock){
	int choice,valid;
	//system("clear");
	printf("\n\n\t\t\tFLIGHT RESERVATION SYSTEM\n\n");
	printf("\t1. Sign In\n");
	printf("\t2. Sign Up\n");
	printf("\t3. Exit\n");
	printf("\tEnter Your Choice:: ");
	scanf("%d", &choice);
	write(sock, &choice, sizeof(choice));
	if (choice == 1){					// Log in
		int id,type;
		char password[50];
		printf("\tlogin id:: ");
		scanf("%d", &id);
		strcpy(password,getpass("\tpassword:: "));
		write(sock, &id, sizeof(id));
		write(sock, &password, sizeof(password));
		read(sock, &valid, sizeof(valid));
		if(valid){
			printf("\tlogin successfully\n");
			read(sock,&type,sizeof(type));
			while(menu(sock,type)!=-1);
			//system("clear");
			return 1;
		}
		else{
			printf("\tLogin Failed : Incorrect password or login id\n");
			return 1;
		}
	}

	else if(choice == 2){					// Sign up
		int type,id;
		char name[50],password[50],secret_pin[6];
		system("clear");
		printf("\n\tEnter The Type Of Account:: \n");
		printf("\t0. Admin\n\t2. Customer\n");
		printf("\tYour Response: ");
		scanf("%d", &type);
		printf("\tEnter Your Name: ");
		scanf("%s", name);
		strcpy(password,getpass("\tEnter Your Password: "));

		if(!type){
			while(1){
				strcpy(secret_pin, getpass("\tEnter secret PIN to create ADMIN account: "));
				if(strcmp(secret_pin, "secret")!=0) 					
					printf("\tInvalid PIN. Please Try Again.\n");
				else
					break;
			}
		}

		write(sock, &type, sizeof(type));
		write(sock, &name, sizeof(name));
		write(sock, &password, strlen(password));
		
		read(sock, &id, sizeof(id));
		printf("\tRemember Your login id For Further Logins as :: %d\n", id);
		return 2;
	}
	else							// Log out
		return 3;
	
}

//-------------------- Main menu function-----------------------------//

int menu(int sock,int type){
	int choice;
	if(type==2 || type==1){					// Agent and Customer
		printf("\t1. Book Ticket\n");
		printf("\t2. View Bookings\n");
		printf("\t3. Update Booking\n");
		printf("\t4. Cancel booking\n");
		printf("\t5. Logout\n");
		printf("\tYour Choice: ");
		scanf("%d",&choice);
		write(sock,&choice,sizeof(choice));
		return user_function(sock,choice);
	}
	else if(type==0){					// Admin
		printf("\n\t1. CRUD operations on flight\n");
		printf("\t2. CRUD operations on user\n");
		printf("\t3. Logout\n");
		printf("\t Your Choice: ");
		scanf("%d",&choice);
		write(sock,&choice,sizeof(choice));
			if(choice==1){
				printf("\t1. Add flight\n");
				printf("\t2. View flight\n");
				printf("\t3. Modify flight\n");
				printf("\t4. Delete flight\n");
				printf("\t Your Choice: ");
				scanf("%d",&choice);	
				write(sock,&choice,sizeof(choice));
				return crud_flight(sock,choice);
			}
			else if(choice==2){
				printf("\t1. Add User\n");
				printf("\t2. View all users\n");
				printf("\t3. Modify user\n");
				printf("\t4. Delete user\n");
				printf("\t Your Choice: ");
				scanf("%d",&choice);
				write(sock,&choice,sizeof(choice));
				return crud_user(sock,choice);
			
			}
			else if(choice==3)
				return -1;
	}	
	
}

//-------------------------------- crud operations on flight-----------------------------//
 int crud_flight(int sock,int choice){
 	return 0;
 }

// //-------------------------------- crud operations on user-----------------------------//

 int crud_user(int sock,int choice){
	return 0;
 }

// //-------------------------------- User function to book tickets -----------------------------//
 int user_function(int sock,int choice){
	if (choice == 5) return -1;
	else return 0;
 }
