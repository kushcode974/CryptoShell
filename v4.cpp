/*
  CryptoShell v4.0
  Dual Mode:
  1. Portable Mode  -> Encrypt/Decrypt only, no account dependency
  2. Local Mode     -> Account login + saved history + profile

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
const std::string APP_VERSION  = "v4.0";
const std::string APP_SUBTITLE = "Portable + Local Secure Message Encoder & Decoder";
const std::string USERS_FILE   = "cs_users.dat";
const int MAX_ATTEMPTS = 3;
const int BOX_WIDTH    = 68;
const int MAX_HISTORY  = 50;
const int PREVIEW_LEN  = 32;

/* ═══ DATA STRUCTURES ════════════════════════════════════════════════════ */
enum class AppMode { NONE, PORTABLE, LOCAL };

struct HistEntry {
    std::string timestamp, preview, cipher;
};

struct Session {
    bool loggedIn = false;
    AppMode mode = AppMode::NONE;
    std::string username, loginTime;
    std::string sessionPassword;
    int encCount = 0, decCount = 0;
};

struct User {
    std::string username, passHash;
};

static Session gSession;

/* ═══ UTILITIES ══════════════════════════════════════════════════════════ */
void sleepMs(int ms){
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void cls(){
    system(CLEAR);
}

void typeWrite(const std::string& text, int delay = 18){
    for(char c : text){
        std::cout << c << std::flush;
        sleepMs(delay);
    }
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
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%d %b %Y  %H:%M:%S", localtime(&now));
    return std::string(buf);
}

std::string getTimestamp(){
    time_t now = time(nullptr);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return std::string(buf);
}

std::string trim(const std::string& s){
    size_t start = s.find_first_not_of(" \t\r\n");
    if(start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

int visibleLen(const std::string& s){
    int len = 0; bool esc = false;
    for(unsigned char c : s){
        if(c == '\033'){ esc = true; continue; }
        if(esc){ if(std::isalpha(c)) esc = false; continue; }
        if((c & 0xC0) != 0x80) ++len;
    }
    return len;
}

void flushCin(){
    if(std::cin.peek() == '\n')
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

/* ═══ BOX DRAWING ════════════════════════════════════════════════════════ */
void boxTop(const std::string& col = CYAN){
    std::cout << col << "  ╔";
    for(int i = 0; i < BOX_WIDTH; i++) std::cout << "═";
    std::cout << "╗" << RST << "\n";
}
void boxBot(const std::string& col = CYAN){
    std::cout << col << "  ╚";
    for(int i = 0; i < BOX_WIDTH; i++) std::cout << "═";
    std::cout << "╝" << RST << "\n";
}
void boxMid(const std::string& col = CYAN){
    std::cout << col << "  ╠";
    for(int i = 0; i < BOX_WIDTH; i++) std::cout << "═";
    std::cout << "╣" << RST << "\n";
}
void boxLine(const std::string& content, const std::string& borderCol = CYAN){
    int vl = visibleLen(content);
    int pad = BOX_WIDTH - 1 - vl;
    if(pad < 0) pad = 0;
    std::cout << borderCol << "  ║ " << RST
              << content
              << std::string(pad, ' ')
              << borderCol << "║" << RST << "\n";
}
void boxEmpty(const std::string& col = CYAN){ boxLine("", col); }

void printDivider(const std::string& col = GRAY){
    std::cout << col << "  ";
    for(int i = 0; i < BOX_WIDTH + 2; i++) std::cout << "─";
    std::cout << RST << "\n";
}

/* ═══ PROGRESS BAR ═══════════════════════════════════════════════════════ */
void progressBar(const std::string& label, int durationMs){
    const int bw = 28;
    for(int i = 0; i <= bw; i++){
        int pct = (i * 100) / bw;
        std::cout << "\r  " << CYAN << label << " [" << LIME;
        for(int j = 0; j < i; j++) std::cout << "█";
        std::cout << GRAY;
        for(int j = i; j < bw; j++) std::cout << "░";
        std::cout << CYAN << "] " << GOLD << BOLD
                  << std::setw(3) << pct << "%" << RST << std::flush;
        sleepMs(durationMs / bw);
    }
    std::cout << "\n";
}

/* ═══ BADGES ═════════════════════════════════════════════════════════════ */
void badgeOK  (const std::string& m){ std::cout << "\n  " << BG_GREEN  << BLACK << BOLD << " OK  " << RST << "  " << GREEN  << m << RST << "\n"; }
void badgeErr (const std::string& m){ std::cout << "\n  " << BG_RED    << WHITE << BOLD << " ERR " << RST << "  " << RED    << m << RST << "\n"; }
void badgeWarn(const std::string& m){ std::cout << "\n  " << BG_ORANGE << WHITE << BOLD << " WARN" << RST << "  " << ORANGE << m << RST << "\n"; }
void badgeInfo(const std::string& m){ std::cout <<         "  " << INDIGO << BOLD << " i " << RST << "  " << GRAY << m << RST << "\n"; }

/* ═══ HIDDEN INPUT ═══════════════════════════════════════════════════════ */
std::string getHiddenInput(){
    std::string pwd;
#ifdef _WIN32
    char ch;
    while((ch = _getch()) != '\r'){
        if(ch == '\b'){
            if(!pwd.empty()){
                pwd.pop_back();
                std::cout << "\b \b";
            }
        } else {
            pwd.push_back(ch);
            std::cout << CYAN << "*" << RST << std::flush;
        }
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
            if(!pwd.empty()){
                pwd.pop_back();
                std::cout << "\b \b" << std::flush;
            }
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

/* ═══ HASH + CIPHER HELPERS ══════════════════════════════════════════════ */
std::string hashPass(const std::string& pwd){
    uint64_t h = 5381;
    for(unsigned char c : pwd) h = ((h << 5) + h) ^ c;
    for(int i = (int)pwd.size()-1; i >= 0; --i)
        h = ((h << 3) + h) ^ static_cast<unsigned char>(pwd[i]);

    std::ostringstream ss;
    ss << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << h;
    return ss.str();
}

std::string deriveKey(const std::string& password){
    std::string h1 = hashPass(password);
    std::string h2 = hashPass(password + "#CS4");
    std::string key = h1 + h2;
    if(key.size() < 16) key += "A1B2C3D4E5F60789";
    return key.substr(0, 16);
}

static std::string xorLayer(const std::string& t, const std::string& k){
    std::string r = t;
    for(size_t i = 0; i < t.size(); ++i)
        r[i] = static_cast<char>(t[i] ^ k[i % k.size()]);
    return r;
}

static std::string caesarLayer(const std::string& t, int dir, const std::string& key){
    std::string r = t;
    for(size_t i = 0; i < t.size(); ++i){
        int sh = (static_cast<int>(key[i % key.size()]) % 9) + 4;
        r[i] = static_cast<char>(t[i] + dir * sh);
    }
    return r;
}

static std::string toHex(const std::string& b){
    std::ostringstream ss;
    for(unsigned char c : b)
        ss << std::uppercase << std::hex << std::setw(2)
           << std::setfill('0') << (int)c;
    return ss.str();
}

static std::string fromHex(const std::string& h){
    if(h.size() % 2 != 0) return "";
    std::string b;
    for(size_t i = 0; i < h.size(); i += 2){
        try{
            b.push_back(static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16)));
        } catch(...){
            return "";
        }
    }
    return b;
}

std::string encryptMsgWithPassword(const std::string& plain, const std::string& password){
    std::string key = deriveKey(password);
    return toHex(xorLayer(caesarLayer(plain, +1, key), key));
}

std::string decryptMsgWithPassword(const std::string& cipher, const std::string& password){
    std::string key = deriveKey(password);
    std::string bin = fromHex(cipher);
    if(bin.empty()) return "__ERR__";
    return caesarLayer(xorLayer(bin, key), -1, key);
}

/* ═══ FILE HELPERS ═══════════════════════════════════════════════════════ */
std::string histFile(const std::string& u){
    return "cs_hist_" + u + ".dat";
}

std::vector<User> loadUsers(){
    std::vector<User> v;
    std::ifstream f(USERS_FILE);
    std::string line;

    while(std::getline(f, line)){
        line = trim(line);
        if(line.empty()) continue;
        auto p = line.find('|');
        if(p == std::string::npos) continue;

        std::string uname = trim(line.substr(0, p));
        std::string phash = trim(line.substr(p + 1));

        if(!uname.empty() && !phash.empty())
            v.push_back({uname, phash});
    }
    return v;
}

void saveUsers(const std::vector<User>& v){
    std::ofstream f(USERS_FILE, std::ios::trunc);
    for(const auto& u : v)
        f << trim(u.username) << "|" << trim(u.passHash) << "\n";
}

bool userExists(const std::string& u){
    std::string user = trim(u);
    for(const auto& x : loadUsers())
        if(trim(x.username) == user) return true;
    return false;
}

bool registerUser(const std::string& u, const std::string& p){
    std::string user = trim(u);
    if(userExists(user)) return false;

    auto v = loadUsers();
    v.push_back({user, hashPass(p)});
    saveUsers(v);
    return true;
}

bool loginUser(const std::string& u, const std::string& p){
    std::string user = trim(u);
    std::string h = hashPass(p);

    for(const auto& x : loadUsers()){
        if(trim(x.username) == user && trim(x.passHash) == h)
            return true;
    }
    return false;
}

std::vector<HistEntry> loadHistory(const std::string& u){
    std::vector<HistEntry> v;
    std::ifstream f(histFile(u));
    std::string line;

    while(std::getline(f, line)){
        auto p1 = line.find('|'); if(p1 == std::string::npos) continue;
        auto p2 = line.find('|', p1 + 1); if(p2 == std::string::npos) continue;
        v.push_back({line.substr(0, p1), line.substr(p1+1, p2-p1-1), line.substr(p2+1)});
    }
    return v;
}

void saveHistory(const std::string& u, const std::vector<HistEntry>& v){
    std::ofstream f(histFile(u), std::ios::trunc);
    for(const auto& e : v)
        f << e.timestamp << "|" << e.preview << "|" << e.cipher << "\n";
}

void appendHistory(const std::string& u, const std::string& plain, const std::string& cipher){
    auto hist = loadHistory(u);
    std::string raw = plain.substr(0, PREVIEW_LEN);

    for(char& c : raw)
        if(c == '|' || c == '\n') c = ' ';

    hist.push_back({
        getTimestamp(),
        raw + (plain.size() > (size_t)PREVIEW_LEN ? "..." : ""),
        cipher
    });

    if((int)hist.size() > MAX_HISTORY)
        hist.erase(hist.begin(), hist.begin() + (int)hist.size() - MAX_HISTORY);

    saveHistory(u, hist);
}

/* ═══ UI HELPERS ═════════════════════════════════════════════════════════ */
void sectionHeader(const std::string& icon, const std::string& title, const std::string& col){
    std::cout << "\n";
    boxTop(col);
    boxLine(std::string(BOLD)+col+"  "+icon+"  "+title+RST, col);
    boxBot(col);
    std::cout << "\n";
}

void displayOutputBox(const std::string& label, const std::string& data,
                      const std::string& dataCol, const std::string& boxCol){
    const int wrap = BOX_WIDTH - 4;
    std::cout << "\n";
    boxTop(boxCol);
    boxLine(std::string(BOLD)+boxCol+"  "+label+RST, boxCol);
    boxMid(boxCol);
    boxEmpty(boxCol);

    if(data.empty()){
        boxLine(dataCol + "  <empty>" + RST, boxCol);
    } else {
        for(size_t pos = 0; pos < data.size(); pos += wrap)
            boxLine(dataCol+"  "+data.substr(pos, wrap)+RST, boxCol);
    }

    boxEmpty(boxCol);
    boxBot(boxCol);
}

void printBanner(bool compact = false){
    if(!compact){
        std::cout << "\n" << TEAL << BOLD;
        std::cout << "    ======================================================================\n";
        std::cout << "     ██████╗██████╗ ██╗   ██╗██████╗ ████████╗ ██████╗ ███████╗██╗  ██╗\n";
        std::cout << "    ██╔════╝██╔══██╗╚██╗ ██╔╝██╔══██╗╚══██╔══╝██╔═══██╗██╔════╝██║  ██║\n";
        std::cout << "    ██║     ██████╔╝ ╚████╔╝ ██████╔╝   ██║   ██║   ██║███████╗███████║\n";
        std::cout << "    ██║     ██╔══██╗  ╚██╔╝  ██╔═══╝    ██║   ██║   ██║╚════██║██╔══██║\n";
        std::cout << "    ╚██████╗██║  ██║   ██║   ██║        ██║   ╚██████╔╝███████║██║  ██║\n";
        std::cout << "     ╚═════╝╚═╝  ╚═╝   ╚═╝   ╚═╝        ╚═╝    ╚═════╝ ╚══════╝╚═╝  ╚═╝\n";
        std::cout << "    ======================================================================\n";
        std::cout << RST;
    }

    std::cout << "\n";
    boxTop(INDIGO);
    boxLine(std::string(GOLD)+BOLD+"  CryptoShell "+APP_VERSION+"  "+RST+DIM+GRAY+APP_SUBTITLE+RST, INDIGO);

    std::string modeText = "Mode: ";
    if(gSession.mode == AppMode::PORTABLE) modeText += "Portable";
    else if(gSession.mode == AppMode::LOCAL) modeText += "Local";
    else modeText += "None";

    if(gSession.loggedIn){
        boxLine(std::string(GRAY)+"  "+modeText+"   |   User: "+RST+LIME+BOLD+gSession.username+RST+
                GRAY+"   |   "+gSession.loginTime+RST, INDIGO);
    } else {
        boxLine(std::string(GRAY)+"  "+getDateTime()+"   |   "+modeText+RST, INDIGO);
    }

    boxBot(INDIGO);
    std::cout << "\n";
}

void introAnimation(){
    cls();
    std::cout << "\n\n";
    std::cout << CYAN << BOLD;
    typeWrite("        Booting CryptoShell " + APP_VERSION + "...\n", 28);
    std::cout << RST << "\n";
    progressBar("Loading portable module ", 350);
    progressBar("Loading local module    ", 350);
    progressBar("Loading cipher engine   ", 450);
    progressBar("Preparing interface     ", 300);
    std::cout << "\n";
    badgeOK("All systems ready.");
    sleepMs(500);
}

/* ═══ PORTABLE MODE ══════════════════════════════════════════════════════ */
void portableEncrypt(){
    cls(); printBanner(true);
    sectionHeader("M", "PORTABLE ENCRYPT", GREEN);

    flushCin();

    std::cout << "  " << WHITE << BOLD << "  Enter password/key:\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST;
    std::string pwd = getHiddenInput();
    if(pwd.size() < 4){
        badgeErr("Password must be at least 4 characters.");
        waitEnter();
        return;
    }

    std::cout << "\n  " << WHITE << BOLD << "  Enter message:\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST;
    std::string msg;
    std::getline(std::cin, msg);

    if(msg.empty()){
        badgeErr("Message cannot be empty.");
        waitEnter();
        return;
    }

    progressBar("Encrypting message     ", 700);
    std::string cipher = encryptMsgWithPassword(msg, pwd);

    badgeOK("Encryption successful!");
    displayOutputBox("PORTABLE CIPHERTEXT", cipher, GOLD, GREEN);
    badgeInfo("Copy this ciphertext and keep it safe.");
    badgeInfo("To decrypt later, use the same password.");
    waitEnter();
}

void portableDecrypt(){
    cls(); printBanner(true);
    sectionHeader("M", "PORTABLE DECRYPT", YELLOW);

    flushCin();

    std::cout << "  " << WHITE << BOLD << "  Enter password/key:\n" << RST;
    std::cout << "  " << YELLOW << "  -> " << RST;
    std::string pwd = getHiddenInput();
    if(pwd.size() < 4){
        badgeErr("Password must be at least 4 characters.");
        waitEnter();
        return;
    }

    std::cout << "\n  " << WHITE << BOLD << "  Paste ciphertext (hex):\n" << RST;
    std::cout << "  " << CYAN << "  -> " << RST;
    std::string cipher;
    std::getline(std::cin, cipher);

    std::string cleaned;
    for(char c : cipher)
        if(!std::isspace(static_cast<unsigned char>(c))) cleaned.push_back(c);

    if(cleaned.empty()){
        badgeErr("Ciphertext cannot be empty.");
        waitEnter();
        return;
    }

    progressBar("Decrypting message     ", 700);
    std::string plain = decryptMsgWithPassword(cleaned, pwd);
    if(plain == "__ERR__"){
        badgeErr("Invalid ciphertext or wrong input format.");
        waitEnter();
        return;
    }

    badgeOK("Decryption successful!");
    displayOutputBox("DECRYPTED MESSAGE", plain, TEAL, YELLOW);
    waitEnter();
}

void portableMode(){
    gSession.mode = AppMode::PORTABLE;
    gSession.loggedIn = false;
    gSession.username.clear();
    gSession.sessionPassword.clear();
    gSession.loginTime.clear();

    while(true){
        cls(); printBanner(true);
        boxTop(CYAN);
        boxLine(std::string(BOLD)+CYAN+"   PORTABLE MODE"+RST+GRAY+"   |   No account required", CYAN);
        boxMid(CYAN); boxEmpty(CYAN);
        boxLine(std::string(GREEN)+BOLD+"  [ 1 ]"+RST+"  "+LIME+"Encrypt Message"+RST+GRAY+"   -> copy ciphertext anywhere", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(YELLOW)+BOLD+"  [ 2 ]"+RST+"  "+GOLD+"Decrypt Message"+RST+GRAY+"   -> use same password later", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(RED)+BOLD+"  [ 0 ]"+RST+"  "+RED+"Back to Mode Select", CYAN);
        boxEmpty(CYAN); boxBot(CYAN);

        std::cout << "\n  " << CYAN << "  Choice -> " << RST << WHITE;
        std::string inp; std::cin >> inp;
        int ch = -1; try{ ch = std::stoi(inp); }catch(...){}

        if(ch == 1) portableEncrypt();
        else if(ch == 2) portableDecrypt();
        else if(ch == 0) return;
        else { badgeErr("Enter 0 to 2."); sleepMs(600); }
    }
}

/* ═══ LOCAL MODE AUTH ════════════════════════════════════════════════════ */
bool isValidUsername(const std::string& u){
    if(u.size() < 3 || u.size() > 20) return false;
    for(char c : u)
        if(!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
    return true;
}

void doRegister(){
    cls(); printBanner(true);
    sectionHeader("+", "CREATE ACCOUNT", GREEN);

    flushCin();

    std::cout << "\n  " << WHITE << BOLD << "  Username" << RST
              << GRAY << "  (3-20 chars, a-z / 0-9 / _)\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST << WHITE;

    std::string uname;
    std::getline(std::cin, uname);
    uname = trim(uname);

    if(!isValidUsername(uname)){
        badgeErr("Invalid username. Use 3-20 alphanumeric chars / '_'.");
        waitEnter(); return;
    }
    if(userExists(uname)){
        badgeErr("Username already exists.");
        waitEnter(); return;
    }

    std::cout << "\n  " << WHITE << BOLD << "  Password" << RST
              << GRAY << "  (min 4 chars)\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST;
    std::string pwd = getHiddenInput();

    if(pwd.size() < 4){
        badgeErr("Password too short.");
        waitEnter(); return;
    }

    std::cout << "\n  " << WHITE << BOLD << "  Confirm Password\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST;
    std::string pwd2 = getHiddenInput();

    if(pwd != pwd2){
        badgeErr("Passwords do not match.");
        waitEnter(); return;
    }

    spinner("Creating account", 650);

    if(!registerUser(uname, pwd)){
        badgeErr("Failed to create account.");
        waitEnter(); return;
    }

    gSession.loggedIn = true;
    gSession.mode = AppMode::LOCAL;
    gSession.username = uname;
    gSession.sessionPassword = pwd;
    gSession.loginTime = getDateTime();
    gSession.encCount = 0;
    gSession.decCount = 0;

    badgeOK("Account created and logged in!");
    sleepMs(600);
}

bool doLogin(){
    cls(); printBanner(true);
    sectionHeader("->", "LOGIN", CYAN);

    flushCin();

    std::cout << "\n  " << WHITE << BOLD << "  Username\n" << RST;
    std::cout << "  " << CYAN << "  -> " << RST << WHITE;

    std::string uname;
    std::getline(std::cin, uname);
    uname = trim(uname);

    if(uname.empty()){
        badgeErr("Username cannot be empty.");
        waitEnter();
        return false;
    }

    int attempts = 0;
    bool auth = false;
    std::string pwd;

    while(attempts < MAX_ATTEMPTS){
        int left = MAX_ATTEMPTS - attempts;
        std::cout << "\n  " << WHITE << BOLD << "  Password"
                  << RST << GRAY << "  (" << left << " attempt"
                  << (left > 1 ? "s" : "") << " left)\n" << RST;
        std::cout << "  " << CYAN << "  -> " << RST;

        pwd = getHiddenInput();

        if(loginUser(uname, pwd)){
            auth = true;
            break;
        }

        ++attempts;
        if(attempts < MAX_ATTEMPTS) badgeErr("Wrong username or password.");
        std::cout << "\n";
    }

    if(!auth){
        boxTop(RED);
        boxLine(std::string(RED)+BOLD+"  ACCESS DENIED"+RST, RED);
        boxBot(RED);
        waitEnter();
        return false;
    }

    spinner("Authenticating", 600);

    gSession.loggedIn = true;
    gSession.mode = AppMode::LOCAL;
    gSession.username = uname;
    gSession.sessionPassword = pwd;
    gSession.loginTime = getDateTime();
    gSession.encCount = 0;
    gSession.decCount = 0;

    badgeOK("Welcome back, " + uname + "!");
    sleepMs(600);
    return true;
}

void localAuthScreen(){
    gSession.mode = AppMode::LOCAL;

    while(true){
        cls(); printBanner(false);
        boxTop(CYAN); boxEmpty(CYAN);
        boxLine(std::string(BOLD)+CYAN+"    LOCAL MODE AUTH"+RST, CYAN);
        boxEmpty(CYAN); boxMid(CYAN); boxEmpty(CYAN);
        boxLine(std::string(GREEN)+BOLD+"  [ 1 ]"+RST+"  "+LIME+"Login", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(YELLOW)+BOLD+"  [ 2 ]"+RST+"  "+GOLD+"Create Account", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(RED)+BOLD+"  [ 0 ]"+RST+"  "+RED+"Back to Mode Select", CYAN);
        boxEmpty(CYAN); boxBot(CYAN);

        std::cout << "\n  " << CYAN << "  Choice -> " << RST << WHITE;
        std::string inp; std::cin >> inp;
        int ch = -1; try{ ch = std::stoi(inp); }catch(...){}

        if(ch == 1){
            if(doLogin()) return;
        }
        else if(ch == 2){
            doRegister();
            if(gSession.loggedIn) return;
        }
        else if(ch == 0){
            gSession = Session{};
            return;
        }
        else{
            badgeErr("Enter 0, 1 or 2.");
            sleepMs(600);
        }
    }
}

/* ═══ LOCAL MODE FEATURES ════════════════════════════════════════════════ */
void localEncrypt(){
    cls(); printBanner(true);
    sectionHeader("L", "LOCAL ENCRYPT", GREEN);

    flushCin();

    std::cout << "\n  " << WHITE << BOLD << "  Enter message:\n" << RST;
    std::cout << "  " << GREEN << "  -> " << RST;

    std::string msg;
    std::getline(std::cin, msg);

    if(msg.empty()){
        badgeErr("Message cannot be empty.");
        waitEnter(); return;
    }

    progressBar("Encrypting message     ", 750);
    std::string cipher = encryptMsgWithPassword(msg, gSession.sessionPassword);

    appendHistory(gSession.username, msg, cipher);
    gSession.encCount++;

    badgeOK("Encrypted and saved to history!");
    displayOutputBox("ENCRYPTED OUTPUT", cipher, GOLD, GREEN);
    badgeInfo("Saved in local user history.");
    waitEnter();
}

void localDecrypt(){
    cls(); printBanner(true);
    sectionHeader("L", "LOCAL DECRYPT", YELLOW);

    boxTop(RED);
    boxLine(std::string(RED)+BOLD+"  RESTRICTED — Login Password Required"+RST, RED);
    boxBot(RED);
    std::cout << "\n";

    flushCin();

    int attempts = 0;
    bool auth = false;

    while(attempts < MAX_ATTEMPTS){
        int left = MAX_ATTEMPTS - attempts;
        std::cout << "  " << YELLOW << BOLD << "  Account Password"
                  << RST << GRAY << "  (" << left << " attempt"
                  << (left > 1 ? "s" : "") << " left)\n" << RST;
        std::cout << "  " << YELLOW << "  -> " << RST;

        std::string pwd = getHiddenInput();
        if(pwd == gSession.sessionPassword){
            auth = true;
            break;
        }

        ++attempts;
        if(attempts < MAX_ATTEMPTS) badgeErr("Wrong password.");
        std::cout << "\n";
    }

    if(!auth){
        badgeErr("Access denied.");
        waitEnter();
        return;
    }

    std::cout << "\n  " << WHITE << BOLD << "  Paste ciphertext (hex):\n" << RST;
    std::cout << "  " << CYAN << "  -> " << RST;

    std::string cipher;
    std::getline(std::cin, cipher);

    std::string cleaned;
    for(char c : cipher)
        if(!std::isspace(static_cast<unsigned char>(c))) cleaned.push_back(c);

    if(cleaned.empty()){
        badgeErr("Ciphertext cannot be empty.");
        waitEnter(); return;
    }

    progressBar("Decrypting message     ", 750);
    std::string plain = decryptMsgWithPassword(cleaned, gSession.sessionPassword);

    if(plain == "__ERR__"){
        badgeErr("Invalid ciphertext.");
        waitEnter(); return;
    }

    gSession.decCount++;
    badgeOK("Decryption successful!");
    displayOutputBox("DECRYPTED MESSAGE", plain, TEAL, YELLOW);
    waitEnter();
}

void localDashboard(){
    cls(); printBanner(true);
    sectionHeader("D", "DASHBOARD", TEAL);

    auto hist = loadHistory(gSession.username);
    int total = (int)hist.size();

    boxTop(TEAL);
    boxLine(std::string(BOLD)+TEAL+"  USER DASHBOARD"+RST+GRAY+"   |   "+LIME+gSession.username+RST, TEAL);
    boxMid(TEAL); boxEmpty(TEAL);
    boxLine(std::string(GOLD)+BOLD+"  Total Saved    "+RST+CYAN+std::to_string(total)+" messages"+RST, TEAL);
    boxLine(std::string(GOLD)+BOLD+"  This Session   "+RST+
            GREEN+std::to_string(gSession.encCount)+" enc"+RST+
            GRAY+" / "+RST+
            YELLOW+std::to_string(gSession.decCount)+" dec"+RST, TEAL);
    boxLine(std::string(GOLD)+BOLD+"  Login Time     "+RST+GRAY+gSession.loginTime+RST, TEAL);
    boxEmpty(TEAL);
    boxBot(TEAL);

    waitEnter();
}

void localHistory(){
    auto hist = loadHistory(gSession.username);
    int total = (int)hist.size();

    if(total == 0){
        cls(); printBanner(true);
        sectionHeader("*", "HISTORY", MAGENTA);
        badgeWarn("No history found.");
        waitEnter();
        return;
    }

    cls(); printBanner(true);
    sectionHeader("*", "HISTORY", MAGENTA);

    boxTop(MAGENTA);
    boxLine(std::string(BOLD)+MAGENTA+"  Recent Entries"+RST, MAGENTA);
    boxMid(MAGENTA);

    int start = std::max(0, total - 5);
    for(int i = total - 1; i >= start; --i){
        auto& e = hist[i];
        boxLine(std::string(GRAY)+"  "+e.timestamp+RST, MAGENTA);
        boxLine(std::string(CYAN)+"  "+e.preview+RST, MAGENTA);
        std::string cut = e.cipher.substr(0, 48) + (e.cipher.size() > 48 ? "..." : "");
        boxLine(std::string(GOLD)+"  "+cut+RST, MAGENTA);
        if(i > start) boxLine(std::string(GRAY)+"  ----------------------------------------------------"+RST, MAGENTA);
    }

    boxEmpty(MAGENTA);
    boxBot(MAGENTA);
    waitEnter();
}

void localProfile(){
    cls(); printBanner(true);
    sectionHeader("@", "PROFILE", INDIGO);

    auto hist = loadHistory(gSession.username);
    int total = (int)hist.size();

    boxTop(INDIGO); boxEmpty(INDIGO);
    boxLine(std::string(GOLD)+BOLD+"  Username        "+RST+WHITE+gSession.username+RST, INDIGO);
    boxLine(std::string(GOLD)+BOLD+"  Logged In At    "+RST+GRAY+gSession.loginTime+RST, INDIGO);
    boxLine(std::string(GOLD)+BOLD+"  Saved Messages  "+RST+CYAN+std::to_string(total)+RST, INDIGO);
    boxEmpty(INDIGO);
    boxBot(INDIGO);

    std::cout << "\n  " << GRAY << "  [C] Change Password   [0] Back\n" << RST
              << "  " << INDIGO << "  -> " << RST << WHITE;

    std::string inp;
    std::cin >> inp;
    for(char& c : inp) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if(inp == "C"){
        flushCin();

        std::cout << "\n  " << WHITE << BOLD << "  Current Password:\n" << RST;
        std::cout << "  " << INDIGO << "  -> " << RST;
        std::string cur = getHiddenInput();

        if(!loginUser(gSession.username, cur)){
            badgeErr("Wrong current password.");
            waitEnter(); return;
        }

        std::cout << "\n  " << WHITE << BOLD << "  New Password:\n" << RST;
        std::cout << "  " << INDIGO << "  -> " << RST;
        std::string np = getHiddenInput();

        if(np.size() < 4){
            badgeErr("Password too short.");
            waitEnter(); return;
        }

        std::cout << "\n  " << WHITE << BOLD << "  Confirm New Password:\n" << RST;
        std::cout << "  " << INDIGO << "  -> " << RST;
        std::string np2 = getHiddenInput();

        if(np != np2){
            badgeErr("Passwords do not match.");
            waitEnter(); return;
        }

        auto users = loadUsers();
        for(auto& u : users){
            if(u.username == gSession.username){
                u.passHash = hashPass(np);
                break;
            }
        }
        saveUsers(users);
        gSession.sessionPassword = np;

        badgeOK("Password updated successfully!");
        waitEnter();
    }
}

void localAbout(){
    cls(); printBanner(true);
    sectionHeader("?", "ABOUT", MAGENTA);

    boxTop(MAGENTA); boxEmpty(MAGENTA);
    boxLine(std::string(GOLD)+BOLD+"  Portable Mode"+RST+GRAY+"  -> no account, no saved history", MAGENTA);
    boxLine(std::string(GOLD)+BOLD+"  Local Mode   "+RST+GRAY+"  -> account, history, profile", MAGENTA);
    boxLine(std::string(GOLD)+BOLD+"  Storage      "+RST+GRAY+"  -> file-based (works best locally)", MAGENTA);
    boxLine(std::string(GOLD)+BOLD+"  Cipher       "+RST+GRAY+"  -> password-derived Caesar + XOR + Hex", MAGENTA);
    boxEmpty(MAGENTA); boxBot(MAGENTA);

    waitEnter();
}

void localMode(){
    localAuthScreen();
    if(!gSession.loggedIn) return;

    while(true){
        cls(); printBanner(true);
        boxTop(CYAN);
        boxLine(std::string(BOLD)+CYAN+"   LOCAL MODE"+RST+GRAY+"   |   "+LIME+gSession.username+RST, CYAN);
        boxMid(CYAN); boxEmpty(CYAN);
        boxLine(std::string(GREEN)+BOLD+"  [ 1 ]"+RST+"  "+LIME+"Encrypt Message"+RST+GRAY+"   -> save to history", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(YELLOW)+BOLD+"  [ 2 ]"+RST+"  "+GOLD+"Decrypt Message"+RST+GRAY+"   -> login password protected", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(MAGENTA)+BOLD+"  [ 3 ]"+RST+"  "+PINK+"History", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(TEAL)+BOLD+"  [ 4 ]"+RST+"  "+TEAL+"Dashboard", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(INDIGO)+BOLD+"  [ 5 ]"+RST+"  "+INDIGO+"Profile", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(BLUE)+BOLD+"  [ 6 ]"+RST+"  "+BLUE+"About", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(RED)+BOLD+"  [ 0 ]"+RST+"  "+RED+"Logout", CYAN);
        boxEmpty(CYAN); boxBot(CYAN);

        std::cout << "\n  " << CYAN << "  Choice -> " << RST << WHITE;
        std::string inp; std::cin >> inp;
        int ch = -1; try{ ch = std::stoi(inp); }catch(...){}

        switch(ch){
            case 1: localEncrypt(); break;
            case 2: localDecrypt(); break;
            case 3: localHistory(); break;
            case 4: localDashboard(); break;
            case 5: localProfile(); break;
            case 6: localAbout(); break;
            case 0:
                gSession = Session{};
                return;
            default:
                badgeErr("Enter 0 to 6.");
                sleepMs(700);
        }
    }
}

/* ═══ MODE SELECT ════════════════════════════════════════════════════════ */
void modeSelectScreen(){
    while(true){
        cls(); printBanner(false);
        boxTop(CYAN); boxEmpty(CYAN);
        boxLine(std::string(BOLD)+CYAN+"    SELECT MODE"+RST, CYAN);
        boxEmpty(CYAN); boxMid(CYAN); boxEmpty(CYAN);
        boxLine(std::string(GREEN)+BOLD+"  [ 1 ]"+RST+"  "+LIME+"Portable Mode"+RST+GRAY+"  -> Encrypt/Decrypt only", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(YELLOW)+BOLD+"  [ 2 ]"+RST+"  "+GOLD+"Local Mode"+RST+GRAY+"     -> Accounts + History + Profile", CYAN);
        boxEmpty(CYAN);
        boxLine(std::string(RED)+BOLD+"  [ 0 ]"+RST+"  "+RED+"Exit", CYAN);
        boxEmpty(CYAN); boxBot(CYAN);

        std::cout << "\n  " << CYAN << "  Choice -> " << RST << WHITE;
        std::string inp; std::cin >> inp;
        int ch = -1; try{ ch = std::stoi(inp); }catch(...){}

        if(ch == 1) portableMode();
        else if(ch == 2) localMode();
        else if(ch == 0){
            cls();
            std::cout << "\n\n  " << CYAN << BOLD << "  Goodbye!\n\n" << RST;
            exit(0);
        } else {
            badgeErr("Enter 0, 1 or 2.");
            sleepMs(700);
        }
    }
}

/* ═══ MAIN ═══════════════════════════════════════════════════════════════ */
int main(){
    introAnimation();
    modeSelectScreen();
    return 0;
}