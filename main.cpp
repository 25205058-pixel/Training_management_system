#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <exception>
#include <ctime>

using namespace std;

// ============================================================
//  UTILITY NAMESPACE
// ============================================================
namespace Utils {
    template <typename T>
    void printVector(const vector<T>& vec, const string& emptyMsg) {
        if (vec.empty()) { cout << "   " << emptyMsg << "\n"; return; }
        for (const auto& item : vec) cout << "   > " << item << "\n";
    }

    string escape(string s)   { replace(s.begin(), s.end(), ' ', '_'); return s; }
    string unescape(string s) { replace(s.begin(), s.end(), '_', ' '); return s; }

    bool containsCI(const string& hay, const string& ndl) {
        string h=hay, n=ndl;
        transform(h.begin(),h.end(),h.begin(),::tolower);
        transform(n.begin(),n.end(),n.begin(),::tolower);
        return h.find(n)!=string::npos;
    }

    void printProgressBar(int value, int max, int width=28) {
        int f=(max>0)?(value*width/max):0;
        cout<<"["; for(int i=0;i<width;i++) cout<<(i<f?"#":".");  cout<<"] "<<value<<"/"<<max;
    }

    string now() {
        auto t=chrono::system_clock::to_time_t(chrono::system_clock::now());
        string s(ctime(&t));
        if(!s.empty()&&s.back()=='\n') s.pop_back();
        return s;
    }

    void writeSection(ofstream& f, const string& title) {
        f << "\n"
          << "################################################################################\n"
          << "#  " << left << setw(76) << title << " #\n"
          << "################################################################################\n";
    }
    void writeSubSection(ofstream& f, const string& title) {
        f << "\n"
          << "  +----------------------------------------------------------+\n"
          << "  |  " << left << setw(56) << title << "  |\n"
          << "  +----------------------------------------------------------+\n";
    }
    void writeDivider(ofstream& f, char ch='-', int w=70, int indent=4) {
        f << string(indent,' ') << string(w,ch) << "\n";
    }
}

// ============================================================
//  EXCEPTION
// ============================================================
class SecurityException : public exception {
    string msg;
public:
    explicit SecurityException(const string& m) : msg(m) {}
    const char* what() const noexcept override { return msg.c_str(); }
};

// ============================================================
//  DATA STRUCTURES
// ============================================================
namespace InstituteCore {

    struct PlacementRecord {
        string company, role, package, offerDate, joiningDate, status;
    };

    struct User {
        string id, password, name;
        vector<string> messages;
        virtual void printRole() const { cout << "[User: " << name << "]\n"; }
        virtual ~User() = default;
    };

    struct Trainee : public User {
        vector<string>          enrolledPrograms;
        vector<string>          pendingPrograms;
        map<string,string>      projectGrades;
        map<string,int>         attendedClasses;
        float                   feeBalance=0, totalPaid=0;
        bool                    placementEligible=false;
        string                  placementStatus;
        string                  preferredCompany, preferredRole, resume;
        int                     placementScore=0;
        vector<PlacementRecord> placementHistory;
        vector<string>          paymentHistory;

        void printRole() const override {
            string ps = placementStatus.empty() ? "Not Applied" : placementStatus;
            cout << "\n[TRAINEE | " << name << " | Placement: " << ps
                 << " | Dues: $" << fixed << setprecision(2) << feeBalance << "]\n";
        }
        Trainee& operator+=(float fee) { feeBalance+=fee; return *this; }
        Trainee& operator-=(float payment) {
            float a=min(payment,feeBalance); feeBalance-=a; totalPaid+=a;
            paymentHistory.push_back("$"+to_string((int)a)+"|"+Utils::now());
            return *this;
        }
    };

    struct Trainer : public User {
        string domain; vector<string> activeBatches; float salary=0;
        void printRole() const override {
            cout << "\n[TRAINER | " << name << " | " << domain
                 << " | Salary: $" << fixed << setprecision(2) << salary << "]\n";
        }
    };

