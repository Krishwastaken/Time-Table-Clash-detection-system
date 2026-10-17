#include "Input.h"
#include <cctype>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;
namespace {
string trim(const string& s){size_t a=s.find_first_not_of(" \t\r\n");if(a==string::npos)return "";size_t b=s.find_last_not_of(" \t\r\n");return s.substr(a,b-a+1);}
bool parseCSV(const string& line,vector<string>& cells){
 cells.clear();string cell;bool quoted=false,closed=false;
 for(size_t i=0;i<line.size();++i){char c=line[i];
  if(quoted){if(c=='"'){if(i+1<line.size()&&line[i+1]=='"'){cell+='"';++i;}else{quoted=false;closed=true;}}else cell+=c;}
  else if(c==','){cells.push_back(trim(cell));cell.clear();closed=false;}
  else if(c=='"'){if(trim(cell).empty()&&!closed){cell.clear();quoted=true;}else return false;}
  else {if(closed&&!isspace((unsigned char)c))return false;cell+=c;}
 }
 if (quoted) return false;
 cells.push_back(trim(cell));
 return true;
}
int minutes(const string& t){if(t.size()!=5||t[2]!=':'||!isdigit((unsigned char)t[0])||!isdigit((unsigned char)t[1])||!isdigit((unsigned char)t[3])||!isdigit((unsigned char)t[4]))return -1;int h=(t[0]-'0')*10+t[1]-'0',m=(t[3]-'0')*10+t[4]-'0';return h<24&&m<60?h*60+m:-1;}
}
vector<Session> loadSessionsFromCSV(const string& filepath){
 vector<Session> sessions;ifstream file(filepath);if(!file){cerr<<"Error: Cannot open "<<filepath<<'\n';return sessions;}
 string line; if(!getline(file,line)){cerr<<"Error: Empty CSV\n";return sessions;}
 vector<string> headers;if(!parseCSV(line,headers)||headers.size()!=8){cerr<<"Error: Expected eight CSV columns.\n";return sessions;}
 const vector<string> expected={"subject","faculty","room","section","day","starttime","endtime","studentcount"};
 for(size_t i=0;i<8;++i){string h=trim(headers[i]);for(char& c:h)c=(char)tolower((unsigned char)c);if(h!=expected[i]){cerr<<"Error: Unexpected column "<<i+1<<" ("<<headers[i]<<")\n";return sessions;}}
 int lineNo=1,rejected=0;vector<string> row;
 while(getline(file,line)){++lineNo;if(trim(line).empty())continue;
  if(!parseCSV(line,row)||row.size()!=8){cerr<<"Warning line "<<lineNo<<": malformed CSV row\n";++rejected;continue;}
  bool missing=false;for(int i=0;i<7;++i)if(row[i].empty())missing=true;
  if(missing||minutes(row[5])<0||minutes(row[6])<0||minutes(row[5])>=minutes(row[6])){cerr<<"Warning line "<<lineNo<<": missing field or invalid time interval\n";++rejected;continue;}
  try{size_t used=0;int count=stoi(row[7],&used);if(used!=row[7].size()||count<0)throw invalid_argument("count");sessions.emplace_back(row[0],row[1],row[2],row[3],row[4],row[5],row[6],count);}
  catch(const exception&){cerr<<"Warning line "<<lineNo<<": invalid student count\n";++rejected;}
 }
 if(rejected)cerr<<"Input validation: "<<rejected<<" row(s) rejected.\n";
 return sessions;
}
