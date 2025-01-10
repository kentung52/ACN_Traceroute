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

// Function to calculate checksum
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    // Calculate the checksum
    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// Send ICMP request and receive response
void send_icmp(int sockfd, struct sockaddr_in *dest_addr, int ttl, int hop_limit) {
    char sendbuf[512];  // Send buffer
    struct icmp *icmp_hdr = (struct icmp *)sendbuf;  // ICMP header
    struct sockaddr_in recv_addr;  // Address structure for receiving
    socklen_t addr_len = sizeof(recv_addr);
    int seq = 1;  // ICMP sequence number
    int response_received = 0;  // Flag to indicate if response is received

    // Initialize ICMP request
    memset(sendbuf, 0, sizeof(sendbuf));
    icmp_hdr->icmp_type = ICMP_ECHO;  // ICMP Echo Request
    icmp_hdr->icmp_code = 0;
    icmp_hdr->icmp_cksum = 0;
    icmp_hdr->icmp_seq = seq;
    icmp_hdr->icmp_id = getpid();  // Use process ID as ICMP identifier

    // Compute ICMP checksum
    icmp_hdr->icmp_cksum = checksum(icmp_hdr, sizeof(sendbuf));

    // Set TTL (Time To Live) in the IP layer
    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));

    // Send ICMP Echo Request
    printf("Sending ICMP Echo Request with TTL %d...\n", ttl);
    if (sendto(sockfd, sendbuf, sizeof(sendbuf), 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr)) < 0) {
        perror("sendto");
        return;
    }

    // Set timeout for receiving response
    struct timeval timeout;
    timeout.tv_sec = 2;  // Set timeout to 2 seconds
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    char recvbuf[1024];  // Receive buffer
    if (recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0, (struct sockaddr *)&recv_addr, &addr_len) < 0) {
        // If no response is received within the timeout, display a timeout message and return
        printf("***\n");
        return;
    }

    // Parse received IP header and ICMP header
    struct iphdr *ip_hdr = (struct iphdr *)recvbuf;
    struct icmp *icmp_recv_hdr = (struct icmp *)(recvbuf + (ip_hdr->ihl * 4));

    // Check the ICMP response type
    if (icmp_recv_hdr->icmp_type == ICMP_TIME_EXCEEDED) {
        // If ICMP Time Exceeded, display the router's IP address
        printf("Time exceeded from %s\n", inet_ntoa(recv_addr.sin_addr));
        response_received = 1;
    } else if (icmp_recv_hdr->icmp_type == ICMP_ECHOREPLY && icmp_recv_hdr->icmp_id == getpid()) {
        // If ICMP Echo Reply, display the destination's IP address
        printf("%d-hop router IP: %s\n", ttl, inet_ntoa(recv_addr.sin_addr));
        response_received = 1;  // Target reached
    }

    // If hop limit is reached
    if (ttl == hop_limit) {
        printf("Reached hop limit.\n");
        response_received = 1;
    }

    // If no valid response is received, display a timeout message
    if (!response_received) {
        printf("No response within hop limit.\n");
    }
}

// Main function: handle command line arguments and initialize
int main(int argc, char *argv[]) {
    if (argc != 3) {
        // Check if the number of command line arguments is correct
        fprintf(stderr, "Usage: %s <hop-distance> <destination>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int hop_limit = atoi(argv[1]);  // Parse hop limit
    char *destination = argv[2];  // Parse destination IP address
    int sockfd;

    // Create raw socket
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in dest_addr;  // Destination address structure
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;

    // Convert destination IP to network byte order
    if (inet_pton(AF_INET, destination, &dest_addr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }

    // Iterate over each TTL and send ICMP request
    for (int ttl = 1; ttl <= hop_limit; ttl++) {
        send_icmp(sockfd, &dest_addr, ttl, hop_limit);
        sleep(1);  // Wait 1 second between each request
    }

    close(sockfd);  // Close socket
    return 0;
}

