#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define howManyTypesCanBeSubscribed 15
#define maxAmountOfTypesAvailableOnServer 100
struct message {
    long type;
    char text[1024];
    int priority;
};
struct messagesQueue{
    struct message msg;
    struct messagesQueue *next;
};
struct subscription {
    long type;
    int wayOfSubscribing;
    long timeOfSubscription;
    long lengthOfSubscription;
    struct subscription *next;
};
struct availableType {
    long type;
    struct availableType *next;
};
struct login {
    long PID;
    char name[64];
    char password[64];
    struct subscription *subscriptions;
    struct availableType *availableTypes;
    int serverNotification; //send or do not send the notification to a server when you receive the message
    int displayMessages; //synchronically or asynchronically
};

void deleteSubscription(struct subscription **subscriptions, long type) {
    printf("Deleting subscription\n");
    struct subscription *subscriptionIterator = *subscriptions;
    struct subscription *previous;
    if (subscriptionIterator->type == type) {
        *subscriptions = subscriptionIterator->next;
        free(subscriptionIterator);
    } else {
        while (subscriptionIterator && subscriptionIterator->type != type) {
            previous = subscriptionIterator;
            subscriptionIterator = subscriptionIterator->next;
        }
        previous->next = subscriptionIterator->next;
        free(subscriptionIterator);
    }
    printf("Subscription deleted\n");
}

void checkWhetherSomeSubscriptionsHadExpired(struct login *client) {
    struct subscription *loginSubscriptionIterator=client->subscriptions;
    long timeNow;
    while (loginSubscriptionIterator) {
        if (loginSubscriptionIterator->wayOfSubscribing == 2) {
            timeNow = time(NULL);
            if (loginSubscriptionIterator->timeOfSubscription + loginSubscriptionIterator->lengthOfSubscription < timeNow) {
                deleteSubscription(&client->subscriptions, loginSubscriptionIterator->type);
            }
        }
        loginSubscriptionIterator = loginSubscriptionIterator->next;
    }
}

void notifyTheServer(struct login *client){
    struct message instructionMessage;
    instructionMessage.type = 1;
    int instructionID = msgget(10001, 0644 | IPC_CREAT);
    memset(instructionMessage.text, 0, strlen(instructionMessage.text));
    strcpy(instructionMessage.text, "n\0");
    msgsnd(instructionID, &instructionMessage, sizeof(instructionMessage) - sizeof(long), 0);
    struct message notificationMessage;
    notificationMessage.type = 16;
    int notificationID = msgget(10016, 0644 | IPC_CREAT);
    strncpy(notificationMessage.text,client->name,strlen(client->name));
    msgsnd(notificationID, &notificationMessage, sizeof(notificationMessage) - sizeof(long), 0);
}

void addToMessagesQueue(struct messagesQueue **queue,struct message *newMessage){
    struct messagesQueue *queueIterator = *queue;
    int typeAlreadyInQueue=0;
    int typePosition = 0;
    struct messagesQueue *q=malloc(sizeof(struct messagesQueue)),*previous=*queue,*tmp;
    q->msg.type=newMessage->type;
    q->msg.priority=newMessage->priority;
    memset(q->msg.text,0,strlen(q->msg.text));
    strcat(q->msg.text,newMessage->text);
    q->next=NULL;
    if(*queue==NULL){
        *queue=q;
    }else{
        while(queueIterator){
            if(newMessage->priority<=queueIterator->msg.priority)typePosition++;
            if(queueIterator->msg.type==newMessage->type){
                typeAlreadyInQueue=1;
                break;
            }
            queueIterator=queueIterator->next;
        }
        if(typeAlreadyInQueue){
            queueIterator = *queue;
            while(queueIterator){
                if(queueIterator->msg.type==newMessage->type){
                    while(queueIterator->next && queueIterator->next->msg.type==newMessage->type){
                        previous=queueIterator;
                        queueIterator=queueIterator->next;
                    }
                    if(queueIterator->next==NULL){
                        queueIterator->next=q;
                    }else{
                        tmp=queueIterator->next;
                        previous->next=q;
                        q->next=tmp;
                    }
                    break;
                }
                previous=queueIterator;
                queueIterator=queueIterator->next;
            }
        }else{
            if(typePosition==0){
                q->next = *queue;
                *queue=q;
            }else{
                int currentTypePosition=0;
                queueIterator=*queue;
                while(queueIterator){
                    if(queueIterator->msg.type!=previous->msg.type){
                        currentTypePosition++;
                        if(currentTypePosition==typePosition){
                            if(queueIterator->next==NULL){
                                queueIterator->next=q;
                            }else{
                                tmp=queueIterator->next;
                                previous->next=q;
                                q->next=tmp;
                            }
                        }
                    }
                    previous=queueIterator;
                    queueIterator=queueIterator->next;
                }
            }
        }
    }
}

