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


#define PORT 8000

//--------------- structures declaration of flight and user------------//
struct flight{
		int flight_number;
		char flight_name[50];
		int total_seats;
		int available_seats;
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
		int tid;
		int seats;
		};
//---------------------------------------------------------------------//

void service_cli(int sock);
void login(int client_sock);
void signup(int client_sock);
int menu(int client_sock,int type,int id);
void crud_flight(int client_sock);
void crud_user(int client_sock);
int user_function(int client_sock,int choice,int type,int id);
   

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_BUFFER_SIZE 256



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

    // if valid user, show him menu
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
			crud_flight(client_sock);
			return menu(client_sock,type,id);	
		}
		else if(choice==2){				// CRUD options on User
			crud_user(client_sock);
			return menu(client_sock,type,id);
		}
		else if (choice ==3)				// Logout
			return -1;
	}
	else if(type==2 || type==1){				// For agent and customer
		read(client_sock,&choice,sizeof(choice));
		ret = user_function(client_sock,choice,type,id);
		if(ret!=5)
			return menu(client_sock,type,id);
		else if(ret==5)
			return -1;
	}		
}

//---------------------- CRUD operation on flight--------------------//

void crud_flight(int client_sock){
	int valid=0;	
	int choice;
	read(client_sock,&choice,sizeof(choice));
	if(choice==1){  					// Add flight  	
		char tname[50];
		int tid = 0;
		read(client_sock,&tname,sizeof(tname));
		struct flight tdb,temp;
		struct flock lock;
		int fd_flight = open("db/db_flight.txt", O_RDWR);
		
		tdb.flight_number = tid;
		strcpy(tdb.flight_name,tname);
		tdb.total_seats = 10;				// by default, we are taking 10 seats
		tdb.available_seats = 10;

		int fp = lseek(fd_flight, 0, SEEK_END); 

		lock.l_type = F_WRLCK;
		lock.l_start = fp;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd_flight, F_SETLKW, &lock);

		if(fp == 0){
			valid = 1;
			write(fd_flight, &tdb, sizeof(tdb));
			lock.l_type = F_UNLCK;
			fcntl(fd_flight, F_SETLK, &lock);
			close(fd_flight);
			write(client_sock, &valid, sizeof(valid));
		}
		else{
			valid = 1;
			lseek(fd_flight, -1 * sizeof(struct flight), SEEK_END);
			read(fd_flight, &temp, sizeof(temp));
			tdb.flight_number = temp.flight_number + 1;
			write(fd_flight, &tdb, sizeof(tdb));
			write(client_sock, &valid,sizeof(valid));	
		}
		lock.l_type = F_UNLCK;
		fcntl(fd_flight, F_SETLK, &lock);
		close(fd_flight);
		
	}

	else if(choice==2){					// View/ Read flights
		struct flock lock;
		struct flight tdb;
		int fd_flight = open("db/db_flight.txt", O_RDONLY);
		
		lock.l_type = F_RDLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_flight, F_SETLKW, &lock);
		int fp = lseek(fd_flight, 0, SEEK_END);
		int no_of_flights = fp / sizeof(struct flight);
		write(client_sock, &no_of_flights, sizeof(int));

		lseek(fd_flight,0,SEEK_SET);
		while(fp != lseek(fd_flight,0,SEEK_CUR)){
			read(fd_flight,&tdb,sizeof(tdb));
			write(client_sock,&tdb.flight_number,sizeof(int));
			write(client_sock,&tdb.flight_name,sizeof(tdb.flight_name));
			write(client_sock,&tdb.total_seats,sizeof(int));
			write(client_sock,&tdb.available_seats,sizeof(int));
		}
		valid = 1;
		lock.l_type = F_UNLCK;
		fcntl(fd_flight, F_SETLK, &lock);
		close(fd_flight);
	}

	else if(choice==3){					// Update flight
		crud_flight(client_sock);
		int choice,valid=0,tid;
		struct flock lock;
		struct flight tdb;
		int fd_flight = open("db/db_flight.txt", O_RDWR);

		read(client_sock,&tid,sizeof(tid));

		lock.l_type = F_WRLCK;
		lock.l_start = (tid)*sizeof(struct flight);
		lock.l_len = sizeof(struct flight);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_flight, F_SETLKW, &lock);

		lseek(fd_flight, 0, SEEK_SET);
		lseek(fd_flight, (tid)*sizeof(struct flight), SEEK_CUR);
		read(fd_flight, &tdb, sizeof(struct flight));
		
		read(client_sock,&choice,sizeof(int));
		if(choice==1){							// update flight name
			write(client_sock,&tdb.flight_name,sizeof(tdb.flight_name));
			read(client_sock,&tdb.flight_name,sizeof(tdb.flight_name));
			
		}
		else if(choice==2){						// update total number of seats
			write(client_sock,&tdb.total_seats,sizeof(tdb.total_seats));
			read(client_sock,&tdb.total_seats,sizeof(tdb.total_seats));
		}
	
		lseek(fd_flight, -1*sizeof(struct flight), SEEK_CUR);
		write(fd_flight, &tdb, sizeof(struct flight));
		valid=1;
		write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_flight, F_SETLK, &lock);
		close(fd_flight);	
	}

	else if(choice==4){						// Delete flight
		crud_flight(client_sock);
		struct flock lock;
		struct flight tdb;
		int fd_flight = open("db/db_flight.txt", O_RDWR);
		int tid,valid=0;

		read(client_sock,&tid,sizeof(tid));

		lock.l_type = F_WRLCK;
		lock.l_start = (tid)*sizeof(struct flight);
		lock.l_len = sizeof(struct flight);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_flight, F_SETLKW, &lock);
		
		lseek(fd_flight, 0, SEEK_SET);
		lseek(fd_flight, (tid)*sizeof(struct flight), SEEK_CUR);
		read(fd_flight, &tdb, sizeof(struct flight));
		strcpy(tdb.flight_name,"deleted");
		lseek(fd_flight, -1*sizeof(struct flight), SEEK_CUR);
		write(fd_flight, &tdb, sizeof(struct flight));
		valid=1;
		write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_flight, F_SETLK, &lock);
		close(fd_flight);	
	}	
}

