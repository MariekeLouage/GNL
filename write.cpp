/**************************************************************************
***
*** Copyright (c) 1998-2001 Peter Verplaetse, Dirk Stroobandt
***
***  Contact author: pvrplaet@elis.rug.ac.be
***  Affiliation:   Ghent University
***                 Department of Electronics and Information Systems
***                 St.-Pietersnieuwstraat 41
***                 9000 Gent, Belgium
***
***  Permission is hereby granted, free of charge, to any person obtaining
***  a copy of this software and associated documentation files (the
***  "Software"), to deal in the Software without restriction, including
***  without limitation
***  the rights to use, copy, modify, merge, publish, distribute, sublicense,
***  and/or sell copies of the Software, and to permit persons to whom the
***  Software is furnished to do so, subject to the following conditions:
***
***  The above copyright notice and this permission notice shall be included
***  in all copies or substantial portions of the Software.
***
*** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
*** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
*** OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
*** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
*** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
*** OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
*** THE USE OR OTHER DEALINGS IN THE SOFTWARE.
***
***************************************************************************/

#include "main.h"
#include "argread.h"
#include "debug.h"
#include "pvtools.h"
#include <cmath>

int CounterMap::operator[](void *p) {
    pair<map<void *, int>::iterator, bool> mi = counterMap.insert(pair<void *const, int>(p, next));
    if (mi.second)
        ++next;
    return mi.first->second;
}

void Module::WriteTree(const string &name, ModuleType *modType) {
    string filename = name + ".tree";
    ofstream out(filename.c_str());
    if (!out)
        throw ("Cannot open " + filename + " for writing");

    out << "# Tree data " << name << " generated by gnl " << Globals::version << " on " << time << endl;
    WriteInfoHeader(out, modType, "# ");

    out << "\n[top]\n" << number << endl;
    out << "\n[blocks]\n";
    for (list<Block *>::iterator bi = blocks.begin(); bi != blocks.end(); ++bi)
        out << (*bi)->moduleNumber << endl;
    out << "\n[tree]\n";
    for (list<Globals::PtreeNode>::iterator ti = Globals::treeData.begin(); ti != Globals::treeData.end(); ++ti)
        out << ti->parent << " " << ti->child1 << " " << ti->child2 << endl;
}

void Module::WritePtree(const string &name, ModuleType *modType) {
    string filename = name + ".ptree";
    ofstream out(filename.c_str());
    if (!out)
        throw ("Cannot open " + filename + " for writing");

    out << "# ptree " << name << " generated by gnl " << Globals::version << " on " << time << endl;
    WriteInfoHeader(out, modType, "# ");

    for (list<Globals::PtreeNode>::reverse_iterator ti = Globals::treeData.rbegin();
         ti != Globals::treeData.rend(); ++ti) {
        out << ti->parent << " " << ti->area << " " << ti->numBlocks << " " << ti->inputs << " " << ti->outputs
            << " 0 ";
        if (ti->child1 >= 0)
            out << ti->child1 << " " << ti->child2;
        out << endl;
    }
}

