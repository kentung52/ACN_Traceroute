#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <netdb.h>

// 計算校驗和函數
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    // 計算校驗和 (Checksum calculation)
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// 發送 ICMP 請求並接收響應
void send_icmp(int sockfd, struct sockaddr_in *dest_addr, int ttl, int hop_limit) {
    char sendbuf[512];  // 發送緩衝區 (Send buffer)
    struct icmp *icmp_hdr = (struct icmp *)sendbuf;  // ICMP 頭部 (ICMP header)
    struct sockaddr_in recv_addr;  // 用於接收的地址結構 (Address structure for receiving)
    socklen_t addr_len = sizeof(recv_addr);
    int seq = 1;  // ICMP 序列號 (ICMP sequence number)
    int response_received = 0;  // 標記是否收到響應 (Flag to indicate if response is received)

    // 初始化 ICMP 請求 (Initialize ICMP request)
    memset(sendbuf, 0, sizeof(sendbuf));
    icmp_hdr->icmp_type = ICMP_ECHO;  // ICMP Echo Request
    icmp_hdr->icmp_code = 0;
    icmp_hdr->icmp_cksum = 0;
    icmp_hdr->icmp_seq = seq;
    icmp_hdr->icmp_id = getpid();  // 使用進程 ID 作為 ICMP 識別碼 (Use process ID as ICMP identifier)

    // 計算 ICMP 校驗和 (Compute ICMP checksum)
    icmp_hdr->icmp_cksum = checksum(icmp_hdr, sizeof(sendbuf));

    // 設置 IP 層的 TTL (Time To Live)
    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    // 發送 ICMP Echo 請求 (Send ICMP Echo Request)
    printf("Sending ICMP Echo Request with TTL %d...\n", ttl);
    if (sendto(sockfd, sendbuf, sizeof(sendbuf), 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr)) < 0) {
        perror("sendto");
        return;
    }

    // 設置接收的超時時間 (Set receive timeout)
    struct timeval timeout;
    timeout.tv_sec = 2;  // 超時時間設為 2 秒 (Set timeout to 2 seconds)
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char recvbuf[1024];  // 接收緩衝區 (Receive buffer)
    if (recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&recv_addr, &addr_len) < 0) {
        // 如果在超時內沒有收到響應，顯示超時信息並返回 (If no response within timeout, display message and return)
        printf("***\n");
        return;
    }

    // 解析接收到的 IP 頭部和 ICMP 頭部 (Parse received IP and ICMP headers)
    struct iphdr *ip_hdr = (struct iphdr *)recvbuf;
    struct icmp *icmp_recv_hdr = (struct icmp *)(recvbuf + (ip_hdr->ihl * 4));

    // 檢查 ICMP 響應類型 (Check ICMP response type)
    if (icmp_recv_hdr->icmp_type == ICMP_TIME_EXCEEDED) {
        // 如果是 ICMP 時間超時，顯示路由器的 IP 地址 (If ICMP Time Exceeded, display router IP)
        printf("Time exceeded from %s\n", inet_ntoa(recv_addr.sin_addr));
        response_received = 1;
    } else if (icmp_recv_hdr->icmp_type == ICMP_ECHOREPLY && icmp_recv_hdr->icmp_id == getpid()) {
        // 如果是 ICMP 回顯響應，顯示目標的 IP 地址 (If ICMP Echo Reply, display destination IP)
        printf("%d-hop router IP: %s\n", ttl, inet_ntoa(recv_addr.sin_addr));
        response_received = 1;  // 目標已找到 (Target reached)
    }

    // 如果到達跳數限制 (If reached hop limit)
    if (ttl == hop_limit) {
        printf("Reached hop limit.\n");
        response_received = 1;
    }

    // 如果沒有收到有效響應，顯示超時信息 (If no valid response, display timeout message)
    if (!response_received) {
        printf("No response within hop limit.\n");
    }
}

// 主函數：處理命令行參數並初始化 (Main function: handle command line arguments and initialize)
int main(int argc, char *argv[]) {
    if (argc != 3) {
        // 檢查命令行參數的數量是否正確 (Check if number of arguments is correct)
        fprintf(stderr, "Usage: %s <hop-distance> <destination>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int hop_limit = atoi(argv[1]);  // 解析跳數限制 (Parse hop limit)
    char *destination = argv[2];  // 解析目標 IP 地址 (Parse destination IP address)
    int sockfd;

    // 創建原始套接字 (Create raw socket)
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in dest_addr;  // 目標地址結構 (Destination address structure)
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;

    // 將目標 IP 地址轉換為網路字節順序 (Convert destination IP to network byte order)
    if (inet_pton(AF_INET, destination, &dest_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    // 遍歷每個 TTL，逐跳發送 ICMP 請求 (Iterate over each TTL and send ICMP request)
    for (int ttl = 1; ttl <= hop_limit; ttl++) {
        send_icmp(sockfd, &dest_addr, ttl, hop_limit);
        sleep(1);  // 每次請求之間等待 1 秒 (Wait 1 second between each request)
    }

    close(sockfd);  // 關閉套接字 (Close socket)
    return 0;
}

