#include <signal.h>
#include <malloc.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <ctype.h>
#include "chatServer.h"


static int end_server = 0;
static int ws = 3;

void intHandler(int SIG_INT) {
    /* use a flag to end_server to break the main loop */
    end_server = 1;
}

int main (int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: server <port>\n");
        return 1;
    }
    int port = atoi(argv[1]);
    if(port < 1 || port > 65536){
        printf("Usage: server <port>\n");
        return 1;
    }
    signal(SIGINT, intHandler);

    conn_pool_t* pool = malloc(sizeof(conn_pool_t));
    if (pool == NULL) {
        perror("error: malloc\n");
        return 1;
    }
    initPool(pool);

    /*************************************************************/
    /* Create an AF_INET stream socket to receive incoming      */
    /* connections on                                            */
    /*************************************************************/
    int listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket == -1) {
        perror("error: socket\n");
        free(pool);
        return 1;
    }
    ws = listen_socket;
    /*************************************************************/
    /* Set socket to be nonblocking. All of the sockets for      */
    /* the incoming connections will also be nonblocking since   */
    /* they will inherit that state from the listening socket.   */
    /*************************************************************/
    int on = 1;
    if (ioctl(listen_socket, FIONBIO, (char *)&on) == -1) {
        perror("error: ioctl\n");
        close(listen_socket);
        free(pool);
        return 1;
    }
    /*************************************************************/
    /* Bind the socket                                           */
    /*************************************************************/
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(atoi(argv[1]));

    if (bind(listen_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("error: bind\n");
        close(listen_socket);
        free(pool);
        return 1;
    }

    /*************************************************************/
    /* Set the listen back log                                   */
    /*************************************************************/
    if (listen(listen_socket, SOMAXCONN) == -1) {
        perror("error: listen\n");
        close(listen_socket);
        free(pool);
        return 1;
    }

    /*************************************************************/
    /* Initialize fd_sets  			                             */
    /*************************************************************/
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->write_set);
    FD_SET(listen_socket,&pool->ready_read_set);
    pool->maxfd = listen_socket;
    /*************************************************************/
    /* Loop waiting for incoming connects, for incoming data or  */
    /* to write data, on any of the connected sockets.           */
    /*************************************************************/

    do
    {
        /**********************************************************/
        /* Copy the master fd_set over to the working fd_set.     */
        /**********************************************************/
        pool->read_set = pool->ready_read_set;
        pool->write_set = pool->ready_write_set;
        /**********************************************************/
        /* Call select() 										  */
        /**********************************************************/
        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
        pool->nready = select(pool->maxfd + 1, &pool->read_set, &pool->write_set, NULL, 0);
        if (pool->nready == -1) {
            if(!end_server) {
                perror("error: select\n");
                end_server = 1;
                break;
            }
        }
        /**********************************************************/
        /* One or more descriptors are readable or writable.      */
        /* Need to determine which ones they are.                 */
        /**********************************************************/
        int sd;
        for (sd = 0; sd <= pool->maxfd && pool->nready >0; ++sd)
        {
            /* Each time a ready descriptor is found, one less has  */
            /* to be looked for.  This is being done so that we     */
            /* can stop looking at the working set once we have     */
            /* found all of the descriptors that were ready         */

            /*******************************************************/
            /* Check to see if this descriptor is ready for read   */
            /*******************************************************/
            if (FD_ISSET(sd, &pool->read_set))
            {
                pool->nready--;
                /***************************************************/
                /* A descriptor was found that was readable		   */
                /* if this is the listening socket, accept one      */
                /* incoming connection that is queued up on the     */
                /*  listening socket before we loop back and call   */
                /* select again. 						            */
                /****************************************************/
                if (sd == listen_socket) {
                    int new_sd = accept(listen_socket, NULL, NULL);
                    if(new_sd == -1) {
                        perror("error: accept\n");
                        continue;
                    }
                    if (addConn(new_sd, pool) == -1){
                        perror("error: malloc\n");
                        close(new_sd);
                    }
                    printf("New incoming connection on sd %d\n", new_sd);
                    continue;
                }
                /****************************************************/
                /* If this is not the listening socket, an 			*/
                /* existing connection must be readable				*/
                /* Receive incoming data his socket             */
                /****************************************************/
                char buf[BUFFER_SIZE];
                printf("Descriptor %d is readable\n", sd);
                ssize_t bytes_read = read(sd,buf,BUFFER_SIZE);
                printf("%d bytes received from sd %d\n", (int)bytes_read, sd);
                /* If the connection has been closed by client 		*/
                /* remove the connection (removeConn(...))    		*/
                if (bytes_read == -1) {
                    continue;
                } else if (bytes_read == 0) {
                    removeConn(sd, pool);
                    printf("Connection closed for sd %d\n",sd);
                    continue;
                }

                /**********************************************/
                /* Data was received, add msg to all other    */
                /* connectios					  			  */
                /**********************************************/
                addMsg(sd, buf, (int)bytes_read, pool);


            } /* End of if (FD_ISSET()) */
            /*******************************************************/
            /* Check to see if this descriptor is ready for write  */
            /*******************************************************/
            if (FD_ISSET(sd, &pool->write_set)) {
                /* try to write all msgs in queue to sd */
                writeToClient(sd, pool);
            }
            /*******************************************************/


        } /* End of loop through selectable descriptors */

    } while (end_server == 0);

    /*************************************************************/
    /* If we are here, Control-C was typed,						 */
    /* clean up all open connections					         */
    /*************************************************************/
    // Clean up all open connections
    conn_t *current = pool->conn_head;
    while (current != NULL) {
        conn_t *next = current->next;
        removeConn(current->fd, pool);
        current = next;
    }

    free(pool);
    return 0;
}


