#include <iostream>
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <cstdio>
#include <sys/time.h>
#include <unistd.h>
#include <thread>
#include <stdio.h>
#include <atomic>
#include <unordered_map>
#include "tsl/robin_map.h"


using namespace std;
timeval ltm_basic;
#define LOG_INFO(message, args...)  fprintf(stdout, "%-10s | " message "\n" , timenow(), ## args)
static inline char *timenow() {
    static char buffer[64];
    timeval ltm;
    gettimeofday( &ltm, NULL);
    sprintf(buffer,"[%.2lfs]", (ltm.tv_sec + ltm.tv_usec/1000000.0 - ltm_basic.tv_sec - ltm_basic.tv_usec/1000000.0));
    return buffer;
}

struct Pair { char t, o; };
#define P(T) T, '0',  T, '1', T, '2', T, '3', T, '4', T, '5', T, '6', T, '7', T, '8', T, '9'
static const Pair s_pairs[] = { P('0'), P('1'), P('2'), P('3'), P('4'), P('5'), P('6'), P('7'), P('8'), P('9') };

#define W(N, I) *(Pair*)&b[N] = s_pairs[I]
#define A(N) t = (uint64_t(1) << (32 + N / 5 * N * 53 / 16)) / uint32_t(1e##N) + 1 + N/6 - N/8, t *= u, t >>= N / 5 * N * 53 / 16, t += N / 6 * 4, W(0, t >> 32)
#define S(N) b[N] = char(uint64_t(10) * uint32_t(t) >> 32) + '0'
#define D(N) t = uint64_t(100) * uint32_t(t), W(N, t >> 32)

#define L0 b[0] = char(u) + '0'
#define L1 W(0, u)
#define L2 A(1), S(2)
#define L3 A(2), D(2)
#define L4 A(3), D(2), S(4)
#define L5 A(4), D(2), D(4)
#define L6 A(5), D(2), D(4), S(6)
#define L7 A(6), D(2), D(4), D(6)
#define L8 A(7), D(2), D(4), D(6), S(8)
#define L9 A(8), D(2), D(4), D(6), D(8)
#define LN(N) (L##N, b += N + 1)
#define LZ LN
#define LG(F) (u<100 ? u<10 ? F(0) : F(1) : u<1000000 ? u<10000 ? u<1000 ? F(2) : F(3) : u<100000 ? F(4) : F(5) : u<100000000 ? u<10000000 ? F(6) : F(7) : u<1000000000 ? F(8) : F(9))
//static inline __attribute__((always_inline))
inline char* u64toa_jeaiii(uint64_t n, char* b)
{
    uint32_t u;
    uint64_t t;

    if (uint32_t(n >> 32) == 0){
        u = uint32_t(n);
        return LG(LZ);
    }

    uint64_t a = n / 100000000;

    if (uint32_t(a >> 32) == 0)
    {
        u = uint32_t(a);
        LG(LN);
    }
    else
    {
        u = uint32_t(a / 100000000);
        LG(LN);
        u = a % 100000000;
        LN(7);
    }

    u = n % 100000000;
    return LZ(7);
}

string accountFile = "./scale1/account.csv";
string transferFile = "./scale1/transfer.csv";
string resultFile = "./scale1/result.csv";
const uint32_t MAXV = 10000005;
const uint32_t MAXA = 100000;
const uint32_t THREAD_NUM = 32;
tsl::robin_map<uint64_t, uint32_t> id2index(MAXV);
uint64_t index2id[MAXV];
uint32_t read_edge_offset[THREAD_NUM];
uint32_t v_total;
uint32_t e_total;
struct Edge{
    uint32_t to;
    uint64_t timestamp;
    uint64_t amount;
    uint32_t left,right;
};
Edge *edges, *redges;
uint32_t *flg, *rflg;
char* ans_str[THREAD_NUM];
uint64_t ans_str_len[THREAD_NUM][128];
uint32_t thread_node_cnt[THREAD_NUM+1];
void readAccount(){
    auto fd = open(accountFile.c_str(), O_RDONLY);
    size_t fsize = lseek(fd, 0, SEEK_END);
    char* fhead = (char*)mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    char* pt = fhead;
    char* ed = fhead+fsize;
    uint32_t v_cnt = 0;
    while(pt<ed){
        uint64_t num = 0;
        while( *pt != '\n' ){
            num = num * 10 + (*pt - '0');
            pt++;
        }
        if(id2index.find(num)==id2index.end()){
            id2index[num] = v_cnt;
            index2id[v_cnt] = num;
            v_cnt++;
        }
        pt++;
    }
    v_total = v_cnt;
    LOG_INFO("v: %d",v_total);
}

