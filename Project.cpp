/*
  CryptoShell v3.1 — CLI-Based Secure Message Encoder & Decoder
  Fix: cin buffer flushed before every getHiddenInput() call
  Compile: g++ -std=c++17 -o cryptoshell cryptoshell.cpp
  Run    : ./cryptoshell
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <limits>
#include <ctime>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstdint>
#include <cctype>

#ifdef _WIN32
  #include <windows.h>
  #include <conio.h>
  #define CLEAR "cls"
#else
  #include <termios.h>
  #include <unistd.h>
  #define CLEAR "clear"
#endif

/* ═══ ANSI PALETTE ═══════════════════════════════════════════════════════ */
#define RST       "\033[0m"
#define BOLD      "\033[1m"
#define DIM       "\033[2m"
#define BLACK     "\033[30m"
#define RED       "\033[91m"
#define GREEN     "\033[92m"
#define YELLOW    "\033[93m"
#define BLUE      "\033[94m"
#define MAGENTA   "\033[95m"
#define CYAN      "\033[96m"
#define WHITE     "\033[97m"
#define GRAY      "\033[90m"
#define ORANGE    "\033[38;5;214m"
#define LIME      "\033[38;5;118m"
#define TEAL      "\033[38;5;51m"
#define GOLD      "\033[38;5;220m"
#define PINK      "\033[38;5;213m"
#define INDIGO    "\033[38;5;105m"
#define BG_GREEN  "\033[42m"
#define BG_RED    "\033[41m"
#define BG_ORANGE "\033[48;5;130m"

/* ═══ CONFIG ═════════════════════════════════════════════════════════════ */
const std::string APP_VERSION  = "v3.1";
const std::string APP_SUBTITLE = "CLI-Based Secure Message Encoder & Decoder";
const std::string CIPHER_KEY   = "Xk9#Qm@Z7!pL";
const std::string DECRYPT_PASS = "2583";       // extra pin for decryption
const std::string USERS_FILE   = "cs_users.dat";
const int MAX_ATTEMPTS = 3;
const int BOX_WIDTH    = 64;
const int MAX_HISTORY  = 50;
const int PREVIEW_LEN  = 32;

/* ═══ DATA STRUCTURES ════════════════════════════════════════════════════ */
struct HistEntry { std::string timestamp, preview, cipher; };
struct Session {
    bool loggedIn = false;
    std::string username, loginTime;
    int encCount = 0, decCount = 0;
};
struct User { std::string username, passHash; };

static Session gSession;

