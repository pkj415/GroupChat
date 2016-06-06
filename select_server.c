#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>

#define MAXSTRINGLENGTH 128

void Die(char *str){
   perror(str);
   exit(0);
}

int isMember(int key, int *arrayOfMem){
   int i;
   for(i=0;i<FD_SETSIZE;i++){
      if(arrayOfMem[i]==key)
         return 1;
   }
   return 0;
}

struct group{
   int groupNo;
   int numberOfMembers;
   int arrayOfMem[FD_SETSIZE];
};

struct groupNode{
   struct group g;
   struct groupNode *next;
};

int main(int argc, char *argv[])
{  
   if(argc!=2){
      printf("Format is %s <Port Number>\n", argv[0]);
      exit(0);
   }
   
   struct groupNode *myList;

   int sd, clientSockets[FD_SETSIZE];
   memset(clientSockets,0,sizeof(clientSockets));
   struct sockaddr_in server, client;
   char buf[MAXSTRINGLENGTH], subBuf1[MAXSTRINGLENGTH], subBuf2[MAXSTRINGLENGTH];
   fd_set readfds;

   if((sd = socket (AF_INET,SOCK_STREAM,0))==-1){
      Die("Error creating server TCP socket.\n");
   }
   server.sin_family = AF_INET;
   server.sin_addr.s_addr = htonl(INADDR_ANY);
   server.sin_port = htons(atoi(argv[1]));

   int optval = 1;
   setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
   if(bind(sd, (struct sockaddr*) &server, sizeof(server))==-1){
      Die("Error binding server TCP socket.\n");
   }
   int opt = 1;
   if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0 )
   {
        Die("setsockopt");
   }
   if(listen(sd,30)==-1){
      Die("Error listening from server.\n");
   }
   //struct sockaddr_in clientAddr;
   //socklen_t clntAddrLen = sizeof(clientAddr);

   while(1){
      FD_ZERO(&readfds);
      FD_SET(sd, &readfds);
      int max_fd = sd, i, newSock;
      for (i = 0 ; i < FD_SETSIZE ; i++) 
        {    
            if(clientSockets[i] > 0)
                FD_SET( clientSockets[i], &readfds);
            if(clientSockets[i] > max_fd)
                max_fd = clientSockets[i];
        }

      if(select( max_fd + 1 , &readfds , NULL , NULL , NULL)==-1){
         Die("Error in select call.\n");
      }

      if (FD_ISSET(sd, &readfds)) 
        {
            memset(&client,0,sizeof(client));
            socklen_t clientSize = sizeof(client);
            if ((newSock = accept(sd, (struct sockaddr *)&client, &clientSize))<0)
            {
                Die("Accept Error.\n");
            }
            //add new socket to array of sockets
            for (i = 0; i < FD_SETSIZE; i++) 
            {
                if(clientSockets[i] == 0 )
                {
                    clientSockets[i] = newSock;
                    //printf("Adding client to list of sockets as %d\n" , i);
                    break;
                }
            }
            if (i == FD_SETSIZE){
                  printf ("Too many Clients\n");
                  exit(0);
            }
        }

        //Iterate through all clients that are connected at present.
        for (i = 0; i < FD_SETSIZE; i++) 
        {    
            if (clientSockets[i] > 0 && FD_ISSET( clientSockets[i] , &readfds)) 
            {     
                  //printf("FD %d selected\n", clientSockets[i]);
                  memset(buf,0,sizeof(buf));
                  memset(subBuf1,0,sizeof(subBuf1));
                  memset(subBuf2,0,sizeof(subBuf2));
                  ssize_t numBytesRcvd = recv(clientSockets[i], buf, MAXSTRINGLENGTH, 0);
                  if(numBytesRcvd<0)
                     Die("recvfrom() failed in server.\n");

                  //Client closed
                  if(numBytesRcvd==0){
                     close(clientSockets[i]);
                     struct groupNode *crwl = myList;
                     while(crwl!=NULL){
                           int k;
                           for(k=0;k<FD_SETSIZE;k++){
                              if(crwl->g.arrayOfMem[k]==clientSockets[i]){
                                 crwl->g.arrayOfMem[k]=0;
                                 break;
                              }
                           }
                        crwl = crwl->next;
                     }
                     clientSockets[i]=0;
                     continue;
                  }
                  
                  const char s[2] = "$";
                  char *token;
                  token = strtok(buf, s);
                  
                  //If there is a message to a group.
                  if(strcmp(token,"GROUPMSG")==0){
                     token = strtok(NULL, s);
                     if(token==NULL){
                        printf("Wrong Message Format.\n");
                        continue;
                     }
                     int groupNumber = atoi(token);
                     token = strtok(NULL, s);
                     if(token==NULL){
                        printf("Wrong Message Format.\n");
                        continue;
                     }
                     //Search for group and message group. If doesn't exist print error.
                     struct groupNode *crwl = myList;
                     int flagFound=0;
                     while(crwl!=NULL){
                        if(crwl->g.groupNo == groupNumber){
                           flagFound=1;
                           if(isMember(clientSockets[i], crwl->g.arrayOfMem)==0){
                              printf("Client is not a group member of group .Hence, message not sent.\n");
                              break;
                           }
                           int k;
                           for(k=0;k<FD_SETSIZE;k++){
                              if(crwl->g.arrayOfMem[k]>0&&crwl->g.arrayOfMem[k]!=clientSockets[i]){
                                 ssize_t numBytesSent = send(crwl->g.arrayOfMem[k], token, strlen(token)+1, 0);
                                 if(numBytesSent==-1){
                                    perror("Send error : ");
                                    printf("to Client No %d\n", i);
                                 }
                              }
                           }
                           break;
                        }
                        crwl = crwl->next;
                     }
                     if(flagFound==0){
                        printf("Group %d Not found.\n", groupNumber);
                     }
                  }

                  //New group to be created.
                  else if(strcmp(token,"GROUP")==0){
                     token = strtok(NULL, s);
                     if(token==NULL){
                        printf("Wrong Message Format.\n");
                        continue;
                     }
                     int groupNumber = atoi(token);
                     //Search for group and add to group. If doesn't exist, create one.
                     struct groupNode *crwl = myList;
                     int flagGroupFound = 0;
                     
                     //First element not present.
                     if(crwl==NULL){
                        //printf("GROUP list was empty. Group %d created.\n", groupNumber);
                        myList = (struct groupNode*)malloc(sizeof(struct groupNode));
                        myList->g.groupNo = groupNumber;
                        myList->g.numberOfMembers = 0;
                        myList->next=NULL;
                        memset(myList->g.arrayOfMem, 0, sizeof(myList->g.arrayOfMem));
                        int k;
                        for(k=0;k<FD_SETSIZE;k++){
                           if(myList->g.arrayOfMem[k]==0){
                              myList->g.arrayOfMem[k]=clientSockets[i];
                              break;
                           }
                        }
                     }

                     //First element present.
                     else{
                        while(crwl->next!=NULL){
                           if(crwl->g.groupNo==groupNumber){
                              //printf("GROUP was found. Client %d added to group %d\n",clientSockets[i], groupNumber);
                              if(isMember(clientSockets[i], crwl->g.arrayOfMem)==0){
                                 int k;
                                 for(k=0;k<FD_SETSIZE;k++){
                                    if(crwl->g.arrayOfMem[k]==0){
                                       crwl->g.arrayOfMem[k]=clientSockets[i];
                                       break;
                                    }
                                 }
                              }
                              flagGroupFound = 1;
                              break;
                           }
                           crwl = crwl->next;
                        }
                        if(crwl->g.groupNo==groupNumber){
                              //printf("GROUP was found. Client %d added to group %d\n",clientSockets[i], groupNumber);
                              if(isMember(clientSockets[i], crwl->g.arrayOfMem)==0){
                                 int k;
                                 for(k=0;k<FD_SETSIZE;k++){
                                    if(crwl->g.arrayOfMem[k]==0){
                                       crwl->g.arrayOfMem[k]=clientSockets[i];
                                       break;
                                    }
                                 }   
                              }
                              flagGroupFound = 1;
                        }
                        if(flagGroupFound==0){
                           //printf("GROUP list was not empty. Group %d created.\n", groupNumber);
                           crwl->next = (struct groupNode*)malloc(sizeof(struct groupNode));
                           crwl->next->g.groupNo = groupNumber;
                           crwl->next->g.numberOfMembers = 0;
                           crwl->next->next=NULL;
                           memset(crwl->next->g.arrayOfMem, 0, sizeof(crwl->next->g.arrayOfMem));
                           int k;
                           for(k=0;k<FD_SETSIZE;k++){
                              if(crwl->next->g.arrayOfMem[k]==0){
                                 crwl->next->g.arrayOfMem[k]=clientSockets[i];
                                 break;
                              }
                           }
                        }
                     }
                  }

                  //Message for all.
                  else{
                     int j;
                     for(j=0;j<FD_SETSIZE; j++){
                        if(clientSockets[j]>0&&clientSockets[j]!=clientSockets[i]){
                           ssize_t numBytesSent = send(clientSockets[j], buf, MAXSTRINGLENGTH, 0);
                           if(numBytesSent==-1){
                              perror("Send error : ");
                              printf("to Client No %d\n", i);
                           }
                        }
                     }
                  }
            }
        }
      
   }
   close(sd);
}

