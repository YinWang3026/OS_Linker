#include "stdio.h" //fopen, fclose, printf, getline
#include "string.h" //strlen, strtok
#include "stdlib.h"
#include "errno.h"
#include "ctype.h" //isdigit(), isalpha(), isalnum()

//Definitions
typedef struct{
     char* sym;
     int absAddr;
}Symbol;

typedef struct{
     //Instruction* memoryInst[512]; //Machine size
     int baseAddr; //base addr (X+1) = baseAddr(X) + len(X)
     int length; //module size
     int id; //module number

     int cap; //symbolList capacity
     int size; //symbolList curr size
     Symbol* symbolList; //At least 256 symbol=value pairs
}Module;

typedef struct{
     char* symtable; //"Symbol Table"
     char* memmap; //"Memory Map"
     int cap; //moduleList capacity
     int size; //moduleList curr size
     Module* moduleList;
}SymbolTable;

//Global variables
FILE* fptr; //File pointer
int linenum; //Current line number
int lineoffset; //Current line offset
int finalPosition; //Last character in the line
int instrCount; //Total instr read so far, at most 512

//Functions
//Symbols
Symbol* createSymbol(int, char*);
void deallocSymbol(Symbol*);

//Modules
Module* createModule();
void deallocModule(Module*);
void addSymbol(Symbol*);

//SymbolTable
SymbolTable* createSymbolTable();
void deallocSymbolTable(SymbolTable*);
void printSymbolTable(SymbolTable*);
void addModule(Module*);

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
void __parseerror(int,int,int); //Err code, line number, offset



int main(int argc, char *argv[]) {

     tokenizer(argv[1]);
     printf("------\n");
     initGlobalVar(argv[1]);
     passOne();
     fclose(fptr);
/*
     initGlobalVar(argv[1]);
     passTwo();
     fclose(fptr);*/
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
     instrCount = 0; 
}

void tokenizer(const char* filename) {
     char* temp;
     initGlobalVar(filename);
     while ((temp = getToken()) != NULL){
          printf("Token: %d:%d : %s\n", linenum, lineoffset, temp);
     }
     printf("Final Spot in File : line=%d offset=%d\n", linenum,finalPosition);
     fclose(fptr);
}

char* getToken(){
     static char* linePtr; //Need to remember the head ptr
     size_t n = 0; //For getline size
     char* token;
     int result;

     token = strtok(NULL," \t\n"); //Trying to get a token
     while (token == NULL) { //No token, then try to read a line, keep going until token is found
          free(linePtr); //Document said free linePtr when done
          linePtr = NULL;
          result = getline(&linePtr, &n, fptr);
          if (result < 1) { //EOF or Error in getline
               lineoffset = finalPosition;
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
          __parseerror(0, linenum, lineoffset);
     }
     return result;
}

char* readSym(){
     char* sym;
     int i, len;
     
     sym = getToken();
     if (sym == NULL) { //EoF reached
          __parseerror(1, linenum, finalPosition);
     }
     //Check length
     len = strlen(sym);
     if (len > 16){
          __parseerror(3, linenum, lineoffset);
     }
     //Check [a-Z][a-Z0-9]*
     if (isalpha(sym[0]) == 0){
          __parseerror(1, linenum, lineoffset);
     }
     for (i=1; i<len; i++){
          if (isalnum(sym[i]) == 0){
               __parseerror(1, linenum, lineoffset);
          }
     }
     return sym;
}

char readIEAR(){
     char* token;

     token = getToken();
     if (token == NULL) { //EoF reached
          __parseerror(2, linenum, finalPosition);
     }
     if (strlen(token) != 1 || 
     !(strcmp(token,"I") == 0 || strcmp(token,"A") == 0 
     ||strcmp(token,"E") == 0 || strcmp(token,"R") == 0)) {
          __parseerror(2, linenum, lineoffset);
     }
     return *token;
}

void passOne() {
     int defCount, useCount, instCount, i, val, operand;
     char addressMode;
     char* sym;

     while (1){
          //createModule();

          defCount = readInt();
          if (defCount > 16){
               __parseerror(4, linenum, lineoffset);
          } else if(defCount == -1) {
               return;
          }
          printf("defcount: %d\n",defCount);
          for (i=0; i<defCount; i++) {
               sym = readSym();
               if ((val = readInt()) == -1){
                    __parseerror(0, linenum, finalPosition);
               }
               printf("sym: %s, val: %d\n", sym, val);
               //createSymbol(sym,val); //this would change in pass2
          }

          useCount = readInt();
          if (useCount > 16){
               __parseerror(5, linenum, lineoffset);
          } else if(useCount == -1) {
               __parseerror(0, linenum, finalPosition);
          }
          printf("usecount: %d\n",useCount);
          for (i=0; i<useCount; i++){
               sym = readSym();
               printf("sym: %s\n", sym);
               //we don’t do anything here this would change in pass2 
          }
          instCount = readInt();
          if (instCount > 512) {
               __parseerror(6, linenum, lineoffset);
          } else if (instCount == -1){
               __parseerror(0, linenum, finalPosition);
          }
          printf("instCount: %d\n",instCount);
          for (i=0; i<instCount; i++){
               addressMode = readIEAR();
               if ((operand = readInt()) == -1){
                    __parseerror(2, linenum, finalPosition);
               }
               printf("mode: %c, operand: %d\n",addressMode, operand);

               // various checks
               //this would change in pass2
          }
     } 
}

//Parse error aborts execution
void __parseerror(int errcode, int line, int offset){
     static char* errstr[] = {
        "NUM_EXPECTED",
        "SYM_EXPECTED",
        "ADDR_EXPECTED",
        "SYM_TOO_LONG",
        "TOO_MANY_DEF_IN_MODULE",
        "TOO_MANY_USE_IN_MODULE",
        "TOO_MANY_INSTR",
     };
     printf("Parse Error line %d offset %d: %s\n", line, offset, errstr[errcode]);
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