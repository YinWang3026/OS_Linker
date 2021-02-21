#include "stdio.h" //fopen, fclose, printf, getline
#include "string.h" //strlen, strtok
#include "stdlib.h"
#include "errno.h"
#include "ctype.h" //isdigit(), isalpha(), isalnum()

//Definitions
typedef struct{
     int baseAddr; //base addr (X+1) = baseAddr(X) + len(X)
     //int length; // length = code count, module size = length - 1
     int id;
}Module;

typedef struct{
     char* sym; //the symbol
     int absAddr; //absaddr = relative addr + module base addr
     int relAddr; //relative addr
     int definedAlready; //If true, print error
     int used; //Was this symbol used
     Module mod; //meta info for module this symboled defined in
}Symbol;

typedef struct{
     int cap; //symbolList capacity
     int size; //symbolList curr size
     Symbol** symbolList;
}SymbolTable;

typedef struct{
     char** tokens;
     int size;
     int cap;
}UseList;

//Global variables
FILE* fptr; //File pointer
SymbolTable* mySymTable;
int linenum; //Current line number
int lineoffset; //Current line offset
int totalInstr; //Total instr read so far, at most 512

//Functions
//Symbols
Symbol* createSymbol(char*,Module);
void deallocSymbol(Symbol*);
void printSymbol(Symbol*);

//SymbolTable
SymbolTable* createSymbolTable();
void deallocSymbolTable(SymbolTable*);
void printSymbolTable(SymbolTable*);
void addSymbolToTable(SymbolTable*, Symbol*);
Symbol* findSymbolInTable(SymbolTable* st, char* token);

//UseList
UseList* createUseList();
void deallocUseList(UseList*);
void addToUseList(UseList* u, char* token);

//Initalization
void initGlobalVar(const char*); //Initalizes the global vars above

//Token related
char* getToken(); //Gets tokens from input file
void tokenizer(const char*); //Calls getToken() and prints line num and offset
int readInt(); //Calls gettoken, should return an int
Symbol readSymbol(); //Calls gettoken, should return a symbol
char readIEAR(); //Calls gettoken, should return "I,A,E,R" char

//Parser related
void passOne(); //First pass
void passTwo(); //Second pass
void __parseerror(int); //Err code, line number, offset
void __nonTerminatingError(int, Symbol*);
void __warnings(int, int, Symbol*);
void rule5Violation(int indexOffset, int length); //length = size of module
void rule4Violation();

int main(int argc, char *argv[]) {

     //tokenizer(argv[1]);
     //printf("------\n");
     mySymTable = createSymbolTable();
     initGlobalVar(argv[1]);
     passOne();
     fclose(fptr);

     initGlobalVar(argv[1]);
     passTwo();
     fclose(fptr);
     deallocSymbolTable(mySymTable);
     return 0;
}

void initGlobalVar(const char* filename) {
     fptr = fopen(filename,"r");
     if (fptr == NULL) {
          fprintf(stderr, "Cannot open file: %s.\n", filename);
          exit(-1);
     }
     linenum = 0;
     lineoffset = 0;
     totalInstr = 0;
}

Symbol* createSymbol(char* token, Module m){
     Symbol* s = (Symbol*)malloc(sizeof(Symbol));
     if (s == NULL){
          fprintf(stderr, "createSymbol:s Failed to allocate memory.\n");
          exit(-1);
     }
     s->sym = (char*)malloc(17*sizeof(char)); //Max 16 characters + 1 null char
     if (s == NULL){
          fprintf(stderr, "createSymbol:sym Failed to allocate memory.\n");
          exit(-1);
     }
     strcpy(s->sym,token);
     s->definedAlready = 0;
     s->absAddr = 0;
     s->relAddr = 0;
     s->used = 0;
     s->mod.baseAddr = m.baseAddr;
     s->mod.id = m.id;
     return s;
}

void deallocSymbol(Symbol* s) {
     free(s->sym);
     free(s);
}

void printSymbol(Symbol* s){
     printf("%s=%d ",s->sym, s->absAddr);
     if (s->definedAlready == 1){ //Rule 2 violation, already defined
          __nonTerminatingError(2,NULL);
     }
     printf("\n");
}

SymbolTable* createSymbolTable(){
     SymbolTable* st = (SymbolTable*)malloc(sizeof(SymbolTable));
     if (st == NULL){
          fprintf(stderr, "createSymbolTable:st Failed to allocate memory.\n");
          exit(-1);
     }
     st->cap = 2; //starting cap
     st->size = 0; //empty start
     st->symbolList = (Symbol**)malloc(st->cap*sizeof(Symbol*));
     if (st->symbolList == NULL){
          fprintf(stderr, "createSymbolTable:symbolList Failed to allocate memory.\n");
          exit(-1);
     }
     return st;
}

void deallocSymbolTable(SymbolTable* st){
     int i;
     for (i = 0; i<st->size; i++){
          deallocSymbol(st->symbolList[i]);
     }
     free(st);
}