void Module::WriteHnl(const string &name, ModuleType *modType) {
    //get map of all the library cells
    map<string, Librarycell *> cells;
    for (list<Block *>::iterator bi = blocks.begin(); bi != blocks.end(); ++bi)
        cells[(*bi)->cell->Name()] = (*bi)->cell;

    string filename = name + ".hnl";
    ofstream out(filename.c_str());
    if (!out)
        throw ("Cannot open " + filename + " for writing");

    out << "# Netlist " << name << " generated by gnl " << Globals::version << " on " << time << endl;
    WriteInfoHeader(out, modType, "# ");

    //write library cells
    for (map<string, Librarycell *>::iterator li = cells.begin(); li != cells.end(); ++li) {
        out << (li->second->Sequential() ? "sequential " : "combinational ") << li->first << endl;
        if (li->second->I()) {
            out << "input";
            for (int i = 1; i <= li->second->I(); ++i)
                out << " i" << i;
            out << endl;
        }
        if (li->second->O()) {
            out << "output";
            for (int i = 1; i <= li->second->O(); ++i)
                out << " o" << i;
            out << endl;
        }
        out << "area " << (argRead.areaAsWeight ? li->second->Weight() : li->second->Size()) << endl;
        out << "end\n\n";
    }

    //write blocklist
    out << "circuit " << name << endl;
    CounterMap netMap;
    if (!inputs.empty()) {
        out << "input";
        for (list<InputNet *>::iterator ni = inputs.begin(); ni != inputs.end(); ++ni)
            out << " n" << netMap[*ni];
        out << endl;
    }
    if (!outputs.empty()) {
        out << "output";
        for (list<OutputNet *>::iterator ni = outputs.begin(); ni != outputs.end(); ++ni)
            out << " n" << netMap[*ni];
        out << endl;
    }

    for (list<Block *>::iterator bi = blocks.begin(); bi != blocks.end(); ++bi) {
        out << (*bi)->cell->Name();
        for (vector<Net *>::iterator ni = (*bi)->inputs.begin(); ni != (*bi)->inputs.end(); ++ni)
            out << " n" << netMap[*ni];
        for (vector<OutputNet *>::iterator ni = (*bi)->outputs.begin(); ni != (*bi)->outputs.end(); ++ni)
            out << " n" << netMap[*ni];
        out << endl;
    }
    out << "end\n";
}

void Module::WriteNetD(const string &name) {
    string filename = name + ".netD";
    ofstream out(filename.c_str());
    if (!out)
        throw ("Cannot open " + filename + " for writing");

    out << "0\n";
    out << (TotalPinCount() + numInputs + numOutputs) << endl;
    out << (internalNets.size() + numInputs + numOutputs) << endl;
    out << (numBlocks + numInputs + numOutputs) << endl;
    out << (numBlocks - 1) << endl;

    int padCounter = 0;
    CounterMap cellMap;

    for (list<InputNet *>::iterator ni = inputs.begin(); ni != inputs.end(); ++ni)
        (*ni)->WriteNetD(out, cellMap, padCounter);
    for (list<OutputNet *>::iterator ni = outputs.begin(); ni != outputs.end(); ++ni)
        (*ni)->WriteNetD(out, cellMap, padCounter, 1);
    for (list<OutputNet *>::iterator ni = internalNets.begin(); ni != internalNets.end(); ++ni)
        (*ni)->WriteNetD(out, cellMap, padCounter);
}

void Module::WriteNets(const string &name, ModuleType *modType) {
    string filename = name + ".nodes";
    ofstream out(filename.c_str());
    if (!out)
        throw ("Cannot open " + filename + " for writing");

    CounterMap cellMap;
    out << "UCLA nodes 1.0\n";
    out << "# Netlist " << name << " generated by gnl " << Globals::version << " on " << time << endl;
    WriteInfoHeader(out, modType, "# ");
    out << "NumNodes : " << (numBlocks + numInputs + numOutputs) << endl;
    out << "NumTerminals : " << (numInputs + numOutputs) << endl;

    for (int i = 1; i <= numInputs + numOutputs; ++i)
        out << "pad_" << i << " terminal\n";
    for (list<Block *>::iterator bi = blocks.begin(); bi != blocks.end(); ++bi)
        out << (*bi)->cell->Name() << '_' << cellMap[*bi] << endl;
    out.close();

    filename = name + ".nets";
    out.open(filename.c_str());
    if (!out)
        throw ("Cannot open " + filename + " for writing");

    out << "UCLA nets  1.0\n";
    out << "# Netlist " << name << " generated by gnl " << Globals::version << " on " << time << endl;
    WriteInfoHeader(out, modType, "# ");
    out << "NumNets : " << (internalNets.size() + numInputs + numOutputs) << endl;
    out << "NumPins : " << (TotalPinCount() + numInputs + numOutputs) << endl;

    int padCounter = 0;
    for (list<InputNet *>::iterator ni = inputs.begin(); ni != inputs.end(); ++ni)
        (*ni)->WriteNets(out, cellMap, padCounter);
    for (list<OutputNet *>::iterator ni = outputs.begin(); ni != outputs.end(); ++ni)
        (*ni)->WriteNets(out, cellMap, padCounter, 1);
    for (list<OutputNet *>::iterator ni = internalNets.begin(); ni != internalNets.end(); ++ni)
        (*ni)->WriteNets(out, cellMap, padCounter);
}

