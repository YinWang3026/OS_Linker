#include "stdio.h" //fopen, fclose, printf, getline
#include "string.h" //strlen, strtok
#include "stdlib.h"
#include "errno.h"
#include "ctype.h" //isdigit(), isalpha(), isalnum()

//Definitions
typedef struct{
     char* sym; //the symbol
     int absAddr; //absaddr = relative addr + module base addr
     int definedAlready; //If true, print error
}Symbol;

typedef struct{
     int baseAddr; //base addr (X+1) = baseAddr(X) + len(X)
     int length; // length = code count, module size = length - 1
     int id;
}Module;

typedef struct{
     char* symtable; //"Symbol Table"
     char* memmap; //"Memory Map"
     int cap; //symbolList capacity
     int size; //symbolList curr size
     Symbol** symbolList;
}SymbolTable;

//Global variables
FILE* fptr; //File pointer
SymbolTable* mySymTable;
int linenum; //Current line number
int lineoffset; //Current line offset
int totalInstr; //Total instr read so far, at most 512

//Functions
//Symbols
Symbol* createSymbol(char*,int);
void deallocSymbol(Symbol*);
void printSymbol(Symbol*);

//Modules
Module* createModule(int,int);
void deallocModule(Module*);

//SymbolTable
SymbolTable* createSymbolTable();
void deallocSymbolTable(SymbolTable*);
void printSymbolTable(SymbolTable*);
void addSymbolToTable(SymbolTable*, Symbol*);

//Initalization
void initGlobalVar(const char*); //Initalizes the global vars above
void cleanUp(); //Opposite of init global

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
void __warnings(int, Module*, Symbol*);



int main(int argc, char *argv[]) {

     tokenizer(argv[1]);
     printf("------\n");
     initGlobalVar(argv[1]);
     passOne();
     cleanUp();
     /*
     initGlobalVar(argv[1]);
     passTwo();
     fclose(fptr);*/
     return 0;
}

Module* createModule(int currentModule, int currentAddr){
     Module* mod = (Module*) malloc(sizeof(Module));
     if (mod == NULL){
          printf(stderr, "createModule: Failed to allocate memory");
          exit(-1);
     }
     mod->baseAddr = currentAddr;
     mod->id = currentModule;
     mod->length = 0;
     return mod;
}
void deallocModule(Module* mod){
     free(mod);
}

Symbol* createSymbol(char* s, int addr){
     Symbol* symb = (Symbol*)malloc(sizeof(Symbol));
     if (symb == NULL){
          printf(stderr, "createSymbol: Failed to allocate memory");
          exit(-1);
     }
     symb->sym = s;
     symb->definedAlready = 0;
     symb->absAddr = addr;
     return symb;
}

void deallocSymbol(Symbol* sym) {
     free(sym);
}

void printSymbol(Symbol* symb){
     printf("%s=%d",symb->sym, symb->absAddr);
}

SymbolTable* createSymbolTable(){
     SymbolTable* st = (SymbolTable*)malloc(sizeof(SymbolTable));
     st->symtable = (char*)malloc(14*sizeof(char));
     strcpy(st->symtable,"Symbol Table\n");
     st->memmap = (char*)malloc(12*sizeof(char));
     strcpy(st->symtable,"Memory Map\n");
     st->cap = 1;
     st->size = 0;
     st->symbolList = (Symbol**)malloc(st->cap*sizeof(Symbol*));
     return st;
}

void deallocSymbolTable(SymbolTable* st){
     free(st->symtable);
     free(st->memmap);
     int i;
     for (i = 0; i<st->size; i++){
          deallocSymbol(st->symbolList[i]);
     }
     free(st);
}

void printSymbolTable(SymbolTable* st){
     int i;
     for (i = 0; i<st->size; i++){
          printSymbol(st->symbolList[i]);
     }
}

void addSymbolToTable(SymbolTable* st, Symbol* s){
     st->symbolList[st->size] = s;
     st->size += 1;
     if (st->size == st->cap){
          st->cap *= 2;
          st->symbolList = realloc(st->symbolList,st->cap);
     }
}

void initGlobalVar(const char* filename) {
     fptr = fopen(filename,"r");
     if (fptr == NULL) {
          fprintf(stderr, "Cannot open file: %s.\n", filename);
          exit(-1);
     }
     mySymTable = createSymbolTable();
     linenum = 0;
     lineoffset = 0;
     totalInstr = 0;
}

