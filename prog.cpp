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

// �p�����M���
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    // �p�����M (Checksum calculation)
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// �o�e ICMP �ШD�ñ����T��
void send_icmp(int sockfd, struct sockaddr_in *dest_addr, int ttl, int hop_limit) {
    char sendbuf[512];  // �o�e�w�İ� (Send buffer)
    struct icmp *icmp_hdr = (struct icmp *)sendbuf;  // ICMP �Y�� (ICMP header)
    struct sockaddr_in recv_addr;  // �Ω󱵦����a�}���c (Address structure for receiving)
    socklen_t addr_len = sizeof(recv_addr);
    int seq = 1;  // ICMP �ǦC�� (ICMP sequence number)
    int response_received = 0;  // �аO�O�_�����T�� (Flag to indicate if response is received)

    // ��l�� ICMP �ШD (Initialize ICMP request)
    memset(sendbuf, 0, sizeof(sendbuf));
    icmp_hdr->icmp_type = ICMP_ECHO;  // ICMP Echo Request
    icmp_hdr->icmp_code = 0;
    icmp_hdr->icmp_cksum = 0;
    icmp_hdr->icmp_seq = seq;
    icmp_hdr->icmp_id = getpid();  // �ϥζi�{ ID �@�� ICMP �ѧO�X (Use process ID as ICMP identifier)

    // �p�� ICMP ����M (Compute ICMP checksum)
    icmp_hdr->icmp_cksum = checksum(icmp_hdr, sizeof(sendbuf));

    // �]�m IP �h�� TTL (Time To Live)
    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    // �o�e ICMP Echo �ШD (Send ICMP Echo Request)
    printf("Sending ICMP Echo Request with TTL %d...\n", ttl);
    if (sendto(sockfd, sendbuf, sizeof(sendbuf), 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr)) < 0) {
        perror("sendto");
        return;
    }

    // �]�m�������W�ɮɶ� (Set receive timeout)
    struct timeval timeout;
    timeout.tv_sec = 2;  // �W�ɮɶ��]�� 2 �� (Set timeout to 2 seconds)
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char recvbuf[1024];  // �����w�İ� (Receive buffer)
    if (recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&recv_addr, &addr_len) < 0) {
        // �p�G�b�W�ɤ��S�������T���A��ܶW�ɫH���ê�^ (If no response within timeout, display message and return)
        printf("***\n");
        return;
    }

    // �ѪR�����쪺 IP �Y���M ICMP �Y�� (Parse received IP and ICMP headers)
    struct iphdr *ip_hdr = (struct iphdr *)recvbuf;
    struct icmp *icmp_recv_hdr = (struct icmp *)(recvbuf + (ip_hdr->ihl * 4));

    // �ˬd ICMP �T������ (Check ICMP response type)
    if (icmp_recv_hdr->icmp_type == ICMP_TIME_EXCEEDED) {
        // �p�G�O ICMP �ɶ��W�ɡA��ܸ��Ѿ��� IP �a�} (If ICMP Time Exceeded, display router IP)
        printf("Time exceeded from %s\n", inet_ntoa(recv_addr.sin_addr));
        response_received = 1;
    } else if (icmp_recv_hdr->icmp_type == ICMP_ECHOREPLY && icmp_recv_hdr->icmp_id == getpid()) {
        // �p�G�O ICMP �^���T���A��ܥؼЪ� IP �a�} (If ICMP Echo Reply, display destination IP)
        printf("%d-hop router IP: %s\n", ttl, inet_ntoa(recv_addr.sin_addr));
        response_received = 1;  // �ؼФw��� (Target reached)
    }

    // �p�G��F���ƭ��� (If reached hop limit)
    if (ttl == hop_limit) {
        printf("Reached hop limit.\n");
        response_received = 1;
    }

    // �p�G�S�����즳���T���A��ܶW�ɫH�� (If no valid response, display timeout message)
    if (!response_received) {
        printf("No response within hop limit.\n");
    }
}

// �D��ơG�B�z�R�O��Ѽƨê�l�� (Main function: handle command line arguments and initialize)
int main(int argc, char *argv[]) {
    if (argc != 3) {
        // �ˬd�R�O��Ѽƪ��ƶq�O�_���T (Check if number of arguments is correct)
        fprintf(stderr, "Usage: %s <hop-distance> <destination>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int hop_limit = atoi(argv[1]);  // �ѪR���ƭ��� (Parse hop limit)
    char *destination = argv[2];  // �ѪR�ؼ� IP �a�} (Parse destination IP address)
    int sockfd;

    // �Ыح�l�M���r (Create raw socket)
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in dest_addr;  // �ؼЦa�}���c (Destination address structure)
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;

    // �N�ؼ� IP �a�}�ഫ�������r�`���� (Convert destination IP to network byte order)
    if (inet_pton(AF_INET, destination, &dest_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    // �M���C�� TTL�A�v���o�e ICMP �ШD (Iterate over each TTL and send ICMP request)
    for (int ttl = 1; ttl <= hop_limit; ttl++) {
        send_icmp(sockfd, &dest_addr, ttl, hop_limit);
        sleep(1);  // �C���ШD�������� 1 �� (Wait 1 second between each request)
    }

    close(sockfd);  // �����M���r (Close socket)
    return 0;
}