    struct Manager : public User {
        string domain;
        void printRole() const override { cout << "\n[MANAGER | " << name << "]\n"; }
    };

    struct TrainingProgram {
        string id, name, domain, scheduleTime;
        int    durationWeeks=0, maxSeats=0, enrolledSeats=0;
        float  feeAmount=0;
        string assignedTrainerId;
        int    totalTopics=0, completedTopics=0, totalClassesHeld=0;
        vector<string> modules;
        string certExamQuestion, certExamAnswer;
        int    examDurationMins=0;
        int    placedCount=0; float avgPackage=0; string topRecruiter;

        static void printCourseHeader() {
            cout << left
                 << setw(8)<<"ID" << setw(38)<<"COURSE NAME" << setw(14)<<"DOMAIN"
                 << setw(20)<<"SCHEDULE" << setw(5)<<"WKS" << setw(9)<<"FEE" << "SEATS\n"
                 << string(104,'-') << "\n";
        }
        friend ostream& operator<<(ostream& os, const TrainingProgram& p) {
            string sn=p.name.length()>36?p.name.substr(0,33)+"...":p.name;
            string ss=p.scheduleTime.length()>18?p.scheduleTime.substr(0,15)+"...":p.scheduleTime;
            os << left << setw(8)<<p.id << setw(38)<<sn << setw(14)<<p.domain
               << setw(20)<<ss << setw(5)<<p.durationWeeks
               << "$" << setw(8)<<p.feeAmount << p.enrolledSeats<<"/"<<p.maxSeats;
            return os;
        }
    };

    struct Inquiry     { string id, name, contact, question, status; };
    struct Appointment { string id, name, purpose, date, status; };
}
using namespace InstituteCore;

// ============================================================
//  MAIN SYSTEM
// ============================================================
class TrainingInstitute {
private:
    unordered_map<string,Trainee>         trainees;
    unordered_map<string,Trainer>         trainers;
    unordered_map<string,Manager>         managers;
    unordered_map<string,TrainingProgram> programs;
    unordered_map<string,Inquiry>         inquiries;
    unordered_map<string,Appointment>     appointments;
    vector<string> placementPool, hiringPartners;
    string currentUser, currentRole;

    // ========== FILE INTEGRATION ==========

