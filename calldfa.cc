/*
 * Represents a program as the set of call-DFAs defined by its functions.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>

#include <string>
#include <vector>
#include <limits>

#include "InstructionDecoder.h"
#include "Instruction.h"

#include "CodeSource.h"
#include "CodeObject.h"
#include "Function.h"
#include "dyntypes.h"

#include "graphlet.h"
#include "colors.h"
#include "supergraph.h"

using namespace std;
using namespace Dyninst;
using namespace Dyninst::ParseAPI;
using namespace Dyninst::InstructionAPI;
using namespace graphlets;

void usage(char *s)
{
    printf("Usage: %s [options] <binary>\n"
           "       --exclude <file> [exclusion list]\n"
           "       --graph [just draw graph]\n"
           "       --libmap <file> [library func list]\n"
           "       --commasep [comma separated graphlets]\n",s);
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
char * LIBMAP = NULL;
bool COMMASEP = false;
bool GRAPH = false;
bool ANON = false;

int parse_options(int argc, char**argv)
{
    int ch;

    static struct option long_options[] = {
        {"help",no_argument,0,'h' },
        {"exclude",required_argument,0,'e' },
        {"graph",no_argument,0,'g'},
        {"commasep",no_argument,0,'c' },
        {"libmap",required_argument,0,'l'}
    };

    int option_index = 0;

    while((ch=
        getopt_long(argc,argv,"h",long_options,&option_index)) != -1)
    {
        switch(ch) {
            case 'e':
                EXCLUDE = optarg;
                break;
            case 'c':
                COMMASEP = true;
                break;
            case 'g':
                GRAPH = true;
                break;
            case 'l':
                LIBMAP = optarg;
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


void load_exclude(dyn_hash_map<string,bool> & exclude)
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

void load_libmap(dyn_hash_map<string,unsigned short> & libmap)
{
    char * buf = NULL;
    size_t n = 0;
    ssize_t read;
    unsigned short ind = 0;

    FILE * exin = fopen(LIBMAP,"r");
    if(!exin) {
        fprintf(stderr,"Can't open library func map file %s: %s\n",
            EXCLUDE,strerror(errno));
        return;
    }

    while(-1 != (read = getline(&buf,&n,exin))) {
        chomp(buf);            
        libmap[string(buf)] = ++ind;
    }
    
    if(buf) 
        free(buf); 
}

void targets(Block * b, set<Block*> &t)
{
    Intraproc ip;
    NoSinkPredicate pred(&ip);

    Block::edgelist::iterator bit = b->targets().begin(&pred);
    for( ; bit != b->targets().end(); ++bit)
        t.insert((*bit)->trg());
}

const int NULLDEF = numeric_limits<int>::max();
/*
 * Calls kill other call defs
 */
void reaching_defs(
    Function *f, 
    vector<Block*> & blocks, 
    vector< set<int> > & defs,
    dyn_hash_map<void*,int> & bmap)
{
    vector<int> work;
    vector<int> call_gen;
    Block * b = f->entry();
    int bidx = bmap[b];

    // figure out the call generators
    call_gen.resize(blocks.size(),-1);
    Function::edgelist::iterator cit = f->callEdges().begin();
    for( ; cit != f->callEdges().end(); ++cit) {
        int cidx = bmap[(*cit)->src()];
        call_gen[ cidx ] = cidx;
    }

    // entry block generates the "no call" definition
    defs[bidx].insert(NULLDEF);
    work.push_back(bidx);

    while(!work.empty()) {
        bidx = work.back();
        work.pop_back();
        b = blocks[bidx];

        if(call_gen[bidx] != -1)
            defs[bidx].insert(call_gen[bidx]);

        set<Block*> targs;
        targets(b,targs);
        set<Block*>::iterator sit = targs.begin();
        for( ; sit != targs.end(); ++sit) {
            Block * t = *sit;
            int tidx = bmap[t];

            // if b makes a call, it kills all calls except that one
            // else it passes its defs
    
            set<int> newdefs = defs[tidx];
            if(call_gen[bidx] != -1)
                newdefs.insert(call_gen[bidx]);
            else
                newdefs.insert(defs[bidx].begin(),defs[bidx].end());

            if(newdefs != defs[tidx]) {
                defs[tidx] = newdefs;
                work.push_back(tidx);
            }
        }
    } 
}

