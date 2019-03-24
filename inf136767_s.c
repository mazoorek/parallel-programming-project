#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

struct message {
    long type;
    char text[1024];
    int priority;
};
struct login {
    long PID;
    char name[64];
    char password[64];
    struct subscription *subscriptions;
    int serverNotification; //send or do not send the notification to a server when you receive the message
    int displayMessages; //synchronically or asynchronically
    struct login *next;
};
struct clients {
    struct login clientData;
    struct clients *next;
};
struct subscription {
    long type;
    int wayOfSubscribing;
    long timeOfSubscription;
    long lengthOfSubscription;
    struct subscription *next;
};
struct registered {
    char name[64];
    char password[64];
    struct subscription *subscriptions;
    int serverNotification; //send or do not send the notification to a server when you receive the message
    int displayMessages; //synchronically or asynchronically
    struct registered *next;
};
struct availableType {
    long type;
    struct availableType *next;
};
void saveRegisteredDatabase(struct registered *registeredUsers) {
    printf("Updating database\n");
    FILE *registeredDatabase;
    registeredDatabase = fopen("inf136767_registeredUsersDatabase.txt", "w");
    struct registered *registeredIterator = registeredUsers;
    struct subscription *subscriptionIterator;
    while (registeredIterator) {
        fprintf(registeredDatabase, "%s;", registeredIterator->name);
        fprintf(registeredDatabase, "%s;", registeredIterator->password);
        fprintf(registeredDatabase, "%d;", registeredIterator->serverNotification);
        fprintf(registeredDatabase, "%d;", registeredIterator->displayMessages);
        subscriptionIterator = registeredIterator->subscriptions;
        if (subscriptionIterator == NULL) {
            fprintf(registeredDatabase, "\n");
        } else {
            while (subscriptionIterator->next) {
                fprintf(registeredDatabase, "%ld-", subscriptionIterator->type);
                fprintf(registeredDatabase, "%d-", subscriptionIterator->wayOfSubscribing);
                fprintf(registeredDatabase, "%ld-", subscriptionIterator->lengthOfSubscription);
                fprintf(registeredDatabase, "%ld,", subscriptionIterator->timeOfSubscription);
                subscriptionIterator = subscriptionIterator->next;
            }
            fprintf(registeredDatabase, "%ld-", subscriptionIterator->type);
            fprintf(registeredDatabase, "%d-", subscriptionIterator->wayOfSubscribing);
            fprintf(registeredDatabase, "%ld-", subscriptionIterator->lengthOfSubscription);
            fprintf(registeredDatabase, "%ld;\n", subscriptionIterator->timeOfSubscription);
        }
        registeredIterator = registeredIterator->next;
    }
    fclose(registeredDatabase);
    printf("Database updated\n");
}