void printSymbolTable(SymbolTable* st){
     int i;
     printf("Symbol Table\n");
     for (i = 0; i<st->size; i++){
          printSymbol(st->symbolList[i]);
     }
     printf("\nMemory Map\n");
}

Symbol* findSymbolInTable(SymbolTable* st, char* token){
     int i;
     Symbol* s;
     for (i = 0; i<st->size; i++){
          s = st->symbolList[i];
          if (strcmp(s->sym,token)==0){ //Found a match
               return s;
          }
     }
     return NULL; //Not Found
}

void addSymbolToTable(SymbolTable* st, Symbol* s){
     //Check if symbol already in table
     //If yes, mark defined and return
     Symbol* temp;
     if ((temp = findSymbolInTable(st,s->sym)) != NULL){
          temp->definedAlready = 1; //Set to true
          deallocSymbol(s); //Deleting s
          return;
     } 
     //Else insert it
     st->symbolList[st->size] = s;
     st->size += 1;
     if (st->size == st->cap){
          st->cap *= 2;
          st->symbolList = realloc(st->symbolList,st->cap*sizeof(Symbol*));
          if (st->symbolList == NULL){
               fprintf(stderr, "addSymbolToTable:symbolList Failed to allocate memory.\n");
               exit(-1);
          }
     }
}


UseList* createUseList();
void deallocUseList(UseList*);
void addToUseList(UseList* u, char* token){
     
}

void tokenizer(const char* filename) {
     char* temp;
     initGlobalVar(filename);
     while ((temp = getToken()) != NULL){
          printf("Token: %d:%d : %s\n", linenum, lineoffset, temp);
     }
     printf("Final Spot in File : line=%d offset=%d\n", linenum,lineoffset);
     fclose(fptr);
}

char* getToken(){
     static char* linePtr; //Need to remember the head ptr
     static int finalPosition; //Final position of the line
     static size_t n = 0; //For getline size
     char* token;
     int result;

     token = strtok(NULL," \t\n"); //Trying to get a token
     while (token == NULL) { //No token, then try to read a line, keep going until token is found
          free(linePtr); //Document said free linePtr when done
          linePtr = NULL;
          result = getline(&linePtr, &n, fptr);
          if (result < 1) { //EOF or Error in getline
               lineoffset = finalPosition; //Setting offset to final position
               return NULL;
          }
          linenum += 1; //Read a line, increment line number
          token = strtok(linePtr, " \t\n"); //Parse the line
     }

     lineoffset = token - linePtr + 1; //Current addr - head addr + 1 = current position
     finalPosition = lineoffset + strlen(token); //Current offset + len(token) = last position,eof
     return token;
}

int readInt(){
     char* token;
     char* temp;
     int result;

     token = getToken();
     if (token == NULL){ //Eof reached
          return -1;
     }
     result = strtol(token, &temp, 10);
     if (*temp != '\0'){ //Deference it
          __parseerror(0);
     }
     return result;
}

char* readSym(){
     char* sym;
     int i, len;
     
     sym = getToken();
     if (sym == NULL) { //EoF reached
          __parseerror(1);
     }
     //Check length
     len = strlen(sym);
     if (len > 16){
          __parseerror(3);
     }
     //Check [a-Z][a-Z0-9]*
     if (isalpha(sym[0]) == 0){
          __parseerror(1);
     }
     for (i=1; i<len; i++){
          if (isalnum(sym[i]) == 0){
               __parseerror(1);
          }
     }
     return sym;
}

char readIEAR(){
     char* token;

     token = getToken();
     if (token == NULL) { //EoF reached
          __parseerror(2);
     }
     if (strlen(token) != 1 || 
     !(strcmp(token,"I") == 0 || strcmp(token,"A") == 0 
     ||strcmp(token,"E") == 0 || strcmp(token,"R") == 0)) {
          __parseerror(2);
     }
     return *token;
}

void passOne() {
     int defCount, useCount, moduleSize, i, rel, operand, currentBaseAddr, currentModule;
     //char addressMode;
     char* symToken;
     Symbol* symbol;
     Module mod;

     currentBaseAddr = 0; //Base addr starts from 0
     currentModule = 1; //Module number starts from 1
     while (1){
          //Createmodule()
          mod.baseAddr = currentBaseAddr;
          mod.id = currentModule;

          //Reading defList
          defCount = readInt();
          if (defCount > 16){
               __parseerror(4);
          } else if(defCount == -1) {
               break; //EoF break loop
          }
          for (i=0; i<defCount; i++) {
               symToken = readSym();
               symbol = createSymbol(symToken,mod);
               if ((rel = readInt()) == -1){
                    __parseerror(0);
               }
               symbol->relAddr = rel; //Relative addr
               symbol->absAddr = rel + currentBaseAddr; //Abs addr = Rel + Base
               addSymbolToTable(mySymTable, symbol);
          }
          //Reading useList
          useCount = readInt();
          if (useCount > 16){
               __parseerror(5);
          } else if(useCount == -1) {
               __parseerror(0);
          }
          for (i=0; i<useCount; i++){
               symToken = readSym();
               //Do nothing else
          }

          //Reading program text
          moduleSize = readInt(); //module length
          if (moduleSize > 512) {
               __parseerror(6);
          } else if (moduleSize == -1){
               __parseerror(0);
          }
          totalInstr += moduleSize; //Counting total instr so far in the input
          if (totalInstr > 512) { //Too many instr
               __parseerror(6);
          }
          for (i=0; i<moduleSize; i++){
               readIEAR(); //Discarding return value
               if ((operand = readInt()) == -1){
                    __parseerror(2);
               }
               //Feels like nothing to do here for pass 1 also
          }
          rule5Violation(defCount,moduleSize);
          currentModule += 1; //Updating module number
          currentBaseAddr += moduleSize; //Update base addr for next module
     } 
}

