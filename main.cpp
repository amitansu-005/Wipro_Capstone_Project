// File: main.cpp
// Title: File Explorer Application (Linux)
// Name: Amitansu Singh
// College: ITER SOA University
// Build: g++ -std=gnu++17 -O2 -Wall -Wextra -o filex main.cpp
// Run:   ./filex

#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/time.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <time.h>

using namespace std;
static const char* C_RESET = "\033[0m";
static const char* C_BLUE  = "\033[34m";
static const char* C_GREEN = "\033[32m";
static const char* C_CYAN  = "\033[36m";
static bool useColor = true;

static bool isTerminal() {
    return isatty(STDOUT_FILENO);
}

static string colorName(const string& name, const struct stat& st) {
    if (!useColor || !isTerminal()) return name;
    if (S_ISDIR(st.st_mode)) return string(C_BLUE) + name + C_RESET;
    if (S_ISLNK(st.st_mode)) return string(C_CYAN) + name + C_RESET;
    if (st.st_mode & S_IXUSR) return string(C_GREEN) + name + C_RESET;
    return name;
}

static string humanSize(off_t sz) {
    const char* units[] = {"B","KB","MB","GB","TB"};
    int i=0;
    double s = (double)sz;
    while(s>=1024.0 && i<4){ s/=1024.0; i++; }
    char buf[64];
    snprintf(buf, sizeof(buf), (s<10 && i>0) ? "%.1f %s" : "%.0f %s", s, units[i]);
    return buf;
}

static string permString(mode_t m){
    string p;
    p += S_ISDIR(m) ? 'd' : (S_ISLNK(m) ? 'l' : '-');
    p += (m & S_IRUSR) ? 'r' : '-';
    p += (m & S_IWUSR) ? 'w' : '-';
    p += (m & S_IXUSR) ? 'x' : '-';
    p += (m & S_IRGRP) ? 'r' : '-';
    p += (m & S_IWGRP) ? 'w' : '-';
    p += (m & S_IXGRP) ? 'x' : '-';
    p += (m & S_IROTH) ? 'r' : '-';
    p += (m & S_IWOTH) ? 'w' : '-';
    p += (m & S_IXOTH) ? 'x' : '-';
    return p;
}

static string ownerName(uid_t uid){
    if(auto* pw = getpwuid(uid)) return pw->pw_name ? string(pw->pw_name) : to_string(uid);
    return to_string(uid);
}

static string groupName(gid_t gid){
    if(auto* gr = getgrgid(gid)) return gr->gr_name ? string(gr->gr_name) : to_string(gid);
    return to_string(gid);
}

static string timeString(time_t t){
    char buf[64];
    struct tm lt; localtime_r(&t, &lt);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &lt);
    return buf;
}

static bool isDotOrDotDot(const char* n){
    return strcmp(n, ".")==0 || strcmp(n, "..")==0;
}

static void listDir(const string& path){
    DIR* d = opendir(path.c_str());
    if(!d){ perror("opendir"); return; }
    vector<string> names;
    while(auto* e = readdir(d)){
        if(isDotOrDotDot(e->d_name)) continue;
        names.emplace_back(e->d_name);
    }
    closedir(d);
    sort(names.begin(), names.end(), [&](const string& a, const string& b){
        struct stat sa{}, sb{};
        string fa = path + "/" + a, fb = path + "/" + b;
        stat(fa.c_str(), &sa); stat(fb.c_str(), &sb);
        bool da = S_ISDIR(sa.st_mode), db = S_ISDIR(sb.st_mode);
        if(da!=db) return da>db;
        return a < b;
    });

    printf("%-11s %-8s %-8s %10s %-16s %s\n", "PERMISSIONS","OWNER","GROUP","SIZE","MODIFIED","NAME");
    for(auto& name: names){
        string full = path + "/" + name;
        struct stat st{};
        if(lstat(full.c_str(), &st)==-1){ perror("lstat"); continue; }
        string shown = colorName(name, st);
        printf("%-11s %-8s %-8s %10s %-16s %s\n",
            permString(st.st_mode).c_str(),
            ownerName(st.st_uid).c_str(),
            groupName(st.st_gid).c_str(),
            humanSize(st.st_size).c_str(),
            timeString(st.st_mtime).c_str(),
            shown.c_str());
    }
}