void Module::WriteNetD2(const string &name) {
    string filename = name + ".netD2";
    ofstream out(filename.c_str());
    if (!out)
        throw ("Cannot open " + filename + " for writing");

    int numIn = 0, numOut = 0;
    for (list<InputNet *>::iterator ni = inputs.begin(); ni != inputs.end(); ++ni)
        numIn += (*ni)->sinks.size();
    for (list<OutputNet *>::iterator ni = outputs.begin(); ni != outputs.end(); ++ni)
        numOut += (*ni)->sinks.size() + 1;
    for (list<OutputNet *>::iterator ni = internalNets.begin(); ni != internalNets.end(); ++ni)
        numOut += (*ni)->sinks.size();

    out << "0\n";
    out << 2 * (numIn + numOut) << endl;
    out << numIn + numOut << endl;
    out << numBlocks + numOutputs + numIn << endl;
    out << numBlocks - 1 << endl;

    int padCounter = 0;
    CounterMap cellMap;

    for (list<InputNet *>::iterator ni = inputs.begin(); ni != inputs.end(); ++ni)
        (*ni)->WriteNetD2(out, cellMap, padCounter);
    for (list<OutputNet *>::iterator ni = outputs.begin(); ni != outputs.end(); ++ni)
        (*ni)->WriteNetD2(out, cellMap, padCounter, 1);
    for (list<OutputNet *>::iterator ni = internalNets.begin(); ni != internalNets.end(); ++ni)
        (*ni)->WriteNetD2(out, cellMap, padCounter);
}

void Module::Net::WriteNetD(ofstream &out, CounterMap &cellMap) {
    for (list<Terminal>::iterator ti = sinks.begin(); ti != sinks.end(); ++ti)
        out << 'a' << cellMap[ti->first] << " l I\n";
}

void Module::InputNet::WriteNetD(ofstream &out, CounterMap &cellMap, int &padCounter) {
    out << 'p' << (++padCounter) << " s O\n";
    Net::WriteNetD(out, cellMap);
}

void Module::OutputNet::WriteNetD(ofstream &out, CounterMap &cellMap, int &padCounter, bool external) {
    out << 'a' << cellMap[source.first] << " s O\n";
    if (external)
        out << 'p' << (++padCounter) << " l I\n";
    Net::WriteNetD(out, cellMap);
}

void Module::Net::WriteNets(ofstream &out, CounterMap &cellMap) {
    for (list<Terminal>::iterator ti = sinks.begin(); ti != sinks.end(); ++ti)
        out << ti->first->cell->Name() << '_' << cellMap[ti->first] << " I\n";
}

void Module::InputNet::WriteNets(ofstream &out, CounterMap &cellMap, int &padCounter) {
    out << "NetDegree : " << (sinks.size() + 1) << endl;
    out << "pad_" << (++padCounter) << " O\n";
    Net::WriteNets(out, cellMap);
}

void Module::OutputNet::WriteNets(ofstream &out, CounterMap &cellMap, int &padCounter, bool external) {
    out << "NetDegree : " << (sinks.size() + external + 1) << endl;
    out << source.first->cell->Name() << '_' << cellMap[source.first] << " O\n";
    if (external)
        out << "pad_" << (++padCounter) << " I\n";
    Net::WriteNets(out, cellMap);
}

void Module::InputNet::WriteNetD2(ofstream &out, CounterMap &cellMap, int &padCounter) {
    for (list<Terminal>::iterator ti = sinks.begin(); ti != sinks.end(); ++ti) {
        out << 'p' << (++padCounter) << " s O\n";
        out << 'a' << cellMap[ti->first] << " l I\n";
    }
}

void Module::OutputNet::WriteNetD2(ofstream &out, CounterMap &cellMap, int &padCounter, bool external) {
    if (external) {
        out << 'a' << cellMap[source.first] << " s O\n";
        out << 'p' << (++padCounter) << " l I\n";
    }
    for (list<Terminal>::iterator ti = sinks.begin(); ti != sinks.end(); ++ti) {
        out << 'a' << cellMap[source.first] << " s O\n";
        out << 'a' << cellMap[ti->first] << " l I\n";
    }
}

