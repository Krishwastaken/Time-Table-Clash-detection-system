#include "ConflictAnalysis.h"
#include "ReportFormatter.h"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <set>
#include <stack>
#include <tuple>
#include <utility>
using namespace std;
namespace {
constexpr size_t NONE = static_cast<size_t>(-1);
int dayOrder(string day) {
    transform(day.begin(), day.end(), day.begin(), [](unsigned char c){return static_cast<char>(tolower(c));});
    const vector<string> days={"monday","tuesday","wednesday","thursday","friday","saturday","sunday"};
    auto it=find(days.begin(),days.end(),day);
    return it==days.end()?7:static_cast<int>(it-days.begin());
}
}
namespace ConflictAnalysis {
vector<Incident> consolidate(const vector<Clash>& clashes,const vector<Session>& sessions) {
    map<pair<size_t,size_t>, vector<Clash>> grouped;
    for (const auto& c:clashes) {
        if(c.firstSession>=sessions.size() || (c.secondSession!=NONE && c.secondSession>=sessions.size())) continue;
        size_t a=c.firstSession,b=c.secondSession;
        if(b!=NONE && b<a) swap(a,b);
        grouped[{a,b}].push_back(c);
    }
    vector<Incident> result;
    for(auto& entry:grouped) {
        auto& reasons=entry.second;
        sort(reasons.begin(),reasons.end(),[](const Clash& a,const Clash& b){return static_cast<int>(a.type)<static_cast<int>(b.type);});
        result.push_back({entry.first.first,entry.first.second,std::move(reasons)});
    }
    sort(result.begin(),result.end(),[&](const Incident& a,const Incident& b){
        const auto& x=sessions[a.first];const auto& y=sessions[b.first];
        return make_tuple(dayOrder(x.getDay()),x.getStartTime(),a.first,a.second)<make_tuple(dayOrder(y.getDay()),y.getStartTime(),b.first,b.second);
    });
    return result;
}
vector<Component> connectedGroups(const vector<Clash>& clashes,size_t sessionCount) {
    vector<set<size_t>> adj(sessionCount);
    for(const auto& c:clashes) {
        if(c.firstSession>=sessionCount||c.secondSession>=sessionCount||c.firstSession==c.secondSession) continue;
        adj[c.firstSession].insert(c.secondSession);
        adj[c.secondSession].insert(c.firstSession);
    }
    vector<char> seen(sessionCount,false);
    vector<Component> result;
    for(size_t i=0;i<sessionCount;++i) {
        if(seen[i]||adj[i].empty())continue;
        Component comp;
        stack<size_t> pending;pending.push(i);seen[i]=true;
        while(!pending.empty()) {
            size_t u=pending.top();pending.pop();comp.sessions.push_back(u);
            comp.conflictEdges+=adj[u].size();
            for(size_t v:adj[u])if(!seen[v]){seen[v]=true;pending.push(v);}
        }
        sort(comp.sessions.begin(),comp.sessions.end());
        comp.conflictEdges/=2;
        result.push_back(std::move(comp));
    }
    sort(result.begin(),result.end(),[](const Component& a,const Component& b){return a.sessions.front()<b.sessions.front();});
    return result;
}
void printIncidents(ostream& out,const vector<Session>& sessions,const vector<Clash>& clashes) {
    const auto incidents=consolidate(clashes,sessions);
    out<<"\n================ CONSOLIDATED CONFLICTS ================\n";
    out<<"Raw conflict reasons: "<<clashes.size()<<" | Distinct incidents: "<<incidents.size()<<"\n";
    for(size_t i=0;i<incidents.size();++i) {
        const auto& x=incidents[i];const auto& a=sessions[x.first];
        out<<"\n["<<setw(3)<<setfill('0')<<i+1<<setfill(' ')<<"] "<<a.getDay()<<" "<<a.getStartTime()<<"-"<<a.getEndTime()<<"\n";
        out<<"  Session A (#"<<x.first+1<<"): "<<a.getSubject()<<" | "<<a.getSection()<<" | "<<a.getFaculty()<<" | "<<a.getRoom()<<"\n";
        if(x.second!=NONE) {
            const auto& b=sessions[x.second];
            out<<"  Session B (#"<<x.second+1<<"): "<<b.getSubject()<<" | "<<b.getSection()<<" | "<<b.getFaculty()<<" | "<<b.getRoom()<<"\n";
        }
        for(const auto& reason:x.reasons) out<<"    - "<<ReportFormatter::typeName(reason.type)<<": "<<reason.description<<"\n";
    }
    if(incidents.empty())out<<"No conflicts detected.\n";
    out<<"========================================================\n";
}
void printGroups(ostream& out,const vector<Session>& sessions,const vector<Clash>& clashes) {
    const auto groups=connectedGroups(clashes,sessions.size());
    out<<"\n================= CONFLICT GRAPH (DFS) =================\n";
    out<<"Connected groups: "<<groups.size()<<" (single-session capacity issues excluded)\n";
    for(size_t i=0;i<groups.size();++i) {
        const auto& g=groups[i];
        out<<"\nGroup "<<i+1<<" | Sessions: "<<g.sessions.size()<<" | Unique pairwise edges: "<<g.conflictEdges<<"\n";
        for(size_t id:g.sessions){const auto& s=sessions[id];out<<"  #"<<id+1<<" "<<s.getDay()<<" "<<s.getStartTime()<<"-"<<s.getEndTime()<<" | "<<s.getSubject()<<" | "<<s.getSection()<<" | "<<s.getRoom()<<"\n";}
    }
    if(groups.empty())out<<"No connected conflict groups.\n";
    out<<"========================================================\n";
}
}
