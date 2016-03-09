#ifndef _GRAPHLET_H_
#define _GRAPHLET_H_

#include <set>
#include <vector>
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace graphlets {

/*
 * Graphlets are described by the edges of each node
 * 
 * There is a partial ordering over edges:
 *   inset > outset > selfset
 * 
 *   comparison of the edge sets is magnitude and then on
 *   edge types (edge types are ints)
 */

class edgeset {
 friend class node;
 public:
    edgeset() { }
    edgeset(std::multiset<int> types) :
        types_(types.begin(),types.end())
    {
    }
    edgeset(std::set<int> types) :
        types_(types.begin(),types.end())
    {
    }
    ~edgeset() { }

    void set(std::multiset<int> types) {
        types_.clear();
        types_.insert(types_.begin(),types.begin(),types.end());
    }

    bool operator<(edgeset const& o) const
    {
        if(this == &o)
            return false;

        unsigned mysz = types_.size();
        unsigned osz = o.types_.size();
       
        if(mysz != osz)
            return mysz < osz;
        else {
            if(mysz == 0) 
                return false;
            std::vector<int>::const_iterator ait = types_.begin();
            std::vector<int>::const_iterator bit = o.types_.begin();
            for( ; ait != types_.end(); ++ait, ++bit) {
                if(*ait < *bit)
                    return true;
                else if(*ait > *bit)
                    return false;
            }
            return false;
        }
        
    }

    void print() const {
        std::vector<int>::const_iterator it = types_.begin();
        for( ; it != types_.end(); ++it) {
            printf(" %d",*it);
        }
    }

    std::string toString() const {
        std::stringstream ret;
        std::vector<int>::const_iterator it = types_.begin();
        for( ; it != types_.end(); ++it) {
            ret << " " << *it;
        }
        return ret.str();
    }

    std::string compact() const {
        std::stringstream ret;
        std::unordered_map<int,int> unique;
        std::vector<int>::const_iterator it = types_.begin();
        for( ; it != types_.end(); ++it)
            unique[*it] +=1;
        std::unordered_map<int,int>::iterator uit = unique.begin();
        for( ; uit != unique.end(); ) {
            ret << (*uit).first;
            if((*uit).second > 1)
                ret << "x" << (*uit).second;
            if(++uit != unique.end())
                ret << ".";
        }
        return ret.str();
    }

 private:
    // XXX must be sorted
    std::vector<int> types_;
};

class node {
 public:
    node() : color_(0),colors_(false) { }
    node(std::multiset<int> & ins, std::multiset<int> & outs, std::multiset<int> &self) :
        ins_(ins), outs_(outs), self_(self),color_(0)
    { }
    node(std::multiset<int> & ins, std::multiset<int> & outs, std::multiset<int> &self, unsigned short color) :
        ins_(ins), outs_(outs), self_(self),color_(color)
    { }
    node(std::set<int> & ins, std::set<int> & outs, std::set<int> &self, unsigned short color) :
        ins_(ins), outs_(outs), self_(self),color_(color)
    { }

    ~node() { }

    void setIns(std::multiset<int> & ins) {
        ins_.set(ins);
    }
    void setOuts(std::multiset<int> & outs) {
        outs_.set(outs);
    }
    void setSelf(std::multiset<int> & self) {
        self_.set(self);
    }

    bool operator<(node const& o) const {
        if(this == &o)
            return false;

        if(color_ < o.color_)
            return true;
        else if(o.color_ < color_)
            return false;
        if(ins_ < o.ins_)
            return true;
        else if(o.ins_ < ins_)
            return false;
        else if(outs_ < o.outs_)
            return true;
        else if(o.outs_ < outs_)
            return false;
        else if(self_ < o.self_)
            return true;
        else
            return false;
    }

    void print() const {
        printf("{"); ins_.print(); printf(" }");
        printf(" {"); outs_.print(); printf(" }");
        printf(" {"); self_.print(); printf(" }");
    }

    std::string toString() const {
        std::stringstream ret;
        ret << "[ {" << ins_.toString() << " }";
        ret << " {" << outs_.toString() << " }";
        ret << " {" << self_.toString() << " } ]";
        return ret.str();
    }

    std::string compact(bool colors) const {
        std::stringstream ret;
        ret << ins_.compact() << "/" ;
        ret << outs_.compact() << "/";
        ret << self_.compact();
        if(colors)
            ret << "/" << color_;
        return ret.str();
    }

 private:
    edgeset ins_;
    edgeset outs_;
    edgeset self_;
    unsigned short color_;
    bool colors_;
};


class graphlet {
 public:
    graphlet() {}
    ~graphlet() {}

    void addNode(node const& node) {
        nodes_.insert(node);
    }

    bool operator<(graphlet const& o) const {
        if(this == &o)
            return false;

        unsigned asz = nodes_.size();
        unsigned bsz = o.nodes_.size();
        if(asz != bsz)
            return asz < bsz;
        else if(asz == 0)
            return false;

        std::multiset<node>::const_iterator ait = nodes_.begin();
        std::multiset<node>::const_iterator bit = o.nodes_.begin();
       
        for( ; ait != nodes_.end(); ++ait,++bit) {
            if(*ait < *bit)
                return true;
            else if(*bit < *ait)
                return false;
        }
        return false;
    }

    void print() const {
        std::multiset<node>::const_iterator it = nodes_.begin();
        for( ; it != nodes_.end(); ++it) {
            (*it).print();
            printf(" ");
        }
        printf("\n");
    }

    std::string toString() const {
        std::stringstream ret;
        std::multiset<node>::const_iterator it = nodes_.begin();
        for( ; it != nodes_.end(); ++it) {
            ret << (*it).toString() << " ";
        }
        return ret.str();
    }

    std::string compact(bool color) const {
        std::stringstream ret;
        std::multiset<node>::const_iterator it = nodes_.begin();

        for( ; it != nodes_.end(); ) {
            ret << (*it).compact(color);
            if(++it != nodes_.end())
                ret << "_";
        }
        return ret.str();
    }

    unsigned size() const { return nodes_.size(); }

 private:
    std::multiset<node> nodes_;
};

}

#endif