void Module::WriteInfoHeader(ofstream &info, ModuleType *modType, string prefix) {
    info << prefix << "Command line: " << argRead.ar_commandLine << endl;
    info << prefix << "\n";
    info << prefix << "Basic circuit parameters:\n";
    char buf[1024];
    //info.form("%s   blocks: %6d\n",prefix.c_str(),numBlocks);
    sprintf(buf, "%s   blocks: %6d\n", prefix.c_str(), numBlocks);
    info << buf;
    int I, O, P, numPins = numInputs + numOutputs;
    modType->GetIO(area, I, O);
    P = I + O;
    sprintf(buf, "%s   inputs:  %6d   (%6d)\n", prefix.c_str(), numInputs, I);
    info << buf;
    sprintf(buf, "%s   outputs: %6d   (%6d)\n", prefix.c_str(), numOutputs, O);
    info << buf;
    sprintf(buf, "%s   pins:    %6d   (%6d)\n", prefix.c_str(), numPins, P);
    info << buf;
    sprintf(buf, "%s   g_frac:  %6.4f   (%6.4f)\n", prefix.c_str(), double(numOutputs) / numPins, double(O) / P);
    info << buf;
    modType->WriteRegions(info, prefix);
    info << prefix << endl;
    if (Globals::circuit == modType && !Globals::hierarchy.empty()) {
        info << prefix << "Hierarchy:\n";
        for (map<string, list<string> >::iterator hi = Globals::hierarchy.begin();
             hi != Globals::hierarchy.end(); ++hi) {
            info << prefix << "  " << hi->first << endl;
            bool first = 1;
            for (list<string>::iterator li = hi->second.begin(); li != hi->second.end(); ++li) {
                info << prefix << "    ";
                if (first) {
                    info << "\\";
                    first = 0;
                } else
                    info << " ";
                info << "| " << (*li) << endl;
            }
        }
        info << prefix << endl;
    }
}

void Module::WriteInfo(const string &name, ModuleType *modType) {
    string filename = name + ".info";
    ofstream info(filename.c_str());
    if (!info)
        throw ("Cannot open " + filename + " for writing");
    info << "Netlist " << name << " generated by gnl " << Globals::version << " on " << time << endl;
    WriteInfoHeader(info, modType);
}

void Module::WritePlots(const string &name, ModuleType *modType) {
    modType->WriteRtd(name);
    modType->WriteDat(name);
    string filename = name + ".plot";
    ofstream plot(filename.c_str());
    if (!plot)
        throw ("Cannot open " + filename + " for writing");
    plot << "set logscale xy\n";
    plot << "set data style linespoints\n\n";
    plot << "plot \"" << name << ".rtd\" tit \"Scattered T data\" with points, \\\n";
    plot << "     \"" << name << ".dat\" using 1:2 tit \"Target mean T\" with lines\n";
    plot << "\npause -1\n\n";
    plot << "plot \"" << name << ".dat\" using 1:2 tit \"Target mean T\" with lines, \\\n";
    plot << "     \"" << name << ".dat\" using 1:8 tit \"Actual mean T\", \\\n";
    plot << "     \"" << name << ".dat\" using 1:3 tit \"Target mean I\" with lines, \\\n";
    plot << "     \"" << name << ".dat\" using 1:9 tit \"Actual mean I\", \\\n";
    plot << "     \"" << name << ".dat\" using 1:4 tit \"Target mean O\" with lines, \\\n";
    plot << "     \"" << name << ".dat\" using 1:10 tit \"Actual mean O\", \\\n";
    plot << "     \"" << name << ".dat\" using 1:6 tit \"Target sigma T\" with lines, \\\n";
    plot << "     \"" << name << ".dat\" using 1:12 tit \"Actual sigma T\"\n";
    plot << "\npause -1\n\n";
    plot << "set nologscale y\n";
    plot << "plot \"" << name << ".dat\" using 1:5 tit \"Target mean g\" with lines, \\\n";
    plot << "     \"" << name << ".dat\" using 1:11 tit \"Actual mean g\", \\\n";
    plot << "     \"" << name << ".dat\" using 1:7 tit \"Target sigma g\" with lines, \\\n";
    plot << "     \"" << name << ".dat\" using 1:13 tit \"Actual sigma g\"\n";
    plot << "\npause -1\n";
}

