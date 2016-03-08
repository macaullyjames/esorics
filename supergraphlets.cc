/*
 * Generates graphlets for varying levels of simplification of the graph
 * due to node merging. Merge level 0 is the same as the graphlets.cc
 * utility
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
#include <set>

#include <boost/iterator/filter_iterator.hpp>
using boost::make_filter_iterator; 


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
           "       --color [color nodes based on instructions]\n"
           "       --merge <n> [number of merge iterations]\n"
           "       --graph [just print graph]\n"
           "       --anon [anonymous, collapsed edges]\n"
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
bool COMMASEP = false;
bool COLOR = false;
bool GRAPH = false;
bool ANON = false;
int MERGE = 0;

int parse_options(int argc, char**argv)
{
    int ch;

    static struct option long_options[] = {
        {"help",no_argument,0,'h' },
        {"exclude",required_argument,0,'e' },
        {"color",no_argument,0,'l'},
        {"graph",no_argument,0,'g'},
        {"merge",required_argument,0,'n'},
        {"anon",no_argument,0,'a'},
        {"commasep",no_argument,0,'c' }
    };

    int option_index = 0;

    while((ch=
        getopt_long(argc,argv,"hce:",long_options,&option_index)) != -1)
    {
        switch(ch) {
            case 'e':
                EXCLUDE = optarg;
                break;
            case 'c':
                COMMASEP = true;
                break;
            case 'n':
                MERGE = atoi(optarg);
                break;
            case 'l':
                COLOR = true;
                break;
            case 'g':
                GRAPH = true;
                break;
            case 'a':
                ANON = true;
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

unsigned short node_color(Block * A)
{
    using namespace Dyninst::InstructionAPI;
    unsigned short ret = 0;
    
    CodeRegion * cr = A->region();
    const unsigned char* bufferBegin =
            (const unsigned char*)(cr->getPtrToInstruction(A->start()));
    if(!bufferBegin)
        return 0;

    InstructionDecoder dec(bufferBegin, A->end() - A->start(), cr->getArch());
    while(Instruction::Ptr insn = dec.decode()) {
        InsnColor::insn_color c = InsnColor::lookup(insn);    
        if(c != InsnColor::NOCOLOR) {
            assert(c <= 16);
            ret |= (1 << c); 
        }
    }
    return ret; 
}

bool nsi(Edge* e)
{
    NoSinkPredicate nspred;
    Intraproc pred;
    return nspred(e) && pred(e);
}

// Using `seen' here to avoid duplicating the subgraphs
// corresponding to the shared areas of functions.
//
// This may or may not be sensible
graph * 
func_to_graph(Function * f, dyn_hash_map<Address,bool> & seen)
{
    dyn_hash_map<Address,snode*> node_map;

    int nctr = 0;

    graph * g = new graph();
    Function::blocklist blocks = f->blocks();

    dyn_hash_map<size_t,bool> done_edges;
    for(auto bit = blocks.begin(); bit != blocks.end(); ++bit) {
            Block * b = *bit;
            snode * n = g->addNode();

            char nm[16];
            snprintf(nm,16,"n%d",nctr++);
            n->name_ = std::string(nm);

            node_map[b->start()] = n;
            if(COLOR)
                n->setColor(new InsnColor(node_color(b)));
    }
    
    unsigned idx = 0;

    for(auto bit = blocks.begin(); bit != blocks.end(); ++bit) {
        Block * b = *bit;

        if(seen.find(b->start()) != seen.end())
            continue;
        seen[b->start()] = true;

        snode * n = g->nodes()[idx];
        for(auto eit = make_filter_iterator(nsi, b->sources().begin(), b->sources().end());
            eit != make_filter_iterator(nsi, b->sources().end(), b->sources().end());
            eit++) {
            Edge * e = *eit;

            if(done_edges.find((size_t)e) != done_edges.end())
                continue;
            done_edges[(size_t)e] = true;

            if(node_map.find(e->src()->start()) != node_map.end()) {
                (void)g->link(node_map[e->src()->start()],n,e->type());
            }
        }
        for(auto eit = make_filter_iterator(nsi, b->targets().begin(), b->targets().end());
            eit != make_filter_iterator(nsi, b->targets().end(), b->targets().end());
            eit++) {
            Edge * e = *eit;

            if(done_edges.find((size_t)e) != done_edges.end())
                continue;
            done_edges[(size_t)e] = true;

            if(node_map.find(e->trg()->start()) != node_map.end()) {
                (void)g->link(n,node_map[e->trg()->start()],e->type());
            }
        }
        ++idx;
    }
    return g;
}

int main(int argc, char **argv)
{
    dyn_hash_map<string, bool> exclude;
    SymtabCodeSource *sts;
    CodeObject *co;
    struct stat sbuf;

    srand((unsigned int)time(NULL));

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

    auto & funcs = co->funcs();

    if(EXCLUDE)
        load_exclude(exclude);    

    if(GRAPH) {
        printf("digraph G {\n");
    }
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


        graph * g = func_to_graph(f,visited);

        // iteratively compress
        for(int m=0;m<MERGE;++m) {
            //unsigned sz = g->nodes().size();
            g->compact();
            //fprintf(stderr,"[%d] compacted graph from %d nodes to %ld nodes\n",
                //m,sz,g->nodes().size());
        }
        
        if(GRAPH)
            g->todot(nid);
        else
            g->mkgraphlets(counts,COLOR,ANON);
        delete g;
    }

    if(GRAPH) {
        printf("}\n");
    }

    std::map<graphlet,int>::const_iterator cit = counts.begin();
    for( ; cit != counts.end(); ++cit) {
        char const* sep;
        if(COMMASEP)
            sep = ",";
        else
            sep = "\n";
        printf("SG_%s:%d%s",(cit->first).compact(COLOR).c_str(),cit->second,sep);
    }

    if(COMMASEP)
        printf("\n");

    delete co;
    delete sts;

    return 0;
}