void passTwo() {
     int defCount, useCount, moduleSize, i, currentBaseAddr, currentModule, instCount,op, opcode, operand;
     char addressMode;
     char* symToken;
     Module mod;

     instCount = 0;
     currentBaseAddr = 0; //Base addr starts from 0
     currentModule = 1; //Module number starts from 1
     
     printSymbolTable(mySymTable); //Starting the printing process
     while (1){
          //Createmodule()
          mod.baseAddr = currentBaseAddr;
          mod.id = currentModule;   

          //Reading defList
          defCount = readInt();
          if(defCount == -1) {
               break; //EoF
          }
          for (i=0; i<defCount; i++) {
               readSym();
               readInt();
               //Discarded return val
               //Feels like nothing to do here for pass 2
          }

          //Reading useList
          useCount = readInt();
          for (i=0; i<useCount; i++){
               Symbol* symbol;
               symToken = readSym();
               symbol = findSymbolInTable(mySymTable,symToken);
               if (symbol == NULL){
                    printf("sym: %s is not defined\n",symToken);
               } else{
                    symbol->used = 1; //Set used
               }
               //Need to maintain a use list
          }

          //Reading program text
          moduleSize = readInt(); //module length
          for (i=0; i<moduleSize; i++){

               addressMode = readIEAR();
               op = readInt();
               opcode = op/1000;
               operand = op%1000;
               switch (addressMode)
               {
               case 'I':
                    break;
               case 'E':
                    break;
               case 'A':
                    break;
               case 'R':
                    break;
               }
               op = opcode*1000 + operand;
               printf("%d: %d\n",instCount, op);
               instCount += 1;
          }
          currentModule += 1; //Updating module number
          currentBaseAddr += moduleSize; //Update base addr for next module
     }
     rule4Violation();
}

void rule5Violation(int indexOffset, int length){ 
     int i;
     Symbol* s;
     for (i = mySymTable->size-indexOffset; i<mySymTable->size; i++){
          //Checking if rel addr > mod length - 1
          s = mySymTable->symbolList[i];
          if (s->relAddr > length-1){
               __warnings(5,length,s);
               s->relAddr = 0;
               s->absAddr = s->mod.baseAddr;
          }
     }
}

void rule4Violation(){
     int i;
     Symbol* s;
     for (i = 0; i<mySymTable->size; i++){
          s = mySymTable->symbolList[i];
          if (s->used == 0){ //defined but not used
               __warnings(4,0,s);
          }
     }
}

//Parse error aborts execution
void __parseerror(int errcode){
     static char* errstr[] = {
        "NUM_EXPECTED",
        "SYM_EXPECTED",
        "ADDR_EXPECTED",
        "SYM_TOO_LONG",
        "TOO_MANY_DEF_IN_MODULE",
        "TOO_MANY_USE_IN_MODULE",
        "TOO_MANY_INSTR",
     };
     printf("Parse Error line %d offset %d: %s\n", linenum, lineoffset, errstr[errcode]);
     exit(-1);
}

void __nonTerminatingError(int errcode, Symbol* s) {
     switch(errcode) { //Code is based on rule number
          case 8:
               printf("Error: Absolute address exceeds machine size; zero used\n");
               break;
          case 9:
               printf("Error: Relative address exceeds module size; zero used\n");
               break;
          case 6:
               printf("Error: External address exceeds length of uselist; treated as immediate\n");
               break;
          case 3:
               printf("Error: %s is not defined; zero used\n", s->sym);
               break;
          case 2:
               printf("Error: This variable is multiple times defined; first value used\n");
               break;
          case 10:
               printf("Error: Illegal immediate value; treated as 9999\n");
               break;
          case 11:
               printf("Error: Illegal opcode; treated as 999\n");
               break;
          default: //Incase I type in something wrong and cant figure out why
               printf("\n");
     }
}

void __warnings(int errcode, int modLength, Symbol* s){
     switch(errcode){ //Code based on rule number
          case 5:
               printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", s->mod.id, s->sym, s->relAddr, modLength-1);
               break;
          case 7:
               printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", s->mod.id, s->sym);
               break;
          case 4:
               printf("Warning: Module %d: %s was defined but never used\n", s->mod.id, s->sym);
               break;
          default:
               printf("\n");
     }
}