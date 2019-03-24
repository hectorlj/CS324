#include<arpa/inet.h>
#include<netinet/in.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
typedef unsigned int dns_rr_ttl;
typedef unsigned short dns_rr_type;
typedef unsigned short dns_rr_class;
typedef unsigned short dns_rdata_len;
typedef unsigned short dns_rr_count;
typedef unsigned short dns_query_id;
typedef unsigned short dns_flags;

typedef struct {
	char *name;
	dns_rr_type type;
	dns_rr_class class;
	dns_rr_ttl ttl;
	dns_rdata_len rdata_len;
	unsigned char *rdata;
} dns_rr;

struct dns_answer_entry;
struct dns_answer_entry {
	char *value;
	struct dns_answer_entry *next;
};
typedef struct dns_answer_entry dns_answer_entry;

void free_answer_entries(dns_answer_entry *ans) {
	dns_answer_entry *next;
	while (ans != NULL) {
		next = ans->next;
		free(ans->value);
		free(ans);
		ans = next;
	}
}

void print_bytes(unsigned char *bytes, int byteslen) {
	int i, j, byteslen_adjusted;
	unsigned char c;

	if (byteslen % 8) {
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	} else {
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++) {
		if (!(i % 8)) {
			if (i > 0) {
				for (j = i - 8; j < i; j++) {
					if (j >= byteslen_adjusted) {
						printf("  ");
					} else if (j >= byteslen) {
						printf("  ");
					} else if (bytes[j] >= '!' && bytes[j] <= '~') {
						printf(" %c", bytes[j]);
					} else {
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted) {
				printf("\n%02X: ", i);
			}
		} else if (!(i % 4)) {
			printf(" ");
		}
		if (i >= byteslen_adjusted) {
			continue;
		} else if (i >= byteslen) {
			printf("   ");
		} else {
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
}

void canonicalize_name(char *name) {
	/*
	 * Canonicalize name in place.  Change all upper-case characters to
	 * lower case and remove the trailing dot if there is any.  If the name
	 * passed is a single dot, "." (representing the root zone), then it
	 * should stay the same.
	 *
	 * INPUT:  name: the domain name that should be canonicalized in place
	 */
	
	int namelen, i;

	// leave the root zone alone
	if (strcmp(name, ".") == 0) {
		return;
	}

	namelen = strlen(name);
	// remove the trailing dot, if any
	if (name[namelen - 1] == '.') {
		name[namelen - 1] = '\0';
	}

	// make all upper-case letters lower case
	for (i = 0; i < namelen; i++) {
		if (name[i] >= 'A' && name[i] <= 'Z') {
			name[i] += 32;
		}
	}
}

int name_ascii_to_wire(char *name, unsigned char *wire) {
	/* 
	 * Convert a DNS name from string representation (dot-separated labels)
	 * to DNS wire format, using the provided byte array (wire).  Return
	 * the number of bytes used by the name in wire format.
	 *
	 * INPUT:  name: the string containing the domain name
	 * INPUT:  wire: a pointer to the array of bytes where the
	 *              wire-formatted name should be constructed
	 * OUTPUT: the length of the wire-formatted name.
	 */
    
    int n = 0;
    unsigned int done = 0;
    char* token;
    while(!done){
        token = strsep(&name, ".");
        if(token == NULL) { done = 1; }
        else {
            unsigned int num = (strlen(token) / sizeof(char));
            wire[n] = num;
            n++;
            for(int i = 0; i < num; i++){
                wire[n] = token[i];
                n++;
            }
        }
    }
    wire[n] = 0;
    return n;
}   

char *name_ascii_from_wire(unsigned char *wire, int *indexp) {
	/* 
	 * Extract the wire-formatted DNS name at the offset specified by
	 * *indexp in the array of bytes provided (wire) and return its string
	 * representation (dot-separated labels) in a char array allocated for
	 * that purpose.  Update the value pointed to by indexp to the next
	 * value beyond the name.
	 *
	 * INPUT:  wire: a pointer to an array of bytes
	 * INPUT:  indexp, a pointer to the index in the wire where the
	 *              wire-formatted name begins
	 * OUTPUT: a string containing the string representation of the name,
	 *              allocated on the heap.
	 */
    char* out = malloc(sizeof(char) * 2048);
    unsigned int index = *indexp;
    unsigned int wEnd = 0;
    unsigned int outN = 0;
    unsigned int done = 0;
    while(!done){
        if(wire[index] == 0){
            if(*indexp < index){
                *indexp = index;
            }
            done = 1;
        } else if (wire[index] == 0xc0){
            index++;
            *indexp = index;
            index = wire[index];
        } else {
            if(index != *indexp){
                out[outN] = '.';
                outN++;
            }
            int numCharsInToken = wire[index];
            index++;
            for (int i = 0; i < numCharsInToken; i++){
                out[outN] = wire[index];
                outN++;
                index++;
            }
        }
    } 
    return out;
}

dns_rr rr_from_wire(unsigned char *wire, int *indexp, int query_only) {
	/* 
	 * Extract the wire-formatted resource record at the offset specified by
	 * *indexp in the array of bytes provided (wire) and return a 
	 * dns_rr (struct) populated with its contents. Update the value
	 * pointed to by indexp to the next value beyond the resource record.
	 *
	 * INPUT:  wire: a pointer to an array of bytes
	 * INPUT:  indexp: a pointer to the index in the wire where the
	 *              wire-formatted resource record begins
	 * INPUT:  query_only: a boolean value (1 or 0) which indicates whether
	 *              we are extracting a full resource record or only a
	 *              query (i.e., in the question section of the DNS
	 *              message).  In the case of the latter, the ttl,
	 *              rdata_len, and rdata are skipped.
	 * OUTPUT: the resource record (struct)
	 */
   /* dns_rr result = malloc(sizeof(dns_rr));
    char* name = malloc(1024);
    int i = 0;
  if((*indexp & 0xc0) == 0xc0){
      i = (*indexp)[1];
        name_ascii_from_wire(wire, &i, name);
        result->name = name;
        result->type = ((indexp)[2] << 8) | (indexp)[3];
        result->class = ((index)[4] << 8) | (indexp)[5]
        result->ttl = ((index)[6] << 24 ) | ((index)[7] << 16) | ((indexp)[8] << 8) | ((indexp)[9]);
        result=>rdata_len = ((indexp)[10] << 8) | (indexp)[11];
        result->rdata = &((indexp)[12]);
        (indexp) += 12 + result->rdata_len;
        return result;
    } else {
        unsigned j = (indexp) - wire;
        int namelen = name_ascii_from_wire(wire, &j, name);
-    }*/

}


int rr_to_wire(dns_rr rr, unsigned char *wire, int query_only) {
	/* 
	 * Convert a DNS resource record struct to DNS wire format, using the
	 * provided byte array (wire).  Return the number of bytes used by the
	 * name in wire format.
	 *
	 * INPUT:  rr: the dns_rr struct containing the rr record
	 * INPUT:  wire: a pointer to the array of bytes where the
	 *             wire-formatted resource record should be constructed
	 * INPUT:  query_only: a boolean value (1 or 0) which indicates whether
	 *              we are constructing a full resource record or only a
	 *              query (i.e., in the question section of the DNS
	 *              message).  In the case of the latter, the ttl,
	 *              rdata_len, and rdata are skipped.
	 * OUTPUT: the length of the wire-formatted resource record.
	 *
	 */
    
}

unsigned short create_dns_query(char *qname, dns_rr_type qtype, unsigned char *wire) {
	/* 
	 * Create a wire-formatted DNS (query) message using the provided byte
	 * array (wire).  Create the header and question sections, including
	 * the qname and qtype.
	 *
	 * INPUT:  qname: the string containing the name to be queried
	 * INPUT:  qtype: the integer representation of type of the query (type A == 1)
	 * INPUT:  wire: the pointer to the array of bytes where the DNS wire
	 *               message should be constructed
	 * OUTPUT: the length of the DNS wire message
	 */
    //ID
    wire[0] = rand();
    wire[1] = rand();
    //QR, Op, flags
    wire[2] = 0x01;
    wire[3] = 0x00;
    //questions
    wire[4] = 0x00;
    wire[5] = 0x01;
    //answer RRs
    wire[6] = 0x00;
    wire[7] = 0x00;
    //auth rrs
    wire[8] = 0x00;
    wire[9] = 0x00;
    //add rrs
    wire[10] = 0x00;
    wire[11] = 0x00;
    //message
    int offset = name_ascii_to_wire(qname, &wire[12]);
    //type
    int next = 12 + offset + 1;
    //if(strcmp(qname, ".") == 0){
    //    wire[next] = qtype;
    //} else {
    wire[next] = 0x00;
    wire[next++] = 0x00;
    wire[next++] = qtype;
    //}
    //class
    wire[next++] = 0x00;
    wire[next++] = 0x01;
    return next;
}

dns_answer_entry *get_answer_address(char *qname, dns_rr_type qtype, unsigned char *wire) {
	/* 
	 * Extract the IPv4 address from the answer section, following any
	 * aliases that might be found, and return the string representation of
	 * the IP address.  If no address is found, then return NULL.
	 *
	 * INPUT:  qname: the string containing the name that was queried
	 * INPUT:  qtype: the integer representation of type of the query (type A == 1)
	 * INPUT:  wire: the pointer to the array of bytes representing the DNS wire message
	 * OUTPUT: a linked list of dns_answer_entrys the value member of each
	 * reflecting either the name or IP address.  If
	 */

    int answersLen = wire[7];
    if(answersLen == 0) { return 0; }
    /*Question*/
    unsigned int index = 12;
    qname = name_ascii_from_wire(wire, &index);
    index++;
    /*The type*/
    index++;
    index++;
    /*The Class*/
    index++;
    index++;

    /*Answer*/
    dns_answer_entry* output = malloc(sizeof(dns_answer_entry));
    dns_answer_entry* curr = output;

    for(int i = 0; i < answersLen; i++){
        char* aname;
        int lastIndex;
        if(wire[index] == 0xc0){
            index++;
            lastIndex = index;
            index = wire[index];
            aname = name_ascii_from_wire(wire, &index);
            index = lastIndex;
            index++;
        }

        /*Type*/
        index++;
        int atype = wire[index];
        index++;
        /*Class*/
        index++;
        index++;
        /*Time*/
        index++;
        index++;
        index++;
        index++;
        /*Answer size*/
        index++;
        int aSize = wire[index];
        index++;
        if(strcmp(aname, qname) == 0 && atype == qtype){
            
            for(int j = 0; j < aSize; j++){
                char* item = malloc(sizeof(char) * 1024);
                if(j == aSize - 1) {
                    sprintf(item, "%u", wire[index]);
                } else {
                    sprintf(item, "%u.", wire[index]);
                }
                if(curr->value == NULL){
                    curr->value = item;
                } else {
                    curr->value = strcat(curr->value, item);
                }                
                index++;
            }

            if( i != answersLen - 1){
                curr->next = malloc(sizeof(dns_answer_entry));
                curr = curr->next;
            }
        } else if(strcmp(aname, qname) == 0 && atype == 5){
            qname = name_ascii_from_wire(wire, &index);
            curr->value = qname;
            index++;
            if(i != answersLen - 1) {
                curr->next = malloc(sizeof(dns_answer_entry));
                curr = curr->next;
            }
        } 
    }
    return output;
}

int send_recv_message(unsigned char *request, int requestlen, unsigned char *response, char *server, unsigned short port) {
	/* 
	 * Send a message (request) over UDP to a server (server) and port
	 * (port) and wait for a response, which is placed in another byte
	 * array (response).  Create a socket, "connect()" it to the
	 * appropriate destination, and then use send() and recv();
	 *
	 * INPUT:  request: a pointer to an array of bytes that should be sent
	 * INPUT:  requestlen: the length of request, in bytes.
	 * INPUT:  response: a pointer to an array of bytes in which the
	 *             response should be received
	 * OUTPUT: the size (bytes) of the response received
	 */
    
    struct sockaddr_in resolver_addr;
    memset((char*) &resolver_addr, 0, sizeof(struct sockaddr_in));
    resolver_addr.sin_family = AF_INET; 
    resolver_addr.sin_port = htons(port);
    resolver_addr.sin_addr.s_addr = inet_addr(server);

    int mySock = socket(AF_INET, SOCK_DGRAM, 0);
    


    connect(mySock, (struct sockaddr*)&resolver_addr, sizeof(struct sockaddr_in));

    send(mySock, request, requestlen, 0);

    int numBytesReceived = recv(mySock, response, 1024, 0);

    close(mySock);
    return numBytesReceived;
}

dns_answer_entry *resolve(char *qname, char *server, char *port) {
    unsigned char sentMessage[1024];
    unsigned char returnMessage[1024];
    unsigned char qtype = 1;
    unsigned short s = (unsigned short) atoi(port);
    int numBytesOfQuery = create_dns_query(qname, qtype, &sentMessage[0]);
    int numBytesReceived = send_recv_message(&sentMessage[0], numBytesOfQuery, &returnMessage[0], server, s);
    dns_answer_entry* output = get_answer_address(qname, qtype, &returnMessage[0]);
    return output;
}

int main(int argc, char *argv[]) {
	char *port;
	dns_answer_entry *ans_list, *ans;
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <domain name> <server> [ <port> ]\n", argv[0]);
		exit(1);
	}
	if (argc > 3) {
		port = argv[3];
	} else {
		port = "53";
	}
	ans = ans_list = resolve(argv[1], argv[2], port);
	while (ans != NULL) {
		printf("%s\n", ans->value);
		ans = ans->next;
	}
	if (ans_list != NULL) {
		free_answer_entries(ans_list);
	}
}
