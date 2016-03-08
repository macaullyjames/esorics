/*
 * Generates a set of 3-graphlets describing the user-generated portion of
 * the given program binary. 
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

#include "InstructionDecoder.h"
#include "Instruction.h"

#include "CodeSource.h"
#include "CodeObject.h"
#include "Function.h"
#include "dyntypes.h"

#include "graphlet.h"
#include "colors.h"

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
           "       --nodes <n> [number of nodes]\n"
           "       --byfunc [print functions separately]\n"
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
int NODES = 3;
bool BYFUNC = false;

int parse_options(int argc, char**argv)
{
    int ch;

    static struct option long_options[] = {
        {"help",no_argument,0,'h' },
        {"exclude",required_argument,0,'e' },
        {"color",no_argument,0,'l'},
        {"nodes",required_argument,0,'n'},
        {"commasep",no_argument,0,'c' },
        {"byfunc",no_argument,0,'b' }
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
                NODES = atoi(optarg);
                break;
            case 'l':
                COLOR = true;
                break;
            case 'b':
                BYFUNC = true;
                break;
            default:
                printf("Illegal option %c\n",ch);
            case 'h':
                usage(argv[0]);
                exit(1);
        }
    }

    if(BYFUNC)
        COMMASEP=true;

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

// build edge type sets for A given B and C
node edge_sets(Block * A, Block * B, Block * C)
{
    multiset<int> ins;
    multiset<int> outs;
    multiset<int> selfs;
    unsigned short color = 0;

    Block::edgelist srcs = A->sources();
    Block::edgelist trgs = A->targets();
    
    Block::edgelist::iterator eit;

    for(eit=srcs.begin();eit != srcs.end(); ++eit) {
        if((*eit)->src() == B || (*eit)->src() == C)
            ins.insert((*eit)->type());
        else if((*eit)->src() == A)
            selfs.insert((*eit)->type());
    }
    for(eit=trgs.begin();eit != trgs.end(); ++eit) {
        if((*eit)->trg() == B || (*eit)->trg() == C)
            outs.insert((*eit)->type());
        // don't duplicate self edges
    }

    if(COLOR)
        color = node_color(A);

    return node(ins,outs,selfs,color);
}

void mkgraphlets(Function * f, 
    std::map<graphlet,int> & counts,
    dyn_hash_map<Address,bool> & seen)
{
    NoSinkPredicate nosink;

    // Foreach block in the function
    //   for each pair of its neighboring *blocks*
    //     make a graphlet describing this triple & record it 
    Function::blocklist & blocks = f->blocks();
    Function::blocklist::iterator bit = blocks.begin();

    for( ; bit != blocks.end(); ++bit) {
        Block * b = *bit;

        if(seen.find(b->start()) != seen.end())
            continue;
        seen[b->start()] = true;

        Block::edgelist srcs = b->sources();
        Block::edgelist trgs = b->targets();
        Block::edgelist::iterator eit;

        // Step one: reduce the edge set to a block set
        std::set<Block*> srcblks;
        std::set<Block*> trgblks;
  
        for(eit=srcs.begin(&nosink);eit!=srcs.end();++eit) {
            if((*eit)->src() != b)
                srcblks.insert((*eit)->src());
        }
        for(eit=trgs.begin(&nosink);eit!=trgs.end();++eit) {
            if((*eit)->trg() != b) 
                trgblks.insert((*eit)->trg());
        }

        // Step two: build graphlets from various pairs:
        std::set<Block*>::iterator A;
        std::set<Block*>::iterator B;

        // 1. source & source
        for(A=srcblks.begin();A!=srcblks.end();++A) {
            B=A;++B;
            for( ; B != srcblks.end(); ++B) {
                graphlet g;
                g.addNode( edge_sets(*A,*B,b) );
                g.addNode( edge_sets(*B,*A,b) );
                g.addNode( edge_sets(b,*A,*B) );
                counts[g] += 1; 
                
                //printf("1 made graphlet size %d %s\n",g.size(),g.toString().c_str());
            }
        } 
    
        // 2. trg & trg
        for(A=trgblks.begin();A!=trgblks.end();++A) {
            B=A;++B;
            for( ; B!=trgblks.end();++B) {
                graphlet g;
                g.addNode( edge_sets(*A,*B,b) );
                g.addNode( edge_sets(*B,*A,b) );
                g.addNode( edge_sets(b,*A,*B) );
                counts[g] += 1; 
                //printf("2 made graphlet size %d %s\n",g.size(),g.toString().c_str());
            }
        }
                
        // 3. source & trg
        for(A=srcblks.begin();A!=srcblks.end();++A) {
            for(B=trgblks.begin();B!=trgblks.end();++B) {
                if(*A == *B)
                    continue;
                graphlet g;
                g.addNode( edge_sets(*A,*B,b) );
                g.addNode( edge_sets(*B,*A,b) );
                g.addNode( edge_sets(b,*A,*B) );
                counts[g] += 1; 
                //printf("3 made graphlet size %d %s\n",g.size(),g.toString().c_str());
            }
        }
    }
}

void print(std::map<graphlet,int>& counts)
{
    std::map<graphlet,int>::const_iterator cit = counts.begin();
    for( ; cit != counts.end(); ++cit) {
        const char * sep;
        if(COMMASEP)
            sep = ",";
        else
            sep = "\n";
        printf("%s:%d%s",(cit->first).compact(COLOR).c_str(),cit->second,sep);
    }

    if(COMMASEP)
        printf("\n");
}

int main(int argc, char **argv)
{
    dyn_hash_map<string, bool> exclude;
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

        mkgraphlets(f,counts,visited);

        if(BYFUNC) {
            printf("%lx,",f->addr());
            print(counts);
            counts.clear();
            visited.clear();
        }
    }

    if(!BYFUNC)
        print(counts);

    delete co;
    delete sts;

    return 0;
}