void receiveNewMessages(struct login *client){
    checkWhetherSomeSubscriptionsHadExpired(client);
    struct subscription *subscriptionIterator = client->subscriptions;
    int receiveMessagesID=msgget((key_t) client->PID, 0644 | IPC_CREAT);
    int noMoreMessagesOfThisType=0;
    struct message receivedMessage;
    int somethingReceived=0;
    struct messagesQueue *queue=NULL;
    char typeString[8];
    int i;
    if(client->displayMessages==1)printf("New Messages:\n");
    while(subscriptionIterator){
        noMoreMessagesOfThisType=0;
        do{
            if(msgrcv(receiveMessagesID,&receivedMessage,sizeof(receivedMessage)-sizeof(long),subscriptionIterator->type,IPC_NOWAIT)==-1){
                noMoreMessagesOfThisType=1;
            }else{
                somethingReceived=1;
                i=0;
                memset(typeString,0,strlen(typeString));
                for(;i<strlen(receivedMessage.text);i++){
                    if(receivedMessage.text[i]==';')break;
                }
                strncpy(typeString,receivedMessage.text,i);
                if(i<7){
                    typeString[i]='\0';
                }
                struct message newMessage;
                long type = strtol(typeString,NULL,10);
                newMessage.type=type;
                newMessage.priority=receivedMessage.priority;
                memset(newMessage.text,0,sizeof(newMessage.text));
                strcpy(newMessage.text,receivedMessage.text);
                addToMessagesQueue(&queue,&newMessage);
            }
        }while(!noMoreMessagesOfThisType);
        subscriptionIterator=subscriptionIterator->next;
    }
    if(!somethingReceived && client->displayMessages==1){
        printf("\nThere are no new messages\n\n");
    }
    if(somethingReceived){
        if(client->displayMessages==2)printf("New Messages:\n");
        struct messagesQueue *queueIterator=queue,*previous=queue;
        printf("\nType:%ld\n",queueIterator->msg.type);
        int i,j;
        char actualMessage[1024];
        char userName[64];
        while(queueIterator){
            if(previous->msg.type!=queueIterator->msg.type){
                printf("\nType:%ld\n",queueIterator->msg.type);
            }
            i=0;
            for(;i<strlen(queueIterator->msg.text);i++){
                if(queueIterator->msg.text[i]==';')break;
            }
            j=i+1;
            for(;j<strlen(queueIterator->msg.text);j++){
                if(queueIterator->msg.text[j]==';')break;
            }
            strncpy(userName,queueIterator->msg.text+i+1,j-i-1);
            userName[j-i-1]='\0';
            strncpy(actualMessage,queueIterator->msg.text+j+1,strlen(queueIterator->msg.text)-j-1);
            printf("Message:%s\n",actualMessage);
            printf("User sending:%s\n\n",userName);
            previous=queueIterator;
            queueIterator=queueIterator->next;
        }
        if(client->serverNotification==1)notifyTheServer(client);
    }
}

void getAvailableTypes(struct login *client) {
    client->availableTypes = malloc(sizeof(struct availableType));
    int instructionID = msgget(10001, 0644 | IPC_CREAT);
    struct message instructionMessage;
    instructionMessage.priority = 1;
    instructionMessage.type = 1;
    strcpy(instructionMessage.text, "a\0");
    msgsnd(instructionID, &instructionMessage, sizeof(instructionMessage) - sizeof(long), 0);
    struct message availableTypesMessage;
    int availableTypesID = msgget(10004, 0644 | IPC_CREAT);
    msgrcv(availableTypesID, &availableTypesMessage, sizeof(availableTypesMessage) - sizeof(long), 4, 0);
    int i, typeStartIndex = 0;
    char typeString[8];
    struct availableType *availableTypes = NULL;
    struct availableType *tmp, *type;
    for (i = 0; i < strlen(availableTypesMessage.text); i++) {
        if (availableTypesMessage.text[i] == ';') {
            strncpy(typeString, availableTypesMessage.text + typeStartIndex, i - typeStartIndex);
            typeStartIndex = i + 1;
            if (availableTypes == NULL) {
                availableTypes = malloc(sizeof(struct availableType));
                availableTypes->type = strtol(typeString, NULL, 10);
                availableTypes->next = NULL;
            } else {
                tmp = availableTypes;
                while (tmp->next) {
                    tmp = tmp->next;
                }
                type = malloc(sizeof(struct availableType));
                type->type = strtol(typeString, NULL, 10);
                type->next = NULL;
                tmp->next = type;
                tmp = availableTypes;
            }
        }
    }
    client->availableTypes = availableTypes;
}

void showClientsSubscriptions(struct subscription *subscriptions) {
    printf("Types of message you subscribe:\n");
    struct subscription *subscriptionIterator = subscriptions;
    if (subscriptionIterator == NULL) {
        printf("You don't subscribe any type\n");
    } else {
        while (subscriptionIterator) {
            printf("type:%ld\n", subscriptionIterator->type);
            if (subscriptionIterator->wayOfSubscribing == 1) {
                printf("Way of subscribing: permament\n");
            } else {
                printf("Way of subscribing: temporary\n");
                long timeLeft =
                        subscriptionIterator->timeOfSubscription + subscriptionIterator->lengthOfSubscription -
                        time(NULL);
                printf("Seconds left: %ld\n", timeLeft);
            }
            struct tm * timeinfo=localtime (&subscriptionIterator->timeOfSubscription);
            printf ("The time you have subscribed the type: %s", asctime(timeinfo));
            subscriptionIterator = subscriptionIterator->next;
        }
    }
}

void setSubscription(struct subscription **subscriptions, long type,struct login *client) {
    struct subscription *s = malloc(sizeof(struct subscription));
    char wayOfSubscribing;
    int c;
    long numberOfSeconds;
    printf("Choose the way you wish to subscribe your type of message\n");
    printf("(1)Permament\n");
    printf("(2)Temporary\n");
    do {
        c = getchar();
        wayOfSubscribing = (char) c;
        if(client->displayMessages==2)receiveNewMessages(client);
    } while (wayOfSubscribing != '1' && wayOfSubscribing != '2');
    s->type = type;
    s->wayOfSubscribing = wayOfSubscribing - 48;
    if (s->wayOfSubscribing == 2) {
        printf("choose how long(in seconds) do you wish to subscribe your type of message:\n");
        scanf("%ld", &numberOfSeconds);
        if(client->displayMessages==2)receiveNewMessages(client);
        s->lengthOfSubscription = numberOfSeconds;
    } else {
        s->lengthOfSubscription = 0;
    }
    s->timeOfSubscription = time(NULL);
    s->next = NULL;
    if (*subscriptions == NULL) {
        *subscriptions = s;
    } else {
        struct subscription *tmp = *subscriptions;
        while (tmp->next) {
            tmp = tmp->next;
        }
        tmp->next = s;
    }
}

void showAvailableTypes(struct login *client){
    printf("Available types on the server:\n");
    struct availableType *availableTypesList = client->availableTypes;
    while (availableTypesList) {
        printf("%ld\n", availableTypesList->type);
        availableTypesList = availableTypesList->next;
    }
}