void deleteSubscription(struct subscription **subscriptions, long type) {
    printf("Deleting subscription\n");
    struct subscription *subscriptionIterator = *subscriptions;
    struct subscription *previous;
    printf("My type:%ld\n",type);
    printf("First type:%ld\n",subscriptionIterator->type);
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

void checkWhetherSomeSubscriptionsHadExpired(struct registered **registeredUsers, struct login **loggedUsers) {
    printf("Checking whether some subscriptions have expired\n");
    struct registered *registeredIterator = *registeredUsers;
    struct login *loginIterator = *loggedUsers;
    struct subscription *rSubscriptionIterator;
    long timeNow;
    int someExpired = 0;
    while (registeredIterator) {
        rSubscriptionIterator = registeredIterator->subscriptions;
        while (rSubscriptionIterator) {
            if (rSubscriptionIterator->wayOfSubscribing == 2) {
                timeNow = time(NULL);
                if (rSubscriptionIterator->timeOfSubscription + rSubscriptionIterator->lengthOfSubscription < timeNow) {
                    someExpired = 1;
                    deleteSubscription(&registeredIterator->subscriptions, rSubscriptionIterator->type);
                    while (loginIterator) {
                        if (strcmp(loginIterator->name, registeredIterator->name) == 0) {
                            deleteSubscription(&loginIterator->subscriptions, rSubscriptionIterator->type);
                            break;
                        }
                        loginIterator = loginIterator->next;
                    }
                }
            }
            rSubscriptionIterator = rSubscriptionIterator->next;
        }
        registeredIterator = registeredIterator->next;
    }
    if (someExpired) {
        printf("Some subscriptions have expired\n");
        saveRegisteredDatabase(*registeredUsers);
    } else {
        printf("None of subscriptions have expired\n");
    }
}

void checkRegistrationNameValidation(struct registered **registeredUsers) {
    printf("Checking if registration name is already occupied\n");
    struct message nameVerificationMessage;
    nameVerificationMessage.type = 5;
    nameVerificationMessage.priority = 5;
    struct message nameVerificationMessageResponse;
    nameVerificationMessageResponse.type = 8;
    nameVerificationMessageResponse.priority = 8;
    int nameValidationID = msgget(10005, 0644 | IPC_CREAT);
    msgrcv(nameValidationID, &nameVerificationMessage, sizeof(nameVerificationMessage) - sizeof(long), 5, 0);
    char name[64];
    int i, j;
    for (j = 0; j < strlen(nameVerificationMessage.text); j++) {
        if (nameVerificationMessage.text[j] == ';')break;
    }
    strncpy(name, nameVerificationMessage.text, j);
    name[j] = '\0';
    for (i = j + 1; i < strlen(nameVerificationMessage.text); i++) {
        if (nameVerificationMessage.text[i] == ';')break;
    }
    char pidString[12];
    strncpy(pidString, nameVerificationMessage.text + j + 1, i - j);
    long pid = strtol(pidString, NULL, 10);
    int occupied = 0;
    struct registered *iterator = *registeredUsers;
    while (iterator) {
        if (strcmp(iterator->name, name) == 0) {
            occupied = 1;
            break;
        }
        iterator = iterator->next;
    }
    int nameValidationResponseID = msgget(pid, 0644 | IPC_CREAT);
    memset(nameVerificationMessageResponse.text, 0, strlen(nameVerificationMessageResponse.text));
    if (occupied == 0) {
        strcat(nameVerificationMessageResponse.text, "validated\0");
        printf("Registration name is free\n");
    } else {
        strcat(nameVerificationMessageResponse.text, "occupied\0");
        printf("Registration name is occupied\n");
    }
    msgsnd(nameValidationResponseID, &nameVerificationMessageResponse,
           sizeof(nameVerificationMessageResponse) - sizeof(long), 0);
}

void sendAvailableTypes() {
    printf("Sending available types\n");
    struct message availableTypesMessage;
    int id = msgget(10004, 0644 | IPC_CREAT);
    FILE *registeredTypesDatabase;
    registeredTypesDatabase = fopen("inf136767_registeredTypesDatabase.txt", "r");
    fgets(availableTypesMessage.text, sizeof(availableTypesMessage.text), registeredTypesDatabase);
    fclose(registeredTypesDatabase);
    if (strlen(availableTypesMessage.text) == 0) {
        registeredTypesDatabase = fopen("inf136767_registeredTypesDatabase.txt", "a+");
        strcat(availableTypesMessage.text, "20;\0");
        printf("%s\n",availableTypesMessage.text);
        //availableTypesMessage.text[3]='\0';
        fprintf(registeredTypesDatabase, "%s", availableTypesMessage.text);
        fclose(registeredTypesDatabase);
    }
    availableTypesMessage.type = 4;
    availableTypesMessage.priority = 4;
    msgsnd(id, &availableTypesMessage, sizeof(availableTypesMessage) - sizeof(long), 0);
}

void addAvailableType(){
    printf("Adding new type\n");
    int getNewTypeID = msgget(10013,0644|IPC_CREAT);
    struct message getNewTypeMessage;
    msgrcv(getNewTypeID,&getNewTypeMessage,sizeof(getNewTypeMessage)-sizeof(long),13,0);
    long newType=strtol(getNewTypeMessage.text,NULL,10);
    FILE *registeredTypesDatabase;
    char line[1024];
    registeredTypesDatabase = fopen("inf136767_registeredTypesDatabase.txt", "r");
    fgets(line, sizeof(line), registeredTypesDatabase);
    fclose(registeredTypesDatabase);
    int i, typeStartIndex = 0;
    char typeString[8];
    struct availableType *availableTypes = NULL;
    struct availableType *tmp, *type;
    for (i = 0; i < strlen(line); i++) {
        if (line[i] == ';') {
            strncpy(typeString, line + typeStartIndex, i - typeStartIndex);
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
    int alreadyInTypesList=0;
    tmp=availableTypes;
    while(tmp){
        if(tmp->type==newType){
            alreadyInTypesList=1;
            printf("Type was already in registered types database\n");
            break;
        }
        tmp=tmp->next;
    }
    if(!alreadyInTypesList){
        FILE *registeredDatabase;
        registeredDatabase = fopen("inf136767_registeredTypesDatabase.txt", "a+");
        fprintf(registeredDatabase, "%s;", getNewTypeMessage.text);
        fclose(registeredDatabase);
        printf("Type added\n");
    }
}

void showLoginDatabase(struct login *loggedUsers) {
    struct login *iterator = loggedUsers;
    struct subscription *subscriptionIterator;
    printf("\nAll logged users:\n\n");
    if (loggedUsers == NULL) {
        printf("There are no logged users\n\n");
    }
    while (iterator) {
        printf("User:\n");
        printf("Name:%s\n", iterator->name);
        printf("Password:%s\n", iterator->password);
        printf("PID:%ld\n", iterator->PID);
        printf("With or without notifications to server(1/2):%d\n", iterator->serverNotification);
        printf("Synchronically or Asynchronically(1/2):%d\n", iterator->displayMessages);
        subscriptionIterator = iterator->subscriptions;
        printf("User's subscriptions:\n");
        if (subscriptionIterator == NULL) {
            printf("User don't subscribe any type\n");
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
                    printf("Length of subscription:%ld\n", subscriptionIterator->lengthOfSubscription);
                    printf("Seconds left: %ld\n", timeLeft);
                }
                struct tm * timeinfo=localtime (&subscriptionIterator->timeOfSubscription);
                printf ("The time the user has subscribed the type: %s", asctime(timeinfo));
                subscriptionIterator = subscriptionIterator->next;
            }
        }
        iterator = iterator->next;
        printf("\n\n");
    }
}

void logOut(struct login **loggedUsers, long PID){
    printf("Logging out user\n");
    struct login *loginIterator = *loggedUsers;
    struct login *previous;
    if (loginIterator->PID == PID) {
        *loggedUsers = loginIterator->next;
        free(loginIterator);
    } else {
        while (loginIterator && loginIterator->PID != PID) {
            previous = loginIterator;
            loginIterator = loginIterator->next;
        }
        previous->next = loginIterator->next;
        free(loginIterator);
    }
    printf("User logged out\n");
}

void logOutUser(struct login **loggedUsers){
    printf("Starting logging out procedure\n");
    int logOutID = msgget(10011, 0664 | IPC_CREAT);
    struct message logOutMessage;
    msgrcv(logOutID,&logOutMessage,sizeof(logOutMessage)-sizeof(long),11,0);
    long PID = strtol(logOutMessage.text,NULL,10);
    logOut(loggedUsers,PID);
    showLoginDatabase(*loggedUsers);
}

void loginUser(struct registered **registeredUsers, struct login **loggedUsers) {
    printf("login verification\n");
    struct message loginMessage;
    int loginID = msgget(10003, 0644 | IPC_CREAT);
    char name[64];
    char password[64];
    char pidString[12];
    msgrcv(loginID, &loginMessage, sizeof(loginMessage) - sizeof(long), 3, 0);
    int i, j;
    for (i = 0; i < strlen(loginMessage.text); i++) {
        if (loginMessage.text[i] == ';') {
            break;
        }
    }
    strncpy(name, loginMessage.text, i);
    strcat(name + i, "\0");
    for (j = i + 1; j < strlen(loginMessage.text); j++) {
        if (loginMessage.text[j] == ';') {
            break;
        }
    }
    strncpy(password, loginMessage.text + i + 1, j - i - 1);
    password[j - i - 1] = '\0';
    for (i = j + 1; i < strlen(loginMessage.text); i++) {
        if (loginMessage.text[i] == ';') {
            break;
        }
    }
    strncpy(pidString, loginMessage.text + j + 1, i - j - 1);
    pidString[i - j - 1] = '\0';
    struct registered *registeredIterator = *registeredUsers;
    int validated = 0;
    while (registeredIterator) {
        if (strcmp(name, registeredIterator->name) == 0) {
            if (strcmp(password, registeredIterator->password) == 0) {
                validated = 1;
                break;
            }
        }
        registeredIterator = registeredIterator->next;
    }
    long pid = strtol(pidString, NULL, 10);
    int alreadyLogged=0;
    if(validated==1){
        struct login *loginIterator = *loggedUsers;
        while(loginIterator){
            if(strcmp(name,loginIterator->name)==0){
                if(getpgid((__pid_t) loginIterator->PID) >= 0){
                    alreadyLogged=1;
                    break;
                }else{
                    logOut(loggedUsers,loginIterator->PID);
                    break;
                }
            }
            loginIterator=loginIterator->next;
        }
    }
    struct message validationMessage;
    validationMessage.type = 6;
    validationMessage.priority = 6;
    int validationID = msgget(pid, 0644 | IPC_CREAT);
    memset(validationMessage.text, 0, strlen(validationMessage.text));
    if (validated == 0) {
        printf("wrong login\n");
        strcat(validationMessage.text, "wrong login\0");
        if (loginMessage.text[strlen(loginMessage.text)] != 'r') {
            msgsnd(validationID, &validationMessage, sizeof(validationMessage) - sizeof(long), 0);
        }
    } else if(alreadyLogged==1){
        printf("User already logged in\n");
        strcat(validationMessage.text, "User already logged in\0");
        if (loginMessage.text[strlen(loginMessage.text)] != 'r') {
            msgsnd(validationID, &validationMessage, sizeof(validationMessage) - sizeof(long), 0);
        }
    } else {
        strcat(validationMessage.text, "validated\0");
        printf("login validated\n");
        checkWhetherSomeSubscriptionsHadExpired(registeredUsers,loggedUsers);
        if (loginMessage.text[strlen(loginMessage.text)] != 'r') { //r on the end of message when registering
            msgsnd(validationID, &validationMessage, sizeof(validationMessage) - sizeof(long), 0);
        }
        struct login *newLoggedUser = malloc(sizeof(struct login));
        newLoggedUser->PID = pid;
        strcpy(newLoggedUser->name, registeredIterator->name);
        strcpy(newLoggedUser->password, registeredIterator->password);
        newLoggedUser->displayMessages = registeredIterator->displayMessages;
        newLoggedUser->serverNotification = registeredIterator->serverNotification;
        newLoggedUser->subscriptions = registeredIterator->subscriptions;
        newLoggedUser->next = NULL;
        if (*loggedUsers == NULL) {
            *loggedUsers = newLoggedUser;
        } else {
            struct login *loginIterator = *loggedUsers;
            while (loginIterator->next) {
                loginIterator = loginIterator->next;
            }
            loginIterator->next = newLoggedUser;
        }
        showLoginDatabase(*loggedUsers);
    }
}

void addRegisteredUserToStruct(char line[], struct registered **r) {
    (*r)->subscriptions = NULL;
    int j, passwordBeginIndex;
    for (j = 0; j < strlen(line); j++) {
        if (line[j] == ';') break;
    }
    strncpy((*r)->name, line, j);
    (*r)->name[j] = '\0';
    j += 1;
    passwordBeginIndex = j;
    for (j = passwordBeginIndex; j < strlen(line); j++) {
        if (line[j] == ';') break;
    }
    strncpy((*r)->password, line + passwordBeginIndex, j - passwordBeginIndex);
    (*r)->password[j - passwordBeginIndex] = '\0';
    j += 1;
    (*r)->serverNotification = line[j] - 48;
    j += 2;
    (*r)->displayMessages = line[j] - 48;
    j += 2;
    int typeStartIndex = j, subLengthStartIndex, subTimeStartIndex, numberOfDashes = 0;
    struct subscription *s = NULL, *iterator;
    for (; j < strlen(line); j++) {
        if (line[j] == '-') {
            if (numberOfDashes == 0) {
                s = malloc(sizeof(struct subscription));
                s->next = NULL;
                char *str = malloc(20 * sizeof(char));
                strncpy(str, line + typeStartIndex, j - typeStartIndex);
                s->type = strtol(str, NULL, 10);
            } else if (numberOfDashes == 1) {
                s->wayOfSubscribing = line[j - 1] - 48;;
                subLengthStartIndex = j + 1;
            } else if (numberOfDashes == 2) {
                char *str = malloc(20 * sizeof(char));
                strncpy(str, line + subLengthStartIndex, j - subLengthStartIndex);
                s->lengthOfSubscription = strtol(str, NULL, 10);
                subTimeStartIndex = j + 1;
            }
            numberOfDashes++;
        }
        if (line[j] == ',' || line[j] == ';') {
            numberOfDashes = 0;
            typeStartIndex = j + 1;
            char *str = malloc(20 * sizeof(char));
            strncpy(str, line + subTimeStartIndex, j - subTimeStartIndex);
            s->timeOfSubscription = strtol(str, NULL, 10);
            if ((*r)->subscriptions == NULL) {
                (*r)->subscriptions = s;
            } else {
                iterator = (*r)->subscriptions;
                while (iterator->next) {
                    iterator = iterator->next;
                }
                iterator->next = s;
            }
        }
        if (line[j] == ';')break;
    }
}

void showRegisteredDatabase(struct registered *registeredUsers) {
    struct registered *iterator = registeredUsers;
    struct subscription *subscriptionIterator;
    printf("\nAll registered users:\n\n");
    if (registeredUsers == NULL) {
        printf("There are no registered users\n\n");
    }
    while (iterator) {
        printf("User:\n");
        printf("Name:%s\n", iterator->name);
        printf("Password:%s\n", iterator->password);
        printf("With or without notifications to server(1/2):%d\n", iterator->serverNotification);
        printf("Synchronically or Asynchronically(1/2):%d\n", iterator->displayMessages);
        subscriptionIterator = iterator->subscriptions;
        printf("User's subscriptions:\n");
        if (subscriptionIterator == NULL) {
            printf("User don't subscribe any type\n");
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
                    printf("Length of subscription:%ld\n", subscriptionIterator->lengthOfSubscription);
                    printf("Seconds left: %ld\n", timeLeft);
                }
                struct tm * timeinfo=localtime (&subscriptionIterator->timeOfSubscription);
                printf ("The time the user has subscribed the type: %s", asctime(timeinfo));
                subscriptionIterator = subscriptionIterator->next;
            }
        }
        iterator = iterator->next;
        printf("\n\n");
    }
}

