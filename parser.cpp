#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

enum marker{
    identifier,
    keyword,
    constant,
    string,
    operation,
    punctuator,
    defect
};

struct LexItem{
    char *word;
    int line;
    marker type;
    LexItem *next;
};

class Error{
    char *msg;
    LexItem *lex;
public:
    Error(const char *str);
    Error(const char *str,LexItem *q);
    ~Error();
    Error(const Error& Drop);
    void Report();
};

class Scanner{
    enum state{
        home,
        ident,
        keywd,
        count,
        quote,
        equal,
        comnt,
        error
    };
    struct item{
        char symbol;
        item *next;
    };
    state flag;
    bool delay;
    int current_line;
    item *q;
    LexItem *first;
private:
    void Step(char c);
    void HandleHome(char c);
    void HandleIdent(char c);
    void HandleKeywd(char c);
    void HandleCount(char c);
    void HandleQuote(char c);
    void HandleEqual(char c);
    void HandleComnt(char c);
    void AddLexeme(marker note);
    bool IsOperation(char c);
    bool CheckIdentifier();
    bool CheckKeyword();
    void Append(char c);
    void Clean();
    int Length();
    char *GetWord();
public:
    Scanner();
    ~Scanner();
    void Feed(char c);
    LexItem *Push();
};

class Parser{
    LexItem *cur_lex;
private:
    void Next();
    void A();
    void B();
    void C();
    void C1();
    void C2();
    void C3();
    void D();
    void D1();
    void D2();
    void D3();
    void D4();
    void E();
    bool IsVariable();
    bool IsFunction();
    bool IsConstant();
    bool IsString();
    bool IsLex(const char *str);
public:
    Parser(LexItem *q);
    void Analyze();
};

bool cmpstr(const char *str1,const char *str2);

char *dupstr(const char *str);

int lenstr(const char *str);

Error::Error(const char *str)
{
    msg=dupstr(str);
    lex=0;
}

Error::Error(const char *str,LexItem *q)
{
    msg=dupstr(str);
    lex=q;
}

Error::~Error()
{
    if (msg){
        delete []msg;
    }
}

Error::Error(const Error& Drop)
{
    msg=dupstr(Drop.msg);
    lex=Drop.lex;
}

void Error::Report()
{
    fprintf(stderr,"%s\n",msg);
    if (lex){
        fprintf(stderr,"word: %s\n",lex->word);
        fprintf(stderr,"line: %i\n",lex->line);
    }
    fprintf(stderr,"Error was detected...\n");
}

Scanner::Scanner()
{
    flag=home;
    delay=false;
    current_line=1;
    q=0;
    first=0;
}

Scanner::~Scanner()
{
    Clean();
}

void Scanner::Feed(char c)
{
    Step(c);
    if (delay){
        delay=false;
        Step(c);
    }
    if (c=='\n'){
        current_line++;
    }
}

LexItem *Scanner::Push()
{
    LexItem *tmp=first;
    if (flag==error){
        while (tmp->next){
            tmp=tmp->next;
        }
        throw Error("lexical error",tmp);
    }
    if (flag!=home){
        throw Error("no return to home",tmp);
    }
    return first;
}

void Scanner::Step(char c)
{
    switch (flag)
    {
        case home:
            HandleHome(c);
            break;
        case ident:
            HandleIdent(c);
            break;
        case keywd:
            HandleKeywd(c);
            break;
        case count:
            HandleCount(c);
            break;
        case quote:
            HandleQuote(c);
            break;
        case equal:
            HandleEqual(c);
            break;
        case comnt:
            HandleComnt(c);
            break;
        case error:
            flag=error;
    }
}
 
void Scanner::HandleHome(char c)
{
    if (c==' '||c=='\t'||c=='\n')
        return;
    if (c=='#'){
        flag=comnt;
        return;
    }
    if (c=='"'){
        flag=quote;
        return;
    }
    Append(c);
    if (IsOperation(c)){
        AddLexeme(operation);
        return;
    }
    if (c=='{'||c=='}'||c==';'){
        AddLexeme(punctuator);
        return;
    }
    if (c=='$'||c=='?'){
        flag=ident;
        return;
    }
    if (c>='a'&&c<='z'){
        flag=keywd;
        return;
    }
    if (c>='0'&&c<='9'){
        flag=count;
        return;
    }
    if (c=='='){
        flag=equal;
        return;
    }
    AddLexeme(defect);
    flag=error;
}

void Scanner::HandleIdent(char c)
{
    if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')){
        Append(c);
    }
    else{
        if (CheckIdentifier()){
            AddLexeme(identifier);
            delay=true;
            flag=home;
        }
        else{
            AddLexeme(defect);
            flag=error;
        }
    }
}

void Scanner::HandleKeywd(char c)
{
    if (c>='a'&&c<='z'){
        Append(c);
    }
    else{
        if (CheckKeyword()){
            AddLexeme(keyword);
            delay=true;
            flag=home;
        }
        else{
            AddLexeme(defect);
            flag=error;
        }
    }
}

