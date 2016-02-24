#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>

#include <vector>
#include <ext/hash_map>
#include <map>

#include "InstructionDecoder.h"
#include "Instruction.h"

#include "CodeSource.h"
#include "CodeObject.h"
#include "Function.h"

#include "feature.h"

using namespace std;
using namespace __gnu_cxx;
using namespace Dyninst;
using namespace Dyninst::ParseAPI;
using namespace Dyninst::InstructionAPI;

void usage(char *s)
{
    fprintf(stderr,"Usage: %s [options] <binary>\n"
                "       --class <class tag>\n"
                "       --nort     [skip RT-discovered funcs]\n"
                "       --name     [print name]\n"
                "       --exclude <file> [exclusion list]\n"
                "       --help [display this message]\n",s);
}

/* Getopt declarations */
extern char *optarg;
extern int optind;
extern int optopt;
extern int opterr;
extern int optreset;

char * CLASS_TAG = NULL;
bool NORT = false;
bool NOPLT = false;
char * EXCLUDE = NULL;

int parse_options(int argc, char**argv)
{
    int ch;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"class",required_argument,0,'c'},
        {"nort",0,0,'r'},
        {"noplt",0,0,'p'},
        {"noprint",0,0,'n'},
        {"help",0,0,'h'},
        {"exclude",required_argument,0,'e'},
    };

    while((ch = 
        getopt_long(argc,argv,"",long_options,&option_index))!=-1)
    {
        switch(ch) {
            case 'c':
                CLASS_TAG = optarg;
                break;
            case 'r':
                NORT = true;
                break;
            case 'p':
                NOPLT = true;
                break;
            case 'e':
                EXCLUDE = optarg;
                break;
            case 'h':
            default:
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

void print(map<string,int>&counts)
{
    map<string,int>::const_iterator cit= counts.begin();
    for( ; cit != counts.end(); ++cit) {
        printf(",%s:%d",(*cit).first.c_str(),(*cit).second);
    }
}

int main(int argc, char**argv) {
    hash_map<string, bool> exclude;
    SymtabCodeSource *sts;
    CodeObject *co;
    struct stat sbuf;
    int binindex;
    FeatureVector fv;
    
    map<string, int> counts;

    if(argc-1<(binindex=parse_options(argc,argv))) {
        usage(argv[0]);
        exit(1);
    }

    if(0 != stat(argv[binindex],&sbuf)) {
        fprintf(stderr,"Binary target %s not found\n",argv[binindex]);
        perror("");
        exit(1);
    }

    if(EXCLUDE)
        load_exclude(exclude);

    sts = new SymtabCodeSource(argv[binindex]);
    co = new CodeObject(sts);

    co->parse();
    CodeObject::funclist & funcs = co->funcs();
    for(CodeObject::funclist::iterator fit = funcs.begin();
        fit != funcs.end();
        ++fit)
    {
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

        if(!(*fit)->blocks().empty()) {
            if(NORT && (*fit)->src() == RT)
                continue;
    
            if(NOPLT && sts->linkage().find((*fit)->addr()) !=
                        sts->linkage().end())
                continue;
            
            fv.eval((*fit),true,false);
            FeatureVector::iterator fvit = fv.begin();
            for( ; fvit != fv.end(); ++fvit) {
                counts[(*fvit)->format()] += 1;
            }
        }
    }

    if(CLASS_TAG)
       printf("%s",CLASS_TAG);

    print(counts); 
    printf("\n");

    delete co;
    delete sts;
}

