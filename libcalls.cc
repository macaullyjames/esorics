/*
 * Generate the set of library calls the program makes
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <string>
#include <vector>
#include <ext/hash_map>

#include "InstructionDecoder.h"
#include "Instruction.h"

#include "CodeSource.h"
#include "CodeObject.h"
#include "Function.h"

using namespace std;
using namespace __gnu_cxx;
using namespace Dyninst;
using namespace Dyninst::ParseAPI;
using namespace Dyninst::InstructionAPI;

void usage(char *s)
{
    printf("Usage: %s [options] <binary>\n"
           "       --exclude <file> [exclusion list]\n"
           "       --listcall [print all plt funcs]\n"
           "       --commasep [comma separated ngrams]\n",s);
}

FILE * out;

/* getopt declarations */
extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

/* options */
char * EXCLUDE = NULL;
bool COMMASEP = false;
bool LISTALL = false;

int parse_options(int argc, char**argv)
{
    int ch;

    static struct option long_options[] = {
        {"help",no_argument,0,'h' },
        {"exclude",required_argument,0,'e' },
        {"commasep",no_argument,0,'c' },
        {"listall",no_argument,0,'l' }
    };

    int option_index = 0;

    while((ch=
        getopt_long(argc,argv,"hcn:e:",long_options,&option_index)) != -1)
    {
        switch(ch) {
            case 'e':
                EXCLUDE = optarg;
                break;
            case 'c':
                COMMASEP = true;
                break;
            case 'l':
                LISTALL = true;
                break;
            default:
                printf("Illegal option %c\n",ch);
            case 'h':
                usage(argv[0]);
                exit(1);
        }
    }

    return optind;
}

inline void
chomp(char*s)
{
    for( ; *s != '\0'; ++s)
        if(*s == '\n') {
            *s = '\0';
            break;
        }
}


void load_exclude(hash_map<string,bool> & exclude)
{
    char * buf = NULL;
    size_t n = 0;
    ssize_t read;

    FILE * exin = fopen(EXCLUDE,"r");
    if(!exin) {
        fprintf(stderr,"Can't open exclude file %s: %s\n",
            EXCLUDE,strerror(errno));
        return;
    }

    while(-1 != (read = getline(&buf,&n,exin))) {
        chomp(buf);            
        exclude[string(buf)] = true;
    }
    
    if(buf) 
        free(buf); 
}

int main(int argc, char **argv)
{
    hash_map<string, bool> exclude;
    SymtabCodeSource *sts;
    CodeObject *co;
    struct stat sbuf;

    int binindex = parse_options(argc, argv);
    if(argc-1<binindex) {
        usage(argv[0]);
        exit(1);
    }
    
    if(0 != stat(argv[binindex],&sbuf)) {
        fprintf(stderr,"Failed to stat %s: ",argv[binindex]);
        perror("");
        exit(1);
    }
    sts = new SymtabCodeSource( argv[binindex] );
    co = new CodeObject( sts );
    co->parse();

    CodeObject::funclist & funcs = co->funcs();

    if(EXCLUDE)
        load_exclude(exclude);    

   
    hash_map<string, int> pltcnts; 

    if(LISTALL) {
        std::map<Address, std::string>::iterator pltit =
            co->cs()->linkage().begin();
        for( ; pltit != co->cs()->linkage().end(); ++pltit)
            pltcnts[(*pltit).second] = 0;
    }

    hash_map<string,bool> real_funcs;

    CodeObject::funclist::iterator fit = funcs.begin();
    for( ; fit != funcs.end(); ++fit) {
        Function * f = *fit;
        if(exclude.find(f->name()) != exclude.end()) {
            //fprintf(stderr,"skipped %s\n",f->name().c_str());
            continue; 
        }

        if(strncmp(f->name().c_str(),"std::",5) == 0 ||
           strncmp(f->name().c_str(),"__gnu_cxx::",11) == 0) {
            //fprintf(stderr,"skipped %s\n",f->name().c_str());
            continue;
        }

        Function::edgelist & calls = f->callEdges();
        Function::edgelist::iterator it = calls.begin();
        for( ; it != calls.end(); ++it) {
            Edge * e = *it;
            std::map<Address, std::string>::iterator pltit =
                co->cs()->linkage().find(e->trg()->start());
            if(pltit != co->cs()->linkage().end()) {
                pltcnts[(*pltit).second] += 1;
            }      
        }

        if(co->cs()->linkage().find(f->addr()) == co->cs()->linkage().end()) {
            real_funcs[f->name()] = true;
        }
    }

    hash_map<string,int>::iterator cit = pltcnts.begin();
    for( ; cit != pltcnts.end(); ++cit) {
        char const* sep;
        if(COMMASEP)
            sep = ",";
        else
            sep = "\n";


        if(real_funcs.find((*cit).first) == real_funcs.end())
            printf("%s:%d%s",(*cit).first.c_str(),(*cit).second,sep);
    }

    if(COMMASEP)
        printf("\n");

    delete co;
    delete sts;

    return 0;
}