void cleanUp(){
     close(fptr);
     deallocSymbolTable(mySymTable);
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
     int defCount, useCount, instCount, i, val, operand, currentBaseAddr, currentModule;
     char addressMode;
     char* symToken;
     Symbol* symbol;
     Module* mod;

     currentBaseAddr = 0; //Base addr starts from 0
     currentModule = 1; //Module number starts from 1
     while (1){
          mod = createModule(currentModule, currentBaseAddr);
       
          //Reading definition list
          defCount = readInt();
          if (defCount > 16){
               __parseerror(4);
          } else if(defCount == -1) {
               return; //EoF
          }
          for (i=0; i<defCount; i++) {
               symToken = readSym();
               symbol = createSymbol(symToken,0);
               if ((val = readInt()) == -1){
                    __parseerror(0);
               }
               symbol->absAddr = val+currentBaseAddr; //abs addr = module base + relative
               addSymbolToTable(mySymTable, symbol);
          }

          //Reading use list
          useCount = readInt();
          if (useCount > 16){
               __parseerror(5);
          } else if(useCount == -1) {
               __parseerror(0);
          }
          for (i=0; i<useCount; i++){
               symToken = readSym();
          }

          //Reading program text
          instCount = readInt(); //module length
          if (instCount > 512) {
               __parseerror(6);
          } else if (instCount == -1){
               __parseerror(0);
          }
          mod->length = instCount;
          currentBaseAddr += instCount; //Update base addr for next module
          totalInstr += instCount;
          if (totalInstr > 512) { //Too many instr
               __parseerror(6);
          }
          for (i=0; i<instCount; i++){
               addressMode = readIEAR();
               if ((operand = readInt()) == -1){
                    __parseerror(2);
               }

               // various checks
               //this would change in pass2
          }
          deallocModule(mod); //The module is removed
          currentModule += 1;
     } 
}


void passTwo() {
     int defCount, useCount, instCount, i, val, operand, currentBaseAddr, currentModule;
     char addressMode;
     char* symToken;
     Symbol* symbol;
     Module* mod;

     currentBaseAddr = 0; //Base addr starts from 0
     currentModule = 1; //Module number starts from 1
     while (1){
          mod = createModule(currentModule, currentBaseAddr);
       
          //Reading definition list
          defCount = readInt();
          if (defCount > 16){
               __parseerror(4);
          } else if(defCount == -1) {
               return; //EoF
          }
          for (i=0; i<defCount; i++) {
               symToken = readSym();
               symbol = createSymbol(symToken,0);
               if ((val = readInt()) == -1){
                    __parseerror(0);
               }
               symbol->absAddr = val+currentBaseAddr; //abs addr = module base + relative
               addSymbolToTable(mySymTable, symbol);
          }

          //Reading use list
          useCount = readInt();
          if (useCount > 16){
               __parseerror(5);
          } else if(useCount == -1) {
               __parseerror(0);
          }
          for (i=0; i<useCount; i++){
               symToken = readSym();
          }

          //Reading program text
          instCount = readInt(); //module length
          if (instCount > 512) {
               __parseerror(6);
          } else if (instCount == -1){
               __parseerror(0);
          }
          mod->length = instCount;
          currentBaseAddr += instCount; //Update base addr for next module
          totalInstr += instCount;
          if (totalInstr > 512) { //Too many instr
               __parseerror(6);
          }
          for (i=0; i<instCount; i++){
               addressMode = readIEAR();
               if ((operand = readInt()) == -1){
                    __parseerror(2);
               }

               // various checks
               //this would change in pass2
          }
          deallocModule(mod); //The module is removed
          currentModule += 1;
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

void __nonTerminatingError(int errcode, Symbol* symb) {
     switch(errcode) { //Code is based on rule number
          case 8:
               printf("Error: Absolute address exceeds machine size; zero used");
               break;
          case 9:
               printf("Error: Relative address exceeds module size; zero used");
               break;
          case 6:
               printf("Error: External address exceeds length of uselist; treated as immediate");
               break;
          case 3:
               printf("Error: %s is not defined; zero used", symb->sym);
               break;
          case 2:
               printf("Error: This variable is multiple times defined; first value used");
               break;
          case 10:
               printf("Error: Illegal immediate value; treated as 9999");
               break;
          case 11:
               printf("Error: Illegal opcode; treated as 999");
               break;
          default: //Incase I type in something wrong
               printf("Invalid code.\n");
               exit(-1);
     }
}

void __warnings(int errcode, Module* mod, Symbol* symb){
     switch(errcode){ //Code based on rule number
          case 5:
               printf("Warning: Module %d: %s too big %d (max=%d) assume zero relative\n", mod->id, symb->sym, symb->absAddr, mod->length);
               break;
          case 7:
               printf("Warning: Module %d: %s appeared in the uselist but was not actually used\n", mod->id, symb->sym);
               break;
          case 4:
               printf("Warning: Module %d: %s was defined but never used\n", mod->id, symb->sym);
               break;
          default: //Incase I type in something wrong
               printf("Invalid code.\n");
               exit(-1);
     }
}