void readTransfer_mt(int tid, char* st, char* ed){
    uint64_t src = 0, dst = 0, timestamp = 0, amount = 0;
    uint32_t e_cnt = 0;
    uint64_t pre_src = -1;
    uint32_t pre_src_index = -1;
    uint32_t src_index, dst_index;

    uint32_t offset = read_edge_offset[tid];


    char* pt = st;
    while(pt<ed){
        src = 0;
        dst = 0;
        timestamp = 0;
        amount = 0;
        while(*pt!=','){
            src = src * 10 + (*pt - '0');
            pt++;
        }
        pt++;
        while(*pt!= ','){
            dst = dst * 10 + (*pt - '0');
            pt++;
        }
        pt++;
        while(*pt!=','){
            timestamp = timestamp * 10 + (*pt - '0');
            pt++;
        }
        pt++;
        while(*pt!='\n'){
            if(*pt!='.')
                amount = amount * 10 + (*pt - '0');
            pt++;
        }
        pt++;
        e_cnt++;

        if(src==pre_src){
            src_index = pre_src_index;
        }
        else{
            src_index = id2index[src];
            pre_src = src;
            pre_src_index = src_index;
            flg[src_index] = offset;
            thread_node_cnt[tid]++;
        }
        dst_index = id2index[dst];
        edges[offset++] = {dst_index, timestamp, amount, 0, 0};
    }
}


bool cmp_forward(const Edge &edge1,const Edge &edge2){return edge1.amount<edge2.amount;}
bool cmp_forward1(const Edge &edge1,const Edge &edge2){return edge1.amount*10<edge2.amount*9;}
bool cmp_forward2(const Edge &edge1,const Edge &edge2){return edge1.amount*11<edge2.amount*10;}

