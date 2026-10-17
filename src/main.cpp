#include "Input.h"
#include "sorting.h"
#include "ClashDetector.h"
#include "ReportFormatter.h"
#include "ConflictAnalysis.h"
#include <iostream>
#include <limits>
#include <string>
#include <vector>
using namespace std;
int main(int argc,char**argv){
 string path=argc>1?argv[1]:"data/timetable1.csv";
 vector<Session> sessions;vector<Clash> clashes;bool analyzed=false;
 auto load=[&](){sessions=loadSessionsFromCSV(path);clashes.clear();analyzed=false;if(!sessions.empty()){sortSessions(sessions);cout<<"Loaded "<<sessions.size()<<" sessions from "<<path<<"\n";}else cout<<"No sessions loaded. Check the file path and CSV format.\n";};
 load();
 while(true){
  cout<<"\n=====================================================\n";
  cout<<"         TIMETABLE CLASH DETECTION SYSTEM\n";
  cout<<"=====================================================\n";
  cout<<" File: "<<path<<" | Sessions: "<<sessions.size()<<"\n";
  cout<<"-----------------------------------------------------\n";
  cout<<" 1. Load another CSV\n 2. Run clash detection\n 3. View summary\n 4. View detailed clashes\n 5. Filter clashes by type\n 6. Export TXT and CSV reports\n 7. Display sorted timetable\n 8. Consolidated conflicts\n 9. Conflict graph (DFS)\n 0. Exit\n Choice: ";
  int choice;if(!(cin>>choice)){if(cin.eof())break;cin.clear();cin.ignore(numeric_limits<streamsize>::max(),'\n');cout<<"Invalid input. Enter a number.\n";continue;}
  cin.ignore(numeric_limits<streamsize>::max(),'\n');
  if(choice==0)break;
  if(choice==1){cout<<"CSV path: ";getline(cin,path);load();continue;}
  if(choice==2){if(sessions.empty()){cout<<"Load a timetable first.\n";continue;}clashes=ClashDetector::detectClashes(sessions);analyzed=true;ReportFormatter::summary(sessions,clashes);continue;}
  if(choice==8||choice==9){if(!analyzed){cout<<"Run detection (option 2) first.\n";continue;}if(choice==8)ConflictAnalysis::printIncidents(cout,sessions,clashes);else ConflictAnalysis::printGroups(cout,sessions,clashes);continue;}
  if(choice==7){for(size_t i=0;i<sessions.size();++i)cout<<i+1<<". "<<sessions[i]<<'\n';continue;}
  if(choice<3||choice>6){cout<<"Unknown option.\n";continue;}
  if(!analyzed){cout<<"Run detection (option 2) first.\n";continue;}
  if(choice==3)ReportFormatter::summary(sessions,clashes);
  else if(choice==4)ReportFormatter::details(sessions,clashes);
  else if(choice==5){cout<<"Filter: 0 Faculty | 1 Room | 2 Section | 3 Capacity | 4 Duplicate\nType: ";int type;if(cin>>type&&type>=0&&type<=4)ReportFormatter::details(sessions,clashes,type);else{cin.clear();cout<<"Invalid type.\n";}cin.ignore(numeric_limits<streamsize>::max(),'\n');}
  else if(choice==6){const bool txt=ReportFormatter::exportTXT("clash_report.txt",sessions,clashes);const bool csv=ReportFormatter::exportCSV("clash_report.csv",sessions,clashes);cout<<(txt&&csv?"Reports saved: clash_report.txt, clash_report.csv\n":"Could not save one or more reports.\n");}
 }
 cout<<"Goodbye!\n";
}