//---------------------- CRUD operation on user--------------------//
void crud_user(int client_sock){
	int valid=0;	
	int choice;
	read(client_sock,&choice,sizeof(choice));
	if(choice==1){    					// Add user
		char name[50],password[50];
		int type;
		read(client_sock, &type, sizeof(type));
		read(client_sock, &name, sizeof(name));
		read(client_sock, &password, sizeof(password));
		
		struct user udb;
		struct flock lock;
		int fd_user = open("db/db_user.txt", O_RDWR);
		int fp = lseek(fd_user, 0, SEEK_END);
		
		lock.l_type = F_WRLCK;
		lock.l_start = fp;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd_user, F_SETLKW, &lock);

		if(fp==0){
			udb.login_id = 1;
			strcpy(udb.name, name);
			strcpy(udb.password, password);
			udb.type=type;
			write(fd_user, &udb, sizeof(udb));
			valid = 1;
			write(client_sock,&valid,sizeof(int));
			write(client_sock, &udb.login_id, sizeof(udb.login_id));
		}
		else{
			fp = lseek(fd_user, -1 * sizeof(struct user), SEEK_END);
			read(fd_user, &udb, sizeof(udb));
			udb.login_id++;
			strcpy(udb.name, name);
			strcpy(udb.password, password);
			udb.type=type;
			write(fd_user, &udb, sizeof(udb));
			valid = 1;
			write(client_sock,&valid,sizeof(int));
			write(client_sock, &udb.login_id, sizeof(udb.login_id));
		}
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
		
	}

	else if(choice==2){					// View user list
		struct flock lock;
		struct user udb;
		int fd_user = open("db/db_user.txt", O_RDONLY);
		
		lock.l_type = F_RDLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_user, F_SETLKW, &lock);
		int fp = lseek(fd_user, 0, SEEK_END);
		int no_of_users = fp / sizeof(struct user);
		no_of_users--;
		write(client_sock, &no_of_users, sizeof(int));

		lseek(fd_user,0,SEEK_SET);
		while(fp != lseek(fd_user,0,SEEK_CUR)){
			read(fd_user,&udb,sizeof(udb));
			if(udb.type!=0){
				write(client_sock,&udb.login_id,sizeof(int));
				write(client_sock,&udb.name,sizeof(udb.name));
				write(client_sock,&udb.type,sizeof(int));
			}
		}
		valid = 1;
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);
	}

	else if(choice==3){					// Update user
		crud_user(client_sock);
		int choice,valid=0,uid;
		char pass[50];
		struct flock lock;
		struct user udb;
		int fd_user = open("db/db_user.txt", O_RDWR);

		read(client_sock,&uid,sizeof(uid));

		lock.l_type = F_WRLCK;
		lock.l_start =  (uid-1)*sizeof(struct user);
		lock.l_len = sizeof(struct user);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_user, F_SETLKW, &lock);

		lseek(fd_user, 0, SEEK_SET);
		lseek(fd_user, (uid-1)*sizeof(struct user), SEEK_CUR);
		read(fd_user, &udb, sizeof(struct user));
		
		read(client_sock,&choice,sizeof(int));
		if(choice==1){					// update name
			write(client_sock,&udb.name,sizeof(udb.name));
			read(client_sock,&udb.name,sizeof(udb.name));
			valid=1;
			write(client_sock,&valid,sizeof(valid));		
		}
		else if(choice==2){				// update password
			read(client_sock,&pass,sizeof(pass));
			if(!strcmp(udb.password,pass))
				valid = 1;
			write(client_sock,&valid,sizeof(valid));
			read(client_sock,&udb.password,sizeof(udb.password));
		}
	
		lseek(fd_user, -1*sizeof(struct user), SEEK_CUR);
		write(fd_user, &udb, sizeof(struct user));
		if(valid)
			write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);	
	}

	else if(choice==4){						// Delete any particular user
		crud_user(client_sock);
		struct flock lock;
		struct user udb;
		int fd_user = open("db/db_user.txt", O_RDWR);
		int uid,valid=0;

		read(client_sock,&uid,sizeof(uid));

		lock.l_type = F_WRLCK;
		lock.l_start =  (uid-1)*sizeof(struct user);
		lock.l_len = sizeof(struct user);
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_user, F_SETLKW, &lock);
		
		lseek(fd_user, 0, SEEK_SET);
		lseek(fd_user, (uid-1)*sizeof(struct user), SEEK_CUR);
		read(fd_user, &udb, sizeof(struct user));
		strcpy(udb.name,"deleted");
		strcpy(udb.password,"");
		lseek(fd_user, -1*sizeof(struct user), SEEK_CUR);
		write(fd_user, &udb, sizeof(struct user));
		valid=1;
		write(client_sock,&valid,sizeof(valid));
		lock.l_type = F_UNLCK;
		fcntl(fd_user, F_SETLK, &lock);
		close(fd_user);	
	}
}