/*
 * Construct a graph of entry, exit and call nodes, where edges indicate
 * reaching definitions
 */
graph * collapse(
    Function *f, 
    vector<Block*> & blocks, 
    vector< set<int> > & defs,
    dyn_hash_map<void*,int> & bmap,
    dyn_hash_map<std::string,unsigned short> & libmap)
{
    vector<snode*> callnodes(blocks.size(),NULL);
    graph * g = new graph();
    snode * entry = g->addNode();

    // 1. Set up nodes for the call blocks
    Function::edgelist::iterator cit = f->callEdges().begin();
    for( ; cit != f->callEdges().end(); ++cit) {
        int cidx = bmap[(*cit)->src()];
        callnodes[ cidx ] = g->addNode();

        // color
        std::map<Address, std::string>::iterator pltit =
            f->obj()->cs()->linkage().find((*cit)->trg()->start());
        if(pltit != f->obj()->cs()->linkage().end()) {
            LibCallColor * c = new LibCallColor(libmap,(*pltit).second);
            callnodes[cidx]->setColor(c);
        } else {
            LocalCallColor * c = new LocalCallColor();
            callnodes[cidx]->setColor(c);
        }
    }

    // 2. Link the call nodes to one another
    for(unsigned i=0;i<callnodes.size();++i) {
        if(callnodes[i]) {
            set<int> & d = defs[i];
            set<int>::iterator it = d.begin();
            for( ; it != d.end(); ++it) {
                if(*it == NULLDEF)
                    g->link(entry,callnodes[i],0);
                else if(*it != (int)i)
                    g->link(callnodes[*it],callnodes[i],0);
            }
        }
    }

    // 3. Find exit nodes && link according to reaching defs
    Intraproc pred1;
    NoSinkPredicate pred2(&pred1);
    for(unsigned i=0;i<blocks.size();++i) {
        Block * b = blocks[i];
        Block::edgelist::iterator it = b->targets().begin(&pred2);
        int tcnt = 0;
        for( ; it != b->targets().end(); ++it) {
            ++tcnt;
        }
        if(tcnt == 0) {
            snode * exit = g->addNode();
            set<int> & d = defs[i];
            set<int>::iterator it = d.begin();
            for( ; it != d.end(); ++it) {
                if(*it == NULLDEF)
                    g->link(entry,exit,0);
                else
                    g->link(callnodes[*it],exit,0);
            }
        }
    }
    return g;
}

graph * mkcalldfa(Function *f,
    dyn_hash_map<std::string,unsigned short> & libmap)
{
    vector<Block*> blocks;
    vector< set<int> > defs; 
    dyn_hash_map<void*,int> bmap;

    Function::blocklist::iterator bit = f->blocks().begin();
    for( ; bit != f->blocks().end(); ++bit) {
        bmap[*bit] = blocks.size();
        blocks.push_back(*bit);
    }
    defs.resize( blocks.size() );

    // 1. Reaching definitions on call blocks
    reaching_defs(f,blocks,defs,bmap);
    
    // 2. Node collapse
   return collapse(f,blocks,defs,bmap,libmap);
    
}

int main(int argc, char **argv)
{
    dyn_hash_map<string, bool> exclude;
    dyn_hash_map<string, unsigned short> libmap;
    SymtabCodeSource *sts;
    CodeObject *co;
    struct stat sbuf;

    // to ensure we don't duplicate addresses
    dyn_hash_map<Address,bool> visited;

    // graphlet counts
    std::map<graphlet,int> counts;

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

    if(LIBMAP) {
        load_libmap(libmap);
    }

    if(GRAPH)
        printf("digraph G {\n"); 
    int nid = 0;

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

        graph * g = mkcalldfa(f,libmap);
        if(GRAPH) {
            g->todot(nid,true);
        }
        else
            g->mkgraphlets(counts,true,false);  // color, not anonymous
        delete g;
    }

    if(GRAPH) {
        printf("}\n");
    }

    std::map<graphlet,int>::const_iterator cit = counts.begin();
    for( ; cit != counts.end(); ++cit) {
        char * sep;
        if(COMMASEP)
            sep = ",";
        else
            sep = "\n";
        printf("CD_%s:%d%s",(cit->first).compact(true).c_str(),cit->second,sep);
    }

    if(COMMASEP)
        printf("\n");

    delete co;
    delete sts;

    return 0;
}