void addSubscription(struct login *client){
    int c;
    long chosenType;
    printf("Choose type of message you wish to subscribe:\n");
    showAvailableTypes(client);
    printf("\n");
    struct subscription *occupiedTypes = client->subscriptions;
    struct availableType *availableTypesList;
    int alreadyChosen, isAvailableToChoose;
    char chosenTypeString[8];
    do {
        availableTypesList = client->availableTypes;
        memset(chosenTypeString,0,strlen(chosenTypeString));
        alreadyChosen = 0;
        isAvailableToChoose = 0;
        int iterator = 0;
        while ((c = getchar()) != '\n') {
            chosenTypeString[iterator++] = (char) c;
            if (iterator == (8)) break;
        }
        if(iterator<8)chosenTypeString[iterator]='\0';
        chosenType = strtol(chosenTypeString, NULL, 10);
        while (occupiedTypes) {
            if (occupiedTypes->type == chosenType) {
                printf("\nYou already subscribe this type of message\n");
                printf("Choose again\n");
                alreadyChosen = 1;
                break;
            }
            occupiedTypes = occupiedTypes->next;
        }
        if(!alreadyChosen){
            while (availableTypesList) {
                if (availableTypesList->type == chosenType) {
                    isAvailableToChoose = 1;
                    break;
                }
                availableTypesList = availableTypesList->next;
            }
            if (isAvailableToChoose == 0) {
                printf("\nSuch type is not available on the server\n");
                printf("Choose again:\n");
            }
        }
    } while (alreadyChosen || !isAvailableToChoose);
    setSubscription(&client->subscriptions, chosenType,client);
}

int canSubscribeMoreTypes(struct login *client){
    getAvailableTypes(client);
    int howManyTypesAreAvailable=0;
    struct availableType *availableTypesList = client->availableTypes;
    while (availableTypesList) {
        howManyTypesAreAvailable++;
        availableTypesList = availableTypesList->next;
    }
    struct subscription *occupiedTypes=client->subscriptions;
    int howManyTypesAreOccupied=0;
    while (occupiedTypes) {
        howManyTypesAreOccupied++;
        if (howManyTypesAreAvailable == howManyTypesAreOccupied) {
            printf("\nYou already subscribe all available types\n");
            return 0;
        }
        if (howManyTypesAreAvailable == howManyTypesCanBeSubscribed) {
            printf("\nYou have reached maximum amount of types that you can subscribe\n");
            return 0;
        }
        occupiedTypes = occupiedTypes->next;
    }
    return 1;
}

void chooseSubscriptions(struct login *client) {
    printf("Choose your subsrciptions:\n");
    int c;
    char chosenOption; // 1 - Client wish to subscribe more types, 2 - client do not wish that
    do {
        if(!canSubscribeMoreTypes(client))break;
        addSubscription(client);
        showClientsSubscriptions(client->subscriptions);
        printf("Do you wish to subscribe more types?\n");
        printf("(1)Yes\n");
        printf("(2)No\n");
        do {
            c = getchar();
            chosenOption = (char) c;
            if(client->displayMessages==2)receiveNewMessages(client);
        } while (chosenOption != '1' && chosenOption != '2');
        getchar();//clearing buffer
    } while (chosenOption != '2');
    showClientsSubscriptions(client->subscriptions);
}

