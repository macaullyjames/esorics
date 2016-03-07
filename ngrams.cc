/*
 * Generate the set of ngrams describing the user-generated portion of
 * the given program binary. 
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <string>
#include <vector>
#include <unordered_map>

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
           "       -n <n> [length of ngrams]\n"
           "       --exclude <file> [exclusion list]\n",s);
}

FILE * out;

/* getopt declarations */
extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

/* options */
int N = 0;
char * EXCLUDE = NULL;

int parse_options(int argc, char**argv)
{
    int ch;

    static struct option long_options[] = {
        {"help",no_argument,0,'h' },
        {"exclude",required_argument,0,'e' },
        {"commasep",no_argument,0,'c' }
    };

    int option_index = 0;

    while((ch=
        getopt_long(argc,argv,"hcn:e:",long_options,&option_index)) != -1)
    {
        switch(ch) {
            case 'n':
                N = strtoul(optarg,NULL,10);
                break;
            case 'e':
                EXCLUDE = optarg;
                break;
            default:
                printf("Illegal option %c\n",ch);
            case 'h':
                usage(argv[0]);
                exit(1);
        }
    }

    if(0 == N) {
        printf("Length of ngrams is required\n");
        usage(argv[0]);
        exit(1);
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


void load_exclude(unordered_map<string,bool> & exclude)
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

void print_ngram(unsigned char * ngstart, unsigned char * ngbuf)
{
    printf("<");
    for(int i=0;i<N;++i) {
        printf("%02x",*(ngstart++));
        if(ngstart - ngbuf >= N)
            ngstart = ngbuf;
    }
    printf(">,");
}

void mkngrams(SymtabCodeSource * sts, FuncExtent * fe, 
    IBSTree<FuncExtent> & visited)
{
    Address a;
    unsigned char ngrambuf[N];
    unsigned char * ngstart = ngrambuf;
    unsigned char * ngcur = ngrambuf;
    int cnt = 0;
    set<FuncExtent*> lookup;

    for(a = fe->start(); a < fe->end(); ++a) {

        if(visited.find(a,lookup) > 0) {
            // already visited, skip
            lookup.clear();
            continue;
        }

        unsigned char * byte = (unsigned char*)sts->getPtrToInstruction(a);
        *(ngcur++) = *byte;

        ++cnt;

        if(ngcur - ngrambuf >= N)
            ngcur = ngrambuf;
        
        if(cnt >= N) {
            print_ngram(ngstart++,ngrambuf); 
            if(ngstart - ngrambuf >= N)
                ngstart = ngrambuf; 
        }
    } 

    visited.insert(fe);
}

int main(int argc, char **argv)
{
    unordered_map<string, bool> exclude;
    SymtabCodeSource *sts;
    CodeObject *co;
    struct stat sbuf;

    // to ensure we don't duplicate addresses
    IBSTree<FuncExtent> visited;

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

    auto & funcs = co->funcs();

    if(EXCLUDE)
        load_exclude(exclude);    

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

        vector<FuncExtent *> const& extents = f->extents();
        for(unsigned i=0;i<extents.size();++i) {
            FuncExtent * fe = extents[i];
    
            mkngrams(sts,fe,visited);        
        }
    }
    printf("\n");

    delete co;
    delete sts;

    return 0;
}
