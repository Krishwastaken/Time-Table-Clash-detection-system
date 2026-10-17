#include "ReportFormatter.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
using namespace std;
namespace {
constexpr size_t NONE = static_cast<size_t>(-1);
int orderDay(const string& d) { static const vector<string> days={"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"}; for(size_t i=0;i<days.size();++i) if(days[i]==d) return (int)i; return 7; }
vector<Clash> ordered(vector<Clash> clashes,const vector<Session>& sessions){
 sort(clashes.begin(),clashes.end(),[&](const Clash&a,const Clash&b){
  const Session& x=sessions[a.firstSession]; const Session& y=sessions[b.firstSession];
  return make_tuple(orderDay(x.getDay()),x.getStartTime(),(int)a.type,a.firstSession,a.secondSession)<make_tuple(orderDay(y.getDay()),y.getStartTime(),(int)b.type,b.firstSession,b.secondSession);
 }); return clashes;
}
string csvField(const string& s){ if(s.find_first_of(",\"\r\n")==string::npos) return s; string result="\""; for(char c:s){if(c=='\"') result+='\"';result+=c;}return result+"\"";}
void detailsTo(ostream& out,const vector<Session>& sessions,const vector<Clash>& clashes,int filter){
 auto sorted=ordered(clashes,sessions); size_t number=0;
 for(const auto& c:sorted){if(filter>=0 && (int)c.type!=filter) continue; ++number;
  const auto& a=sessions[c.firstSession];
  out<<"\n["<<setw(3)<<setfill('0')<<number<<setfill(' ')<<"] "<<ReportFormatter::typeName(c.type)<<" | "<<a.getDay()<<" "<<a.getStartTime()<<"-"<<a.getEndTime()<<"\n";
  out<<"  Reason    : "<<c.description<<"\n";
  out<<"  Session A : "<<a.getSubject()<<" | "<<a.getSection()<<" | "<<a.getFaculty()<<" | "<<a.getRoom()<<"\n";
  if(c.secondSession!=NONE && c.secondSession<sessions.size()){const auto& b=sessions[c.secondSession];out<<"  Session B : "<<b.getSubject()<<" | "<<b.getSection()<<" | "<<b.getFaculty()<<" | "<<b.getRoom()<<"\n";}
 }
 if(number==0) out<<"\nNo matching conflicts.\n";
}
}
namespace ReportFormatter {
const char* typeName(ClashType t){switch(t){case ClashType::Faculty:return "FACULTY";case ClashType::Room:return "ROOM";case ClashType::Section:return "SECTION";case ClashType::RoomCapacity:return "CAPACITY";case ClashType::DuplicateSession:return "DUPLICATE";}return "UNKNOWN";}
void summary(const vector<Session>& sessions,const vector<Clash>& clashes){
 array<int,5> counts{}; set<string> sections,faculty,rooms;for(const auto& s:sessions){sections.insert(s.getSection());faculty.insert(s.getFaculty());rooms.insert(s.getRoom());}
 for(const auto& c:clashes) counts.at((int)c.type)++;
 cout<<"\n============================================================\n";
 cout<<"                 TIMETABLE ANALYSIS SUMMARY\n";
 cout<<"============================================================\n";
 cout<<"  Sessions: "<<sessions.size()<<"   Sections: "<<sections.size()<<"   Faculty: "<<faculty.size()<<"   Rooms: "<<rooms.size()<<"\n";
 cout<<"  Total conflict records: "<<clashes.size()<<"\n";
 for(int i=0;i<5;++i) cout<<"  "<<left<<setw(14)<<typeName((ClashType)i)<<right<<counts[i]<<"\n";
 cout<<"============================================================\n";
}
void details(const vector<Session>& s,const vector<Clash>& c,int filter){cout<<"\n==================== DETAILED CLASHES ====================\n";detailsTo(cout,s,c,filter);cout<<"==========================================================\n";}
bool exportTXT(const string& path,const vector<Session>& s,const vector<Clash>& c){ofstream f(path);if(!f)return false;f<<"TIMETABLE CLASH REPORT\nSessions: "<<s.size()<<"\nConflicts: "<<c.size()<<"\n";detailsTo(f,s,c,-1);return (bool)f;}
bool exportCSV(const string& path,const vector<Session>& s,const vector<Clash>& c){ofstream f(path);if(!f)return false;f<<"type,day,startTime,endTime,reason,subjectA,facultyA,roomA,sectionA,subjectB,facultyB,roomB,sectionB\n";
 for(const auto& x:ordered(c,s)){const auto&a=s[x.firstSession];const Session* b=(x.secondSession!=NONE&&x.secondSession<s.size())?&s[x.secondSession]:nullptr;
 vector<string> fields={typeName(x.type),a.getDay(),a.getStartTime(),a.getEndTime(),x.description,a.getSubject(),a.getFaculty(),a.getRoom(),a.getSection(),b?b->getSubject():"",b?b->getFaculty():"",b?b->getRoom():"",b?b->getSection():""};
 for(size_t i=0;i<fields.size();++i){if(i)f<<',';f<<csvField(fields[i]);}f<<'\n';
 }return (bool)f;}
}