void Scanner::HandleCount(char c)
{
    if (c>='0'&&c<='9'){
        Append(c);
    }
    else{
        AddLexeme(constant);
        delay=true;
        flag=home;
    }
}

void Scanner::HandleQuote(char c)
{
    if (c=='"'){
        AddLexeme(string);
        flag=home;
    }
    else{
        Append(c);
    }
}

void Scanner::HandleEqual(char c)
{
    if (c=='='||c=='!'||c=='<'||c=='>'){
        Append(c);
        AddLexeme(operation);
        flag=home;
    }
    else{
        AddLexeme(operation);
        delay=true;
        flag=home;
    }
}

void Scanner::HandleComnt(char c)
{
    if (c=='\n'){
        flag=home;
    }
}

void Scanner::AddLexeme(marker note)
{
    LexItem *tmp;
    if (!q){
        return;
    }
    if (first){
        tmp=first;
        while (tmp->next){
            tmp=tmp->next;
        }
        tmp->next=new LexItem;
        tmp=tmp->next;
    }
    else{
        first=new LexItem;
        tmp=first;
    }
    tmp->next=0;
    tmp->word=GetWord();
    tmp->line=current_line;
    tmp->type=note;
    Clean();
}

bool Scanner::IsOperation(char c)
{
    int i=0;
    const char str[]="+-*/&|!<>()[],";
    while (str[i]!='\0'){
        if (str[i]==c)
            return true;
        i++;
    }
    return false;
}

bool Scanner::CheckIdentifier()
{
    const char *const functions[]={
        "?print","?sell","?buy","?prod","?build","?myid",
        "?money","?raw","?product","?factory","?active",  
        "?RawPrice","?ProdPrice","?RawCount","?ProdCount",
        "?BuyPrice","?SellPrice","?BuyCount","?SellCount",0
    };
    char *lex=GetWord();
    int i=0;
    if (lex[0]!='?'){
        delete []lex;
        return true;
    }
    while (functions[i]!=0){
        if (!cmpstr(lex,functions[i])){
            delete []lex;
            return true;
        }
        i++;
    }
    delete []lex;
    return false;
}

bool Scanner::CheckKeyword()
{
    const char *const keywords[]={
        "start","finish","if","while",0
    };
    char *lex=GetWord();
    int i=0;
    while (keywords[i]!=0){
        if (!cmpstr(lex,keywords[i])){
            delete []lex;
            return true;
        }
        i++;
    }
    delete []lex;
    return false;
}

void Scanner::Append(char c)
{
    item *p;
    if (q){
        p=q;
        while (p->next){
            p=p->next;
        }
        p->next=new item;
        p=p->next;
    }
    else{
        q=new item;
        p=q;
    }
    p->next=0;
    p->symbol=c;
}

void Scanner::Clean()
{
    item *tmp;
    while (q){
        tmp=q;
        q=q->next;
        delete tmp;
    }
}

int Scanner::Length()
{
    int length=0;
    item *tmp=q;
    while (tmp){
        tmp=tmp->next;
        length++;
    }
    return length;
}

char *Scanner::GetWord()
{
    int i,N=Length();
    char *str=new char [N+1];
    item *tmp=q;
    for(i=0;i<N;i++){
        str[i]=tmp->symbol;
        tmp=tmp->next;
    }
    str[N]='\0';
    return str;
}

Parser::Parser(LexItem *q)
{
    cur_lex=q;
}

void Parser::Analyze()
{
    if (cur_lex){
        A();
        fprintf(stderr,"Compilation successful\n");
        fprintf(stderr,"%i lines compiled\n",cur_lex->line);
    }
    else{
        throw Error("no input tokens",cur_lex);
    }
}

void Parser::Next()
{
    if (cur_lex->next){
        cur_lex=cur_lex->next;
    }
    else{
        throw Error("unexpected end of tokens",cur_lex);
    }
}

void Parser::A()
{
    if (!IsLex("start")){
        throw Error("expected keyword \"start\"",cur_lex);
    }
    Next();
    B();
    if (!IsLex("finish")){
        throw Error("expected keyword \"finish\"",cur_lex);
    }
}

void Parser::B()
{
    if (IsLex("{")){
        Next();
        C();
        while (IsLex(";")){
            Next();
            C();
        }
        if (!IsLex("}")){
            throw Error("expected \"}\"",cur_lex);
        }
        Next();
    }
    else{
        throw Error("expected \"{\"",cur_lex);
    }
}

void Parser::C()
{
    if (IsLex("if")||IsLex("while")){
        Next();
        C1();
        B();
    }
    else{
        if (IsFunction()){
            Next();
            C2();
        }
        else{
            if (IsVariable()){
                Next();
                C3();
            }
        }
    }
}