void getNewUser(struct registered **registeredUsers) {
    printf("saving to registered database file and to registered struct\n");
    struct message registrationMessage;
    int id = msgget(10002, 0644 | IPC_CREAT);
    msgrcv(id, &registrationMessage, sizeof(registrationMessage) - sizeof(long), 2, 0);
    printf("%s\n", registrationMessage.text);
    FILE *registeredDatabase;
    registeredDatabase = fopen("inf136767_registeredUsersDatabase.txt", "a+");
    fprintf(registeredDatabase, "%s\n", registrationMessage.text);
    fclose(registeredDatabase);
    struct registered *r = malloc(sizeof(struct registered));
    addRegisteredUserToStruct(registrationMessage.text, &r);
    if (*registeredUsers == NULL) {
        *registeredUsers = r;
    } else {
        struct registered *iterator = *registeredUsers;
        while (iterator->next) {
            iterator = iterator->next;
        }
        iterator->next = r;
    }
    showRegisteredDatabase(*registeredUsers);
}

void notificationFromUser(){
    struct message notificationMessage;
    notificationMessage.type = 16;
    int notificationID = msgget(10016, 0644 | IPC_CREAT);
    msgrcv(notificationID, &notificationMessage, sizeof(notificationMessage) - sizeof(long),16, 0);
    printf("User %s got the message\n",notificationMessage.text);
}