void readTransfer(){
    auto fd = open(transferFile.c_str(), O_RDONLY);
    size_t fsize = lseek(fd, 0, SEEK_END);
    char* fhead = (char*)mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fd, 0);
    char* st[THREAD_NUM];
    char* ed[THREAD_NUM];
    uint64_t split = fsize/THREAD_NUM;


    for(uint32_t i=0;i<THREAD_NUM;i++){
        if(i==0)
            st[i] = fhead;
        else
            st[i] = ed[i-1];

        if(i==THREAD_NUM-1)
            ed[i] = fhead+fsize;
        else{
            char* now = st[i] + split;
            while(*now!='\n'){
                now++;
            }
            now++;
            uint64_t pre_num = -1;
            uint64_t cur_num = -1;
            char* cur_offset = nullptr;
            while(cur_num==pre_num){
                cur_offset = now;
                cur_num = 0;
                while(*now!=','){
                    cur_num = cur_num*10+(*now-'0');
                    now++;
                }
                if(pre_num==-1){
                    pre_num = cur_num;
                }
                while(*now!='\n'){
                    now++;
                }
                now++;
            }
            ed[i] = cur_offset;
        }
    }
    thread count_td[THREAD_NUM];
    for(uint32_t i=0;i<THREAD_NUM;i++){
        count_td[i] = thread(
                [=](){
                    uint32_t cnt = 0;
                    char* st_pos = st[i];
                    char* ed_pos = ed[i];
                    for(char* pos=st_pos;pos<ed_pos;pos++){
                        if(*pos=='\n'){
                            cnt++;
                        }
                    }
                    read_edge_offset[i] = cnt;
                }
        );
    }
    for(uint32_t i=0;i<THREAD_NUM;i++){
        count_td[i].join();
    }



    e_total = 0;
    for(uint32_t i=0;i<THREAD_NUM;i++){
        uint32_t tmp = read_edge_offset[i];
        read_edge_offset[i] = e_total;
        e_total += tmp;
    }


    edges = (Edge *) mmap(nullptr, e_total * sizeof(Edge), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    madvise(edges, e_total * sizeof(Edge), MADV_HUGEPAGE);
    flg = (uint32_t *) mmap(nullptr, (v_total+1) * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    madvise(flg, (v_total+1) * sizeof(uint32_t), MADV_HUGEPAGE);



    thread td[THREAD_NUM];
    for(uint32_t i=0;i<THREAD_NUM;i++){
        td[i] = thread(readTransfer_mt, i, st[i], ed[i]);
    }
    for(uint32_t i=0;i<THREAD_NUM;i++)
        td[i].join();
    flg[v_total] = e_total;
    LOG_INFO("e: %d", e_total);

    for(uint32_t i=1;i<THREAD_NUM;i++){
        thread_node_cnt[i]+=thread_node_cnt[i-1];
    }

    for(uint32_t i=THREAD_NUM-1;i>=1;i--){
        thread_node_cnt[i]=thread_node_cnt[i-1];
    }
    thread_node_cnt[0]=0;
    thread_node_cnt[THREAD_NUM]=v_total;


    redges = (Edge *) mmap(nullptr, e_total * sizeof(Edge), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    madvise(redges, e_total * sizeof(Edge), MADV_HUGEPAGE);
    rflg = (uint32_t *) mmap(nullptr, (v_total+1) * sizeof(uint32_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    madvise(rflg, (v_total+1) * sizeof(uint32_t), MADV_HUGEPAGE);

    uint32_t **tmp_rloc = new uint32_t* [THREAD_NUM];
    for(uint32_t i=0;i <THREAD_NUM;i++)
        tmp_rloc[i] = new uint32_t[v_total]{};
    vector<thread> threads_3(THREAD_NUM);
    for(uint32_t k=0;k<THREAD_NUM;k++){

        threads_3[k] = thread(
                [=]() {
                    for(uint32_t i=thread_node_cnt[k];i<thread_node_cnt[k+1];i++){
                        for(uint32_t j=flg[i];j<flg[i+1];j++){
                            tmp_rloc[k][edges[j].to]++;
                        }
                    }
                });
    }
    for (auto &thread : threads_3) {
        thread.join();
    }
    for(uint32_t j=0;j<THREAD_NUM;j++){
        for(uint32_t i=0;i<v_total;i++){
            rflg[i]+=tmp_rloc[j][i];
            if(j>0){
                tmp_rloc[j][i]+=tmp_rloc[j-1][i];
            }
        }
    }
    for(int32_t j=THREAD_NUM-1;j>=0;j--){
        for(uint32_t i=0;i<v_total;i++){
            if(j==0){
                tmp_rloc[0][i]=0;
            }else{
                tmp_rloc[j][i]=tmp_rloc[j-1][i];
            }
        }
    }
    uint64_t sum = 0;
    for(uint32_t i=0;i<v_total;i++){
        sum += rflg[i];
        rflg[i] = sum-rflg[i];
    }
    rflg[v_total] = e_total;

    vector<thread> threads_4(THREAD_NUM);
    for(uint32_t t=0;t<THREAD_NUM;t++){

        threads_4[t] = thread(
                [=]() {
                    for(uint32_t i=thread_node_cnt[t];i<thread_node_cnt[t+1];i++){
                        for(uint32_t j=flg[i];j<flg[i+1];j++){
                            uint32_t k = edges[j].to;
                            uint32_t r = rflg[k]+(tmp_rloc[t][k]++);
                            redges[r] = edges[j];
                            redges[r].to = i;
                        }
                    }
                });
    }
    for (auto &thread : threads_4) {
        thread.join();
    }






    #pragma omp parallel for schedule(dynamic)
    for(uint32_t i=0;i<v_total;i++){
        sort(edges+flg[i],edges+flg[i+1],cmp_forward);
        sort(redges+rflg[i],redges+rflg[i+1],cmp_forward);
    }


    LOG_INFO("bound cut begin");


    Edge tmp{0,0,0,0,0};
    #pragma omp parallel for schedule(dynamic, 32*200) private(tmp)
    for(uint32_t i=0;i<e_total;i++){
        Edge& e1 = edges[i];
        // e1.left = flg[e1.to];
        // while(e1.left<flg[e1.to+1]&&edges[e1.left].amount*10<e1.amount*9)
        //     e1.left++;
        tmp.amount = e1.amount;
        e1.left = lower_bound(edges+flg[e1.to], edges+flg[e1.to+1], tmp, cmp_forward1) - edges;
        while(e1.left<flg[e1.to+1]&&edges[e1.left].timestamp<=e1.timestamp)
            e1.left++;

        e1.right = e1.left;
        while(e1.right <flg[e1.to+1]&&edges[e1.right ].amount*10<=e1.amount*11)
            e1.right ++;
        while(e1.left<e1.right &&edges[e1.right -1].timestamp<=e1.timestamp)
            e1.right --;
    }

    #pragma omp parallel for schedule(dynamic, 32*200) private(tmp)
    for(uint32_t i=0;i<e_total;i++){
        Edge& re1 = redges[i];
        // re1.left = rflg[re1.to];
        // while(re1.left<rflg[re1.to+1]&&redges[re1.left].amount*11<re1.amount*10)
        //     re1.left++;
        tmp.amount = re1.amount;
        re1.left = lower_bound(redges+rflg[re1.to], redges+rflg[re1.to+1], tmp, cmp_forward2) - redges;
        while(re1.left<rflg[re1.to+1]&&redges[re1.left].timestamp>=re1.timestamp)
            re1.left++;
        re1.right = re1.left;
        while(re1.right<rflg[re1.to+1]&&redges[re1.right].amount*9<=re1.amount*10)
            re1.right++;
        while(re1.left<re1.right&&redges[re1.right-1].timestamp>=re1.timestamp)
            re1.right--;
    }

    LOG_INFO("bound cut end");




}
struct Back{
    uint32_t v[3];
    uint64_t t[3];
    uint64_t a[3];
    int nxt;
    inline void set(uint32_t v0, uint32_t v1, uint32_t v2,
                    uint64_t t0, uint64_t t1, uint64_t t2,
                    uint64_t a0, uint64_t a1, uint64_t a2,
                    uint32_t nnn){
        v[0] = v0;
        v[1] = v1;
        v[2] = v2;
        t[0] = t0;
        t[1] = t1;
        t[2] = t2;
        a[0] = a0;
        a[1] = a1;
        a[2] = a2;
        nxt = nnn;
    }
};

uint32_t back_cnt[THREAD_NUM][128];
uint32_t ans_cnt[THREAD_NUM][128];



#include <omp.h>
#pragma ide diagnostic ignored "openmp-use-default-none"
void search(){

#pragma omp parallel
    {
        uint32_t tid = omp_get_thread_num();
        Back *back1 = new Back[MAXA];
        uint32_t *head1 = new uint32_t[v_total];
        ans_cnt[tid][0] = 0;
        ans_str_len[tid][0] = 0;
        ans_str[tid] = new char[25000000];
        for(uint32_t i=0;i<v_total;i++){
            head1[i] = -1;
        }
        char* p = ans_str[tid]+ans_str_len[tid][0];
#pragma omp for schedule(dynamic)
        for (uint32_t s = 0; s < v_total; s++) {
            uint32_t cnt = 0;
            uint32_t v1,v2,v3;
            uint64_t t1,t2,t3;
            uint64_t a1,a2,a3;
            for(uint32_t i1=rflg[s];i1<rflg[s+1];i1++){
                Edge& e1 = redges[i1];
                v1 = e1.to;
                t1 = e1.timestamp;
                a1 = e1.amount;
                for(uint32_t i2=e1.left;i2<e1.right;i2++){
                    Edge& e2 = redges[i2];
                    v2 = e2.to;
                    t2 = e2.timestamp;
                    a2 = e2.amount;
                    if(t2>=t1) continue;
                    if(v2==s) continue;
                    //if(a2*9>a1*10||a2*11<a1*10) continue;
                    for(uint32_t i3=e2.left;i3<e2.right;i3++){
                        Edge& e3 = redges[i3];
                        v3 = e3.to;
                        t3 = e3.timestamp;
                        a3 = e3.amount;
                        if(t3>=t2) continue;
                        if(v3==s||v3==v1) continue;
                        //if(a3*9>a2*10||a3*11<a2*10) continue;
                        back1[cnt].set(v3, v2, v1, t3, t2, t1, a3, a2, a1, head1[v3]);
                        head1[v3] = cnt;
                        cnt++;
                    }
                }
            }
            back_cnt[tid][0] = cnt;
            for(uint32_t i1=flg[s];i1<flg[s+1];i1++){
                Edge& e1 = edges[i1];
                v1 = e1.to;
                t1 = e1.timestamp;
                a1 = e1.amount;
                if(head1[v1]!=-1){
                    uint32_t now = head1[v1];
                    while(now!=-1){
                        Back& b = back1[now];
                        now = b.nxt;
                        if(t1>=b.t[0]) continue;
                        if(a1*9>b.a[0]*10||a1*11<b.a[0]*10) continue;
                        //if(b.a[2]*9>a1*10||b.a[2]<a1*10) continue;
                        ans_cnt[tid][0]++;
                        // if(tid==-1)
                        //     printf("(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld))\n",
                        //            index2id[s],t1,a1,
                        //            index2id[b.v[0]],b.t[0],b.a[0],
                        //            index2id[b.v[1]],b.t[1],b.a[1],
                        //            index2id[b.v[2]],b.t[2],b.a[2],
                        //            index2id[s]);

                        *(p++) = '(';
                        p = u64toa_jeaiii(index2id[s], p);
                        *(p++) = ')';
                        *(p++) = '-';
                        *(p++) = '[';
                        p = u64toa_jeaiii(t1, p);
                        *(p++) = ',';
                        *(u64toa_jeaiii(a1, p+1)-3) = '.';
                        p = u64toa_jeaiii(a1/100, p)+3;
                        *(p++) = ']';
                        *(p++) = '-';
                        *(p++) = '>';

                        *(p++) = '(';
                        p = u64toa_jeaiii(index2id[b.v[0]], p);
                        *(p++) = ')';
                        *(p++) = '-';
                        *(p++) = '[';
                        p = u64toa_jeaiii(b.t[0], p);
                        *(p++) = ',';
                        *(u64toa_jeaiii(b.a[0], p+1)-3) = '.';
                        p = u64toa_jeaiii(b.a[0]/100, p)+3;
                        *(p++) = ']';
                        *(p++) = '-';
                        *(p++) = '>';

                        *(p++) = '(';
                        p = u64toa_jeaiii(index2id[b.v[1]], p);
                        *(p++) = ')';
                        *(p++) = '-';
                        *(p++) = '[';
                        p = u64toa_jeaiii(b.t[1], p);
                        *(p++) = ',';
                        *(u64toa_jeaiii(b.a[1], p+1)-3) = '.';
                        p = u64toa_jeaiii(b.a[1]/100, p)+3;
                        *(p++) = ']';
                        *(p++) = '-';
                        *(p++) = '>';

                        *(p++) = '(';
                        p = u64toa_jeaiii(index2id[b.v[2]], p);
                        *(p++) = ')';
                        *(p++) = '-';
                        *(p++) = '[';
                        p = u64toa_jeaiii(b.t[2], p);
                        *(p++) = ',';
                        *(u64toa_jeaiii(b.a[2], p+1)-3) = '.';
                        p = u64toa_jeaiii(b.a[2]/100, p)+3;
                        *(p++) = ']';
                        *(p++) = '-';
                        *(p++) = '>';

                        *(p++) = '(';
                        p = u64toa_jeaiii(index2id[s], p);
                        *(p++) = ')';

                        *(p++) = '\n';
                    }
                }
                for(uint32_t i2=e1.left;i2<e1.right;i2++){
                    Edge& e2 = edges[i2];
                    v2 = e2.to;
                    t2 = e2.timestamp;
                    a2 = e2.amount;
                    if(t1>=t2) continue;
                    if(v2==s) continue;

                    //if(a1*9>a2*10||a1*11<a2*10) continue;
                    if(head1[v2]!=-1){
                        uint32_t now = head1[v2];
                        while(now!=-1){
                            Back& b = back1[now];
                            now = b.nxt;
                            if(t2>=b.t[0]) continue;
                            if(a2*9>b.a[0]*10||a2*11<b.a[0]*10) continue;
                            //if(b.a[2]*9>a1*10||b.a[2]<a1*10) continue;
                            if(b.v[1]==v1||b.v[2]==v1)continue;
                            ans_cnt[tid][0]++;

                            // if(tid==-1)
                            //     printf("(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld))\n",
                            //            index2id[s],t1,a1,
                            //            index2id[v1],t2,a2,
                            //            index2id[b.v[0]],b.t[0],b.a[0],
                            //            index2id[b.v[1]],b.t[1],b.a[1],
                            //            index2id[b.v[2]],b.t[2],b.a[2],
                            //            index2id[s]);

                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[s], p);
                            *(p++) = ')';
                            *(p++) = '-';
                            *(p++) = '[';
                            p = u64toa_jeaiii(t1, p);
                            *(p++) = ',';
                            *(u64toa_jeaiii(a1, p+1)-3) = '.';
                            p = u64toa_jeaiii(a1/100, p)+3;
                            *(p++) = ']';
                            *(p++) = '-';
                            *(p++) = '>';


                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[v1], p);
                            *(p++) = ')';
                            *(p++) = '-';
                            *(p++) = '[';
                            p = u64toa_jeaiii(t2, p);
                            *(p++) = ',';
                            *(u64toa_jeaiii(a2, p+1)-3) = '.';
                            p = u64toa_jeaiii(a2/100, p)+3;
                            *(p++) = ']';
                            *(p++) = '-';
                            *(p++) = '>';

                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[b.v[0]], p);
                            *(p++) = ')';
                            *(p++) = '-';
                            *(p++) = '[';
                            p = u64toa_jeaiii(b.t[0], p);
                            *(p++) = ',';
                            *(u64toa_jeaiii(b.a[0], p+1)-3) = '.';
                            p = u64toa_jeaiii(b.a[0]/100, p)+3;
                            *(p++) = ']';
                            *(p++) = '-';
                            *(p++) = '>';

                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[b.v[1]], p);
                            *(p++) = ')';
                            *(p++) = '-';
                            *(p++) = '[';
                            p = u64toa_jeaiii(b.t[1], p);
                            *(p++) = ',';
                            *(u64toa_jeaiii(b.a[1], p+1)-3) = '.';
                            p = u64toa_jeaiii(b.a[1]/100, p)+3;
                            *(p++) = ']';
                            *(p++) = '-';
                            *(p++) = '>';

                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[b.v[2]], p);
                            *(p++) = ')';
                            *(p++) = '-';
                            *(p++) = '[';
                            p = u64toa_jeaiii(b.t[2], p);
                            *(p++) = ',';
                            *(u64toa_jeaiii(b.a[2], p+1)-3) = '.';
                            p = u64toa_jeaiii(b.a[2]/100, p)+3;
                            *(p++) = ']';
                            *(p++) = '-';
                            *(p++) = '>';

                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[s], p);
                            *(p++) = ')';

                            *(p++) = '\n';
                        }
                    }
                    for(uint32_t i3=e2.left;i3<e2.right;i3++){
                        Edge& e3 = edges[i3];
                        v3 = e3.to;
                        t3 = e3.timestamp;
                        a3 = e3.amount;
                        if(t2>=t3) continue;
                        if(v3==v1) continue;
                        //if(a2*9>a3*10||a2*11<a3*10) continue;
                        if(v3==s){
                            ans_cnt[tid][0]++;

                            // if(tid==-1)
                            //     printf("(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)\n",
                            //            index2id[s],t1,a1,
                            //            index2id[v1],t2,a2,
                            //            index2id[v2],t3,a3,
                            //            index2id[s]);

                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[s], p);
                            *(p++) = ')';
                            *(p++) = '-';
                            *(p++) = '[';
                            p = u64toa_jeaiii(t1, p);
                            *(p++) = ',';
                            *(u64toa_jeaiii(a1, p+1)-3) = '.';
                            p = u64toa_jeaiii(a1/100, p)+3;
                            *(p++) = ']';
                            *(p++) = '-';
                            *(p++) = '>';

                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[v1], p);
                            *(p++) = ')';
                            *(p++) = '-';
                            *(p++) = '[';
                            p = u64toa_jeaiii(t2, p);
                            *(p++) = ',';
                            *(u64toa_jeaiii(a2, p+1)-3) = '.';
                            p = u64toa_jeaiii(a2/100, p)+3;
                            *(p++) = ']';
                            *(p++) = '-';
                            *(p++) = '>';

                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[v2], p);
                            *(p++) = ')';
                            *(p++) = '-';
                            *(p++) = '[';
                            p = u64toa_jeaiii(t3, p);
                            *(p++) = ',';
                            *(u64toa_jeaiii(a3, p+1)-3) = '.';
                            p = u64toa_jeaiii(a3/100, p)+3;
                            *(p++) = ']';
                            *(p++) = '-';
                            *(p++) = '>';

                            *(p++) = '(';
                            p = u64toa_jeaiii(index2id[s], p);
                            *(p++) = ')';
                            *(p++) = '\n';

                            continue;
                        }
                        if(head1[v3]!=-1){
                            uint32_t now = head1[v3];
                            while(now!=-1){
                                Back& b = back1[now];
                                now = b.nxt;
                                if(t3>=b.t[0]) continue;
                                if(a3*9>b.a[0]*10||a3*11<b.a[0]*10) continue;
                                //if(b.a[2]*9>a1*10||b.a[2]<a1*10) continue;
                                if(b.v[1]==v1||b.v[1]==v2||b.v[2]==v1||b.v[2]==v2)continue;
                                ans_cnt[tid][0]++;
                                // if(tid==-1)
                                //     printf("(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld)-[%lld,%lld]->(%lld))\n",
                                //            index2id[s],t1,a1,
                                //            index2id[v1],t2,a2,
                                //            index2id[v2],t3,a3,
                                //            index2id[b.v[0]],b.t[0],b.a[0],
                                //            index2id[b.v[1]],b.t[1],b.a[1],
                                //            index2id[b.v[2]],b.t[2],b.a[2],
                                //            index2id[s]);

                                *(p++) = '(';
                                p = u64toa_jeaiii(index2id[s], p);
                                *(p++) = ')';
                                *(p++) = '-';
                                *(p++) = '[';
                                p = u64toa_jeaiii(t1, p);
                                *(p++) = ',';
                                *(u64toa_jeaiii(a1, p+1)-3) = '.';
                                p = u64toa_jeaiii(a1/100, p)+3;
                                *(p++) = ']';
                                *(p++) = '-';
                                *(p++) = '>';

                                *(p++) = '(';
                                p = u64toa_jeaiii(index2id[v1], p);
                                *(p++) = ')';
                                *(p++) = '-';
                                *(p++) = '[';
                                p = u64toa_jeaiii(t2, p);
                                *(p++) = ',';
                                *(u64toa_jeaiii(a2, p+1)-3) = '.';
                                p = u64toa_jeaiii(a2/100, p)+3;
                                *(p++) = ']';
                                *(p++) = '-';
                                *(p++) = '>';

                                *(p++) = '(';
                                p = u64toa_jeaiii(index2id[v2], p);
                                *(p++) = ')';
                                *(p++) = '-';
                                *(p++) = '[';
                                p = u64toa_jeaiii(t3, p);
                                *(p++) = ',';
                                *(u64toa_jeaiii(a3, p+1)-3) = '.';
                                p = u64toa_jeaiii(a3/100, p)+3;
                                *(p++) = ']';
                                *(p++) = '-';
                                *(p++) = '>';

                                *(p++) = '(';
                                p = u64toa_jeaiii(index2id[b.v[0]], p);
                                *(p++) = ')';
                                *(p++) = '-';
                                *(p++) = '[';
                                p = u64toa_jeaiii(b.t[0], p);
                                *(p++) = ',';
                                *(u64toa_jeaiii(b.a[0], p+1)-3) = '.';
                                p = u64toa_jeaiii(b.a[0]/100, p)+3;
                                *(p++) = ']';
                                *(p++) = '-';
                                *(p++) = '>';

                                *(p++) = '(';
                                p = u64toa_jeaiii(index2id[b.v[1]], p);
                                *(p++) = ')';
                                *(p++) = '-';
                                *(p++) = '[';
                                p = u64toa_jeaiii(b.t[1], p);
                                *(p++) = ',';
                                *(u64toa_jeaiii(b.a[1], p+1)-3) = '.';
                                p = u64toa_jeaiii(b.a[1]/100, p)+3;
                                *(p++) = ']';
                                *(p++) = '-';
                                *(p++) = '>';

                                *(p++) = '(';
                                p = u64toa_jeaiii(index2id[b.v[2]], p);
                                *(p++) = ')';
                                *(p++) = '-';
                                *(p++) = '[';
                                p = u64toa_jeaiii(b.t[2], p);
                                *(p++) = ',';
                                *(u64toa_jeaiii(b.a[2], p+1)-3) = '.';
                                p = u64toa_jeaiii(b.a[2]/100, p)+3;
                                *(p++) = ']';
                                *(p++) = '-';
                                *(p++) = '>';

                                *(p++) = '(';
                                p = u64toa_jeaiii(index2id[s], p);
                                *(p++) = ')';

                                *(p++) = '\n';
                            }
                        }
                    }
                }
            }

            ans_str_len[tid][0] = p - (ans_str[tid]);
            for(uint32_t i=0;i<back_cnt[tid][0];i++){
                head1[back1[i].v[0]] = -1;
            }

        }

    };

    uint32_t ans_sum = 0;
    for(uint32_t i=0;i<THREAD_NUM;i++){
        ans_sum += ans_cnt[i][0];
    }
    LOG_INFO("ans: %d", ans_sum);
}
void writeAnswer(){
    size_t fsize = 0;
    for(uint32_t i=0;i<THREAD_NUM;i++){
        fsize += ans_str_len[i][0];
    }
    auto fd = open(resultFile.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    char* fhead = (char*)mmap(NULL,fsize,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    lseek(fd, fsize-1, SEEK_SET);
    write(fd, "\0", 1);
    char* fnow = fhead;
    for(uint32_t i=0;i<THREAD_NUM;i++){
        memcpy(fnow, ans_str[i], ans_str_len[i][0]);
        fnow += ans_str_len[i][0];
    }
    munmap(fhead, fsize);
    close(fd);
}

int main(int argc, char *argv[]){
    gettimeofday( &ltm_basic, NULL);
    accountFile = argv[1];
    transferFile = argv[2];
    resultFile = argv[3];
    cout<<accountFile<<endl;
    cout<<transferFile<<endl;
    cout<<resultFile<<endl;
    LOG_INFO("start");
    readAccount();
    readTransfer();
    LOG_INFO("read ok");
 
    search();
    LOG_INFO("search ok");
    writeAnswer();
    LOG_INFO("write ok");
    LOG_INFO("end");
}