/* ═══ UTILITIES ══════════════════════════════════════════════════════════ */
void sleepMs(int ms){
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
void cls(){ system(CLEAR); }

void typeWrite(const std::string& text, int delay = 18){
    for(char c : text){ std::cout << c << std::flush; sleepMs(delay); }
}

void spinner(const std::string& msg, int durationMs){
    const char* frames[] = {"| ","/ ","- ","\\ "};
    int cycles = durationMs / 80;
    for(int i = 0; i < cycles; ++i){
        std::cout << "\r  " << CYAN << frames[i % 4] << WHITE << msg << RST << std::flush;
        sleepMs(80);
    }
    std::cout << "\r" << std::string(msg.size() + 10, ' ') << "\r" << std::flush;
}

std::string getDateTime(){
    time_t now = time(nullptr); char buf[64];
    strftime(buf, sizeof(buf), "%d %b %Y  %H:%M:%S", localtime(&now));
    return std::string(buf);
}
std::string getTimestamp(){
    time_t now = time(nullptr); char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return std::string(buf);
}

// Count visible terminal characters (skip ANSI escapes & continuation bytes)
int visibleLen(const std::string& s){
    int len = 0; bool esc = false;
    for(unsigned char c : s){
        if(c == '\033'){ esc = true; continue; }
        if(esc){ if(std::isalpha(c)) esc = false; continue; }
        if((c & 0xC0) != 0x80) ++len;
    }
    return len;
}

/* ═══ BOX DRAWING ════════════════════════════════════════════════════════ */
void boxTop(const std::string& col = CYAN){
    std::cout << col << "  \xe2\x95\x94";
    for(int i = 0; i < BOX_WIDTH; i++) std::cout << "\xe2\x95\x90";
    std::cout << "\xe2\x95\x97" << RST << "\n";
}
void boxBot(const std::string& col = CYAN){
    std::cout << col << "  \xe2\x95\x9a";
    for(int i = 0; i < BOX_WIDTH; i++) std::cout << "\xe2\x95\x90";
    std::cout << "\xe2\x95\x9d" << RST << "\n";
}
void boxMid(const std::string& col = CYAN){
    std::cout << col << "  \xe2\x95\xa0";
    for(int i = 0; i < BOX_WIDTH; i++) std::cout << "\xe2\x95\x90";
    std::cout << "\xe2\x95\xa3" << RST << "\n";
}
void boxLine(const std::string& content, const std::string& borderCol = CYAN){
    int vl  = visibleLen(content);
    int pad = BOX_WIDTH - 1 - vl;
    if(pad < 0) pad = 0;
    std::cout << borderCol << "  \xe2\x95\x91 " << RST
              << content
              << std::string(pad, ' ')
              << borderCol << "\xe2\x95\x91" << RST << "\n";
}
void boxEmpty(const std::string& col = CYAN){ boxLine("", col); }

void printDivider(const std::string& col = GRAY){
    std::cout << col << "  ";
    for(int i = 0; i < BOX_WIDTH + 2; i++) std::cout << "\xe2\x94\x80";
    std::cout << RST << "\n";
}

/* ═══ PROGRESS BAR ═══════════════════════════════════════════════════════ */
void progressBar(const std::string& label, int durationMs){
    const int bw = 28;
    for(int i = 0; i <= bw; i++){
        int pct = (i * 100) / bw;
        std::cout << "\r  " << CYAN << label << " [" << LIME;
        for(int j = 0; j < i;  j++) std::cout << "\xe2\x96\x88";
        std::cout << GRAY;
        for(int j = i; j < bw; j++) std::cout << "\xe2\x96\x91";
        std::cout << CYAN << "] " << GOLD << BOLD
                  << std::setw(3) << pct << "%" << RST << std::flush;
        sleepMs(durationMs / bw);
    }
    std::cout << "\n";
}

/* ═══ STATUS BADGES ══════════════════════════════════════════════════════ */
void badgeOK  (const std::string& m){ std::cout << "\n  " << BG_GREEN  << BLACK << BOLD << " OK  " << RST << "  " << GREEN  << m << RST << "\n"; }
void badgeErr (const std::string& m){ std::cout << "\n  " << BG_RED    << WHITE << BOLD << " ERR " << RST << "  " << RED    << m << RST << "\n"; }
void badgeWarn(const std::string& m){ std::cout << "\n  " << BG_ORANGE << WHITE << BOLD << " WARN" << RST << "  " << ORANGE << m << RST << "\n"; }
void badgeInfo(const std::string& m){ std::cout <<         "  " << INDIGO << BOLD << " i " << RST << "  " << GRAY << m << RST << "\n"; }

/* ═══ FLUSH LEFTOVER NEWLINE (must call before every getHiddenInput) ═════
   std::cin >> x leaves '\n' sitting in the buffer.
   getHiddenInput uses raw read() so it picks up that '\n' and returns
   an empty string immediately — hence the "wrong password" bug.
   Calling flushCin() first drains the buffer.
   ════════════════════════════════════════════════════════════════════════ */
void flushCin(){
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

/* ═══ HIDDEN PASSWORD INPUT ══════════════════════════════════════════════ */
std::string getHiddenInput(){
    std::string pwd;
#ifdef _WIN32
    char ch;
    while((ch = _getch()) != '\r'){
        if(ch == '\b'){ if(!pwd.empty()){ pwd.pop_back(); std::cout << "\b \b"; } }
        else { pwd.push_back(ch); std::cout << CYAN << "*" << RST << std::flush; }
    }
    std::cout << "\n";
#else
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    char ch;
    while(read(STDIN_FILENO, &ch, 1) == 1 && ch != '\n'){
        if(ch == 127 || ch == '\b'){
            if(!pwd.empty()){ pwd.pop_back(); std::cout << "\b \b" << std::flush; }
        } else {
            pwd.push_back(ch);
            std::cout << CYAN << "*" << RST << std::flush;
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << "\n";
#endif
    return pwd;
}

void waitEnter(){
    std::cout << "\n" << GRAY << DIM
              << "  Press " << RST << CYAN << BOLD << "[ENTER]"
              << RST << GRAY << DIM << " to continue..." << RST;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cin.get();
}

/* ═══ CIPHER ENGINE (Caesar → XOR → Hex) ════════════════════════════════ */
static std::string xorLayer(const std::string& t, const std::string& k){
    std::string r = t;
    for(size_t i = 0; i < t.size(); ++i)
        r[i] = static_cast<char>(t[i] ^ k[i % k.size()]);
    return r;
}
static std::string caesarLayer(const std::string& t, int dir){
    std::string r = t;
    for(size_t i = 0; i < t.size(); ++i){
        int sh = (static_cast<int>(CIPHER_KEY[i % CIPHER_KEY.size()]) % 9) + 4;
        r[i] = static_cast<char>(t[i] + dir * sh);
    }
    return r;
}
static std::string toHex(const std::string& b){
    std::ostringstream ss;
    for(unsigned char c : b)
        ss << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    return ss.str();
}
static std::string fromHex(const std::string& h){
    if(h.size() % 2 != 0) return "";
    std::string b;
    for(size_t i = 0; i < h.size(); i += 2){
        try { b.push_back(static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16))); }
        catch(...){ return ""; }
    }
    return b;
}
std::string encryptMsg(const std::string& p){
    return toHex(xorLayer(caesarLayer(p, +1), CIPHER_KEY));
}
std::string decryptMsg(const std::string& c){
    std::string s = fromHex(c);
    if(s.empty()) return "__ERR__";
    return caesarLayer(xorLayer(s, CIPHER_KEY), -1);
}

/* ═══ PASSWORD HASHING (djb2 double-pass → 16-char hex) ═════════════════ */
std::string hashPass(const std::string& pwd){
    uint64_t h = 5381;
    for(unsigned char c : pwd)         h = ((h << 5) + h) ^ c;
    for(int i = (int)pwd.size()-1; i >= 0; --i) h = ((h << 3) + h) ^ static_cast<unsigned char>(pwd[i]);
    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << h;
    return ss.str();
}

/* ═══ USER / HISTORY FILE I/O ════════════════════════════════════════════ */
std::string histFile(const std::string& u){ return "cs_hist_" + u + ".dat"; }

std::vector<User> loadUsers(){
    std::vector<User> v; std::ifstream f(USERS_FILE); std::string line;
    while(std::getline(f, line)){
        auto p = line.find('|'); if(p == std::string::npos) continue;
        v.push_back({line.substr(0, p), line.substr(p + 1)});
    }
    return v;
}
void saveUsers(const std::vector<User>& v){
    std::ofstream f(USERS_FILE, std::ios::trunc);
    for(auto& u : v) f << u.username << "|" << u.passHash << "\n";
}
bool userExists(const std::string& u){
    for(auto& x : loadUsers()) if(x.username == u) return true;
    return false;
}
bool registerUser(const std::string& u, const std::string& p){
    if(userExists(u)) return false;
    auto v = loadUsers();
    v.push_back({u, hashPass(p)});
    saveUsers(v); return true;
}
bool loginUser(const std::string& u, const std::string& p){
    std::string h = hashPass(p);
    for(auto& x : loadUsers()){
        if(x.username == u && x.passHash == h) return true;
    }
    return false;
}

std::vector<HistEntry> loadHistory(const std::string& u){
    std::vector<HistEntry> v; std::ifstream f(histFile(u)); std::string line;
    while(std::getline(f, line)){
        auto p1 = line.find('|'); if(p1 == std::string::npos) continue;
        auto p2 = line.find('|', p1 + 1); if(p2 == std::string::npos) continue;
        v.push_back({line.substr(0, p1), line.substr(p1+1, p2-p1-1), line.substr(p2+1)});
    }
    return v;
}
void saveHistory(const std::string& u, const std::vector<HistEntry>& v){
    std::ofstream f(histFile(u), std::ios::trunc);
    for(auto& e : v) f << e.timestamp << "|" << e.preview << "|" << e.cipher << "\n";
}
void appendHistory(const std::string& u, const std::string& plain, const std::string& cipher){
    auto hist = loadHistory(u);
    std::string raw = plain.substr(0, PREVIEW_LEN);
    for(char& c : raw) if(c == '|' || c == '\n') c = ' ';
    hist.push_back({getTimestamp(), raw + (plain.size() > (size_t)PREVIEW_LEN ? "..." : ""), cipher});
    if((int)hist.size() > MAX_HISTORY)
        hist.erase(hist.begin(), hist.begin() + (int)hist.size() - MAX_HISTORY);
    saveHistory(u, hist);
}

/* ═══ BANNER ══════════════════════════════════════════════════════════════ */
void printBanner(bool compact = false){
    if(!compact){
        std::cout << "\n" << TEAL << BOLD;
        std::cout << "    ===================================================================\n";
        std::cout << "     ██████╗██████╗ ██╗   ██╗██████╗ ████████╗ ██████╗\n";
        std::cout << "    ██╔════╝██╔══██╗╚██╗ ██╔╝██╔══██╗╚══██╔══╝██╔═══██╗\n";
        std::cout << "    ██║     ██████╔╝ ╚████╔╝ ██████╔╝   ██║   ██║   ██║\n";
        std::cout << "    ██║     ██╔══██╗  ╚██╔╝  ██╔═══╝    ██║   ██║   ██║\n";
        std::cout << "    ╚██████╗██║  ██║   ██║   ██║        ██║   ╚██████╔╝\n";
        std::cout << "     ╚═════╝╚═╝  ╚═╝   ╚═╝   ╚═╝        ╚═╝    ╚═════╝\n";
        std::cout << RST << CYAN << BOLD;
        std::cout << "     ███████╗██╗  ██╗███████╗██╗     ██╗\n";
        std::cout << "     ██╔════╝██║  ██║██╔════╝██║     ██║\n";
        std::cout << "     ███████╗███████║█████╗  ██║     ██║\n";
        std::cout << "     ╚════██║██╔══██║██╔══╝  ██║     ██║\n";
        std::cout << "     ███████║██║  ██║███████╗███████╗███████╗\n";
        std::cout << "     ╚══════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝\n";
        std::cout << "    ===================================================================\n";
        std::cout << RST;
    }
    std::cout << "\n";
    boxTop(INDIGO);
    std::string t1 = std::string(GOLD)+BOLD+"  CryptoShell "+APP_VERSION+"  "+RST+DIM+GRAY+APP_SUBTITLE+RST;
    boxLine(t1, INDIGO);
    if(gSession.loggedIn){
        std::string t2 = std::string(GRAY)+"  Logged in as: "+RST+LIME+BOLD+gSession.username+RST+
                         GRAY+"   |   "+gSession.loginTime+RST;
        boxLine(t2, INDIGO);
    } else {
        boxLine(std::string(GRAY)+"  "+getDateTime()+"   |   Not logged in"+RST, INDIGO);
    }
    boxBot(INDIGO);
    std::cout << "\n";
}

/* ═══ REUSABLE SECTION HEADER ════════════════════════════════════════════ */
void sectionHeader(const std::string& icon, const std::string& title, const std::string& col){
    std::cout << "\n"; boxTop(col);
    boxLine(std::string(BOLD)+col+"  "+icon+"  "+title+RST, col);
    boxBot(col); std::cout << "\n";
}

/* ═══ OUTPUT BOX ═════════════════════════════════════════════════════════ */
void displayOutputBox(const std::string& label, const std::string& data,
                      const std::string& dataCol, const std::string& boxCol){
    const int wrap = BOX_WIDTH - 4;
    std::cout << "\n"; boxTop(boxCol);
    boxLine(std::string(BOLD)+boxCol+"  "+label+RST, boxCol);
    boxMid(boxCol); boxEmpty(boxCol);
    for(size_t pos = 0; pos < data.size(); pos += wrap)
        boxLine(dataCol+"  "+data.substr(pos, wrap)+RST, boxCol);
    boxEmpty(boxCol); boxBot(boxCol);
}

/* ═══ INTRO ANIMATION ════════════════════════════════════════════════════ */
void introAnimation(){
    cls(); std::cout << "\n\n";
    std::cout << CYAN << BOLD;
    typeWrite("        Booting CryptoShell " + APP_VERSION + "...\n", 30);
    std::cout << RST << "\n";
    progressBar("Loading cipher engine   ", 600);
    progressBar("Loading user database   ", 400);
    progressBar("Loading auth module     ", 500);
    progressBar("Securing environment    ", 400);
    std::cout << "\n";
    badgeOK("All systems ready.");
    sleepMs(500);
}

/* ═══ REGISTER ═══════════════════════════════════════════════════════════ */
bool isValidUsername(const std::string& u){
    if(u.size() < 3 || u.size() > 20) return false;
    for(char c : u) if(!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    return true;
}

void doRegister(){
    cls(); printBanner(true);
    sectionHeader("+", "CREATE ACCOUNT", GREEN);

    // Username — cin.ignore already done by authScreen before we got here
    std::cout << "\n  " << WHITE << BOLD << "  Username" << RST
              << GRAY << "  (3-20 chars, a-z / 0-9 / _)\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST << WHITE;
    std::string uname;
    std::getline(std::cin, uname);
    uname.erase(0, uname.find_first_not_of(" \t"));
    if(!uname.empty() && uname.find_last_not_of(" \t") != std::string::npos)
        uname.erase(uname.find_last_not_of(" \t") + 1);

    if(!isValidUsername(uname)){ badgeErr("Invalid username. Use 3-20 alphanumeric chars / '_'."); waitEnter(); return; }
    if(userExists(uname))       { badgeErr("Username '" + uname + "' is already taken.");            waitEnter(); return; }

    // Password (no cin.ignore needed — getline consumed the previous '\n')
    std::cout << "\n  " << WHITE << BOLD << "  Password" << RST
              << GRAY << "  (min 4 chars)\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST;
    std::string pwd = getHiddenInput();
    if(pwd.size() < 4){ badgeErr("Password too short (min 4 characters)."); waitEnter(); return; }

    std::cout << "\n  " << WHITE << BOLD << "  Confirm Password\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST;
    std::string pwd2 = getHiddenInput();
    if(pwd != pwd2){ badgeErr("Passwords do not match."); waitEnter(); return; }

    spinner("Creating account", 700);
    registerUser(uname, pwd);
    badgeOK("Account created! Welcome, " + uname + ".");
    sleepMs(600);

    gSession.loggedIn=true; gSession.username=uname; gSession.loginTime=getDateTime(); gSession.encCount=0; gSession.decCount=0;
}

/* ═══ LOGIN ══════════════════════════════════════════════════════════════ */
bool doLogin(){
    cls(); printBanner(true);
    sectionHeader("->", "LOGIN", CYAN);

    // Username — cin.ignore already consumed by authScreen
    std::cout << "\n  " << WHITE << BOLD << "  Username\n" << RST;
    std::cout << "  " << CYAN << "  -> " << RST << WHITE;
    std::string uname;
    std::getline(std::cin, uname);
    uname.erase(0, uname.find_first_not_of(" \t"));
    if(!uname.empty() && uname.find_last_not_of(" \t") != std::string::npos)
        uname.erase(uname.find_last_not_of(" \t") + 1);

    if(uname.empty()){ badgeErr("Username cannot be empty."); waitEnter(); return false; }

    // Password attempts — getHiddenInput safe (getline consumed '\n')
    int attempts = 0; bool auth = false;
    while(attempts < MAX_ATTEMPTS){
        int left = MAX_ATTEMPTS - attempts;
        std::cout << "\n  " << WHITE << BOLD << "  Password"
                  << RST << GRAY << "  (" << left << " attempt"
                  << (left > 1 ? "s" : "") << " left)\n" << RST;
        std::cout << "  " << CYAN << "  -> " << RST;
        std::string pwd = getHiddenInput();   // safe: getline consumed '\n'
        if(loginUser(uname, pwd)){ auth = true; break; }
        ++attempts;
        if(attempts < MAX_ATTEMPTS) badgeErr("Wrong password. Try again.");
        std::cout << "\n";
    }
    if(!auth){
        std::cout << "\n"; boxTop(RED);
        boxLine(std::string(RED)+BOLD+"  ACCESS DENIED — Too many failed attempts."+RST, RED);
        boxBot(RED); waitEnter(); return false;
    }
    spinner("Authenticating", 700);
    gSession.loggedIn=true; gSession.username=uname; gSession.loginTime=getDateTime(); gSession.encCount=0; gSession.decCount=0;
    badgeOK("Welcome back, " + uname + "!");
    sleepMs(600); return true;
}

/* ═══ AUTH SCREEN ════════════════════════════════════════════════════════ */
void authScreen(){
    while(true){
        cls(); printBanner(false);
        boxTop(CYAN); boxEmpty(CYAN);
        boxLine(std::string(BOLD)+CYAN+"    WELCOME TO CRYPTOSHELL"+RST, CYAN);
        boxEmpty(CYAN); boxMid(CYAN); boxEmpty(CYAN);
        boxLine(std::string(GREEN) +BOLD+"  [ 1 ]"+RST+"  "+LIME+"Login to your account"+RST, CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(YELLOW)+BOLD+"  [ 2 ]"+RST+"  "+GOLD+"Create a new account"+RST, CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(RED)   +BOLD+"  [ 0 ]"+RST+"  "+RED +"Exit"+RST, CYAN);
        boxEmpty(CYAN); boxBot(CYAN);
        std::cout << "\n  " << CYAN << "  Choice -> " << RST << WHITE;

        std::string inp; std::cin >> inp;
        int ch = -1; try{ ch = std::stoi(inp); }catch(...){}

        // IMPORTANT: flush '\n' left by cin >> before getline in doLogin/doRegister
        flushCin();

        if     (ch == 1){ if(doLogin())    return; }
        else if(ch == 2){ doRegister();    if(gSession.loggedIn) return; }
        else if(ch == 0){ cls(); std::cout << "\n\n  " << CYAN << BOLD << "  Goodbye!\n\n" << RST; exit(0); }
        else             { badgeErr("Enter 0, 1 or 2."); sleepMs(700); }
    }
}

/* ═══ DASHBOARD ══════════════════════════════════════════════════════════ */
void showDashboard(){
    cls(); printBanner(true);
    auto hist  = loadHistory(gSession.username);
    int  total = (int)hist.size();

    boxTop(TEAL);
    boxLine(std::string(BOLD)+TEAL+"  DASHBOARD"+RST+GRAY+"   |   "+LIME+gSession.username+RST, TEAL);
    boxMid(TEAL); boxEmpty(TEAL);
    boxLine(std::string(GOLD)+BOLD+"  Total Messages   "+RST+CYAN+std::to_string(total)+" encrypted"+RST, TEAL);
    boxLine(std::string(GOLD)+BOLD+"  This Session     "+RST+
            GREEN+std::to_string(gSession.encCount)+" enc"+RST+
            GRAY+"  /  "+RST+
            YELLOW+std::to_string(gSession.decCount)+" dec"+RST, TEAL);
    boxLine(std::string(GOLD)+BOLD+"  Logged In At     "+RST+GRAY+gSession.loginTime+RST, TEAL);
    boxEmpty(TEAL);

    if(total > 0){
        boxMid(TEAL);
        boxLine(std::string(BOLD)+WHITE+"  Recent History — last 5"+RST, TEAL);
        boxMid(TEAL); boxEmpty(TEAL);
        int start = std::max(0, total - 5);
        for(int i = total - 1; i >= start; --i){
            auto& e = hist[i];
            std::ostringstream hdr;
            hdr << "  " << GOLD << BOLD << "#"
                << std::setw(3) << std::setfill(' ') << (i+1) << RST
                << "  " << GRAY << e.timestamp << RST;
            boxLine(hdr.str(), TEAL);
            boxLine(std::string("         ") + CYAN + e.preview + RST, TEAL);
            std::string cs = e.cipher.substr(0, 42) + (e.cipher.size() > 42 ? "..." : "");
            boxLine(std::string("         ") + GOLD + DIM + cs + RST, TEAL);
            if(i > start)
                boxLine(std::string(GRAY)+"         "+std::string(50,'-')+RST, TEAL);
        }
        boxEmpty(TEAL);
    } else {
        boxMid(TEAL);
        boxLine(std::string(GRAY)+"  No history yet. Start encrypting!"+RST, TEAL);
        boxEmpty(TEAL);
    }
    boxBot(TEAL);
    std::cout << "\n";
    badgeInfo("Press ENTER to go to main menu.");
    waitEnter();
}

/* ═══ HISTORY VIEWER ═════════════════════════════════════════════════════ */
void sectionHistory(){
    auto hist  = loadHistory(gSession.username);
    int  total = (int)hist.size();
    if(total == 0){
        cls(); printBanner(true);
        sectionHeader("*", "ENCRYPTION HISTORY", MAGENTA);
        badgeWarn("No history found. Encrypt a message first!");
        waitEnter(); return;
    }
    int page = 0, pageSize = 5;
    int totalPages = (total + pageSize - 1) / pageSize;

    while(true){
        cls(); printBanner(true);
        sectionHeader("*", "ENCRYPTION HISTORY", MAGENTA);
        std::cout << GRAY << "  Page " << (page+1) << " / " << totalPages
                  << "   |   " << total << " records" << RST << "\n\n";
        boxTop(MAGENTA);
        boxLine(std::string(BOLD)+MAGENTA+"  No.   Timestamp              Preview"+RST, MAGENTA);
        boxMid(MAGENTA);

        int startIdx = total - 1 - page * pageSize;
        int endIdx   = std::max(startIdx - pageSize + 1, 0);
        for(int i = startIdx; i >= endIdx; --i){
            auto& e = hist[i];
            std::ostringstream hdr;
            hdr << "  " << GOLD << BOLD
                << std::setw(3) << std::setfill(' ') << (i+1) << RST
                << "  " << GRAY << e.timestamp << RST;
            boxLine(hdr.str(), MAGENTA);
            boxLine(std::string("         ")+CYAN+"MSG: "+e.preview+RST, MAGENTA);
            std::string cs = e.cipher.substr(0, 46) + (e.cipher.size() > 46 ? "..." : "");
            boxLine(std::string("         ")+GOLD+DIM+"HEX: "+cs+RST, MAGENTA);
            if(i > endIdx)
                boxLine(std::string(GRAY)+"         "+std::string(54,'-')+RST, MAGENTA);
        }
        boxEmpty(MAGENTA); boxBot(MAGENTA);

        std::cout << "\n  " << GRAY
                  << "  [N]ext  [P]rev  [V] View full cipher  [0] Back\n" << RST
                  << "  " << MAGENTA << "  -> " << RST << WHITE;
        std::string inp; std::cin >> inp;
        for(char& c : inp) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        if(inp == "N"){ if(page < totalPages-1) page++; else badgeWarn("Last page."); }
        else if(inp == "P"){ if(page > 0) page--; else badgeWarn("First page."); }
        else if(inp == "V"){
            std::cout << "\n  " << CYAN << "  Entry # to view -> " << RST;
            std::string ns; std::cin >> ns;
            int n = -1; try{ n = std::stoi(ns) - 1; }catch(...){}
            if(n >= 0 && n < total){
                displayOutputBox("Full Cipher — Entry #"+std::to_string(n+1),
                                 hist[n].cipher, GOLD, MAGENTA);
                badgeInfo("Time   : " + hist[n].timestamp);
                badgeInfo("Preview: " + hist[n].preview);
                waitEnter();
            } else { badgeErr("Invalid entry number."); sleepMs(700); }
        }
        else if(inp == "0") return;
        else { badgeErr("Unknown command."); sleepMs(500); }
    }
}

/* ═══ ENCRYPT ════════════════════════════════════════════════════════════ */
void sectionEncrypt(){
    cls(); printBanner(true);
    sectionHeader("LOCK", "ENCRYPT MESSAGE", GREEN);
    std::cout << GRAY << "  Algorithm: " << RST << CYAN
              << "Caesar + XOR + Hex  |  Key: 12-byte rotating\n\n" << RST;
    printDivider();

    std::cout << "\n  " << WHITE << BOLD << "  Enter your message:\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST << WHITE;
    flushCin();                           // drain '\n' left by main menu cin >>
    std::string msg; std::getline(std::cin, msg);
    if(msg.empty()){ badgeErr("Message cannot be empty!"); waitEnter(); return; }

    std::cout << "\n";
    progressBar("Encrypting message     ", 800);
    std::string cipher = encryptMsg(msg);
    appendHistory(gSession.username, msg, cipher);
    gSession.encCount++;

    badgeOK("Encrypted & saved to history!");
    displayOutputBox("ENCRYPTED OUTPUT  (hex — safe to share)", cipher, GOLD, GREEN);
    std::cout << "\n";
    badgeInfo("Original : " + std::to_string(msg.size())    + " chars");
    badgeInfo("Cipher   : " + std::to_string(cipher.size()) + " chars (hex)");
    badgeInfo("Saved as entry #" + std::to_string((int)loadHistory(gSession.username).size()));
    waitEnter();
}

/* ═══ DECRYPT ════════════════════════════════════════════════════════════
   BUG FIX: flushCin() is called at entry so that the '\n' left by
   "std::cin >> inp" in the main loop is consumed BEFORE getHiddenInput()
   reads from the raw file descriptor. Without this flush, getHiddenInput
   would instantly see '\n' and return an empty string, making every first
   attempt appear as a wrong password.
   ════════════════════════════════════════════════════════════════════════ */
void sectionDecrypt(){
    cls(); printBanner(true);
    sectionHeader("LOCK", "DECRYPT MESSAGE", YELLOW);

    boxTop(RED);
    boxLine(std::string(RED)+BOLD+"  RESTRICTED — Extra Password Required"+RST, RED);
    boxLine(std::string(GRAY)+"  Pin: separate from your login password. Max "+std::to_string(MAX_ATTEMPTS)+" attempts.", RED);
    boxBot(RED);
    std::cout << "\n";

    // ── KEY FIX: flush '\n' from cin buffer BEFORE first getHiddenInput ──
    flushCin();

    int attempts = 0; bool auth = false;
    while(attempts < MAX_ATTEMPTS){
        int left = MAX_ATTEMPTS - attempts;
        std::cout << "  " << YELLOW << BOLD << "  Decrypt PIN"
                  << RST << GRAY << "  (" << left << " attempt"
                  << (left > 1 ? "s" : "") << " left)\n" << RST;
        std::cout << "  " << YELLOW << "  -> " << RST;

        std::string pwd = getHiddenInput();   // ✓ buffer clean, reads actual input

        if(pwd == DECRYPT_PASS){ auth = true; break; }
        ++attempts;
        if(attempts < MAX_ATTEMPTS) badgeErr("Wrong PIN. Try again.");
        std::cout << "\n";
    }

    if(!auth){
        std::cout << "\n"; boxTop(RED);
        boxLine(std::string(RED)+BOLD+"  ACCESS DENIED"+RST, RED);
        boxBot(RED); waitEnter(); return;
    }

    spinner("Verifying", 600);
    badgeOK("Access granted.");
    printDivider();

    std::cout << "\n  " << WHITE << BOLD << "  Paste encrypted hex string:\n" << RST;
    std::cout << "  " << CYAN << "  -> " << RST << WHITE;
    // getHiddenInput already consumed its '\n'; buffer is clean for getline
    std::string cipher; std::getline(std::cin, cipher);

    std::string cleaned;
    for(char c : cipher)
        if(!std::isspace(static_cast<unsigned char>(c))) cleaned.push_back(c);

    if(cleaned.empty()){ badgeErr("Nothing entered!"); waitEnter(); return; }

    std::cout << "\n";
    progressBar("Decrypting message     ", 800);
    std::string plain = decryptMsg(cleaned);
    if(plain == "__ERR__"){ badgeErr("Invalid ciphertext — corrupted or wrong input."); waitEnter(); return; }

    gSession.decCount++;
    badgeOK("Decryption successful!");
    displayOutputBox("DECRYPTED MESSAGE", plain, TEAL, YELLOW);
    std::cout << "\n";
    badgeInfo("Cipher : " + std::to_string(cleaned.size()) + " hex chars");
    badgeInfo("Plain  : " + std::to_string(plain.size())   + " chars");
    waitEnter();
}

/* ═══ PROFILE ════════════════════════════════════════════════════════════ */
void sectionProfile(){
    cls(); printBanner(true);
    sectionHeader("@", "MY PROFILE", INDIGO);
    auto hist  = loadHistory(gSession.username);
    int  total = (int)hist.size();

    boxTop(INDIGO); boxEmpty(INDIGO);
    boxLine(std::string(GOLD)+BOLD+"  Username        "+RST+WHITE+gSession.username+RST, INDIGO);
    boxLine(std::string(GOLD)+BOLD+"  Logged In       "+RST+GRAY+gSession.loginTime+RST, INDIGO);
    boxLine(std::string(GOLD)+BOLD+"  Total Encrypted "+RST+CYAN+std::to_string(total)+" messages"+RST, INDIGO);
    boxLine(std::string(GOLD)+BOLD+"  This Session    "+RST+
            GREEN+std::to_string(gSession.encCount)+" enc"+RST+
            GRAY+" / "+RST+
            YELLOW+std::to_string(gSession.decCount)+" dec"+RST, INDIGO);
    boxEmpty(INDIGO);
    if(total > 0){
        auto& last = hist.back();
        boxMid(INDIGO);
        boxLine(std::string(BOLD)+WHITE+"  Last Encrypted Message"+RST, INDIGO);
        boxMid(INDIGO); boxEmpty(INDIGO);
        boxLine(std::string(GRAY)+"  Time    : "+RST+last.timestamp, INDIGO);
        boxLine(std::string(GRAY)+"  Preview : "+RST+CYAN+last.preview+RST, INDIGO);
        boxEmpty(INDIGO);
    }
    boxBot(INDIGO);

    std::cout << "\n  " << GRAY << "  [C] Change Password    [0] Back\n" << RST
              << "  " << INDIGO << "  -> " << RST << WHITE;
    std::string inp; std::cin >> inp;
    for(char& c : inp) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if(inp == "C"){
        // KEY FIX: flush '\n' before first getHiddenInput in this sub-menu
        flushCin();

        std::cout << "\n  " << WHITE << BOLD << "  Current Password:\n" << RST;
        std::cout << "  " << INDIGO << "  -> " << RST;
        std::string cur = getHiddenInput();   // ✓ safe after flushCin
        if(!loginUser(gSession.username, cur)){ badgeErr("Wrong current password."); waitEnter(); return; }

        std::cout << "\n  " << WHITE << BOLD << "  New Password (min 4 chars):\n" << RST;
        std::cout << "  " << INDIGO << "  -> " << RST;
        std::string np = getHiddenInput();    // ✓ safe: prev getHiddenInput consumed '\n'
        if(np.size() < 4){ badgeErr("Too short."); waitEnter(); return; }

        std::cout << "\n  " << WHITE << BOLD << "  Confirm New Password:\n" << RST;
        std::cout << "  " << INDIGO << "  -> " << RST;
        std::string np2 = getHiddenInput();
        if(np != np2){ badgeErr("Passwords don't match."); waitEnter(); return; }

        auto users = loadUsers();
        for(auto& u : users)
            if(u.username == gSession.username){ u.passHash = hashPass(np); break; }
        saveUsers(users);
        badgeOK("Password updated successfully!");
        waitEnter();
    }
}

/* ═══ ABOUT ══════════════════════════════════════════════════════════════ */
void sectionAbout(){
    cls(); printBanner(true);
    sectionHeader("?", "ABOUT CRYPTOSHELL", MAGENTA);

    auto rowFn = [](const std::string& k, const std::string& v){
        const int W = 20;
        std::string padded = k;
        int vl = 0; for(char c : k) if((c & 0xC0) != 0x80) ++vl;
        if(vl < W) padded += std::string(W - vl, ' ');
        boxLine(std::string(GOLD)+BOLD+padded+RST+GRAY+v+RST, MAGENTA);
    };

    boxTop(MAGENTA); boxEmpty(MAGENTA);
    rowFn("  App         :", "CryptoShell " + APP_VERSION);
    rowFn("  Type        :", APP_SUBTITLE);
    boxEmpty(MAGENTA); boxMid(MAGENTA); boxEmpty(MAGENTA);
    rowFn("  Layer 1     :", "Variable Caesar Shift (key-driven per char)");
    rowFn("  Layer 2     :", "XOR with 12-byte rotating secret key");
    rowFn("  Layer 3     :", "Hexadecimal encoding (printable output)");
    boxEmpty(MAGENTA); boxMid(MAGENTA); boxEmpty(MAGENTA);
    rowFn("  Accounts    :", "Per-user login (passwords are hashed)");
    rowFn("  Decrypt PIN :", "Extra gate — separate from login password");
    rowFn("  History     :", "Persistent per-user (up to 50 entries)");
    rowFn("  Files       :", "cs_users.dat / cs_hist_<username>.dat");
    boxEmpty(MAGENTA); boxMid(MAGENTA); boxEmpty(MAGENTA);
    boxLine(std::string(WHITE)+BOLD+"  Encryption Pipeline:"+RST, MAGENTA); boxEmpty(MAGENTA);
    boxLine(std::string(CYAN)+"  PLAIN  ->  [Caesar+]  ->  [XOR]  ->  [toHex]  ->  CIPHER"+RST, MAGENTA);
    boxLine(std::string(CYAN)+"  CIPHER ->  [fromHex]  ->  [XOR]  ->  [Caesar-]  ->  PLAIN"+RST, MAGENTA);
    boxEmpty(MAGENTA); boxBot(MAGENTA);
    waitEnter();
}

/* ═══ MAIN MENU ══════════════════════════════════════════════════════════ */
void printMainMenu(){
    boxTop(CYAN);
    boxLine(std::string(BOLD)+CYAN+"   MAIN MENU"+GRAY+"   |   "+LIME+gSession.username+RST, CYAN);
    boxMid(CYAN); boxEmpty(CYAN);
    boxLine(std::string(GREEN)  +BOLD+"  [ 1 ]"+RST+"  "+LIME  +"Encrypt a Message"  +RST+GRAY+"    ->  Encode & save to history",  CYAN);
    boxEmpty(CYAN);
    boxLine(std::string(YELLOW) +BOLD+"  [ 2 ]"+RST+"  "+GOLD  +"Decrypt a Message"  +RST+GRAY+"    ->  Extra PIN protected",        CYAN);
    boxEmpty(CYAN);
    boxLine(std::string(MAGENTA)+BOLD+"  [ 3 ]"+RST+"  "+PINK  +"Encryption History" +RST+GRAY+"    ->  Browse all encrypted msgs",  CYAN);
    boxEmpty(CYAN);
    boxLine(std::string(TEAL)   +BOLD+"  [ 4 ]"+RST+"  "+TEAL  +"Dashboard"          +RST+GRAY+"    ->  Stats & recent activity",    CYAN);
    boxEmpty(CYAN);
    boxLine(std::string(INDIGO) +BOLD+"  [ 5 ]"+RST+"  "+INDIGO+"Profile & Settings"  +RST+GRAY+"    ->  Account / password",        CYAN);
    boxEmpty(CYAN);
    boxLine(std::string(BLUE)   +BOLD+"  [ 6 ]"+RST+"  "+BLUE  +"About"               +RST+GRAY+"    ->  Cipher info & pipeline",    CYAN);
    boxEmpty(CYAN);
    boxLine(std::string(RED)    +BOLD+"  [ 0 ]"+RST+"  "+RED   +"Logout"              +RST+GRAY+"    ->  Return to auth screen",      CYAN);
    boxEmpty(CYAN); boxBot(CYAN);
    std::cout << "\n  " << CYAN << "  Choice -> " << RST << WHITE;
}

/* ═══ MAIN ═══════════════════════════════════════════════════════════════ */
int main(){
    introAnimation();
    authScreen();       // blocks until authenticated

    while(true){
        cls();
        printBanner(true);
        printMainMenu();

        std::string inp; std::cin >> inp;
        int ch = -1; try{ ch = std::stoi(inp); }catch(...){}

        switch(ch){
            case 1: sectionEncrypt(); break;
            case 2: sectionDecrypt(); break;
            case 3: sectionHistory(); break;
            case 4: showDashboard();  break;
            case 5: sectionProfile(); break;
            case 6: sectionAbout();   break;
            case 0:
                gSession.loggedIn=false; gSession.username=""; gSession.loginTime=""; gSession.encCount=0; gSession.decCount=0;
                authScreen();
                break;
            default:
                badgeErr("Enter 0 to 6.");
                sleepMs(700);
                break;
        }
    }
}