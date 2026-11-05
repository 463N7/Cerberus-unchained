#include "wardrive_upload_reply.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* A bounded token tree avoids a heap allocation for every server field. Only
 * strict ASCII names/receipts are interpreted; escaped/non-ASCII strings are
 * still syntax-checked and cannot alias a decision key. */
typedef struct { uint16_t start, end, next; char type; } token_t;
typedef struct { const char *s; size_t n, pos; unsigned count; token_t t[160]; } json_t;
static void space(json_t *j) { while (j->pos < j->n && strchr(" \r\n\t", j->s[j->pos])) ++j->pos; }
static bool ch(json_t *j, char c) { space(j); if (j->pos < j->n && j->s[j->pos] == c) { ++j->pos; return true; } return false; }
static bool hex4(json_t *j, unsigned *v)
{
    *v=0;
    for (unsigned i=0;i<4;++i) {
        if (j->pos == j->n) return false;
        unsigned char c=j->s[j->pos++]; unsigned x;
        if(c>='0'&&c<='9')x=c-'0'; else if(c>='a'&&c<='f')x=c-'a'+10;
        else if(c>='A'&&c<='F')x=c-'A'+10; else return false;
        *v=*v*16+x;
    } return true;
}
static bool string(json_t *j)
{
    if (!ch(j,'"')) return false;
    while(j->pos<j->n) {
        unsigned c=(unsigned char)j->s[j->pos++];
        if(c=='"')return true;
        if(c<32)return false;
        if(c=='\\') {
            if(j->pos==j->n)return false;
            c=(unsigned char)j->s[j->pos++];
            if(c=='u') {
                if(!hex4(j,&c))return false;
                if(c>=0xd800&&c<=0xdbff) {
                    unsigned low;
                    if(j->n-j->pos<2||j->s[j->pos++]!='\\'||j->s[j->pos++]!='u'||
                        !hex4(j,&low)||low<0xdc00||low>0xdfff)return false;
                } else if(c>=0xdc00&&c<=0xdfff)return false;
            } else if(!strchr("\"\\/bfnrt",(int)c))return false;
        } else if(c>=128) {
            unsigned more,min;
            if(c>=0xc2&&c<=0xdf){more=1;min=0x80;c&=31;}
            else if(c>=0xe0&&c<=0xef){more=2;min=0x800;c&=15;}
            else if(c>=0xf0&&c<=0xf4){more=3;min=0x10000;c&=7;} else return false;
            while(more--){if(j->pos==j->n||((unsigned char)j->s[j->pos]&0xc0)!=0x80)return false;
                c=c*64+((unsigned char)j->s[j->pos++]&63);}
            if(c<min||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return false;
        }
    } return false;
}
static bool value(json_t *j,unsigned depth)
{
    space(j); if(depth>10||j->pos==j->n||j->count==160)return false;
    unsigned index=j->count++; token_t *t=&j->t[index];t->start=(uint16_t)j->pos;
    char c=j->s[j->pos]; t->type=c;
    if(c=='{'||c=='[') {
        ++j->pos; char end=c=='{'?'}':']';
        if(!ch(j,end))for(;;) {
            if(c=='{') {
                space(j); unsigned key=j->count;
                if(j->pos==j->n||j->s[j->pos]!='"'||!value(j,depth+1)||!ch(j,':'))return false;
                if(memchr(j->s+j->t[key].start,'\\',j->t[key].end-j->t[key].start))return false;
                /* Reject duplicate keys, including unknown ones. Escaped names
                 * cannot be used as protocol keys and so fail closed. */
                for(unsigned k=index+1;k<key;k=j->t[j->t[k].next].next)
                    if(j->t[k].end-j->t[k].start==j->t[key].end-j->t[key].start&&
                        !memcmp(j->s+j->t[k].start,j->s+j->t[key].start,j->t[k].end-j->t[k].start))return false;
            }
            if(!value(j,depth+1))return false;
            if(ch(j,end))break;
            if(!ch(j,','))return false;
        }
    } else if(c=='"') { if(!string(j))return false; }
    else if(c=='t'||c=='f'||c=='n') {
        const char *v=c=='t'?"true":c=='f'?"false":"null";size_t n=strlen(v);
        if(j->n-j->pos<n||memcmp(j->s+j->pos,v,n))return false;
        j->pos+=n;
    } else {
        if(c=='-')++j->pos;
        if(j->pos==j->n)return false;
        c=j->s[j->pos];
        if(c=='0')++j->pos;
        else { if(c<'1'||c>'9')return false;do{++j->pos;}while(j->pos<j->n&&j->s[j->pos]>='0'&&j->s[j->pos]<='9'); }
        if(j->pos<j->n&&j->s[j->pos]=='.') {
            size_t start=++j->pos;while(j->pos<j->n&&j->s[j->pos]>='0'&&j->s[j->pos]<='9')++j->pos;
            if(j->pos==start)return false;
        }
        if(j->pos<j->n&&(j->s[j->pos]=='e'||j->s[j->pos]=='E')) {
            ++j->pos;if(j->pos<j->n&&(j->s[j->pos]=='+'||j->s[j->pos]=='-'))++j->pos;
            size_t start=j->pos;while(j->pos<j->n&&j->s[j->pos]>='0'&&j->s[j->pos]<='9')++j->pos;
            if(j->pos==start)return false;
        } t->type='0';
    }
    t->end=(uint16_t)j->pos;t->next=(uint16_t)j->count;return true;
}
static int field(const json_t *j,int parent,const char *key)
{
    if(parent<0||j->t[parent].type!='{')return -1;
    size_t n=strlen(key);
    for(unsigned k=(unsigned)parent+1;k<j->t[parent].next;) {
        const token_t *t=&j->t[k];unsigned v=t->next;
        if(t->end-t->start==n+2&&!memcmp(j->s+t->start+1,key,n))return (int)v;
        k=j->t[v].next;
    } return -1;
}
static bool literal(const json_t *j,int i,const char *v)
{return i>=0&&j->t[i].end-j->t[i].start==strlen(v)&&!memcmp(j->s+j->t[i].start,v,strlen(v));}
static bool count(const json_t *j,int i,uint32_t *out)
{
    if(i<0||j->t[i].type!='0')return false;
    uint32_t v=0;
    for(unsigned k=j->t[i].start;k<j->t[i].end;++k){unsigned d=(unsigned char)j->s[k]-'0';
        if(d>9||v>(UINT32_MAX-d)/10)return false;
        v=v*10+d;}*out=v;return true;
}
static bool receipt_id(const json_t *j,int i,char out[81])
{
    if(i<0||j->t[i].type!='"')return false;
    unsigned n=j->t[i].end-j->t[i].start-2;if(!n||n>80)return false;
    const char *s=j->s+j->t[i].start+1;
    for(unsigned k=0;k<n;++k)if(!((s[k]>='0'&&s[k]<='9')||(s[k]>='A'&&s[k]<='Z')||
        (s[k]>='a'&&s[k]<='z')||s[k]=='-'||s[k]=='_'))return false;
    memcpy(out,s,n);out[n]=0;return true;
}
cerberus_upload_reply_t cerberus_upload_reply(cerberus_upload_service_t service,
    unsigned status,const char *body,size_t length,bool complete)
{
    cerberus_upload_reply_t out={.result=CERBERUS_UPLOAD_UNKNOWN};
    if(!complete||!body||!length||length>4096||memchr(body,0,length)||(unsigned)service>=CERBERUS_UPLOAD_SERVICE_COUNT)return out;
    json_t j={.s=body,.n=length};
    if(!value(&j,0))return out;
    space(&j);if(j.pos!=j.n||j.t[0].type!='{')return out;
    const char *key=service==CERBERUS_UPLOAD_WIGLE?"success":service==CERBERUS_UPLOAD_WDGWARS?"ok":"accepted";
    int success=field(&j,0,key);
    if(literal(&j,success,"false")&&(status==400||status==401||status==403||status==413||status==422||status==429||status==200)) {
        out.result=CERBERUS_UPLOAD_REJECTED;return out;
    }
    if(status!=200||!literal(&j,success,"true")||field(&j,0,"error")>=0)return out;
    if(service==CERBERUS_UPLOAD_WIGLE) {
        int results=field(&j,0,"results"),trans=field(&j,results,"transids");
        if(trans<0||j.t[trans].type!='['||trans+1>=j.t[trans].next)return out;
        int first=trans+1;
        if(j.t[first].next!=j.t[trans].next||!receipt_id(&j,field(&j,first,"transId"),out.transaction))return out;
        out.result=CERBERUS_UPLOAD_QUEUED;
    } else {
        if(service==CERBERUS_UPLOAD_WDGWARS) {
            if(!count(&j,field(&j,0,"imported"),&out.imported))return out;
            out.counts_valid=true;uint32_t a=0,b=0;
            int ai=field(&j,0,"no_gps"),bi=field(&j,0,"bad_rows");
            if((ai>=0&&!count(&j,ai,&a))||(bi>=0&&!count(&j,bi,&b))||a>UINT32_MAX-b)return out;
            out.rejected_rows=a+b;
        }
        out.result=CERBERUS_UPLOAD_ACCEPTED;
    } return out;
}
const char *cerberus_upload_result_name(cerberus_upload_result_t result)
{
    static const char *const names[]={"NOT SENT","SENDING","QUEUED","ACCEPTED","REJECTED","CHECK ACCOUNT"};
    return (unsigned)result<sizeof(names)/sizeof(*names)?names[result]:"CHECK ACCOUNT";
}
bool cerberus_upload_reply_merge(cerberus_upload_reply_t *aggregate,const cerberus_upload_reply_t *part)
{
    if(!aggregate||!part)return false;
    static const unsigned priority[]={2,5,1,0,3,5};
    unsigned a=(unsigned)aggregate->result,p=(unsigned)part->result;
    if(a>=6||p>=6){aggregate->result=CERBERUS_UPLOAD_UNKNOWN;return false;}
    if(priority[p]>priority[a]){
        aggregate->result=part->result==CERBERUS_UPLOAD_SENDING?CERBERUS_UPLOAD_UNKNOWN:part->result;
        memcpy(aggregate->transaction,part->transaction,sizeof(aggregate->transaction));
    }
    if(part->counts_valid&&part->result==CERBERUS_UPLOAD_ACCEPTED){
        if(aggregate->imported>UINT32_MAX-part->imported||aggregate->rejected_rows>UINT32_MAX-part->rejected_rows){
            aggregate->result=CERBERUS_UPLOAD_UNKNOWN;return false;
        }
        aggregate->counts_valid=true;aggregate->imported+=part->imported;aggregate->rejected_rows+=part->rejected_rows;
    }
    return true;
}