static bool createFile(const string& path){
    int fd = open(path.c_str(), O_CREAT|O_WRONLY|O_TRUNC, 0644);
    if(fd==-1){ perror("open"); return false; }
    close(fd); return true;
}

static bool createDir(const string& path){
    if(mkdir(path.c_str(), 0755)==-1){ perror("mkdir"); return false; }
    return true;
}

static bool deleteFile(const string& path){
    if(unlink(path.c_str())==-1){ perror("unlink"); return false; }
    return true;
}

static bool deleteEmptyDir(const string& path){
    if(rmdir(path.c_str())==-1){ perror("rmdir"); return false; }
    return true;
}

static bool copyFile(const string& src, const string& dst){
    int in = open(src.c_str(), O_RDONLY);
    if(in==-1){ perror("open src"); return false; }
    struct stat st{}; if(fstat(in, &st)==-1){ perror("fstat"); close(in); return false; }
    int out = open(dst.c_str(), O_CREAT|O_WRONLY|O_TRUNC, st.st_mode);
    if(out==-1){ perror("open dst"); close(in); return false; }

#if defined(__linux__)
    off_t offset = 0;
    while(offset < st.st_size){
        ssize_t sent = sendfile(out, in, &offset, st.st_size - offset);
        if(sent == -1){ perror("sendfile"); close(in); close(out); return false; }
    }
#else
    char buf[1<<16];
    ssize_t r;
    while((r = read(in, buf, sizeof(buf)))>0){
        ssize_t w=0;
        while(w<r){
            ssize_t k = write(out, buf+w, r-w);
            if(k==-1){ perror("write"); close(in); close(out); return false; }
            w += k;
        }
    }
    if(r==-1){ perror("read"); close(in); close(out); return false; }
#endif
    close(in); close(out);
    return true;
}

static bool movePath(const string& src, const string& dst){
    if(rename(src.c_str(), dst.c_str())==-1){ perror("rename"); return false; }
    return true;
}

static bool isDir(const string& p){
    struct stat st{}; if(lstat(p.c_str(), &st)==-1) return false;
    return S_ISDIR(st.st_mode);
}

static bool deleteRecursive(const string& path){
    struct stat st{};
    if(lstat(path.c_str(), &st)==-1){ perror("lstat"); return false; }
    if(S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)){
        DIR* d = opendir(path.c_str());
        if(!d){ perror("opendir"); return false; }
        while(auto* e = readdir(d)){
            if(isDotOrDotDot(e->d_name)) continue;
            string child = path + "/" + e->d_name;
            if(!deleteRecursive(child)){ closedir(d); return false; }
        }
        closedir(d);
        if(rmdir(path.c_str())==-1){ perror("rmdir"); return false; }
        return true;
    } else {
        if(unlink(path.c_str())==-1){ perror("unlink"); return false; }
        return true;
    }
}

static bool ensureDir(const string& p, mode_t mode=0755){
    if(mkdir(p.c_str(), mode)==0) return true;
    if(errno==EEXIST) return true;
    perror("mkdir"); return false;
}

static bool copyRecursive(const string& src, const string& dst){
    struct stat st{};
    if(lstat(src.c_str(), &st)==-1){ perror("lstat"); return false; }
    if(S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)){
        if(!ensureDir(dst, 0755)) return false;
        DIR* d = opendir(src.c_str());
        if(!d){ perror("opendir"); return false; }
        while(auto* e = readdir(d)){
            if(isDotOrDotDot(e->d_name)) continue;
            string s = src + "/" + e->d_name;
            string t = dst + "/" + e->d_name;
            if(!copyRecursive(s, t)){ closedir(d); return false; }
        }
        closedir(d);
        return true;
    } else if(S_ISREG(st.st_mode)){
        return copyFile(src, dst);
    } else if(S_ISLNK(st.st_mode)){
       
        char buf[PATH_MAX]; ssize_t n = readlink(src.c_str(), buf, sizeof(buf)-1);
        if(n==-1){ perror("readlink"); return false; }
        buf[n]='\0';
        if(symlink(buf, dst.c_str())==-1){ perror("symlink"); return false; }
        return true;
    } else {
       
        return copyFile(src, dst);
    }
}