//---------------------- User functions -----------------------//
int user_function(int client_sock,int choice,int type,int id){
	int valid=0;
	if(choice==1){						// book ticket
		crud_flight(client_sock);
		struct flock lockt;
		struct flock lockb;
		struct flight tdb;
		struct booking bdb;
		int fd_flight = open("db/db_flight.txt", O_RDWR);
		int fd_book = open("db/db_booking.txt", O_RDWR);
		int tid,seats;
		read(client_sock,&tid,sizeof(tid));		
				
		lockt.l_type = F_WRLCK;
		lockt.l_start = tid*sizeof(struct flight);
		lockt.l_len = sizeof(struct flight);
		lockt.l_whence = SEEK_SET;
		lockt.l_pid = getpid();
		
		lockb.l_type = F_WRLCK;
		lockb.l_start = 0;
		lockb.l_len = 0;
		lockb.l_whence = SEEK_END;
		lockb.l_pid = getpid();
		
		fcntl(fd_flight, F_SETLKW, &lockt);
		lseek(fd_flight,tid*sizeof(struct flight),SEEK_SET);
		
		read(fd_flight,&tdb,sizeof(tdb));
		read(client_sock,&seats,sizeof(seats));

		if(tdb.flight_number==tid)
		{		
			if(tdb.available_seats>=seats){
				valid = 1;
				tdb.available_seats -= seats;
				fcntl(fd_book, F_SETLKW, &lockb);
				int fp = lseek(fd_book, 0, SEEK_END);
				
				if(fp > 0){
					lseek(fd_book, -1*sizeof(struct booking), SEEK_CUR);
					read(fd_book, &bdb, sizeof(struct booking));
					bdb.booking_id++;
				}
				else 
					bdb.booking_id = 0;

				bdb.type = type;
				bdb.uid = id;
				bdb.tid = tid;
				bdb.seats = seats;
				write(fd_book, &bdb, sizeof(struct booking));
				lockb.l_type = F_UNLCK;
				fcntl(fd_book, F_SETLK, &lockb);
			 	close(fd_book);
			}
		
		lseek(fd_flight, -1*sizeof(struct flight), SEEK_CUR);
		write(fd_flight, &tdb, sizeof(tdb));
		}

		lockt.l_type = F_UNLCK;
		fcntl(fd_flight, F_SETLK, &lockt);
		close(fd_flight);
		write(client_sock,&valid,sizeof(valid));
		return valid;		
	}
	
	else if(choice==2){							// View bookings
		struct flock lock;
		struct booking bdb;
		int fd_book = open("db/db_booking.txt", O_RDONLY);
		int no_of_bookings = 0;
	
		lock.l_type = F_RDLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();
		
		fcntl(fd_book, F_SETLKW, &lock);
	
		while(read(fd_book,&bdb,sizeof(bdb))){
			if (bdb.uid==id)
				no_of_bookings++;
		}

		write(client_sock, &no_of_bookings, sizeof(int));
		lseek(fd_book,0,SEEK_SET);

		while(read(fd_book,&bdb,sizeof(bdb))){
			if(bdb.uid==id){
				write(client_sock,&bdb.booking_id,sizeof(int));
				write(client_sock,&bdb.tid,sizeof(int));
				write(client_sock,&bdb.seats,sizeof(int));
			}
		}
		lock.l_type = F_UNLCK;
		fcntl(fd_book, F_SETLK, &lock);
		close(fd_book);
		return valid;
	}

	else if (choice==3){							// update booking
		int choice = 2,bid,val;
		user_function(client_sock,choice,type,id);
		struct booking bdb;
		struct flight tdb;
		struct flock lockb;
		struct flock lockt;
		int fd_book = open("db/db_booking.txt", O_RDWR);
		int fd_flight = open("db/db_flight.txt", O_RDWR);
		read(client_sock,&bid,sizeof(bid));

		lockb.l_type = F_WRLCK;
		lockb.l_start = bid*sizeof(struct booking);
		lockb.l_len = sizeof(struct booking);
		lockb.l_whence = SEEK_SET;
		lockb.l_pid = getpid();
		
		fcntl(fd_book, F_SETLKW, &lockb);
		lseek(fd_book,bid*sizeof(struct booking),SEEK_SET);
		read(fd_book,&bdb,sizeof(bdb));
		lseek(fd_book,-1*sizeof(struct booking),SEEK_CUR);
		
		lockt.l_type = F_WRLCK;
		lockt.l_start = (bdb.tid)*sizeof(struct flight);
		lockt.l_len = sizeof(struct flight);
		lockt.l_whence = SEEK_SET;
		lockt.l_pid = getpid();

		fcntl(fd_flight, F_SETLKW, &lockt);
		lseek(fd_flight,(bdb.tid)*sizeof(struct flight),SEEK_SET);
		read(fd_flight,&tdb,sizeof(tdb));
		lseek(fd_flight,-1*sizeof(struct flight),SEEK_CUR);

		read(client_sock,&choice,sizeof(choice));
	
		if(choice==1){							// increase number of seats required of booking id
			read(client_sock,&val,sizeof(val));
			if(tdb.available_seats>=val){
				valid=1;
				tdb.available_seats -= val;
				bdb.seats += val;
			}
		}
		else if(choice==2){						// decrease number of seats required of booking id
			valid=1;
			read(client_sock,&val,sizeof(val));
			tdb.available_seats += val;
			bdb.seats -= val;	
		}
		
		write(fd_flight,&tdb,sizeof(tdb));
		lockt.l_type = F_UNLCK;
		fcntl(fd_flight, F_SETLK, &lockt);
		close(fd_flight);
		
		write(fd_book,&bdb,sizeof(bdb));
		lockb.l_type = F_UNLCK;
		fcntl(fd_book, F_SETLK, &lockb);
		close(fd_book);
		
		write(client_sock,&valid,sizeof(valid));
		return valid;
	}
	else if(choice==4){							// Cancel an entire booking
		int choice = 2,bid;
		user_function(client_sock,choice,type,id);
		struct booking bdb;
		struct flight tdb;
		struct flock lockb;
		struct flock lockt;
		int fd_book = open("db/db_booking.txt", O_RDWR);
		int fd_flight = open("db/db_flight.txt", O_RDWR);
		read(client_sock,&bid,sizeof(bid));

		lockb.l_type = F_WRLCK;
		lockb.l_start = bid*sizeof(struct booking);
		lockb.l_len = sizeof(struct booking);
		lockb.l_whence = SEEK_SET;
		lockb.l_pid = getpid();
		
		fcntl(fd_book, F_SETLKW, &lockb);
		lseek(fd_book,bid*sizeof(struct booking),SEEK_SET);
		read(fd_book,&bdb,sizeof(bdb));
		lseek(fd_book,-1*sizeof(struct booking),SEEK_CUR);
		
		lockt.l_type = F_WRLCK;
		lockt.l_start = (bdb.tid)*sizeof(struct flight);
		lockt.l_len = sizeof(struct flight);
		lockt.l_whence = SEEK_SET;
		lockt.l_pid = getpid();

		fcntl(fd_flight, F_SETLKW, &lockt);
		lseek(fd_flight,(bdb.tid)*sizeof(struct flight),SEEK_SET);
		read(fd_flight,&tdb,sizeof(tdb));
		lseek(fd_flight,-1*sizeof(struct flight),SEEK_CUR);

		tdb.available_seats += bdb.seats;
		bdb.seats = 0;
		valid = 1;

		write(fd_flight,&tdb,sizeof(tdb));
		lockt.l_type = F_UNLCK;
		fcntl(fd_flight, F_SETLK, &lockt);
		close(fd_flight);
		
		write(fd_book,&bdb,sizeof(bdb));
		lockb.l_type = F_UNLCK;
		fcntl(fd_book, F_SETLK, &lockb);
		close(fd_book);
		
		write(client_sock,&valid,sizeof(valid));
		return valid;
		
	}
	else if(choice==5)										// Logout
		return 5;

}