void sendWholeConversation(struct login *loggedUsers){
    printf("Sending conversation to user\n");
    FILE *WholeConversationsDatabase;
    WholeConversationsDatabase = fopen("inf136767_conversationDatabase.txt", "r");
    int getConversationID=msgget(10014,0644|IPC_CREAT);
    struct message getConversationMessage;
    msgrcv(getConversationID,&getConversationMessage,sizeof(getConversationMessage)-sizeof(long),14,0);
    struct login *loginIterator = loggedUsers;
    char name[64];
    char typeString[8];
    int i,j;
    for(i=0;i<strlen(getConversationMessage.text);i++){
        if(getConversationMessage.text[i]==';')break;
    }
    strncpy(name,getConversationMessage.text,i);
    name[i]='\0';
    printf("name:%s\n",name);
    for(j=i+1;j<strlen(getConversationMessage.text);j++){
        if(getConversationMessage.text[j]==';')break;
    }
    strncpy(typeString,getConversationMessage.text+i+1,j-i-1);
    if(j-i-1<8)typeString[j-i-1]='\0';
    while(loginIterator){
        if(strcmp(loginIterator->name,name)==0)break;
        loginIterator=loginIterator->next;
    }
    printf("type from user:%s",typeString);
    char line[1024];
    int sendConversationID = msgget((key_t) loginIterator->PID, 0644 | IPC_CREAT);
    struct message nextConversationMessage;
    nextConversationMessage.type=15;
    char typeInFile[8];
    while (fgets(line, sizeof(line), WholeConversationsDatabase)) {
        for(i=0;i<strlen(line);i++){
            if(line[i]==';')break;
        }
        strncpy(typeInFile,line,i);
        if(i<8)typeInFile[i]='\0';
        if(strcmp(typeInFile,typeString)==0){
            memset(nextConversationMessage.text,0,strlen(nextConversationMessage.text));
            strcat(nextConversationMessage.text,line);
            printf("message:%s\n",nextConversationMessage.text);
            msgsnd(sendConversationID,&nextConversationMessage,sizeof(nextConversationMessage)-sizeof(long),0);
        }
    }
    memset(nextConversationMessage.text,0,strlen(nextConversationMessage.text));
    strcat(nextConversationMessage.text,"end\0");
    msgsnd(sendConversationID,&nextConversationMessage,sizeof(nextConversationMessage)-sizeof(long),0);
    fclose(WholeConversationsDatabase);
}

