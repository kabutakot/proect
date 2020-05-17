#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

enum marker{
    keyword,
    identifier,
    literal,
    constant,
    operation,
    punctuator,
    defect
};

struct lexeme_list{
    char *word;
    int line;
    marker type;
    lexeme_list *next;
};

class Automat{
    enum state{
        home,
        ident,
        keywd,
        quote,
        count,
        equal,
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
    lexeme_list *first;
private:
    void add_lexeme(marker note);
    void step(char c);
    void handle_home(char c);
    void handle_ident(char c);
    void handle_keywd(char c);
    void handle_count(char c);
    void handle_quote(char c);
    void handle_equal(char c);
    bool is_operation(char c);
    bool is_keyword();
    void append(char c);
    void clean();
    int length();
    char *getword();
public:
    Automat();
    ~Automat();
    void feed(char c);
    lexeme_list *push(){return first;}
    bool state_home(){return (flag==home);}
};

int cmpstr(const char *str1,const char *str2);
int lenstr(const char *str);

Automat::Automat()
{
    flag=home;
    delay=false;
    current_line=1;
    q=0;
    first=0;
}

Automat::~Automat()
{
    clean();
}

void Automat::feed(char c)
{
    step(c);
    if (delay){
        delay=false;
        step(c);
    }
    if (c=='\n'){
        current_line++;
    }
}

void Automat::add_lexeme(marker note)
{
    lexeme_list *tmp;
    if (!q){
        return;
    }
    if (first){
        tmp=first;
        while (tmp->next){
            tmp=tmp->next;
        }
        tmp->next=new lexeme_list;
        tmp=tmp->next;
    }
    else{
        first=new lexeme_list;
        tmp=first;
    }
    tmp->next=0;
    tmp->word=getword();
    tmp->line=current_line;
    tmp->type=note;
    clean();
}

void Automat::step(char c)
{
    switch (flag)
    {
        case home:
            handle_home(c);
            break;
        case ident:
            handle_ident(c);
            break;
        case keywd:
            handle_keywd(c);
            break;
        case count:
            handle_count(c);
            break;
        case quote:
            handle_quote(c);
            break;
        case equal:
            handle_equal(c);
            break;
        case error:
            flag=error;
    }
}
 
void Automat::handle_home(char c)
{
    if (c==' '||c=='\t'||c=='\n')
        return;
    if (c=='"'){
        flag=quote;
        return;
    }
    append(c);
    if (is_operation(c)){
        add_lexeme(operation);
        return;
    }
    if (c=='{'||c=='}'||c==';'){
        add_lexeme(punctuator);
        return;
    }
    if (c=='#'||c=='@'||c=='$'){
        flag=ident;
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
    if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')){
        flag=keywd;
        return;
    }
    add_lexeme(defect);
    flag=error;
}

void Automat::handle_ident(char c)
{
    if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')){
        append(c);
    }
    else{
        add_lexeme(identifier);
        delay=true;
        flag=home;
    }
}

void Automat::handle_keywd(char c)
{
    if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')){
        append(c);
    }
    else{
        if (is_keyword()){
            add_lexeme(keyword);
            delay=true;
            flag=home;
        }
        else{
            add_lexeme(defect);
            flag=error;
        }
    }
}

void Automat::handle_count(char c)
{
    if (c>='0'&&c<='9'){
        append(c);
    }
    else{
        add_lexeme(constant);
        delay=true;
        flag=home;
    }
}

void Automat::handle_quote(char c)
{
    if (c=='"'){
        add_lexeme(literal);
        flag=home;
    }
    else{
        append(c);
    }
}

void Automat::handle_equal(char c)
{
    append(c);
    if (c=='='||c=='!'||c=='<'||c=='>'||c==':'){
        add_lexeme(operation);
        flag=home;
    }
    else{
        add_lexeme(defect);
        flag=error;
    }
}

bool Automat::is_operation(char c)
{
    int i=0;
    char str[]="+-*/()[]&|!,<>,";
    while (str[i]!='\0'){
        if (str[i]==c)
            return true;
        i++;
    }
    return false;
}

bool Automat::is_keyword()
{
    static const char s1[]="if";
    static const char s2[]="while";
    static const char s3[]="goto";
    static const char s4[]="var";
    static const char s5[]="start";
    static const char s6[]="finish";
    char *key=getword();
    if (!cmpstr(key,s1))
        return true;
    if (!cmpstr(key,s2))
        return true;
    if (!cmpstr(key,s3))
        return true;
    if (!cmpstr(key,s4))
        return true;
    if (!cmpstr(key,s5))
        return true;
    if (!cmpstr(key,s6))
        return true;
    return false;
}

void Automat::append(char c)
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

void Automat::clean()
{
    item *tmp;
    while (q){
        tmp=q;
        q=q->next;
        delete tmp;
    }
}

int Automat::length()
{
    int length=0;
    item *tmp=q;
    while (tmp){
        tmp=tmp->next;
        length++;
    }
    return length;
}

char *Automat::getword()
{
    int i,N=length();
    char *str=new char [N+1];
    item *tmp=q;
    for(i=0;i<N;i++){
        str[i]=tmp->symbol;
        tmp=tmp->next;
    }
    str[N]='\0';
    return str;
}

int cmpstr(const char *str1,const char *str2)
{
    int i=0,len=lenstr(str1);
    if (str1&&str2){
        if (len==lenstr(str2)){
            while (i<=len){
                if (str1[i]!=str2[i])
                    return 1;
                i++;
            }
        }
        else{
            return 1;
        }
    }
    else{
        return 1;
    }
    return 0;
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

void output(lexeme_list *q)
{
    while (q){
        switch (q->type){
            case keyword:
                fprintf(stderr,"keyword     ");
                break;
            case identifier:
                fprintf(stderr,"identifier  ");
                break;
            case literal:
                fprintf(stderr,"literal     ");
                break;
            case constant:
                fprintf(stderr,"constant    ");
                break;
            case operation:
                fprintf(stderr,"operation   ");
                break;
            case punctuator:
                fprintf(stderr,"punctuator  ");
                break;
            case defect:
                fprintf(stderr,"defect      ");
        }
        fprintf(stderr,"%i  ",q->line);
        fprintf(stderr,"%s\n",q->word);
        q=q->next;
    }
}

void start()
{
    int rs;
    char c;
    Automat B;
    lexeme_list *data;
    while((rs=getchar())!=EOF){
        c=(char)rs;
        B.feed(c);
    }
    data=B.push();
    output(data);
    if (!B.state_home()){
        fprintf(stderr,"Automat was not returned to <Home>\n");
    }
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
        perror("open");
        exit(1);
    }
    dup2(fd,0);
    close(fd);
    start();
    return 0;
}