int initPool(conn_pool_t* pool) {
    //initialized all fields
    pool->maxfd = 0;
    pool->nready = 0;
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->ready_write_set);
    pool->conn_head = NULL;
    pool->nr_conns = 0;
    return 0;
}

int addConn(int sd, conn_pool_t* pool) {
    if (pool == NULL) {
        return -1; // Invalid pool pointer
    }
    conn_t *new_conn = (conn_t *)malloc(sizeof(conn_t));
    if (new_conn == NULL) {
        return -1; // Memory allocation failed
    }
    new_conn->fd = sd;
    new_conn->write_msg_head = NULL;
    new_conn->write_msg_tail = NULL;
    new_conn->prev = NULL;
    new_conn->next = pool->conn_head;
    if (pool->conn_head != NULL) {
        pool->conn_head->prev = new_conn;
    }
    pool->conn_head = new_conn;
    pool->nr_conns++;
    if (sd > pool->maxfd) {
        pool->maxfd = sd;
    }
    FD_SET(sd, &pool->ready_read_set);
    return 0;
}


int removeConn(int sd, conn_pool_t* pool) {
    if (pool == NULL) {
        return -1; // Invalid pool pointer
    }

    conn_t *current = pool->conn_head;
    conn_t *prev = NULL;

    while (current != NULL) {
        if (current->fd == sd) {
            if (prev != NULL) {
                prev->next = current->next;
            } else {
                pool->conn_head = current->next;
            }

            if (current->next != NULL) {
                current->next->prev = prev;
            }

            if (sd == pool->maxfd && pool->conn_head == NULL) {
                // Set maxfd to the listening socket if there are no active connections
                pool->maxfd = ws;
            } else if (sd == pool->maxfd) {
                // Update maxfd if needed
                conn_t *temp = pool->conn_head;
                int max = 0;
                while (temp != NULL) {
                    if (temp->fd > max) {
                        max = temp->fd;
                    }
                    temp = temp->next;
                }
                pool->maxfd = max;
            }

            FD_CLR(sd, &pool->ready_read_set);
            FD_CLR(sd, &pool->ready_write_set);
            close(sd);
            free(current);
            pool->nr_conns--;
            printf("removing connection with sd %d \n", sd);
            return 0;
        }

        prev = current;
        current = current->next;
    }

    return -1; // Connection not found
}


int addMsg(int sd, char* buffer, int len, conn_pool_t* pool) {
    if (pool == NULL || buffer == NULL || len <= 0) {
        return -1; // Invalid arguments
    }

    // Convert the message to uppercase
    for (int i = 0; i < len; ++i) {
        buffer[i] = toupper(buffer[i]);
    }

    conn_t *current = pool->conn_head;
    while (current != NULL) {
        if (current->fd != sd) {
            msg_t *new_msg = (msg_t *)malloc(sizeof(msg_t));
            if (new_msg == NULL) {
                return -1; // Memory allocation failed
            }
            new_msg->message = (char *)malloc(len + 1);
            if (new_msg->message == NULL) {
                free(new_msg);
                return -1; // Memory allocation failed
            }
            memcpy(new_msg->message, buffer, len);
            new_msg->message[len] = '\0';
            new_msg->size = len;
            new_msg->next = NULL;
            if (current->write_msg_head == NULL) {
                current->write_msg_head = new_msg;
                current->write_msg_tail = new_msg;
            } else {
                current->write_msg_tail->next = new_msg;
                current->write_msg_tail = new_msg;
            }
            FD_SET(current->fd, &pool->ready_write_set);
        }
        current = current->next;
    }
    return 0;
}


int writeToClient(int sd, conn_pool_t* pool) {
    if (pool == NULL) {
        return -1; // Invalid pool pointer
    }
    conn_t *current = pool->conn_head;
    while (current != NULL) {
        if (current->fd == sd) {
            msg_t *msg = current->write_msg_head;
            while (msg != NULL) {
                ssize_t written = write(sd, msg->message, msg->size);
                if (written < 0) {
                    perror("Write failed");
                    return -1;
                }
                msg_t *temp = msg;
                msg = msg->next;
                free(temp->message);
                free(temp);
            }
            current->write_msg_head = NULL;
            current->write_msg_tail = NULL;
            FD_CLR(sd, &pool->ready_write_set);
            return 0;
        }
        current = current->next;
    }
    return -1; // Connection not found
}