static void showFile(const string& path, size_t maxLines, bool fromTail){
    ifstream in(path);
    if(!in){ perror("open"); return; }
    if(!fromTail){
        string line; size_t c=0;
        while(c<maxLines && getline(in,line)){ cout<<line<<"\n"; c++; }
        if(in.good()) cout<<"... (truncated)\n";
        return;
    } else {
        deque<string> dq; string line;
        while(getline(in,line)){
            dq.push_back(line);
            if(dq.size()>maxLines) dq.pop_front();
        }
        for(auto& s: dq) cout<<s<<"\n";
    }
}

static void catFile(const string& path){
    int fd = open(path.c_str(), O_RDONLY);
    if(fd==-1){ perror("open"); return; }
    char buf[1<<14];
    ssize_t r;
    while((r = read(fd, buf, sizeof(buf)))>0){
        ssize_t w=0;
        while(w<r){
            ssize_t k = write(STDOUT_FILENO, buf+w, r-w);
            if(k==-1){ perror("write"); close(fd); return; }
            w += k;
        }
    }
    if(r==-1) perror("read");
    close(fd);
}

static bool appendLines(const string& path){
    FILE* f = fopen(path.c_str(), "a");
    if(!f){ perror("fopen"); return false; }
    cout<<"Enter text (finish with a single line: END)\n";
    string line;
    while(true){
        if(!getline(cin, line)){ break; }
        if(line=="END") break;
        fprintf(f, "%s\n", line.c_str());
    }
    fclose(f);
    return true;
}

static void searchRecursive(const string& base, const string& pattern){
    DIR* d = opendir(base.c_str());
    if(!d) return;
    while(auto* e = readdir(d)){
        if(isDotOrDotDot(e->d_name)) continue;
        string name = e->d_name;
        string full = base + "/" + name;
        if(name.find(pattern) != string::npos){
            cout<<full<<"\n";
        }
        struct stat st{};
        if(lstat(full.c_str(), &st)==0 && S_ISDIR(st.st_mode)){
            searchRecursive(full, pattern);
        }
    }
    closedir(d);
}

static off_t duRecursive(const string& path){
    struct stat st{};
    if(lstat(path.c_str(), &st)==-1) return 0;
    off_t total = st.st_size;
    if(S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)){
        DIR* d = opendir(path.c_str());
        if(!d) return total;
        while(auto* e = readdir(d)){
            if(isDotOrDotDot(e->d_name)) continue;
            string child = path + "/" + e->d_name;
            total += duRecursive(child);
        }
        closedir(d);
    }
    return total;
}

static void treeRecursive(const string& path, int depth, int level=0){
    if(depth>=0 && level>depth) return;
    DIR* d = opendir(path.c_str());
    if(!d){ perror("opendir"); return; }
    vector<string> names;
    while(auto* e = readdir(d)){
        if(isDotOrDotDot(e->d_name)) continue;
        names.emplace_back(e->d_name);
    }
    closedir(d);
    sort(names.begin(), names.end());
    for(auto& name: names){
        string full = path + "/" + name;
        struct stat st{};
        if(lstat(full.c_str(), &st)==-1) continue;
        cout<<string(level*2, ' ')<<"- "<<name<<"\n";
        if(S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)){
            treeRecursive(full, depth, level+1);
        }
    }
}
static void printHelp(){
    cout << R"(Commands:
  ls [path]                 - list directory (detailed, colored)
  pwd                       - print working directory
  cd <path>                 - change directory
  mkfile <path>             - create file
  mkdir <path>              - create directory
  rm <path>                 - delete file
  rmdir <path>              - delete EMPTY directory
  rmr <path>                - delete directory/file RECURSIVELY (careful)
  cp <src> <dst>            - copy file
  cpr <src> <dst>           - copy directory/file RECURSIVELY
  mv <src> <dst>            - move/rename
  rename <src> <dst>        - alias for mv
  chmod <octal> <path>      - change permission, e.g., chmod 755 file
  info <path>               - detailed info of file/dir
  du [path]                 - disk usage (recursive) for path (default .)
  tree [path] [depth]       - print tree (default path '.' depth 2)
  head <file> [n]           - show first n lines (default 20)
  tail <file> [n]           - show last n lines (default 20)
  cat <file>                - print full file
  write <file>              - append lines, finish with a line 'END'
  search <pattern> [dir]    - recursive name search (default .)
  help                      - show this help
  exit                      - quit
)";
}