void registration(struct login *client) {
    while (getpid() >= 10001 && getpid() <= 10019) {
        if (fork() != 0) exit(0);
    }
    int registrationID = msgget(10002, 0644 | IPC_CREAT);
    struct message registrationMessage;
    registrationMessage.priority = 2;
    registrationMessage.type = 2;
    int registrationLength = 0;
    int nameSize = 64;
    int c, nameEndIndex, passwordStartIndex, passwordEndIndex;
    char messagesOption, displayOption;
    int instructionID = msgget(10001, 0644 | IPC_CREAT);
    struct message instructionMessage;
    strcpy(instructionMessage.text,
           "v\0"); // instruction is checking name validation(chcecking whether it's already in registered database)
    instructionMessage.priority = 1;
    instructionMessage.type = 1;
    struct message nameVerificationMessage;
    nameVerificationMessage.type = 5;
    nameVerificationMessage.priority = 5;
    struct message nameVerificationMessageResponse;
    nameVerificationMessageResponse.type = 8;
    nameVerificationMessageResponse.priority = 8;
    int nameValidationID = msgget(10005, 0644 | IPC_CREAT);
    int nameValidationResponseID = msgget(getpid(), 0644 | IPC_CREAT);
    int pidLength = snprintf(NULL, 0, "%d", getpid());
    char *pidString = malloc(pidLength + 1);
    snprintf(pidString, pidLength + 1, "%d", getpid());
    do {
        memset(registrationMessage.text, 0, strlen(registrationMessage.text));
        registrationLength = 0;
        printf("Enter your name:\n");
        while ((c = getchar()) != '\n') {
            registrationMessage.text[registrationLength++] = (char) c;
            if (registrationLength == (nameSize)) break;
        }
        msgsnd(instructionID, &instructionMessage, sizeof(instructionMessage) - sizeof(long), 0);
        registrationMessage.text[registrationLength] = ';';
        registrationMessage.text[registrationLength + 1] = '\0';
        memset(nameVerificationMessage.text, 0, strlen(nameVerificationMessage.text));
        strcat(nameVerificationMessage.text, registrationMessage.text);
        strcat(nameVerificationMessage.text, pidString);
        strcat(nameVerificationMessage.text, ";\0");
        msgsnd(nameValidationID, &nameVerificationMessage, sizeof(nameVerificationMessage) - sizeof(long), 0);
        msgrcv(nameValidationResponseID, &nameVerificationMessageResponse,
               sizeof(nameVerificationMessageResponse) - sizeof(long), 8, 0);
        printf("%s\n", nameVerificationMessageResponse.text);
    } while (strcmp(nameVerificationMessageResponse.text, "validated") != 0);
    nameEndIndex = registrationLength - 1;
    registrationMessage.text[registrationLength] = ';';
    passwordStartIndex = ++registrationLength;
    printf("Enter your password:\n");
    while ((c = getchar()) != '\n') {
        registrationMessage.text[registrationLength++] = (char) c;
        if (registrationLength == (nameSize + passwordStartIndex)) break;

    }
    passwordEndIndex = registrationLength - 1;
    registrationMessage.text[registrationLength] = ';';
    printf("Choose the way you wish to get messages\n");
    printf("(1)With notifications for a server\n");
    printf("(2)Without notifications for a server\n");
    do {
        c = getchar();
        messagesOption = (char) c;
    } while (messagesOption != '1' && messagesOption != '2');
    registrationMessage.text[++registrationLength] = messagesOption;
    registrationMessage.text[++registrationLength] = ';';
    printf("Choose the way you wish to display your messages\n");
    printf("(1)synchronically\n");
    printf("(2)asynchronically\n");
    do {
        c = getchar();
        displayOption = (char) c;
    } while (displayOption != '1' && displayOption != '2');
    registrationMessage.text[++registrationLength] = displayOption;
    registrationMessage.text[++registrationLength] = ';';
    getchar();//clearing buffer
    chooseSubscriptions(client);
    struct subscription *subIterator = client->subscriptions;
    while (subIterator) {
        registrationLength++;
        int typeLength = snprintf(NULL, 0, "%ld", subIterator->type);
        char *t = malloc(typeLength + 1);
        snprintf(t, typeLength + 1, "%ld", subIterator->type);
        strncpy(registrationMessage.text + registrationLength, t, typeLength);
        free(t);
        registrationLength += typeLength;
        registrationMessage.text[registrationLength] = '-';
        registrationMessage.text[++registrationLength] = (char) (subIterator->wayOfSubscribing + 48);
        registrationMessage.text[++registrationLength] = '-';
        int subLength = snprintf(NULL, 0, "%ld", subIterator->lengthOfSubscription);
        char *len = malloc(subLength + 1);
        snprintf(t, subLength + 1, "%ld", subIterator->lengthOfSubscription);
        strncpy(registrationMessage.text + registrationLength + 1, len, subLength);
        registrationLength += subLength;
        free(len);
        registrationMessage.text[++registrationLength] = '-';
        int subTimeLength = snprintf(NULL, 0, "%ld", subIterator->timeOfSubscription);
        char *tim = malloc(subTimeLength + 1);
        snprintf(tim, subTimeLength + 1, "%ld", subIterator->timeOfSubscription);
        strncpy(registrationMessage.text + registrationLength + 1, tim, subTimeLength);
        free(tim);
        registrationLength += subTimeLength;
        if (subIterator->next == NULL) {
            registrationMessage.text[++registrationLength] = ';';
            break;
        }
        registrationMessage.text[++registrationLength] = ',';
        subIterator = subIterator->next;
    }
    memset(instructionMessage.text, 0, strlen(instructionMessage.text));
    strcpy(instructionMessage.text, "g\0");
    msgsnd(instructionID, &instructionMessage, sizeof(instructionMessage) - sizeof(long), 0);
    registrationMessage.text[++registrationLength] = '\0';
    msgsnd(registrationID, &registrationMessage, sizeof(registrationMessage) - sizeof(long), 0);
    strncpy(client->name, registrationMessage.text, nameEndIndex + 1);
    strncpy(client->password, registrationMessage.text + passwordStartIndex, passwordEndIndex - passwordStartIndex + 1);
    client->name[nameEndIndex + 1] = '\0';
    client->password[passwordEndIndex - passwordStartIndex + 1] = '\0';
    client->PID = getpid();
    instructionID = msgget(10001, 0644 | IPC_CREAT);
    strcpy(instructionMessage.text, "l\0");
    msgsnd(instructionID, &instructionMessage, sizeof(instructionMessage) - sizeof(long), 0);
    struct message loginMessage;
    int loginID = msgget(10003, 0644 | IPC_CREAT);
    loginMessage.priority = 3;
    loginMessage.type = 3;
    strncpy(loginMessage.text, registrationMessage.text, passwordEndIndex + 2);
    loginMessage.text[passwordEndIndex + 2] = '\0';
    strcat(loginMessage.text, pidString);
    strcat(loginMessage.text, ";r\0");
    msgsnd(loginID, &loginMessage, sizeof(loginMessage) - sizeof(long), 0);
}