void Parser::C1()
{
    if (IsLex("(")){
        Next();
        D();
        if (!IsLex(")")){
            throw Error("expected \")\" after condition",cur_lex);
        }
        Next();
    }
    else{
        throw Error("expected \"(\" before condition",cur_lex);
    }
}

void Parser::C2()
{
    if (IsLex("(")){
        Next();
        if (IsLex(")")){
            Next();
        }
        else{
            if (IsString()){
                Next();
            }
            else{
                D();
                if (IsLex(",")){
                    Next();
                    D();
                }
            }
            if (!IsLex(")")){
                throw Error("expected \")\" after parameters",cur_lex);
            }
            Next();
        }
    }
    else{
        throw Error("expected \"(\" before parameters",cur_lex);
    }
}

void Parser::C3()
{
    E();
    if (IsLex("=")){
        Next();
        D();
    }
    else{
        throw Error("expected \"=\"",cur_lex);
    }
}

void Parser::D()
{
    D1();
    while (IsLex("|")||IsLex("&")){
        Next();
        D1();
    }
}

void Parser::D1()
{
    D2();
    if (IsLex("==")||IsLex("=<")||IsLex("<")||
        IsLex("=!")||IsLex("=>")||IsLex(">")){
        Next();
        D2();
    }
}

void Parser::D2()
{
    D3();
    while (IsLex("+")||IsLex("-")){
        Next();
        D3();
    }
}

void Parser::D3()
{
    D4();
    while (IsLex("*")||IsLex("/")){
        Next();
        D4();
    }
}

void Parser::D4()
{
    if (IsVariable()){
        Next();
        E();
    }
    else{
        if (IsFunction()){
            Next();
            C2();
        }
        else{
            if (IsConstant()){
                Next();
            }
            else{
                if (IsLex("(")){
                    Next();
                    D();
                    if (!IsLex(")")){
                        throw Error("no closing bracket",cur_lex);
                    }
                    Next();
                }
                else{
                    if (IsLex("!")){
                        Next();
                        D4();
                    }
                    else{
                        if (IsLex("-")){
                            Next();
                            D4();
                        }
                        else{
                            throw Error("expected operand",cur_lex);
                        }
                    }
                }
            }
        }
    }
}

void Parser::E()
{
    if (IsLex("[")){
        Next();
        D();
        if (!IsLex("]")){
            throw Error("expected \"]\" after array index",cur_lex);
        }
        Next();
    }
}

bool Parser::IsVariable()
{
    return (cur_lex->type==identifier&&cur_lex->word[0]=='$');
}

bool Parser::IsFunction()
{
    return (cur_lex->type==identifier&&cur_lex->word[0]=='?');
}

bool Parser::IsConstant()
{
    return cur_lex->type==constant;
}

bool Parser::IsString()
{
    return cur_lex->type==string;
}

bool Parser::IsLex(const char *str)
{
    return (!cmpstr(cur_lex->word,str));
}

bool cmpstr(const char *str1,const char *str2)
{
    int i=0,len=lenstr(str1);
    if (str1&&str2){
        if (len==lenstr(str2)){
            while (i<=len){
                if (str1[i]!=str2[i])
                    return true;
                i++;
            }
        }
        else{
            return true;
        }
    }
    else{
        return true;
    }
    return false;
}

char *dupstr(const char *str)
{
    int i,len=lenstr(str);
    char *copy=new char[len+1];
    for(i=0;i<len;i++){
        copy[i]=str[i];
    }
    copy[i]='\0';
    return copy;
}   

int lenstr(const char *str)
{
    int i=0,length=0;
    if (str){
        while (str[i]!='\0'){
            length++;
            i++;
        }
    }
    return length;
}

void CleanLex(LexItem *first)
{
    LexItem *tmp;
    while (first){
        tmp=first;
        first=first->next;
        delete[] tmp->word;
        delete tmp;
    }
}

LexItem *scanning()
{
    Scanner B;
    LexItem *token;
    int c;
    while((c=getchar())!=EOF){
        B.Feed(c);
    }
    try{
        token=B.Push();
    }
    catch(Error Drop){
        fprintf(stderr,"Scanner: ");
        Drop.Report();
        exit(1);
    }
    return token;
}

void parsing(LexItem *token)
{
    Parser C(token);
    try{
        C.Analyze();
    }
    catch(Error Drop){
        fprintf(stderr,"Parser: ");
        Drop.Report();
        exit(1);
    }
}

void start()
{
    LexItem *token;
    token=scanning();
    parsing(token);
    CleanLex(token);
}

int main(int argc,char **argv)
{
    int fd;
    if (argc!=2){
        fprintf(stderr,"Wrong count of arguments\n");
        exit(1);
    }
    fd=open(argv[1],O_RDONLY);
    if (fd==-1){
        perror(argv[1]);
        exit(1);
    }
    dup2(fd,0);
    close(fd);
    start();
}
 