static mode_t parseOctal(const string& s){
    // expect a 3- or 4-digit octal
    mode_t m = 0;
    for(char c: s){
        if(c<'0' || c>'7') return (mode_t)-1;
        m = (m<<3) + (c-'0');
    }
    return m;
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cout<<"File Explorer (Linux) - Advanced Single File Version\n";
    cout<<"Type 'help' to see commands.\n\n";

    char cwdBuf[PATH_MAX];
    while(true){
        if(getcwd(cwdBuf, sizeof(cwdBuf))==nullptr) strcpy(cwdBuf, "?");
        cout << "[" << cwdBuf << "]$ " << std::flush;

        string cmd;
        if(!(cin>>cmd)) break;

        if(cmd=="ls"){
            string p=".";
            string rest; getline(cin, rest);
            stringstream ss(rest);
            if(ss>>p) {}
            listDir(p);

        } else if(cmd=="pwd"){
            if(getcwd(cwdBuf, sizeof(cwdBuf))) cout<<cwdBuf<<"\n";
            else perror("getcwd");

        } else if(cmd=="cd"){
            string p; cin>>ws; getline(cin, p);
            if(p.empty()){ cout<<"Usage: cd <path>\n"; continue; }
            while(!p.empty() && isspace((unsigned char)p.front())) p.erase(p.begin());
            while(!p.empty() && isspace((unsigned char)p.back())) p.pop_back();
            if(chdir(p.c_str())==-1) perror("chdir");

        } else if(cmd=="mkfile"){
            string p; cin>>ws; getline(cin, p);
            if(p.empty()){ cout<<"Usage: mkfile <path>\n"; continue; }
            while(!p.empty() && isspace((unsigned char)p.front())) p.erase(p.begin());
            if(createFile(p)) cout<<"Created file: "<<p<<"\n";

        } else if(cmd=="mkdir"){
            string p; cin>>ws; getline(cin, p);
            if(p.empty()){ cout<<"Usage: mkdir <path>\n"; continue; }
            while(!p.empty() && isspace((unsigned char)p.front())) p.erase(p.begin());
            if(createDir(p)) cout<<"Created directory: "<<p<<"\n";

        } else if(cmd=="rm"){
            string p; cin>>ws; getline(cin, p);
            if(p.empty()){ cout<<"Usage: rm <path>\n"; continue; }
            while(!p.empty() && isspace((unsigned char)p.front())) p.erase(p.begin());
            deleteFile(p);

        } else if(cmd=="rmdir"){
            string p; cin>>ws; getline(cin, p);
            if(p.empty()){ cout<<"Usage: rmdir <path>\n"; continue; }
            while(!p.empty() && isspace((unsigned char)p.front())) p.erase(p.begin());
            deleteEmptyDir(p);

        } else if(cmd=="rmr"){
            string p; cin>>ws; getline(cin, p);
            if(p.empty()){ cout<<"Usage: rmr <path>\n"; continue; }
            while(!p.empty() && isspace((unsigned char)p.front())) p.erase(p.begin());
            if(deleteRecursive(p)) cout<<"Deleted recursively: "<<p<<"\n";

        } else if(cmd=="cp"){
            string src, dst;
            if(!(cin>>src>>dst)){ cout<<"Usage: cp <src> <dst>\n"; cin.clear(); string tmp; getline(cin,tmp); continue; }
            if(copyFile(src,dst)) cout<<"Copied.\n";

        } else if(cmd=="cpr"){
            string src, dst;
            if(!(cin>>src>>dst)){ cout<<"Usage: cpr <src> <dst>\n"; cin.clear(); string tmp; getline(cin,tmp); continue; }
            if(copyRecursive(src,dst)) cout<<"Copied recursively.\n";

        } else if(cmd=="mv" || cmd=="rename"){
            string src, dst;
            if(!(cin>>src>>dst)){ cout<<"Usage: "<<cmd<<" <src> <dst>\n"; cin.clear(); string tmp; getline(cin,tmp); continue; }
            if(movePath(src,dst)) cout<<"Moved/Renamed.\n";

        } else if(cmd=="chmod"){
            string modeStr, pth;
            if(!(cin>>modeStr>>pth)){ cout<<"Usage: chmod <octal> <path>\n"; continue; }
            mode_t m = parseOctal(modeStr);
            if(m==(mode_t)-1){ cout<<"Invalid octal mode. Example: 755\n"; continue; }
            if(::chmod(pth.c_str(), m)==-1) perror("chmod");
            else cout<<"Permissions changed.\n";

        } else if(cmd=="info"){
            string p; if(!(cin>>p)){ cout<<"Usage: info <path>\n"; continue; }
            struct stat st{};
            if(lstat(p.c_str(), &st)==-1){ perror("lstat"); continue; }
            cout<<"Path: "<<p<<"\n";
            cout<<"Type: "<<(S_ISDIR(st.st_mode)?"Directory":S_ISLNK(st.st_mode)?"Symlink":S_ISREG(st.st_mode)?"File":"Other")<<"\n";
            cout<<"Perm: "<<permString(st.st_mode)<<"\n";
            cout<<"User: "<<ownerName(st.st_uid)<<"  Group: "<<groupName(st.st_gid)<<"\n";
            cout<<"Size: "<<humanSize(st.st_size)<<"  Modified: "<<timeString(st.st_mtime)<<"\n";

        } else if(cmd=="du"){
            string p="."; if(cin.peek()==' '||cin.peek()=='\t'){ cin>>p; }
            off_t bytes = duRecursive(p);
            cout<<humanSize(bytes)<<"  "<<p<<"\n";

        } else if(cmd=="tree"){
            string p="."; int depth=2;
            string rest; getline(cin, rest);
            stringstream ss(rest);
            if(ss>>p) { if(ss>>depth){} }
            treeRecursive(p, depth);

        } else if(cmd=="head"){
            string f; size_t n=20;
            if(!(cin>>f)){ cout<<"Usage: head <file> [n]\n"; continue; }
            if(cin.peek()==' '||cin.peek()=='\t'){ cin>>n; }
            showFile(f, n, false);

        } else if(cmd=="tail"){
            string f; size_t n=20;
            if(!(cin>>f)){ cout<<"Usage: tail <file> [n]\n"; continue; }
            if(cin.peek()==' '||cin.peek()=='\t'){ cin>>n; }
            showFile(f, n, true);

        } else if(cmd=="cat"){
            string f; if(!(cin>>f)){ cout<<"Usage: cat <file>\n"; continue; }
            catFile(f);

        } else if(cmd=="write"){
            string f; cin>>ws; getline(cin, f);
            if(f.empty()){ cout<<"Usage: write <file>\n"; continue; }
            while(!f.empty() && isspace((unsigned char)f.front())) f.erase(f.begin());
            appendLines(f);

        } else if(cmd=="search"){
            string pat, dir=".";
            if(!(cin>>pat)){ cout<<"Usage: search <pattern> [dir]\n"; continue; }
            if(cin.peek()==' '||cin.peek()=='\t'){ cin>>dir; }
            searchRecursive(dir, pat);

        } else if(cmd=="help"){
            printHelp();

        } else if(cmd=="exit" || cmd=="quit"){
            break;

        } else {
            cout<<"Unknown command. Type 'help'.\n";
            string tmp; getline(cin,tmp);
        }
    }
    return 0;
}