void getYourData(struct login *client) {
    struct message instructionMessage;
    int instructionID = msgget(10001, 0664 | IPC_CREAT);
    instructionMessage.type = 1;
    instructionMessage.priority = 1;
    strcat(instructionMessage.text, "d\0");
    msgsnd(instructionID, &instructionMessage, sizeof(instructionMessage) - sizeof(long), 0);
    struct message getDataRequestMessage;
    int getDataRequestID = msgget(10009, 0664 | IPC_CREAT);
    getDataRequestMessage.type = 9;
    getDataRequestMessage.priority = 9;
    strcat(getDataRequestMessage.text, client->name);
    getDataRequestMessage.text[strlen(client->name)] = '\0';
    msgsnd(getDataRequestID, &getDataRequestMessage, sizeof(getDataRequestMessage) - sizeof(long), 0);
    int dataID = msgget(getpid(), 0644 | IPC_CREAT);
    struct message dataMessage;
    dataMessage.type = 10;
    msgrcv(dataID, &dataMessage, sizeof(dataMessage) - sizeof(long), 10, 0);
    int j = 0, numberOfDashes = 0, typeStartIndex, subLengthStartIndex, subTimeStartIndex;
    client->serverNotification = dataMessage.text[j] - 48;
    j += 2;
    client->displayMessages = dataMessage.text[j] - 48;
    j += 2;
    struct subscription *s = NULL, *iterator;
    typeStartIndex = j;
    for (; j < strlen(dataMessage.text); j++) {
        if (dataMessage.text[j] == '-') {
            if (numberOfDashes == 0) {
                s = malloc(sizeof(struct subscription));
                s->next = NULL;
                char *str = malloc(20 * sizeof(char));
                strncpy(str, dataMessage.text + typeStartIndex, j - typeStartIndex);
                s->type = strtol(str, NULL, 10);
            } else if (numberOfDashes == 1) {
                s->wayOfSubscribing = dataMessage.text[j - 1] - 48;;
                subLengthStartIndex = j + 1;
            } else if (numberOfDashes == 2) {
                char *str = malloc(20 * sizeof(char));
                strncpy(str, dataMessage.text + subLengthStartIndex, j - subLengthStartIndex);
                s->lengthOfSubscription = strtol(str, NULL, 10);
                subTimeStartIndex = j + 1;
            }
            numberOfDashes++;
        }
        if (dataMessage.text[j] == ',' || dataMessage.text[j] == ';') {
            numberOfDashes = 0;
            typeStartIndex = j + 1;
            char *str = malloc(20 * sizeof(char));
            strncpy(str, dataMessage.text + subTimeStartIndex, j - subTimeStartIndex);
            s->timeOfSubscription = strtol(str, NULL, 10);
            if (client->subscriptions == NULL) {
                client->subscriptions = s;
            } else {
                iterator = client->subscriptions;
                while (iterator->next) {
                    iterator = iterator->next;
                }
                iterator->next = s;
            }
        }
        if (dataMessage.text[j] == ';')break;
    }
}

void ManageYourSubscriptions(struct login *client){
    checkWhetherSomeSubscriptionsHadExpired(client);
    int c;
    char subscriptionOption;
    if(client->subscriptions==NULL){
        subscriptionOption=(char)49;// = '1'
    }else{
        printf("\nChoose one of the following options:\n");
        printf("(1) Add new subscription\n");
        printf("(2) Delete a subscription\n");
        printf("(3) Show your subscriptions\n");
        do {
            c = getchar();
            subscriptionOption = (char) c;
            if(client->displayMessages==2)receiveNewMessages(client);
        } while (subscriptionOption != '1' && subscriptionOption != '2' && subscriptionOption != '3');
        getchar(); //clearing bufor;
    }
    struct message instructionMessage;
    instructionMessage.type=1;
    memset(instructionMessage.text,0,strlen(instructionMessage.text));
    strcat(instructionMessage.text,"c\0");
    struct message manageSubscriptionMessage;
    manageSubscriptionMessage.type=12;
    strncpy(manageSubscriptionMessage.text,client->name,strlen(client->name));
    manageSubscriptionMessage.text[strlen(client->name)]=';';
    manageSubscriptionMessage.text[strlen(client->name)+1]='\0';
    char whatToChange[4];
    whatToChange[0]='s';//modify subscriptions
    whatToChange[1]=subscriptionOption;
    whatToChange[2]=';';
    whatToChange[3]='\0';
    strcat(manageSubscriptionMessage.text,whatToChange);
    if(subscriptionOption=='1'){
        if(canSubscribeMoreTypes(client)){
            addSubscription(client);
            struct subscription *subscriptionIterator = client->subscriptions;
            while (subscriptionIterator->next) {
                subscriptionIterator=subscriptionIterator->next;
            }
            int typeLength = snprintf(NULL,0,"%ld",subscriptionIterator->type);
            char *typeString = malloc(typeLength+1);
            snprintf(typeString,typeLength+1,"%ld",subscriptionIterator->type);
            int subLengthSize = snprintf(NULL,0,"%ld",subscriptionIterator->lengthOfSubscription);
            char *subLengthString = malloc(subLengthSize+1);
            snprintf(subLengthString,subLengthSize+1,"%ld",subscriptionIterator->lengthOfSubscription);
            int timeLength = snprintf(NULL,0,"%ld",subscriptionIterator->timeOfSubscription);
            char *timeString = malloc(timeLength+1);
            snprintf(timeString,timeLength+1,"%ld",subscriptionIterator->timeOfSubscription);
            strcat(manageSubscriptionMessage.text,typeString);
            strcat(manageSubscriptionMessage.text,"-\0");
            int length = (int) strlen(manageSubscriptionMessage.text);
            manageSubscriptionMessage.text[length]=(char)(subscriptionIterator->wayOfSubscribing+48);
            manageSubscriptionMessage.text[length+1]='\0';
            strcat(manageSubscriptionMessage.text,"-\0");
            strcat(manageSubscriptionMessage.text,subLengthString);
            strcat(manageSubscriptionMessage.text,"-\0");
            strcat(manageSubscriptionMessage.text,timeString);
            strcat(manageSubscriptionMessage.text,";\0");
        }else{
            return;
        }
    } else if(subscriptionOption=='2'){
        int found=0;
        do {
            showClientsSubscriptions(client->subscriptions);
            long typeToDelete;
            printf("Choose the type you wish to delete:\n");
            scanf("%ld", &typeToDelete);
            if(client->displayMessages==2)receiveNewMessages(client);
            struct subscription *subscriptionIterator = client->subscriptions;
            while (subscriptionIterator) {
                if (subscriptionIterator->type == typeToDelete) {
                    found=1;
                    deleteSubscription(&client->subscriptions,typeToDelete);
                    int typeLength = snprintf(NULL,0,"%ld",typeToDelete);
                    char *typeString = malloc(typeLength+1);
                    snprintf(typeString,typeLength+1,"%ld",typeToDelete);
                    strcat(manageSubscriptionMessage.text,typeString);
                    strcat(manageSubscriptionMessage.text,";\0");
                    break;
                }
                subscriptionIterator = subscriptionIterator->next;
            }
            if(found==0)printf("You can't delete this type\n");
        }while(found==0);
    }else {
        showClientsSubscriptions(client->subscriptions);
        return;
    }
    int instructionID = msgget(10001, 0664 | IPC_CREAT);
    int manageSubscriptionID = msgget(10012, 0664 | IPC_CREAT);
    msgsnd(instructionID,&instructionMessage,sizeof(instructionMessage)-sizeof(long),0);
    msgsnd(manageSubscriptionID,&manageSubscriptionMessage,sizeof(manageSubscriptionMessage)-sizeof(long),0);
    while((c=getchar())!='\n'){}//clearing buffer
}