    void saveData() {
        ofstream db("institute_databasemanagement.txt");
        if (!db.is_open()) { cerr << "[ERROR] Cannot open database.\n"; return; }

        db << "################################################################################\n"
           << "#        MODERN TECH TRAINING INSTITUTE - DATABASE FILE                       #\n"
           << "#   Generated : " << left << setw(62) << Utils::now() << "#\n"
           << "################################################################################\n";

        // SECTION 1: USERS
        Utils::writeSection(db, "SECTION 1 : USERS");
        Utils::writeSubSection(db, "1.1  TRAINEES");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(18)<<"PASSWORD"
           << setw(25)<<"NAME" << setw(10)<<"BALANCE" << setw(10)<<"PAID"
           << setw(6) <<"ELIG" << setw(28)<<"PLACEMENT_STATUS" << "PREF_COMPANY\n"
           << "  " << string(130,'-') << "\n";
        for (const auto& it : trainees) {
            const Trainee& tr = it.second;
            db << "  TRAINEE      " << left << setw(12)<<tr.id << setw(18)<<tr.password
               << setw(25)<<Utils::escape(tr.name) << setw(10)<<fixed<<setprecision(2)<<tr.feeBalance
               << setw(10)<<tr.totalPaid << setw(6) <<tr.placementEligible
               << setw(28)<<(tr.placementStatus.empty()?"None":Utils::escape(tr.placementStatus))
               << (tr.preferredCompany.empty()?"None":Utils::escape(tr.preferredCompany)) << "\n";
        }

        Utils::writeSubSection(db, "1.2  TRAINERS");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(18)<<"PASSWORD"
           << setw(25)<<"NAME" << setw(20)<<"DOMAIN" << "SALARY\n"
           << "  " << string(95,'-') << "\n";
        for (const auto& it : trainers) {
            const Trainer& tr = it.second;
            db << "  TRAINER      " << left << setw(12)<<tr.id << setw(18)<<tr.password
               << setw(25)<<Utils::escape(tr.name) << setw(20)<<Utils::escape(tr.domain)
               << fixed<<setprecision(2)<<tr.salary << "\n";
        }

        Utils::writeSubSection(db, "1.3  MANAGERS");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(18)<<"PASSWORD"
           << setw(25)<<"NAME" << "DEPARTMENT\n"
           << "  " << string(80,'-') << "\n";
        for (const auto& it : managers) {
            const Manager& m = it.second;
            db << "  MANAGER      " << left << setw(12)<<m.id << setw(18)<<m.password
               << setw(25)<<Utils::escape(m.name) << Utils::escape(m.domain) << "\n";
        }

        // SECTION 2: PROGRAMS
        Utils::writeSection(db, "SECTION 2 : TRAINING PROGRAMS");
        db << "  " << left << setw(14)<<"# RECORD" << setw(10)<<"ID" << setw(33)<<"NAME"
           << setw(14)<<"DOMAIN" << setw(20)<<"SCHEDULE" << setw(5)<<"WKS"
           << setw(6) <<"MAX" << setw(6) <<"ENR" << setw(10)<<"FEE"
           << setw(12)<<"TRAINER" << setw(6) <<"TTOP" << setw(6) <<"CTOP"
           << setw(6) <<"CLS" << setw(6) <<"PLCD" << setw(8) <<"AVGPKG" << "TOP_RECRUITER\n"
           << "  " << string(162,'-') << "\n";
        for (const auto& it : programs) {
            const TrainingProgram& pg = it.second;
            db << "  PROGRAM      " << left << setw(10)<<pg.id << setw(33)<<Utils::escape(pg.name)
               << setw(14)<<Utils::escape(pg.domain) << setw(20)<<Utils::escape(pg.scheduleTime)
               << setw(5)<<pg.durationWeeks << setw(6)<<pg.maxSeats << setw(6)<<pg.enrolledSeats
               << setw(10)<<fixed<<setprecision(2)<<pg.feeAmount
               << setw(12)<<(pg.assignedTrainerId.empty()?"None":pg.assignedTrainerId)
               << setw(6)<<pg.totalTopics << setw(6)<<pg.completedTopics << setw(6)<<pg.totalClassesHeld
               << setw(6)<<pg.placedCount << setw(8)<<pg.avgPackage
               << (pg.topRecruiter.empty()?"None":Utils::escape(pg.topRecruiter)) << "\n"
               << "               EXAM_Q=" << (pg.certExamQuestion.empty()?"None":Utils::escape(pg.certExamQuestion))
               << "  EXAM_A=" << (pg.certExamAnswer.empty()?"None":Utils::escape(pg.certExamAnswer))
               << "  EXAM_MINS=" << pg.examDurationMins << "\n";
        }

        // SECTION 3: FRONT DESK
        Utils::writeSection(db, "SECTION 3 : FRONT DESK");
        Utils::writeSubSection(db, "3.1  INQUIRIES");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(22)<<"NAME"
           << setw(16)<<"CONTACT" << setw(12)<<"STATUS" << "QUESTION\n"
           << "  " << string(105,'-') << "\n";
        for (const auto& it : inquiries) {
            const Inquiry& i = it.second;
            db << "  INQUIRY      " << left << setw(12)<<i.id << setw(22)<<Utils::escape(i.name)
               << setw(16)<<i.contact << setw(12)<<Utils::escape(i.status)
               << Utils::escape(i.question) << "\n";
        }

        Utils::writeSubSection(db, "3.2  APPOINTMENTS");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(22)<<"NAME"
           << setw(25)<<"PURPOSE" << setw(22)<<"DATE" << "STATUS\n"
           << "  " << string(100,'-') << "\n";
        for (const auto& it : appointments) {
            const Appointment& a = it.second;
            db << "  APPOINT      " << left << setw(12)<<a.id << setw(22)<<Utils::escape(a.name)
               << setw(25)<<Utils::escape(a.purpose) << setw(22)<<Utils::escape(a.date)
               << Utils::escape(a.status) << "\n";
        }

        // SECTION 4: TRAINEE RECORDS
        Utils::writeSection(db, "SECTION 4 : TRAINEE RECORDS");
        for (const auto& it : trainees) {
            const Trainee& tr = it.second;
            Utils::writeSubSection(db, "Trainee: " + tr.id + "  (" + tr.name + ")");
            db << "    [Enrolled Programs]\n";
            if (tr.enrolledPrograms.empty()) db << "    (none)\n";
            for (const string& pid : tr.enrolledPrograms)
                db << "    ENROLL        " << tr.id << "  " << pid << "\n";
            db << "    [Pending Applications]\n";
            if (tr.pendingPrograms.empty()) db << "    (none)\n";
            for (const string& pid : tr.pendingPrograms)
                db << "    PENDING       " << tr.id << "  " << pid << "\n";
            db << "    [Project Grades]\n";
            if (tr.projectGrades.empty()) db << "    (none)\n";
            for (const auto& gradeIt : tr.projectGrades)
                db << "    GRADE         " << tr.id << "  " << left<<setw(10)<<gradeIt.first << "  " << gradeIt.second << "\n";
            db << "    [Attendance]\n";
            if (tr.attendedClasses.empty()) db << "    (none)\n";
            for (const auto& attIt : tr.attendedClasses)
                db << "    ATTEND        " << tr.id << "  " << left<<setw(10)<<attIt.first << "  " << attIt.second << "\n";
            db << "    [Payment History]\n";
            if (tr.paymentHistory.empty()) db << "    (none)\n";
            for (const string& rec : tr.paymentHistory) {
                string safe=rec; replace(safe.begin(),safe.end(),' ','_');
                db << "    PAYMENT       " << tr.id << "  " << safe << "\n";
            }
            Utils::writeDivider(db,'.',72,4);
        }

        // SECTION 5: PLACEMENT
        Utils::writeSection(db, "SECTION 5 : PLACEMENT DATA");
        Utils::writeSubSection(db, "5.1  PLACEMENT PROFILES");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"TRAINEE_ID"
           << setw(22)<<"PREF_ROLE" << setw(30)<<"RESUME" << setw(6)<<"SCORE\n"
           << "  " << string(90,'-') << "\n";
        for (const auto& it : trainees) {
            const Trainee& tr = it.second;
            db << "  PLAC_PROFILE  " << left << setw(12)<<tr.id
               << setw(22)<<(tr.preferredRole.empty()?"None":Utils::escape(tr.preferredRole))
               << setw(30)<<(tr.resume.empty()?"None":Utils::escape(tr.resume))
               << tr.placementScore << "\n";
        }

        Utils::writeSubSection(db, "5.2  PLACEMENT HISTORY");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"TRAINEE_ID"
           << setw(20)<<"COMPANY" << setw(20)<<"ROLE" << setw(12)<<"PACKAGE"
           << setw(14)<<"OFFER_DATE" << setw(14)<<"JOIN_DATE" << "STATUS\n"
           << "  " << string(120,'-') << "\n";
        for (const auto& it : trainees) {
            const Trainee& tr = it.second;
            for (const auto& pr : tr.placementHistory) {
                db << "  PLAC_REC      " << left << setw(12)<<tr.id
                   << setw(20)<<Utils::escape(pr.company) << setw(20)<<Utils::escape(pr.role)
                   << setw(12)<<Utils::escape(pr.package) << setw(14)<<Utils::escape(pr.offerDate)
                   << setw(14)<<Utils::escape(pr.joiningDate) << Utils::escape(pr.status) << "\n";
            }
        }

        Utils::writeSubSection(db, "5.3  ACTIVE PLACEMENT POOL");
        if (placementPool.empty()) db << "    (empty)\n";
        for (const string& pid : placementPool) db << "    POOL          " << pid << "\n";

        Utils::writeSubSection(db, "5.4  HIRING PARTNERS");
        for (const string& hp : hiringPartners) db << "    PARTNER       " << Utils::escape(hp) << "\n";

        // SECTION 6: PROGRAM MODULES
        Utils::writeSection(db, "SECTION 6 : PROGRAM MODULES");
        for (const auto& it : programs) {
            const TrainingProgram& pg = it.second;
            if (pg.modules.empty()) continue;
            db << "\n  [" << pg.id << "  -  " << pg.name << "]\n";
            for (const string& mod : pg.modules)
                db << "    MODULE        " << it.first << "  " << Utils::escape(mod) << "\n";
        }

        // SECTION 7: TRAINER BATCHES
        Utils::writeSection(db, "SECTION 7 : TRAINER BATCH ASSIGNMENTS");
        for (const auto& it : trainers) {
            const Trainer& tr = it.second;
            if (tr.activeBatches.empty()) continue;
            db << "\n  [" << tr.id << "  -  " << tr.name << "]\n";
            for (const string& bid : tr.activeBatches)
                db << "    BATCH         " << tr.id << "  " << bid << "\n";
        }

        // SECTION 8: MESSAGE QUEUES
        Utils::writeSection(db, "SECTION 8 : PENDING MESSAGE QUEUES");
        for (const auto& it : trainees)
            for (const string& msg : it.second.messages)
                db << "  MSG_TRAINEE   " << it.first << "  " << Utils::escape(msg) << "\n";
        for (const auto& it : trainers)
            for (const string& msg : it.second.messages)
                db << "  MSG_TRAINER   " << it.first << "  " << Utils::escape(msg) << "\n";
        for (const auto& it : managers)
            for (const string& msg : it.second.messages)
                db << "  MSG_MANAGER   " << it.first << "  " << Utils::escape(msg) << "\n";

        db << "\n################################################################################\n"
           << "#  END OF FILE                                                                 #\n"
           << "################################################################################\n";
    }