void sendClientData(struct login *loggedUsers) {
    printf("Sending the data to the client\n");
    struct message getDataMessage;
    int getDataID = msgget(10009, 0664 | IPC_CREAT);
    msgrcv(getDataID, &getDataMessage, sizeof(getDataMessage) - sizeof(long), 9, 0);
    char name[64];
    strcpy(name, getDataMessage.text);
    struct message sendDataMessage;
    struct login *client = loggedUsers;
    while (client && strcmp(client->name, name) != 0) {
        client = client->next;
    }
    sendDataMessage.type = 10;
    sendDataMessage.priority = 10;
    int length = 0;
    sendDataMessage.text[length++] = (char) (client->serverNotification + 48);
    sendDataMessage.text[length++] = ';';
    sendDataMessage.text[length++] = (char) (client->displayMessages + 48);
    sendDataMessage.text[length++] = ';';
    sendDataMessage.text[length] = '\0';
    struct subscription *subscriptionIterator = client->subscriptions;
    while (subscriptionIterator) {
        char *subTypeString, *subLengthString, *subTimeString;
        int subTypeSize = snprintf(NULL, 0, "%ld", subscriptionIterator->type);
        subTypeString = malloc(subTypeSize + 1);
        snprintf(subTypeString, subTypeSize + 1, "%ld", subscriptionIterator->type);
        strcat(sendDataMessage.text, subTypeString);
        strcat(sendDataMessage.text, "-\0");
        if (subscriptionIterator->wayOfSubscribing == 1) strcat(sendDataMessage.text, "1-\0");
        else strcat(sendDataMessage.text, "2-\0");
        int subLengthSize = snprintf(NULL, 0, "%ld", subscriptionIterator->lengthOfSubscription);
        subLengthString = malloc(subLengthSize + 1);
        snprintf(subLengthString, subLengthSize + 1, "%ld", subscriptionIterator->lengthOfSubscription);
        strcat(sendDataMessage.text, subLengthString);
        strcat(sendDataMessage.text, "-\0");
        int subTimeSize = snprintf(NULL, 0, "%ld", subscriptionIterator->timeOfSubscription);
        subTimeString = malloc(subTimeSize + 1);
        snprintf(subTimeString, subTimeSize + 1, "%ld", subscriptionIterator->timeOfSubscription);
        strcat(sendDataMessage.text, subTimeString);
        if (subscriptionIterator->next) {
            strcat(sendDataMessage.text, ",\0");
        } else {
            strcat(sendDataMessage.text, ";\0");
        }
        subscriptionIterator = subscriptionIterator->next;
    }
    int sendDataID = msgget(client->PID, 0644 | IPC_CREAT);
    msgsnd(sendDataID, &sendDataMessage, sizeof(sendDataMessage) - sizeof(long), 0);
    printf("Data has been sent\n");
}