void login(struct login *client) {
    while (getpid() >= 10001 && getpid() <= 10019) {
        if (fork() != 0) exit(0);
    }
    struct message loginMessage;
    struct message validationMessage;
    struct message instructionMessage;
    int instructionID = msgget(10001, 0644 | IPC_CREAT);
    int loginID = msgget(10003, 0644 | IPC_CREAT);
    loginMessage.priority = 3;
    loginMessage.type = 3;
    instructionMessage.priority = 1;
    instructionMessage.type = 1;
    int loginLength;
    int nameSize = 64, c;
    do {
        loginLength = 0;
        memset(loginMessage.text, 0, strlen(loginMessage.text));
        printf("\nEnter your name:\n");
        while ((c = getchar()) != '\n') {
            loginMessage.text[loginLength++] = (char) c;
            if (loginLength == (nameSize)) break;
        }
        loginMessage.text[loginLength++] = ';';
        printf("Enter your password:\n");
        while ((c = getchar()) != '\n') {
            loginMessage.text[loginLength++] = (char) c;
            if (loginLength == (nameSize)) break;
        }
        loginMessage.text[loginLength++] = ';';
        loginMessage.text[loginLength] = '\0';
        int pidLength = snprintf(NULL, 0, "%d", getpid());
        char *pidString = malloc(pidLength + 1);
        snprintf(pidString, pidLength + 1, "%d", getpid());
        strcat(loginMessage.text, pidString);
        strcat(loginMessage.text, ";\0");
        strcpy(instructionMessage.text, "l\0");
        msgsnd(instructionID, &instructionMessage, sizeof(instructionMessage) - sizeof(long), 0);
        msgsnd(loginID, &loginMessage, sizeof(loginMessage) - sizeof(long), 0);
        int validationID = msgget(getpid(), 0644 | IPC_CREAT);
        msgrcv(validationID, &validationMessage, sizeof(validationMessage) - sizeof(long), 6, 0);
        if(strcmp(validationMessage.text, "validated") != 0){
            printf("%s\n",validationMessage.text);
        }
    } while (strcmp(validationMessage.text, "validated") != 0);
    int i, j;
    for (j = 0; j < strlen(loginMessage.text); j++) {
        if (loginMessage.text[j] == ';')break;
    }
    strncpy(client->name, loginMessage.text, j);
    for (i = j + 1; i < strlen(loginMessage.text); i++) {
        if (loginMessage.text[i] == ';')break;
    }
    strncpy(client->password, loginMessage.text + j + 1, i - j);
    client->PID = getpid();
    printf("you're logged in!\n");
    getYourData(client);
    if(client->subscriptions==NULL){
        printf("Your subscriptions have expired:\n");
        ManageYourSubscriptions(client);
    }
}

void logOut(struct login *client){
    int instructionID = msgget(10001, 0664 | IPC_CREAT);
    struct message instructionMessage;
    instructionMessage.type=1;
    memset(instructionMessage.text,0,strlen(instructionMessage.text));
    strcat(instructionMessage.text,"o\0");
    msgsnd(instructionID,&instructionMessage,sizeof(instructionMessage)-sizeof(long),0);
    int logOutID = msgget(10011, 0664 | IPC_CREAT);
    int pidLength = snprintf(NULL, 0, "%ld", client->PID);
    char *pidString = malloc(sizeof(pidLength+1));
    snprintf(pidString,pidLength+1,"%ld",client->PID);
    struct message logOutMessage;
    logOutMessage.type=11;
    strncpy(logOutMessage.text,pidString,pidLength+1);
    msgsnd(logOutID,&logOutMessage,sizeof(logOutMessage)-sizeof(long),0);
}

void showWholeConversation(struct login *client){
    checkWhetherSomeSubscriptionsHadExpired(client);
    int found;
    int iterator = 0,c;
    long chosenType;
    struct subscription *subscriptionIterator;
    char chosenTypeString[8];
    do{
        printf("Choose type you wish to see the conversation of:\n");
        showClientsSubscriptions(client->subscriptions);
        memset(chosenTypeString,0,strlen(chosenTypeString));
        iterator = 0;
        while ((c = getchar()) != '\n') {
            chosenTypeString[iterator++] = (char) c;
            if (iterator == (8)) break;
        }
        if(iterator<8)chosenTypeString[iterator]='\0';
        chosenType = strtol(chosenTypeString, NULL, 10);
        subscriptionIterator = client->subscriptions;
        found=0;
        while(subscriptionIterator){
            if(subscriptionIterator->type==chosenType){
                found=1;
                break;
            }
            subscriptionIterator=subscriptionIterator->next;
        }
        if(found==0)printf("You don't subscribe this type, try again\n");
    }while(found==0);
    struct message instructionMessage;
    instructionMessage.type = 1;
    int instructionID = msgget(10001, 0644 | IPC_CREAT);
    memset(instructionMessage.text, 0, strlen(instructionMessage.text));
    strcpy(instructionMessage.text, "w\0");
    msgsnd(instructionID, &instructionMessage, sizeof(instructionMessage) - sizeof(long), 0);
    int getConversationID=msgget(10014,0644|IPC_CREAT);
    struct message getConversationMessage;
    getConversationMessage.type=14;
    memset(getConversationMessage.text,0,strlen(getConversationMessage.text));
    strcpy(getConversationMessage.text,client->name);
    getConversationMessage.text[strlen(client->name)]='\0';
    strcat(getConversationMessage.text,";\0");
    strcat(getConversationMessage.text,chosenTypeString);
    strcat(getConversationMessage.text,";\0");
    msgsnd(getConversationID,&getConversationMessage,sizeof(getConversationMessage)-sizeof(long),0);
    int getNextMessageID = msgget((key_t) client->PID, 0644 | IPC_CREAT);
    struct message getNextMessage;
    int i,j;
    int howManyMessages=0;
    char userName[64];
    char actualMessage[1024];
    do {
        msgrcv(getNextMessageID, &getNextMessage, sizeof(getNextMessage) - sizeof(long), 15, 0);
        if (strcmp(getNextMessage.text, "end") == 0 && howManyMessages == 0) {
            printf("There are no messages in this conversation\n");
            break;
        }
        if (strcmp(getNextMessage.text, "end") != 0) {
            memset(actualMessage, 0, strlen(actualMessage));
            memset(userName, 0, strlen(userName));
            for (; i < strlen(getNextMessage.text); i++) {
                if (getNextMessage.text[i] == ';')break;
            }
            j = i + 1;
            for (; j < strlen(getNextMessage.text); j++) {
                if (getNextMessage.text[j] == ';')break;
            }
            strncpy(userName, getNextMessage.text + i + 1, j - i - 1);
            userName[j - i - 1] = '\0';
            strncpy(actualMessage, getNextMessage.text + j + 1, strlen(getNextMessage.text) - j - 1);
            printf("Message:%s", actualMessage);
            printf("User sending:%s\n\n", userName);
        }
        howManyMessages++;
    }while(strcmp(getNextMessage.text,"end")!=0);

}

