/*
Server side: concurrent server using fork()
socket->bind->listen->accept->recv/send->close
*/
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 256

#define PORT 8000

//--------------- structures declaration of flight and user------------//
struct flight{
		int flight_id;
		char flight_name[50];
        char depart_point[50];
        char destination_point[50];
        char depart_date[50];
        char return_date[50];
		int total_ecoSeats;
		int available_ecoSeats;
        int total_busiSeats;
		int available_busiSeats;
		};
struct user{
		int login_id;
		char password[50];
		char name[50];
		int type;
		};

struct booking{
		int booking_id;
		int type;
		int uid;
		int fid;
		int seats;
		};
//---------------------------------------------------------------------//

void service_cli(int sock);
void login(int client_sock);
void signup(int client_sock);
int menu(int client_sock,int type,int id);

   

int client_count = 0;  // Global variable to track connection count
int main(void) {
    int socket_desc, client_sock, c; 
    struct sockaddr_in server, client; 
  
    socket_desc = socket(AF_INET, SOCK_STREAM, 0); 
    if (socket_desc == -1) { 
        perror("Could not create socket"); 
        exit(EXIT_FAILURE);
    } 
  
    server.sin_family = AF_INET; 
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT); 
    
    if (bind(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("bind failed. Error");
        exit(EXIT_FAILURE);
    }
    
    listen(socket_desc, 3);  
    c = sizeof(struct sockaddr_in); 

    while (1) {
        client_sock = accept(socket_desc, (struct sockaddr*)&client, (socklen_t*)&c); 
        
        if (client_sock == -1) {
            perror("accept failed");
            continue;
        }

        client_count++;  // Increment the global client counter

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            close(client_sock);
        } else if (pid == 0) {  // Child process
            close(socket_desc);
            service_cli(client_sock);
            close(client_sock);
            exit(EXIT_SUCCESS);
        } else {  // Parent process
            close(client_sock);
            printf("\n\tClient [%d] Connected\n", client_count);  // Print the client counter
        }
    }

    close(socket_desc);
    return 0;
}

void service_cli(int sock){
    int choice;
    do {
        read(sock, &choice, sizeof(int));        
        if(choice == 1)
            login(sock);
        if(choice == 2)
            signup(sock);
        if(choice == 3)
            break;
    } while(1);

    printf("\n\tClient [%d] Disconnected\n", client_count);  // Print the client counter
    close(sock);
}

//-------------------- Login function-----------------------------//


void login(int client_sock) {
    int fd_user = open("db/db_user.txt", O_RDWR);
    int id, type, valid = 0, user_valid = 0;
    char password[50];
    struct user u;
    char buffer[MAX_BUFFER_SIZE];

    read(client_sock, &id, sizeof(id));
    read(client_sock, &password, sizeof(password));

    struct flock lock;

    lock.l_start = (id - 1) * sizeof(struct user);
    lock.l_len = sizeof(struct user);
    lock.l_whence = SEEK_SET;
    lock.l_pid = getpid();

    lock.l_type = F_WRLCK;
    fcntl(fd_user, F_SETLKW, &lock);

    while (read(fd_user, &buffer, sizeof(buffer))) {
        sscanf(buffer, "%d,%49[^,],%49[^,],%d", &u.login_id, u.name, u.password, &u.type);

        if (u.login_id == id) {
            user_valid = 1;
            if (!strcmp(u.password, password)) {
                valid = 1;
                type = u.type;
                break;
            } else {
                valid = 0;
                break;
            }
        } else {
            user_valid = 0;
            valid = 0;
        }
    }

    // same agent is allowed from multiple terminals..
    // so unlocking his user record just after checking his credentials and allowing further
    if (type != 2) {
        lock.l_type = F_UNLCK;
        fcntl(fd_user, F_SETLK, &lock);
        close(fd_user);
    }

    // if valid user, show menu
    if (user_valid) {
        write(client_sock, &valid, sizeof(valid));
        if (valid) {
            write(client_sock, &type, sizeof(type));
            while (menu(client_sock, type, id) != -1);
        }
    } else {
        write(client_sock, &valid, sizeof(valid));
    }

    // same user is not allowed from multiple terminals..
    // so unlocking his user record after he logs out only to not allow him from other terminal
    if (type == 2) {
        lock.l_type = F_UNLCK;
        fcntl(fd_user, F_SETLK, &lock);
        close(fd_user);
    }
}


//-------------------- Signup function-----------------------------//

void signup(int client_sock) {
    int fd_user = open("db/db_user.txt", O_RDWR | O_CREAT | O_APPEND, 0644);
    int type, id = 0;
    char name[50], password[50];
    struct user u, temp;

    read(client_sock, &type, sizeof(type));
    read(client_sock, &name, sizeof(name));
    read(client_sock, &password, sizeof(password));

    if (fd_user == -1) {
        perror("Error opening file");
        close(client_sock);
        return;
    }

    // Lock the file for writing
    flock(fd_user, LOCK_EX);

    // Read the last login_id to determine the next one
    if (read(fd_user, &temp, sizeof(temp)) > 0) {
        u.login_id = temp.login_id + 1;
    } else {
        u.login_id = 1;
    }

    // Write the new user information to the file in CSV format
    dprintf(fd_user, "%d,%s,%s,%d\n", u.login_id, name, password, type);

    // Unlock the file
    flock(fd_user, LOCK_UN);

    // Send the login_id back to the client
    write(client_sock, &u.login_id, sizeof(u.login_id));

    close(fd_user);
}

//-------------------- Main menu function-----------------------------//

int menu(int client_sock,int type,int id){
	int choice,ret;

	// for admin
	if(type==0){
		read(client_sock,&choice,sizeof(choice));
		if(choice==1){					// CRUD options on flight
			return menu(client_sock,type,id);	
		}
		else if(choice==2){				// CRUD options on User
			return menu(client_sock,type,id);
		}
		else if (choice ==3)				// Logout
			return -1;
	}
	else if(type==2 || type==1){				// For agent and customer
		read(client_sock,&choice,sizeof(choice));
		if(ret!=5)
			return menu(client_sock,type,id);
		else if(ret==5)
			return -1;
	}		
}