void changeClientData(struct registered **registeredUsers, struct login **loggedUsers){
    printf("Changing client's data\n");
    struct message changeClientDataMessage;
    int changeClientDataMessageID = msgget(10012, 0664 | IPC_CREAT);
    msgrcv(changeClientDataMessageID,&changeClientDataMessage, sizeof(changeClientDataMessage)-sizeof(long),12,0);
    printf("%s\n",changeClientDataMessage.text);
    int i;
    for(i=0;i<strlen(changeClientDataMessage.text);i++){
        if(changeClientDataMessage.text[i]==';')break;
    }
    char name[64];
    strncpy(name,changeClientDataMessage.text,i);
    name[i]='\0';
    struct login *loggedUser=*loggedUsers;
    struct registered *registeredUser=*registeredUsers;
    while(registeredUser){
        if(strcmp(registeredUser->name,name)==0){
            while(loggedUser){
                if(strcmp(loggedUser->name,name)==0)break;
                loggedUser=loggedUser->next;
            }
            break;
        }
        registeredUser=registeredUser->next;
    }
    i+=1;
    if(changeClientDataMessage.text[i]=='n'){
        i++;
        registeredUser->serverNotification=changeClientDataMessage.text[i]-48;
        loggedUser->serverNotification=changeClientDataMessage.text[i]-48;
    }else if(changeClientDataMessage.text[i]=='d'){
        i++;
        registeredUser->displayMessages=changeClientDataMessage.text[i]-48;
        loggedUser->displayMessages=changeClientDataMessage.text[i]-48;
    }else if(changeClientDataMessage.text[i]=='s'){
        i++;
        if(changeClientDataMessage.text[i]=='1'){
            struct subscription **headSubscription=&registeredUser->subscriptions;
            struct subscription *regIterator = registeredUser->subscriptions;
            struct subscription *s=malloc(sizeof(struct subscription));
            i+=2;
            int j;
            s->next=NULL;
            for(j=i;j<strlen(changeClientDataMessage.text);j++){
                if(changeClientDataMessage.text[j]=='-')break;
            }
            char typeString[8];
            strncpy(typeString,changeClientDataMessage.text+i,j-i);
            typeString[j-i]='\0';
            s->type=strtol(typeString,NULL,10);
            s->wayOfSubscribing=changeClientDataMessage.text[++j]-48;
            i=j+2;
            for(j=i;j<strlen(changeClientDataMessage.text);j++){
                if(changeClientDataMessage.text[j]=='-')break;
            }
            char subLengthString[20];
            strncpy(subLengthString,changeClientDataMessage.text+i,j-i);
            s->lengthOfSubscription=strtol(subLengthString,NULL,10);
            printf("%ld\n",s->lengthOfSubscription);
            i=j+1;
            for(j=i;j<strlen(changeClientDataMessage.text);j++){
                if(changeClientDataMessage.text[j]==';')break;
            }
            char timeString[20];
            strncpy(timeString,changeClientDataMessage.text+i,j-i);
            s->timeOfSubscription=strtol(timeString,NULL,10);
            showLoginDatabase(*loggedUsers);
            if(*headSubscription==NULL){
                *headSubscription=s;
            }else{
                while(regIterator->next){
                    regIterator=regIterator->next;
                }
                regIterator->next=s;
            }
            loggedUser->subscriptions=registeredUser->subscriptions;
        }else if(changeClientDataMessage.text[i]=='2'){
            i+=2;
            int j;
            for(j=i;j<strlen(changeClientDataMessage.text);j++){
                if(changeClientDataMessage.text[j]==';')break;
            }
            char typeStrong[12];
            strncpy(typeStrong,changeClientDataMessage.text+i,j-i);
            long type = strtol(typeStrong,NULL,10);
            deleteSubscription(&registeredUser->subscriptions,type);
            loggedUser->subscriptions=registeredUser->subscriptions;
        }
    }
    showLoginDatabase(*loggedUsers);
    saveRegisteredDatabase(*registeredUsers);
}