void changeNotification(struct login *client){
    int c;
    char notificationOption;
    printf("Choose one of the following options\n");
    printf("(1)With notifications to server\n");
    printf("(2)Without notifications to server\n");
    do {
        c = getchar();
        notificationOption = (char) c;
    } while (notificationOption != '1' && notificationOption != '2');
    if(client->displayMessages==2)receiveNewMessages(client);
    getchar(); //clearing bufor;
    int instructionID = msgget(10001, 0664 | IPC_CREAT);
    struct message instructionMessage;
    instructionMessage.type=1;
    memset(instructionMessage.text,0,strlen(instructionMessage.text));
    strcat(instructionMessage.text,"c\0");
    msgsnd(instructionID,&instructionMessage,sizeof(instructionMessage)-sizeof(long),0);
    int changeNotificationID = msgget(10012, 0664 | IPC_CREAT);
    struct message changeNotificationMessage;
    changeNotificationMessage.type=12;
    strncpy(changeNotificationMessage.text,client->name,strlen(client->name));
    changeNotificationMessage.text[strlen(client->name)]=';';
    changeNotificationMessage.text[strlen(client->name)+1]='\0';
    char whatToChange[3];
    whatToChange[0]='n';//change notification
    whatToChange[1]=notificationOption;
    whatToChange[2]='\0';
    strcat(changeNotificationMessage.text,whatToChange);
    msgsnd(changeNotificationID,&changeNotificationMessage,sizeof(changeNotificationMessage)-sizeof(long),0);
    client->serverNotification=notificationOption-48;
}

void changeDisplayMode(struct login *client){
    int c;
    char displayOption;
    printf("Choose one of the following options\n");
    printf("(1)synchronically\n");
    printf("(2)asynchronically\n");
    do {
        c = getchar();
        displayOption = (char) c;
    } while (displayOption != '1' && displayOption != '2');
    getchar(); //clearing bufor;
    int instructionID = msgget(10001, 0664 | IPC_CREAT);
    struct message instructionMessage;
    instructionMessage.type=1;
    memset(instructionMessage.text,0,strlen(instructionMessage.text));
    strcat(instructionMessage.text,"c\0");
    msgsnd(instructionID,&instructionMessage,sizeof(instructionMessage)-sizeof(long),0);
    int changeDisplayID = msgget(10012, 0664 | IPC_CREAT);
    struct message changeDisplayMessage;
    changeDisplayMessage.type=12;
    strncpy(changeDisplayMessage.text,client->name,strlen(client->name));
    changeDisplayMessage.text[strlen(client->name)]=';';
    changeDisplayMessage.text[strlen(client->name)+1]='\0';
    char whatToChange[3];
    whatToChange[0]='d';//change display
    whatToChange[1]=displayOption;
    whatToChange[2]='\0';
    strcat(changeDisplayMessage.text,whatToChange);
    msgsnd(changeDisplayID,&changeDisplayMessage,sizeof(changeDisplayMessage)-sizeof(long),0);
    client->displayMessages=displayOption-48;
}

void sendMessage(struct login *client) {
    int instructionID = msgget(10001, 0644 | IPC_CREAT);
    int sendMessageID = msgget(10007, 0644 | IPC_CREAT);
    struct message sendMessage;
    sendMessage.type = 7;
    int namesize=64,messageSize = 1024,typeSize=8,c = 0,priority,found;
    int iterator = 0;
    long chosenType;
    struct availableType *availableTypes;
    char chosenTypeString[8];
    do{
        getAvailableTypes(client);
        showAvailableTypes(client);
        memset(chosenTypeString,0,strlen(chosenTypeString));
        iterator = 0;
        printf("Write type of message you wish to send:\n");
        while ((c = getchar()) != '\n') {
            chosenTypeString[iterator++] = (char) c;
            if (iterator == (8)) break;
        }
        if(client->displayMessages==2)receiveNewMessages(client);
        if(iterator<8)chosenTypeString[iterator]='\0';
        chosenType = strtol(chosenTypeString, NULL, 10);
        availableTypes=client->availableTypes;
        found=0;
        while(availableTypes){
            if(availableTypes->type==chosenType){
                found=1;
                break;
            }
            availableTypes=availableTypes->next;
        }
        if(found==0)printf("Such type is not available on the server, try again\n");
    }while(found==0);
    if(iterator<8)chosenTypeString[iterator]='\0';
    memset(sendMessage.text,0,strlen(sendMessage.text));
    if(iterator==8)getchar();//clearing buffer
    strcat(sendMessage.text,chosenTypeString);
    if(iterator==8)sendMessage.text[iterator]='\0';
    strcat(sendMessage.text,";\0");
    printf("Set priority(from 1 to 9)\n");
    do{
        priority=getchar();
        while((c=getchar())!='\n'){}
    }while(priority<49 || priority>57);
    if(client->displayMessages==2)receiveNewMessages(client);
    sendMessage.priority=priority-48;
    printf("Your priority:%d\n",sendMessage.priority);
    int length = (int) strlen(sendMessage.text);
    strcat(sendMessage.text,client->name);
    length+=strlen(client->name);
    sendMessage.text[length++]=';';
    printf("Enter message:\n");
    while ((c = getchar()) != '\n') {
        sendMessage.text[length++] = (char) c;
        if (length == messageSize-namesize-typeSize-1) break;
    }
    if(client->displayMessages==2)receiveNewMessages(client);
    if (length < messageSize) sendMessage.text[length] = '\0';
    struct message instructionMessage;
    instructionMessage.type=1;
    memset(instructionMessage.text,0,strlen(instructionMessage.text));
    strcat(instructionMessage.text,"s\0");
    msgsnd(instructionID,&instructionMessage,sizeof(instructionMessage)-sizeof(long),0);
    msgsnd(sendMessageID, &sendMessage, sizeof(sendMessage) - sizeof(long), 0);
    printf("\nmessage:%s\n", sendMessage.text);
}