    void loadData() {
        ifstream db("institute_databasemanagement.txt");
        if (!db.is_open()) return;

        string token;
        while (db >> token) {
            if (token.empty() || token[0]=='#' || token[0]=='+' || token[0]=='|' ||
                token[0]=='-' || token[0]=='.' || token=="===" || token=="SECTION" ||
                token=="END" || token=="OF" || token=="FILE" || token=="(none)" || token=="(empty)") {
                db.ignore(1000,'\n'); continue;
            }
            string id, e1, e2;

            if (token=="TRAINEE") {
                string pw,n,ps,pref; float fee,paid; bool elig;
                db>>id>>pw>>n>>fee>>paid>>elig>>ps>>pref;
                Trainee tr; tr.id=id; tr.password=pw; tr.name=Utils::unescape(n);
                tr.feeBalance=fee; tr.totalPaid=paid; tr.placementEligible=elig;
                tr.placementStatus=(ps=="None"?"":Utils::unescape(ps));
                tr.preferredCompany=(pref=="None"?"":Utils::unescape(pref));
                trainees[id]=tr; db.ignore(1000,'\n');
            }
            else if (token=="TRAINER") {
                string pw,n,dom; float sal;
                db>>id>>pw>>n>>dom>>sal;
                Trainer tr; tr.id=id; tr.password=pw; tr.name=Utils::unescape(n);
                tr.domain=Utils::unescape(dom); tr.salary=sal;
                trainers[id]=tr; db.ignore(1000,'\n');
            }
            else if (token=="MANAGER") {
                string pw,n,dom;
                db>>id>>pw>>n>>dom;
                Manager m; m.id=id; m.password=pw; m.name=Utils::unescape(n);
                m.domain=Utils::unescape(dom);
                managers[id]=m; db.ignore(1000,'\n');
            }
            else if (token=="PROGRAM") {
                string n,dom,sched,trId,topRec;
                int dur,ms,es,tTop,cTop,tClass,plcd; float fee,avgPkg;
                db>>id>>n>>dom>>sched>>dur>>ms>>es>>fee>>trId>>tTop>>cTop>>tClass>>p