void getTheSentMessage(struct registered **registeredUsers, struct login **loggedUsers){
    printf("Sending message to logged users\n");
    int sentMessageID= msgget(10007, 0644 | IPC_CREAT);
    struct message sentMessage;
    msgrcv(sentMessageID,&sentMessage,sizeof(sentMessage)-sizeof(long),7,0);
    FILE *registeredDatabase;
    registeredDatabase = fopen("inf136767_conversationDatabase.txt", "a+");
    fprintf(registeredDatabase, "%s\n", sentMessage.text);
    fclose(registeredDatabase);
    struct message messageToBeSpread;
    messageToBeSpread.priority=sentMessage.priority;
    char typeString[8];
    int i;
    for(i=0;i<strlen(sentMessage.text);i++){
        if(sentMessage.text[i]==';')break;
        typeString[i]=sentMessage.text[i];
    }
    if(i<7)typeString[i]='\0';
    long type = strtol(typeString,NULL,10);
    messageToBeSpread.type=type;
    int j;
    char name[64];
    for(j=i+1;j<strlen(sentMessage.text);j++){
        if(sentMessage.text[j]==';')break;
    }
    strncpy(name,sentMessage.text+i+1,j-i-1);
    name[j-i-1]='\0';
    checkWhetherSomeSubscriptionsHadExpired(registeredUsers,loggedUsers);
    struct login *loginIterator = *loggedUsers;
    struct subscription *subscriptionIterator;
    while(loginIterator){
        if(strcmp(loginIterator->name,name)!=0){
            memset(messageToBeSpread.text,0,strlen(messageToBeSpread.text));
            subscriptionIterator=loginIterator->subscriptions;
            while(subscriptionIterator){
                if(subscriptionIterator->type==type){
                    int spreadMessageID = msgget((key_t) loginIterator->PID, 0644 | IPC_CREAT);
                    strcat(messageToBeSpread.text,sentMessage.text);
                    msgsnd(spreadMessageID,&messageToBeSpread,sizeof(messageToBeSpread)-sizeof(long),0);
                    printf("message sent\n");
                    break;
                }
                subscriptionIterator=subscriptionIterator->next;
            }
        }
        loginIterator=loginIterator->next;
    }
}