void createANewType(struct login *client){
    getAvailableTypes(client);
    struct availableType *availableTypes = client->availableTypes;
    int howManyTypes=0;
    while(availableTypes){
        howManyTypes++;
        availableTypes=availableTypes->next;
    }
    if(howManyTypes==maxAmountOfTypesAvailableOnServer){
        printf("Can't create more types\n");
        return;
    }
    int c,duplicate = 0;
    long chosenType;
    char chosenTypeString[8];
    int lessThan20;
    do{
        showAvailableTypes(client);
        memset(chosenTypeString,0,strlen(chosenTypeString));
        int iterator = 0;
        lessThan20=0;
        printf("Write type you wish to create(must be greater than or equal to 20):\n");
        while ((c = getchar()) != '\n') {
            chosenTypeString[iterator++] = (char) c;
            if (iterator == (8)) break;
        }
        if(client->displayMessages==2)receiveNewMessages(client);
        if(iterator<8)chosenTypeString[iterator]='\0';
        chosenType = strtol(chosenTypeString, NULL, 10);
        if(chosenType<20){
            printf("Type must be >= 20\n");
            lessThan20=1;
        }
        availableTypes=client->availableTypes;
        duplicate=0;
        while(availableTypes){
            if(availableTypes->type==chosenType){
                printf("This type is already available on the server\n");
                duplicate=1;
                break;
            }
            availableTypes=availableTypes->next;
        }
    }while(duplicate || lessThan20);
    struct message instructionMessage;
    instructionMessage.type=1;
    int instructionID = msgget(10001, 0664 | IPC_CREAT);
    memset(instructionMessage.text,0,strlen(instructionMessage.text));
    strcat(instructionMessage.text,"t\0");
    msgsnd(instructionID,&instructionMessage,sizeof(instructionMessage)-sizeof(long),0);
    struct message newTypeMessage;
    newTypeMessage.type=13;
    int newTypeMessageID = msgget(10013, 0664 | IPC_CREAT);
    strcpy(newTypeMessage.text,chosenTypeString);
    msgsnd(newTypeMessageID,&newTypeMessage,sizeof(newTypeMessage)-sizeof(long),0);
}

void chooseMenuOption(int option,struct login *client){
    if(option=='1'){
        showWholeConversation(client);
    }else if(option=='2'){ ;
        ManageYourSubscriptions(client);
    }else if(option=='3'){
        changeNotification(client);
    }else if(option=='4'){
        changeDisplayMode(client);
    }else if(option=='5'){
        sendMessage(client);
    }else if(option=='6'){
        createANewType(client);
    } else if(option=='7'){
        if(client->displayMessages==2){
            logOut(client);
            exit(0);
        }else{
            receiveNewMessages(client);
        }
    }else if(option=='8'){
        if(client->displayMessages==1){
            logOut(client);
            exit(0);
        }else{
            printf("You have chosen a wrong option\n");
        }
    }else{
        printf("You have chosen a wrong option\n");
    }
}

void showMenu(struct login *client){
    printf("(1) Show whole conversation of a certain type that you subscribe\n");
    printf("(2) Manage your subscriptions\n");
    printf("(3) Change the decision whether you wish to notify server when you receive a message or not\n");
    printf("(4) Change the display of messages mode\n");
    printf("(5) Send a new message\n");
    printf("(6) Create a new message type\n");
    if(client->displayMessages==2){ //asynchronically
        printf("(7) Exit the program\n");
    }else{
        printf("(7) Receive new messages\n");
        printf("(8) Exit the program\n");
    }
}

void menu(struct login *client){
    while(1){
        showMenu(client);
        printf("Choose one of the following options:\n");
        int option,c;
        option = getchar();
        while((c = getchar())!='\n'){} //clearing buffer
        if(client->displayMessages==2)receiveNewMessages(client);
        chooseMenuOption(option,client);
        printf("\n\n");
    }
}

int main(int argc, char *argv[]) {
    int c;
    char startingOption;
    struct login client;
    printf("Choose one of the following options\n");
    printf("(1)Register\n");
    printf("(2)Login\n");
    do {
        c = getchar();
        startingOption = (char) c;
    } while (startingOption != '1' && startingOption != '2');
    getchar(); //clearing bufor;
    if (startingOption == '1')registration(&client);
    if (startingOption == '2')login(&client);
    if(argc==2){
        if(strcmp(argv[1],"synchronically")==0)client.displayMessages=1;
        else if(strcmp(argv[1],"asynchronically")==0)client.displayMessages=2;
    }
    menu(&client);
    return 0;
}