void ModuleType::WriteRtd(const string &name) {
    if (rtdWritten)
        return;

    //get rtd information
    if (forrest.size() != 1)
        throw ("Internal error: forrest is not a tree");
    map<int, map<int, int> > rtd;
    forrest.begin()->second->AddRtdData(rtd);

    //write rtd file
    string filename = name + ".rtd";
    ofstream out(filename.c_str());
    char buf[1024];
    if (!out)
        throw ("Cannot open " + filename + " for writing");
    out << "# rtd for circuit " << name << " generated by gnl " << Globals::version << " on " << time << endl;
    out << "# command line: " << argRead.ar_commandLine << endl << endl;
    for (map<int, map<int, int> >::reverse_iterator bi = rtd.rbegin(); bi != rtd.rend(); ++bi)
        for (map<int, int>::reverse_iterator ti = bi->second.rbegin(); ti != bi->second.rend(); ++ti) {
            sprintf(buf, "%10d %10d %10d\n", bi->first, ti->first, ti->second);
            out << buf;
        }

    rtdWritten = 1;
}

void ModuleType::WriteDat(const string &name) {
    if (datWritten)
        return;
    FillBuckets();

    //write dat file
    string filename = name + ".dat";
    ofstream out(filename.c_str());
    if (!out)
        throw ("Cannot open " + filename + " for writing");
    out << "# data for circuit " << name << " generated by gnl " << Globals::version << " on " << time << endl;
    out << "# command line: " << argRead.ar_commandLine << endl << endl;
    out
            << "#               ----------------------- target value ---------------------        ---------------------- actual value ----------------------\n";
    out
            << "#      B        meanT      meanI      meanO     meanG    stddevT   stddevg        meanT      meanI      meanO     meanG    stddevT   stddevg\n";

    map<int, Region>::iterator ri = regions.begin();

    if (argRead.debugBits & debug::buckets)
        dout << "\n*** Buckets ***\n";

    for (map<int, list<TreeNode *> >::iterator bi = buckets.begin(); bi != buckets.end(); ++bi) {
        //calculate actual values
        int num = 0;
        double bSum = 0, tSum = 0, iSum = 0, oSum = 0, gSum = 0;

        for (list<TreeNode *>::iterator mi = bi->second.begin(); mi != bi->second.end(); ++mi) {
            ++num;
            bSum += log(double((*mi)->Size()));

            if (argRead.debugBits & debug::buckets)
                dout << stringPrintf("%10d %10d\n", (*mi)->Size(), (*mi)->NumTerminals());

            tSum += (*mi)->NumTerminals();
            iSum += (*mi)->NumInputs();
            oSum += (*mi)->NumOutputs();
            gSum += (*mi)->GFraction();
        }
        double B = exp(bSum / num), T = double(tSum) / num, I = double(iSum) / num, O = double(oSum) / num, g =
                double(gSum) / num;

        if (argRead.debugBits & debug::buckets) {
            dout << "-------------------------------------------\n";
            dout << stringPrintf("%10.3f %10.3f\n\n", B, T);
        }

        double sdevT = -1, sdevG = -1;
        if (num > 1) {
            double sqDivTSum = 0, sqDivGSum = 0;
            for (list<TreeNode *>::iterator mi = bi->second.begin(); mi != bi->second.end(); ++mi) {
                double div = (*mi)->NumTerminals() - T;
                sqDivTSum += div * div;
                div = (*mi)->GFraction() - g;
                sqDivGSum += div * div;
            }
            sdevT = sqrt(sqDivTSum / (num - 1));
            sdevG = sqrt(sqDivGSum / (num - 1));
        }
        //calculate target values and print intermediate lines (target only)
        double tgT, tgI, tgO, tgG, tgSdevT, tgSdevG;
        int tgB;
        do {
            GetMeanIO(B, tgT, tgI, tgO, tgG, tgSdevT, tgSdevG);
            if (ri == regions.end())
                break;
            tgB = ri->first;
            if (tgB < B * 1.0001) {
                tgT = ri->second.meanT;
                tgG = ri->second.meanG;
                tgI = tgT * (1 - tgG);
                tgO = tgT * tgG;
                tgSdevT = ri->second.sigmaT;
                tgSdevG = ri->second.sigmaG;
                ++ri;
            }
            if (tgB < B * 0.999)
                WriteDatLine(out, tgB, tgT, tgI, tgO, tgG, tgSdevT, tgSdevG, -1, -1, -1, -1, -1, -1);
        } while (tgB < B * 0.999);
        WriteDatLine(out, B, tgT, tgI, tgO, tgG, tgSdevT, tgSdevG, T, I, O, g, sdevT, sdevG);
    }
    datWritten = 1;
}

