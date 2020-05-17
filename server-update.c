#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>

#define INBUFSIZE 1024
#define FINISH 0
#define WORK 1

sig_atomic_t server_status=WORK;

enum gp{
    on,
    off
};

enum state{
    wait,
    work,
    fin,
    over
};

enum command{
    market,
    info,
    player,
    prod,
    buy,
    sell,
    build,
    reset,
    turn,
    help,
    error,
    missed
};

struct item{
    int num;
    struct item *next;
};

struct offer{
    int count;
    int price;
};

struct request{
    enum command name;
    int oper1;
    int oper2;
};

struct player_bid{
    int count;
    int price;
    struct session *player;
};

struct market{
    int raw;
    int prod;
    int min;
    int max;
};

struct game{
    int month;
    int level;
    int count;
    enum gp play;
};

struct session{
    int id;
    int fd;
    int buf_used;
    char buf[INBUFSIZE];
    int cash;
    int raw;
    int product;
    int factory;
    int order;
    int sold;
    int bought;
    struct offer sale;
    struct offer buy;
    int build[5];
	enum state flag;
    struct session *next;
};

struct item *get_end(struct item *p)
{
    struct item *q=NULL;
    while (p){
        q=p;
        p=p->next;
    }
    return q;
}

struct item *add_elem(struct item *p,int elem)
{
    struct item *q;
    if (p){
        q=get_end(p);
        q->next=malloc(sizeof(*q));
        q=q->next;
    }
    else{
        q=malloc(sizeof(*q));
        p=q;
    }
    q->next=NULL;
    q->num=elem;
    return p;
}

