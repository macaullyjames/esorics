/* 
 * Copyright (c) 1996-2010 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <assert.h>

#include "CodeObject.h"
#include "Function.h"
#include "InstructionDecoder.h"
#include "Instruction.h"
#include "entryIDs.h"
#include "RegisterIDs.h"

#include "feature.h"

#include "Singleton.h"

using namespace std;
using namespace Dyninst;
using namespace Dyninst::ParseAPI;
using namespace Dyninst::InstructionAPI;
using namespace NS_x86;

/** IdiomFeature implementation **/

IdiomTerm::IdiomTerm(Function *f, Address addr) :
    entry_id(ILLEGAL_ENTRY),
    arg1(NOARG),
    arg2(NOARG),
    len(0),
    _formatted(false)
{
    Instruction::Ptr insn;
    InstructionDecoder dec(
        (unsigned char*)f->isrc()->getPtrToInstruction(addr),
        30, // arbitrary
        f->isrc()->getArch());

    insn = dec.decode();
    if(insn) { 
        // set representation

        len = insn->size();

        const Operation & op = insn->getOperation();
        entry_id = op.getID() + 1;

        //fprintf(stderr,"IT[%d,%d",entry_id,len);

        if(entry_id != ILLEGAL_ENTRY) {
            vector<Operand> ops;
            insn->getOperands(ops);
            //fprintf(stderr,",%ld",ops.size());
            
            // we'll take up to two operands... which seems bad. FIXME

            unsigned short args[2] = {NOARG,NOARG};
            
            for(unsigned int i=0;i<2 && i<ops.size();++i) {
                Operand & op = ops[i];
                if(!op.readsMemory() && !op.writesMemory()) {
                    // register or immediate
                    set<RegisterAST::Ptr> regs;
                    op.getReadSet(regs);
                    op.getWriteSet(regs);

                    if(!regs.empty()) {
                        args[i] = (*regs.begin())->getID();
                    } else {
                        // immediate
                        args[i] = IMMARG;
                    }
                } else {
                    args[i] = MEMARG; 
                }
            }

            arg1 = args[0];
            arg2 = args[1];
        } else {
            //fprintf(stderr,",0");
        }


        //fprintf(stderr,",%x,%x],%s\n",arg1,arg2,insn->format().c_str());
    } else {
        //fprintf(stderr,"IT[no insn decoded]\n");
    }
}
extern IdiomTerm WILDCARD_IDIOM;
string
IdiomTerm::human_format() 
{
    string entryname;
    if(*this == WILDCARD_IDIOM)
        entryname = "*";
    else if(entryNames_IAPI.find((entryID)(entry_id-1)) == entryNames_IAPI.end())
        entryname = "[INVALID]";
    else
        entryname = entryNames_IAPI[(entryID)(entry_id-1)];

    RegTable::iterator foundName;


    if(arg1 != NOARG) {
       foundName = Singleton<IA32RegTable>::getInstance().IA32_register_names.find(IA32Regs(arg1));
       if(foundName != Singleton<IA32RegTable>::getInstance().IA32_register_names.end()) 
        entryname += ":" + (*foundName).second.regName;
    }
    if(arg2 != NOARG) {
       foundName = Singleton<IA32RegTable>::getInstance().IA32_register_names.find(IA32Regs(arg2));
       if(foundName != Singleton<IA32RegTable>::getInstance().IA32_register_names.end()) 
            entryname += ":" + (*foundName).second.regName;
    }

    return entryname;        
}

IdiomTerm::IdiomTerm(uint64_t it) :
    _formatted(false)
{
    from_int(it);
}

string
IdiomTerm::format() {
    if(_formatted)
        return _format;

    char buf[64];
    snprintf(buf,64,"%x_%x_%x",entry_id,arg1,arg2);
    _format += buf;
    _formatted = true;
    return _format;
}

bool
IdiomTerm::operator==(const IdiomTerm &it) const {
    return entry_id == it.entry_id &&
           arg1 == it.arg1 &&
           arg2 == it.arg2;
}
bool
IdiomTerm::operator<(const IdiomTerm &it) const {
    if(entry_id < it.entry_id) 
        return true;
    else if(entry_id == it.entry_id) {
        if(arg1 < it.arg1) 
            return true;
        else if(arg1 == it.arg1) 
            if(arg2 < it.arg2)
                return true;
    }
    return false;
}

size_t
IdiomTerm::hash() const {
    return tr1::hash<uint64_t>()(to_int());
}

uint64_t
IdiomTerm::to_int() const {
    uint64_t ret = 0;

    ret += ((uint64_t)entry_id) << 32;
    ret += ((uint64_t)arg1) << 16;
    ret += ((uint64_t)arg2);

    return ret;

}

void
IdiomTerm::from_int(uint64_t it)
{
    entry_id = (it>>32) & 0xffffffff;
    arg1 = (it>>16) & 0xffff;
    arg2 = it & 0xffff;
}

static void split(const char * str, vector<uint64_t> & terms)
{
    const char *s = str, *e = NULL;
    char buf[32];

    while((e = strchr(s,'_')) != NULL) {
        assert(e-s < 32);

        strncpy(buf,s,e-s);
        buf[e-s] = '\0';
        terms.push_back(strtoull(buf,NULL,16));

        s = e+1;
    }
    // last one
    if(strlen(s)) {
        terms.push_back(strtoull(s,NULL,16));
    }
 
}
string 
IdiomFeature::human_format() {
    string ret = "";
    //printf("formatting %s\n",format().c_str());
    for(unsigned i=0;i<_terms.size();++i) {
        ret += ((IdiomTerm*)_terms[i])->human_format();
        if(i<_terms.size()-1)
            ret += "_";
    }
    return ret;
}

IdiomFeature::IdiomFeature(char * str) :
    _formatted(false)
{
    vector<uint64_t> terms;
    //printf("init from **%s**\n",str);
    split(str+1,terms);
    for(unsigned i=0;i<terms.size();++i) {
        // XXX output may have had NOARGs shifted off.
        //     need to shift them back on.

        // we enforce a "no zero entry_id" policy,
        // which makes this safe
        uint64_t t = terms[i];
        if(!(t & ENTRY_MASK)) {
            t = t << ARG_SIZE;
            t |= NOARG;
        }
        if(!(t & ENTRY_MASK)) {
            t = t << ARG_SIZE;
            t |= NOARG;
        }

        //printf("    %lx\n",terms[i]);
        add_term(new IdiomTerm(t));
    }
}

string
IdiomFeature::format() {
    if(_formatted)
        return _format;

    char buf[64];

    _format = "I";

    for(unsigned i=0;i<_terms.size();++i) {
        // XXX shrink output by removing NOARGs from right
        uint64_t out = ((IdiomTerm*)_terms[i])->to_int();
        if((out & 0xffff) == NOARG)
            out = out >> 16;
        if((out & 0xffff) == NOARG)
            out = out >> 16;

        if(i+1<_terms.size())
            snprintf(buf,64,"%lx_",out);
        else
            snprintf(buf,64,"%lx",out);
        _format += buf;
    }

    _formatted = true;
    return _format;
}