void getInstruction(struct registered **registeredUsers, struct login **loggedUsers) {
    struct message instructionMessage;
    int instructionID = msgget(10001, 0644 | IPC_CREAT);
    msgrcv(instructionID, &instructionMessage, sizeof(instructionMessage) - sizeof(long), 1, 0);
    printf("receiving instruction\n");
    if (instructionMessage.text[0] == 'g') {
        getNewUser(registeredUsers);
    } else if (instructionMessage.text[0] == 'a') {
        sendAvailableTypes();
    } else if (instructionMessage.text[0] == 'l') {
        loginUser(registeredUsers, loggedUsers);
    } else if (instructionMessage.text[0] == 'v') {
        checkRegistrationNameValidation(registeredUsers);
    } else if (instructionMessage.text[0] == 'd') {
        sendClientData(*loggedUsers);
    } else if (instructionMessage.text[0] == 'o') {
        logOutUser(loggedUsers);
    } else if (instructionMessage.text[0] == 'c') {
       changeClientData(registeredUsers,loggedUsers);
    } else if (instructionMessage.text[0] == 't') {
        addAvailableType();
    } else if (instructionMessage.text[0] == 's') {
    getTheSentMessage(registeredUsers,loggedUsers);
    } else if (instructionMessage.text[0] == 'w') {
        sendWholeConversation(*loggedUsers);
    } else if (instructionMessage.text[0] == 'n') {
        notificationFromUser();
    }
}

void getRegisteredUsersData(struct registered **registeredUsers) {
    FILE *registeredDatabase;
    registeredDatabase = fopen("inf136767_registeredUsersDatabase.txt", "r");
    char line[1024];
    struct registered *iterator;
    while (fgets(line, sizeof(line), registeredDatabase)) {
        struct registered *r = malloc(sizeof(struct registered));
        addRegisteredUserToStruct(line, &r);
        if (*registeredUsers == NULL) {
            *registeredUsers = r;
        } else {
            iterator = *registeredUsers;
            while (iterator->next) {
                iterator = iterator->next;
            }
            iterator->next = r;
        }
    }
    fclose(registeredDatabase);
}

int main(int argc, char *argv[]) {
    //making sure that the proces will not have one of these pids
    while (getpid() >= 10001 && getpid() <= 10019) {
        if (fork() != 0) exit(0);
    }
    struct registered *registeredUsers = NULL;
    struct login *loggedUsers = NULL;
    getRegisteredUsersData(&registeredUsers);
    checkWhetherSomeSubscriptionsHadExpired(&registeredUsers, &loggedUsers);
    showRegisteredDatabase(registeredUsers);
    while (1) {
        getInstruction(&registeredUsers, &loggedUsers);
    }
    return 0;
}