void clean_item(struct item *p)
{
    struct item *q;
    while (p){
        q=p;
        p=p->next;
        free(q);
    }
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

int mirrow(int count)
{
    int n=0;
    while (count){
        n=n*10;
        n+=count%10;
        count=count/10;
    }
    return n;
}

int lenint(int count)
{
    int length=0;
    if (count==0)
        return 1;
    if (count<0){
        length++;
        count=count*(-1);
    }
    while (count){
        length++;
        count=count/10;
    }
    return length;
}

char *int2str(int count)
{
    int i=0,N=lenint(count);
    char *str=malloc(N+1);
    if (count==0){
        str[0]='0';
        str[1]='\0';
        return str;
    }
    if (count<0){
        str[0]='-';
        i++;
        count=count*(-1);
    }
    count=mirrow(count);
    while (i<N){
        str[i]=count%10+'0';
        count=count/10;
        i++;
    }
    str[N]='\0';
    return str;
}

void send_msg(struct session *tmp,const char *msg)
{
    write(tmp->fd,msg,lenstr(msg));
}

int send_cnt(struct session *tmp,int count)
{
    char *msg=int2str(count);
    int length=lenstr(msg);
    write(tmp->fd,msg,length);
    free(msg);
    return length;
}

void send_space(struct session *tmp,int size)
{
    static const char msg[]=" ";
    if (!size)
        size=1;
    while (size>0){
        write(tmp->fd,msg,1);
        size--;
    }
}

int client_count(struct session *data)
{
    int count=0;
    while (data){
        count++;
        data=data->next;
    }
    return count;
}

int player_alive(struct session *data)
{
    int count=0;
    while (data){
        if (data->flag!=over){
            count++;
        }
        data=data->next;
    }
    return count;
}

int check_id(struct session *data,int id)
{
    while (data){
        if (data->id==id)
            return 1;
        data=data->next;
    }
    return 0;
}

int get_id(struct session *data)
{
    int id=1;
    while (check_id(data,id)){
        id++;
    }
    return id;
}

void starter_kit(struct session *tmp)
{
    int i;
    tmp->cash=10000;
    tmp->raw=4;
    tmp->product=2;
    tmp->factory=2;
    for(i=0;i<5;i++){
        tmp->build[i]=0;
    }
}

void reset_request(struct session *tmp)
{
    tmp->sale.count=0;
    tmp->sale.price=0;
    tmp->buy.count=0;
    tmp->buy.price=0;
    tmp->sold=0;
    tmp->bought=0;
    tmp->order=0;
}

void welcome(struct session *tmp)
{
    static const char s1[]="\nWelcome!\n";
    static const char s2[]="Your game id: ";
    static const char s3[]="\n\n";
    send_msg(tmp,s1);
    send_msg(tmp,s2);
    send_cnt(tmp,tmp->id);
    send_msg(tmp,s3);
}

struct session *get_last(struct session *data)
{
    struct session *tmp=NULL;
    while (data){
        tmp=data;
        data=data->next;
    }
    return tmp;
}

struct session *close_session(struct session *data,int fd)
{
    struct session *tmp,*del;
    if (data){
        if (data->fd==fd){
            tmp=data->next;
            free(data);
            return tmp;
        }
        del=data;
        while (del){
            if (del->fd==fd)
                break;
            del=del->next;
        }
        tmp=data;
        while (tmp->next!=del){
            tmp=tmp->next;
        }
        tmp->next=del->next;
        free(del);
    }
    return data;
}

struct session *rm_client(struct session *data,int fd)
{
    fprintf(stderr,"client out\n");
    data=close_session(data,fd);
    shutdown(fd,2);
    close(fd);
    return data;
}

struct session *rm_clients(struct session *data,struct item *del)
{
    struct item *p=del;
    while (del){
        data=rm_client(data,del->num);
        del=del->next;
    }
    clean_item(p);
    return data;
}

struct session *add_client(struct session *data,int fd)
{
    struct session *tmp;
    fprintf(stderr,"new client\n");
    if (data){
        tmp=get_last(data);
        tmp->next=malloc(sizeof(*tmp));
        tmp=tmp->next;
    }
    else{
        tmp=malloc(sizeof(*tmp));
        data=tmp;
    }
    tmp->next=NULL;
    tmp->fd=fd;
    tmp->id=0;
    tmp->id=get_id(data);
    tmp->buf_used=0;
    tmp->flag=wait;
    starter_kit(tmp);
    reset_request(tmp);
    welcome(tmp);
    return data;
}

enum command search_cmd(const char *arr)
{
    static const char s1[]="market";
    static const char s2[]="info";
    static const char s3[]="player";
    static const char s4[]="prod";
    static const char s5[]="buy";
    static const char s6[]="sell";
    static const char s7[]="build";
    static const char s8[]="reset";
    static const char s9[]="turn";
    static const char s10[]="help";
    if (arr[0]=='\0')
        return missed;
    if (!cmpstr(arr,s1))
        return market;
    if (!cmpstr(arr,s2))
        return info;
    if (!cmpstr(arr,s3))
        return player;
    if (!cmpstr(arr,s4))
        return prod;
    if (!cmpstr(arr,s5))
        return buy;
    if (!cmpstr(arr,s6))
        return sell;
    if (!cmpstr(arr,s7))
        return build;
    if (!cmpstr(arr,s8))
        return reset;
    if (!cmpstr(arr,s9))
        return turn;
    if (!cmpstr(arr,s10))
        return help;
    return error;
}

int end_word(char sym)
{
    return (sym==' '||sym==9||sym=='\0');
}

int get_word(char *arr,const char *str)
{
    int i=0;
    while ((!end_word(str[i]))&&(i<8)){
        arr[i]=str[i];
        i++;
    }
    arr[i]='\0';
    if (!end_word(str[i]))
        return -1;
    return i;
}

int str2int(const char *arr)
{
    int sum=0,k,i=0;
    while (arr[i]!='\0'){
        sum=sum*10;
        k=arr[i]-'0';
        if (k>9||k<0)
            return -1;
        sum=sum+k;
        i++;
    }
    return sum;
}

int skip_space(const char *str)
{
    int i=0;
    while (str[i]==' '||str[i]==9){
        i++;
    }
    return i;
}

struct request analyze(const char *str)
{
    int rc=0;
    char arr[9];
    struct request cmd;
    cmd.oper1=0;
    cmd.oper2=0;
    rc+=skip_space(str+rc);
    rc+=get_word(arr,str+rc);
    if (rc==-1){
        cmd.name=error;
        return cmd;
    }
    cmd.name=search_cmd(arr);
    rc+=skip_space(str+rc);
    rc+=get_word(arr,str+rc);
    if (rc==-1){
        cmd.name=error;
        return cmd;
    }
    cmd.oper1=str2int(arr);
    rc+=skip_space(str+rc);
    rc+=get_word(arr,str+rc);
    if (rc==-1){
        cmd.name=error;
        return cmd;
    }
    cmd.oper2=str2int(arr);
    rc+=skip_space(str+rc);
    if (str[rc]!='\0')
        cmd.name=error;
    return cmd;
}

int check_cmd(struct request cmd)
{
    return (cmd.name==error||cmd.oper1==-1||cmd.oper2==-1);
}

int check_opt(struct request cmd)
{
    switch (cmd.name)
    {
        case info:
        case market:
        case reset:
        case turn:
        case help:
            if (cmd.oper1||cmd.oper2)
                return 1;
            break;
        case build:
        case prod:
            if (!cmd.oper1||cmd.oper2)
                return 1;
            break;
        case buy:
        case sell:
            if (!cmd.oper1||!cmd.oper2)
                return 1;
            break;
        case player:
            if (cmd.oper1<0||cmd.oper2)
                return 1;
            break;
        default:
            return 0;
    }
    return 0;
}

int check_access(struct session *tmp,struct request cmd)
{
    switch (cmd.name)
    {
        case sell:
        case buy:
        case prod:
        case build:
        case reset:
        case turn:
            if (tmp->flag==fin||tmp->flag==over)
                return 1;
        default:
            return 0;
    }
}

int check(struct session *tmp,struct request cmd)
{
    static const char s1[]="Wrong command\n";
    static const char s2[]="Wrong options\n";
    static const char s3[]="This command is not available\n\n";
    static const char s4[]="The game is over for you...\n\n";
    static const char s5[]="Use <help> to get info\n\n";
    if (check_cmd(cmd)){
        send_msg(tmp,s1);
        send_msg(tmp,s5);
        return 1;
    }
    if (check_opt(cmd)){
        send_msg(tmp,s2);
        send_msg(tmp,s5);
        return 1;
    }
    if (check_access(tmp,cmd)){
        if (tmp->flag==fin)
            send_msg(tmp,s3);
        if (tmp->flag==over)
            send_msg(tmp,s4);
        return 1;
    }
    return 0;
}
   
struct market situation(int level,int active)
{
    struct market info;
    switch (level)
    {
        case 1:
            info.raw=1*active;
            info.prod=3*active;
            info.min=800;
            info.max=6500;
            break;
        case 2:
            info.raw=(int)(1.5*active);
            info.prod=(int)(2.5*active);
            info.min=650;
            info.max=6000;
            break;
        case 3:
            info.raw=2*active;
            info.prod=2*active;
            info.min=500;
            info.max=5500;
            break;
        case 4:
            info.raw=(int)(2.5*active);
            info.prod=(int)(1.5*active);
            info.min=400;
            info.max=5000;
            break;
        case 5:
            info.raw=3*active;
            info.prod=1*active;
            info.min=300;
            info.max=4500;
            break;
    }
    return info;
}

void exe_market(struct session *tmp,struct game bank,int active)
{
    static const char s1[]="Current month is ";
    static const char s2[]="th\n";
    static const char s3[]="Players still active:\n";
    static const char s4[]="Bank sells:  items  min.price\n";
    static const char s5[]="Bank buys:   items  max.price\n";
    static const char s6[]="%";
    static const char s7[]="\n";
    static const char s8[]="\r";
    struct market info=situation(bank.level,active);
    int res;
    send_msg(tmp,s1);
    send_cnt(tmp,bank.month);
    send_msg(tmp,s2);
    send_msg(tmp,s3);
    send_msg(tmp,s6);
    send_space(tmp,12);
    send_cnt(tmp,active);
    send_msg(tmp,s7);
    send_msg(tmp,s4);
    send_msg(tmp,s6);
    send_space(tmp,12);
    res=send_cnt(tmp,info.raw);
    send_space(tmp,7-res);
    send_cnt(tmp,info.min);
    send_msg(tmp,s7);
    send_msg(tmp,s5);
    send_msg(tmp,s6);
    send_space(tmp,12);
    res=send_cnt(tmp,info.prod);
    send_space(tmp,7-res);
    send_cnt(tmp,info.max);
    send_msg(tmp,s7);
    send_msg(tmp,s7);
    send_msg(tmp,s8);
}

void exe_info(struct session *tmp,struct session *data)
{
    static const char s1[]="PLAYER  MONEY   RAW  PROD  FACT\n";
    static const char s2[]="%";
    static const char s3[]="\n";
    static const char s4[]="\r";
    int res;
    send_msg(tmp,s1);
    while (data){
        if (data->flag!=over){
            send_msg(tmp,s2);
            res=send_cnt(tmp,data->id);
            send_space(tmp,7-res);
            res=send_cnt(tmp,data->cash);
            send_space(tmp,8-res);
            res=send_cnt(tmp,data->raw);
            send_space(tmp,5-res);
            res=send_cnt(tmp,data->product);
            send_space(tmp,6-res);
            res=send_cnt(tmp,data->factory);
            send_msg(tmp,s3);
        }
        data=data->next;
    }
    send_msg(tmp,s3);
    send_msg(tmp,s4);
}

void exe_player(struct session *tmp,struct session *data,struct request cmd)
{
    static const char s1[]="Data of player: #";
    static const char s2[]="\nmoney: ";
    static const char s3[]="\nraw: ";
    static const char s4[]="\nproduct: ";
    static const char s5[]="\nfactory: ";
    static const char s6[]="\nconstruction will be completed:\n";
    static const char s7[]="1 mon  2 mon  3 mon  4 mon  5 mon\n";
    static const char s8[]="Player not found\n\n";
    static const char s9[]="This player is a bankrupt\n\n";
    static const char s10[]="\n\n";
    int res,i;
    if (!cmd.oper1)
        cmd.oper1=tmp->id;
    while (data){
        if (cmd.oper1==data->id){
            if (data->flag!=over){
                send_msg(tmp,s1);
                send_cnt(tmp,data->id);
                send_msg(tmp,s2);
                send_cnt(tmp,data->cash);
                send_msg(tmp,s3);
                send_cnt(tmp,data->raw);
                send_msg(tmp,s4);
                send_cnt(tmp,data->product);
                send_msg(tmp,s5);
                send_cnt(tmp,data->factory);
                send_msg(tmp,s6);
                send_msg(tmp,s7);
                send_space(tmp,4);
                for(i=0;i<5;i++){
                    res=send_cnt(tmp,data->build[i]);
                    send_space(tmp,7-res);
                }           
                send_msg(tmp,s10);
                return;
            }
            send_msg(tmp,s9);
            return;
        }
        data=data->next;
    }
    send_msg(tmp,s8);
}

void exe_prod(struct session *tmp,struct request cmd)
{
    static const char s1[]="Not enough cash\n\n\r";
    static const char s2[]="Not enough raw\n\n\r";
    static const char s3[]="Not enough factories\n\n\r";
    static const char s4[]="Factory accepted the order\n\n\r";
    int count=tmp->order+cmd.oper1;
    if (tmp->cash-2000*count<=0){
        send_msg(tmp,s1);
        return;
    }
    if (tmp->raw-count<0){
        send_msg(tmp,s2);
        return;
    }
    if (count>tmp->factory){
        send_msg(tmp,s3);
        return;
    }
    tmp->order+=cmd.oper1;
    send_msg(tmp,s4);
}

void exe_buy(struct session *tmp,struct request cmd,int level,int active)
{
    static const char s1[]="Bank sells less raw\n\n\r";
    static const char s2[]="Too low price\n\n\r";
    static const char s3[]="Request accepted\n\n\r";
    struct market info=situation(level,active);
    if (cmd.oper1>info.raw){
        send_msg(tmp,s1);
        return;
    }
    if (cmd.oper2<info.min){
        send_msg(tmp,s2);
        return;
    }
    send_msg(tmp,s3);
    tmp->buy.count=cmd.oper1;
    tmp->buy.price=cmd.oper2;
    return;
}

void exe_sell(struct session *tmp,struct request cmd,int level,int active)
{
    static const char s1[]="Bank buys less products\n\n\r";
    static const char s2[]="Too high price\n\n\r";
    static const char s3[]="You don't have enough product\n\n\r";
    static const char s4[]="Request accepted\n\n\r";
    struct market info=situation(level,active);
    if (cmd.oper1>info.prod){
        send_msg(tmp,s1);
        return;
    }
    if (cmd.oper2>info.max){
        send_msg(tmp,s2);
        return;
    }
    if (cmd.oper1>tmp->product){
        send_msg(tmp,s3);
        return;
    }
    send_msg(tmp,s4);
    tmp->sale.count=cmd.oper1;
    tmp->sale.price=cmd.oper2;
    return;
}

void exe_build(struct session *tmp,struct request cmd)
{
    static const char s1[]="Not enough cash\n\n";
    static const char s2[]="Request accepted\nTotal cash: ";
    static const char s3[]="\n\n";
    static const char s4[]="$";
    if (tmp->cash>=2500*cmd.oper1){
        tmp->cash-=2500*cmd.oper1;
        tmp->build[4]+=cmd.oper1;
        send_msg(tmp,s2);
        send_cnt(tmp,tmp->cash);
        send_msg(tmp,s4);
        send_msg(tmp,s3);
    }
    else{
        send_msg(tmp,s1);
    }
}

void exe_reset(struct session *tmp)
{
    static const char s1[]="All your applictions canceled\n\n";
    reset_request(tmp);
    send_msg(tmp,s1);
}

void exe_turn(struct session *tmp)
{
    static const char s1[]="Now only info-commands are avaible\n\n";
    tmp->flag=fin;
    send_msg(tmp,s1);
}

void exe_help(struct session *tmp)
{
    static const char s1[]="Use commands:\n";
    static const char s2[]="to find out the market situation: market\n";
    static const char s3[]="to get info about all players: info\n";
    static const char s4[]="to get info about player: player <id>\n";
    static const char s5[]="to produce in a factory: prod <count>\n";
    static const char s6[]="to make a buy-offer: buy <count> <price>\n";
    static const char s7[]="to make a sell-offer: sell <count> <price>\n";
    static const char s8[]="to build a new factory: build <count>\n";
    static const char s9[]="to cancell all applications: reset\n";
    static const char s10[]="to finish work this month: turn\n";
    static const char s11[]="to get information: help\n\n";
    send_msg(tmp,s1);
    send_msg(tmp,s2);
    send_msg(tmp,s3);
    send_msg(tmp,s4);
    send_msg(tmp,s5);
    send_msg(tmp,s6);
    send_msg(tmp,s7);
    send_msg(tmp,s8);
    send_msg(tmp,s9);
    send_msg(tmp,s10);
    send_msg(tmp,s11);
}

int waiting(struct session *tmp,int active,int count_start)
{
    static const char s1[]="/";
    static const char s2[]=" have already connected\n";
    static const char s3[]="Waiting for players...\n\n";
    if (tmp->flag==wait){
        send_cnt(tmp,active);
        send_msg(tmp,s1);
        send_cnt(tmp,count_start);
        send_msg(tmp,s2);
        send_msg(tmp,s3);
        return 1;
    }
    return 0;
}

void execute
(char *str,struct session *tmp,struct session *data,struct game bank)
{
    int active=player_alive(data);
    struct request cmd=analyze(str);
    if (waiting(tmp,active,bank.count))
        return;
    if (check(tmp,cmd))
        return;
    switch (cmd.name)
    {
        case market: 
            exe_market(tmp,bank,active);
            break;
        case info:
            exe_info(tmp,data);
            break;
        case player:
            exe_player(tmp,data,cmd);
            break;
        case prod:
            exe_prod(tmp,cmd);
            break;
        case buy:
            exe_buy(tmp,cmd,bank.level,active);
            break;
        case sell:
            exe_sell(tmp,cmd,bank.level,active);
            break;
        case build:
            exe_build(tmp,cmd);
            break;
        case reset:
            exe_reset(tmp);
            break;
        case turn:
            exe_turn(tmp);
            break;
        case help:
            exe_help(tmp);
            break;             
        default:
            return;
    }
}

void copy(char *str,const char *buf,int pos)
{
    int i;
    for(i=0;i<pos;i++){
        str[i]=buf[i];
    }
    str[pos]='\0';
}

void push_buf(struct session *tmp,int pos)
{
    int i,j=0;
    for(i=pos+1;i<tmp->buf_used;i++){
        tmp->buf[j]=tmp->buf[i];
        j++;
    }
    tmp->buf_used-=(pos+1);
}

void serve_buf(struct session *tmp,struct session *data,struct game bank)
{
    int pos,i;
    char *str;
    for(;;){
        pos=-1;
        for(i=0;i<tmp->buf_used;i++){
            if (tmp->buf[i]=='\n'){
                pos=i;
                break;
            }
        }
        if (pos==-1)
            return;
        str=malloc(pos+1);
        copy(str,tmp->buf,pos);
        push_buf(tmp,pos);
        if (str[pos-1]=='\r')
            str[pos-1]='\0';
        execute(str,tmp,data,bank);
        free(str);
    }
}

int manage(struct session *tmp,struct session *data,struct game bank)
{
    static const char err[]="\nToo long request...\n\n";
    int rc,busy=tmp->buf_used;
    rc=read(tmp->fd,tmp->buf+busy,INBUFSIZE-busy);
    if (rc==0)
        return EOF;
    tmp->buf_used+=rc;
    serve_buf(tmp,data,bank);
    if (tmp->buf_used>=INBUFSIZE){
        send_msg(tmp,err);
        tmp->buf_used=0;
    }
    return 0;
}

void sale2bids(struct player_bid *arr,struct session *data)
{
    int i=0;
    while (data){
        if (data->flag!=over){
            arr[i].count=data->sale.count;
            arr[i].price=data->sale.price;
            arr[i].player=data;
            i++;
        }
        data=data->next;
    }
}

void buy2bids(struct player_bid *arr,struct session *data)
{
    int i=0;
    while (data){
        if (data->flag!=over){
            arr[i].count=data->buy.count;
            arr[i].price=data->buy.price;
            arr[i].player=data;
            i++;
        }
        data=data->next;
    }
}
      
void mixing(struct player_bid *arr,int len)
{
    int i,j;
    struct player_bid tmp;
    for(i=len-1;i>0;i--){
        j=(int)((i+1.0)*rand()/(RAND_MAX+1.0));
        tmp=arr[j];
        arr[j]=arr[i];
        arr[i]=tmp;
    }
}

void sort_inc(struct player_bid *arr,int len)
{
    int i,j;
    struct player_bid tmp;
    for(i=0;i<len-1;i++){
        for(j=0;j<len-i-1;j++){
            if (arr[j].price>arr[j+1].price){
                tmp=arr[j];
                arr[j]=arr[j+1];
                arr[j+1]=tmp;
            }
        }
    }
}

void sort_dec(struct player_bid *arr,int len)
{
    int i,j;
    struct player_bid tmp;
    for(i=0;i<len-1;i++){
        for(j=0;j<len-i-1;j++){
            if (arr[j].price<arr[j+1].price){
                tmp=arr[j];
                arr[j]=arr[j+1];
                arr[j+1]=tmp;
            }
        }
    }
}

void buying_up(struct player_bid *arr,int len,int prod)
{
    int i;
    for(i=0;i<len;i++){
        if (prod>=arr[i].count){
            arr[i].player->sold=arr[i].count;
            prod-=arr[i].count;
        }
        else{
            if (prod>0){
                arr[i].player->sold=prod;
                prod=0;
            }
            else{
                arr[i].player->sold=0;
            }   
        }
    }
}

void sell_out(struct player_bid *arr,int len,int raw)
{
    int i;
    for(i=0;i<len;i++){
        if (raw>=arr[i].count){
            arr[i].player->bought=arr[i].count;
            raw-=arr[i].count;
        }
        else{
            if (raw>0){
                arr[i].player->bought=raw;
                raw=0;
            }
            else{
                arr[i].player->bought=0;
            }
        }
    }
}

void send_results(struct session *data)
{
    static const char s1[]="PLAYER  SOLD  PRICE  BOUGHT  PRICE\n";
    static const char s2[]="%";
    static const char s3[]="\n";
    struct session *tmp,*client=data;
    int res;
    while (client){
        send_msg(client,s1);
        tmp=data;
        while (tmp){
            if (tmp->flag!=over){
                send_msg(client,s2);
                res=send_cnt(client,tmp->id);
                send_space(client,7-res);
                res=send_cnt(client,tmp->sold);
                send_space(client,6-res);
                res=send_cnt(client,tmp->sale.price);
                send_space(client,7-res);
                res=send_cnt(client,tmp->bought);
                send_space(client,8-res);
                res=send_cnt(client,tmp->buy.price);
                send_msg(client,s3);
            }
            tmp=tmp->next;
        }
        send_msg(client,s3);
        client=client->next;
    }
}

void auction(struct session *data,int level)
{
    int active=player_alive(data);
    struct player_bid arr[active];
    struct market info=situation(level,active);
    sale2bids(arr,data);
    mixing(arr,active);
    sort_inc(arr,active);
    buying_up(arr,active,info.prod);
    buy2bids(arr,data);
    mixing(arr,active);
    sort_dec(arr,active);
    sell_out(arr,active,info.raw);
    send_results(data);
}

int build_factory(struct session *tmp)
{
    static const char s1[]="Construction completed: ";
    static const char s2[]="Payment for building: ";
    static const char s3[]="\n";
    static const char s4[]="\n\n";
    static const char s5[]="$";
    int price=tmp->build[0]*2500;
    int i;
    if (tmp->build[0]){
        send_msg(tmp,s1);
        send_cnt(tmp,tmp->build[0]);
        send_msg(tmp,s3);
        send_msg(tmp,s2);
        send_cnt(tmp,price);
        send_msg(tmp,s5);
        send_msg(tmp,s4);
        tmp->factory+=tmp->build[0];
    }
    for(i=0;i<4;i++){
        tmp->build[i]=tmp->build[i+1];
    }
    tmp->build[4]=0;
    return price;
}

void finance_report(struct session *tmp,int earning,int payment)
{
    static const char s1[]="You have earned: ";
    static const char s2[]="Payment: ";
    static const char s3[]="Total cash: ";
    static const char s4[]="$\n";
    static const char s5[]="\n";
    static const char s6[]="You go bankrupt...Bye!\n\n";
    send_msg(tmp,s1);
    send_cnt(tmp,earning);
    send_msg(tmp,s4);
    send_msg(tmp,s2);
    send_cnt(tmp,payment);
    send_msg(tmp,s4);
    send_msg(tmp,s3);
    send_cnt(tmp,tmp->cash);
    send_msg(tmp,s4);
    send_msg(tmp,s5);
    if (tmp->cash<0)
        send_msg(tmp,s6);
}

void finance(struct session *data)
{
    int payment,earning;
    struct session *tmp=data;
    while (tmp){
        if (tmp->flag!=over){
            earning=(tmp->sold)*(tmp->sale.price);
            tmp->product-=tmp->sold;
            payment=0;
            payment+=(tmp->product)*500;
            payment+=(tmp->order)*2000;
            tmp->raw-=tmp->order;
            tmp->product+=tmp->order;
            payment+=(tmp->raw)*300;
            tmp->raw+=tmp->bought;
            payment+=(tmp->bought)*(tmp->buy.price);
            payment+=(tmp->factory)*1000;
            payment+=build_factory(tmp);
            tmp->cash-=payment;
            tmp->cash+=earning;
            finance_report(tmp,earning,payment);
        }
        tmp=tmp->next;
    }
}

int find_winner(struct session *data)
{
    static const char s1[]="Congratulations!!! You win!\n\n";
    int winner=0;
    while (data){
        if (data->flag!=over){
            send_msg(data,s1);
            winner=data->id;
            break;
        }
        data=data->next;
    }
    return winner;
}

void finish_game(struct session *data)
{
    static const char s1[]="Player ";
    static const char s2[]=" won the game!!!\n\n";
    static const char s3[]="All players went bankrupt...\n\n";
    struct session *tmp;
    struct item *del;
    int winner=find_winner(data);
    tmp=data;
    if (winner){
        while (tmp){
            if (tmp->flag==over){
                send_msg(tmp,s1);
                send_cnt(tmp,winner);
                send_msg(tmp,s2);
            }
            tmp=tmp->next;
        }
    }
    else{
        while (tmp){
            send_msg(tmp,s3);
            tmp=tmp->next;
        }
    }
    del=NULL;
    tmp=data;
    while (tmp){
        del=add_elem(del,tmp->fd);
        tmp=tmp->next;
    }
    rm_clients(data,del);
}
 
struct session *update(struct session *data)
{
    static const char s1[]="Player ";
    static const char s2[]=" got bankrupt\n\n";
    struct session *tmp,*q;
    tmp=data;
    while (tmp){
        if (tmp->cash<0&&tmp->flag!=over){
            tmp->flag=over;
            q=data;
            while (q){
                send_msg(q,s1);
                send_cnt(q,tmp->id);
                send_msg(q,s2);
                q=q->next;
            }
        }
        tmp=tmp->next;
    }
    if (player_alive(data)<=1){
        finish_game(data);
        data=NULL;
    }
    return data;
}

struct game def_set(int count_start)
{
    struct game bank;
    bank.level=3;
    bank.month=1;
    bank.play=off;
    bank.count=count_start;
    return bank;
}

int change_level(int level)
{
    static const int distribution[5][5]={
        {4,4,2,1,1},
        {3,4,3,1,1},
        {1,3,4,3,1},
        {1,1,3,4,3},
        {1,1,2,4,4}
    };
    int i,sum=0;
    int r=1+(int)(12.0*rand()/(RAND_MAX+1.0));
    for(i=0;i<5;i++){
        sum+=distribution[level-1][i];
        if (sum>=r){
            break;
        }
    }
    return i+1;
}

int check_fin(struct session *data)
{
    while (data){
        if (data->flag==work)
            return 1;
        data=data->next;
    }
    return 0;
}

void wait2work(struct session *data)
{
    static const char s1[]="Game started, now you can play\n";
    static const char s2[]="Have a nice game, good luck!\n\n\r";
    while (data){
        send_msg(data,s1);
        send_msg(data,s2);
        data->flag=work;
        data=data->next;
    }
}

void fin2work(struct session *data)
{
    static const char s1[]="\r";
    while (data){
        if (data->flag!=over){
            data->flag=work;
        }
        send_msg(data,s1);
        data=data->next;
    }
}

void reset_all(struct session *data)
{
    while (data){
        reset_request(data);
        data=data->next;
    }
}

struct session *gameplay(struct session *data,struct game *bank)
{
    if (bank->play==off){
        if (client_count(data)<bank->count)
            return data;
        fprintf(stderr,"game started\n");
        wait2work(data);
        bank->play=on;
        return data;
    }
    if (check_fin(data))
        return data;
    auction(data,bank->level);
    finance(data);
    data=update(data);
    reset_all(data);
    fin2work(data);
    bank->month++;
    bank->level=change_level(bank->level);
    if (!data){
        *bank=def_set(bank->count);
        fprintf(stderr,"game finished\n");
    }
    return data;
}

void break_connection(int fd)
{
    static const char s1[]="\nThe game has already started\n";
    static const char s2[]="Please, try again later\n\n";
    write(fd,s1,lenstr(s1));
    write(fd,s2,lenstr(s2));
    shutdown(fd,2);
    close(fd);
}

void close_server(struct session *data,int ls)
{
    struct item *del=NULL;
    struct session *tmp;
    fprintf(stderr,"\n");
    tmp=data;
    while (tmp){
        del=add_elem(del,tmp->fd);
        tmp=tmp->next;
    }
    rm_clients(data,del);
    shutdown(ls,2);
    close(ls);
    fprintf(stderr,"server is stopped\n");
    exit(0);
}

void handler()
{
    server_status=FINISH;
}

void start(int ls,int count)
{
    struct session *data=NULL,*tmp;
    struct game bank=def_set(count);
    struct item *del;
    int fd,res,max_d;
    fd_set readfds;
    for(;;){
        FD_ZERO(&readfds);
        FD_SET(ls,&readfds);
        max_d=ls;
        tmp=data;
        while (tmp){
            FD_SET(tmp->fd,&readfds);
            if (tmp->fd>max_d)
                max_d=tmp->fd;
            tmp=tmp->next;
        }
        res=select(max_d+1,&readfds,NULL,NULL,NULL);
        if (server_status==FINISH)
            close_server(data,ls);
        if (res<1)
            continue;
        if (FD_ISSET(ls,&readfds)){
            fd=accept(ls,NULL,NULL);
            if (fd!=-1){
                if (bank.play!=on){
                    data=add_client(data,fd);
                }
                else{
                    break_connection(fd);
                }
            }
        }
        tmp=data;
        del=NULL;
        while (tmp){
            if (FD_ISSET(tmp->fd,&readfds)){
                if (EOF==manage(tmp,data,bank))
                    del=add_elem(del,tmp->fd);
            }
            tmp=tmp->next;
        }
        data=rm_clients(data,del);
        data=gameplay(data,&bank);
    }
}

void init(int port,int count)
{
    int ls,opt=1;
    struct sockaddr_in addr;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    ls=socket(AF_INET,SOCK_STREAM,0);
    if (ls==-1){
        perror("socket");
        exit(1);
    }
    if (0!=setsockopt(ls,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))){
        perror("setsockopt");
        exit(1);
    }
    if (0!=bind(ls,(struct sockaddr*)&addr,sizeof(addr))){
        perror("bind");
        exit(1);
    }
    if (-1==listen(ls,32)){
        perror("listen");
        exit(1);
    }
    srand(time(NULL));
    signal(SIGINT,handler);
    signal(SIGPIPE,SIG_IGN);
    fprintf(stderr,"server is running\n");
    start(ls,count);
}

int main(int argc,char **argv)
{
    if (argc<3){
        fprintf(stderr,"Too few arguments\n");
        exit(1);
    }
    if (argc>3){
        fprintf(stderr,"Too many arguments\n");
        exit(1);
    }
    int port=str2int(argv[1]);
    int count=str2int(argv[2]);
    if (port==-1||count==-1){
        fprintf(stderr,"Wrong arguments\n");
        exit(1);
    }
    init(port,count);
    return 0;
}