void ModuleType::WriteDatLine(ofstream &out, double B, double tgT, double tgI, double tgO, double tgG, double tgSdevT,
                              double tgSdevG,
                              double T, double I, double O, double g, double sdevT, double sdevG) {
    char buf[1024];
    sprintf(buf, "%10.3f", B);
    out << buf;

    WriteDatWord(out, tgT);
    WriteDatWord(out, tgI);
    WriteDatWord(out, tgO);
    WriteDatWord(out, tgG, 1);
    WriteDatWord(out, tgSdevT);
    WriteDatWord(out, tgSdevG, 1);

    out << "  ";

    WriteDatWord(out, T);
    WriteDatWord(out, I);
    WriteDatWord(out, O);
    WriteDatWord(out, g, 1);
    WriteDatWord(out, sdevT);
    WriteDatWord(out, sdevG, 1);

    out << endl;
}

void ModuleType::WriteDatWord(ofstream &out, double v, bool g) {
    char buf[1024];
    if (g) {
        if (v < 0)
            sprintf(buf, " %9s", "-    ");
        else
            sprintf(buf, " %9.4f", v);
    } else {
        if (v < 0)
            sprintf(buf, " %10s", "-   ");
        else
            sprintf(buf, " %10.3f", v);
    }
    out << buf;
}

void ModuleType::CompoundNode::AddRtdData(map<int, map<int, int> > &rtd) {
    ++(rtd[area][numInputs + numOutputs]);
    left->AddRtdData(rtd);
    right->AddRtdData(rtd);
}

void ModuleType::FillBuckets() {
    if (buckets.size() != 0)
        return;

    if (forrest.size() != 1)
        throw ("Internal error: forrest is not a tree");

    forrest.begin()->second->FillBucketsWithTree(buckets);
    buckets[buckets.rbegin()->first + 1].push_back(forrest.begin()->second);
}

void ModuleType::TreeNode::FillBucketsWithTree(map<int, list<TreeNode *> > &buckets) {
    buckets[int(log(double(Size())) / log(1.9))].push_back(this);
}

void ModuleType::CompoundNode::FillBucketsWithTree(map<int, list<TreeNode *> > &buckets) {
    TreeNode::FillBucketsWithTree(buckets);
    left->FillBucketsWithTree(buckets);
    right->FillBucketsWithTree(buckets);
}

void ModuleType::WriteRegions(ofstream &out, string prefix) {
    out << prefix << endl;
    out << prefix << "Regions:\n";
    for (map<int, Region>::iterator ri = regions.begin(); ri != regions.end(); ++ri) {
        out << prefix << "  B >= " << ri->first << endl;
        out << prefix << "    meanT  = " << ri->second.meanT << endl;
        out << prefix << "    sigmaT = " << ri->second.sigmaT << endl;
        out << prefix << "    meanG  = " << ri->second.meanG << endl;
        out << prefix << "    sigmaG = " << ri->second.sigmaG << endl;
        if (ri->first > 1) {
            out << prefix << "    p      = " << ri->second.p << endl;
            out << prefix << "    q      = " << ri->second.q << endl;
            out << prefix << "    g_fact = " << ri->second.g_factor << endl;
        }
    }
}

int Module::TotalPinCount() {
    int pins = 0;
    for (list<Block *>::iterator bi = blocks.begin(); bi != blocks.end(); ++bi)
        pins += (*bi)->inputs.size() + (*bi)->outputs.size();
    return pins;
}
