/**
 * ISA - Projekt: TFTP Klient + Server
 * @file tftp-communication.hpp
 * @brief Handling the communication (receiving, sending packets and logging)
 * @author Dalibor Kříčka (xkrick01)
 */


#include <fstream>
#include <arpa/inet.h>
#include <string.h>
#include <filesystem>
#include "tftp-packet-structures.hpp"

#define CLIENT_READ_FILE_SIZE 2048
#define TEMP_FILE_PATH "temp/temp_cin_file"

#define MIN_BLKSIZE_VALUE 8
#define MAX_BLKSIZE_VALUE 65464
#define MIN_TIMEOUT_VALUE 1
#define MAX_TIMEOUT_VALUE 255

#define ERR_CODE_SELECT  -3
#define ERR_CODE_TIMEOUT -2
#define MAX_RETRANSMIT_ATTEMPTS 3

#define CR_VALUE 13

#define TID_NOT_SET_YET -1

#define EXPONENTIAL_BACKOFF_MULTIPLIER 2


//Structure containing connection information
typedef struct connection_info {
    int socket;
    struct sockaddr *address;
    socklen_t address_size;
} connection_info_t;


//Structure containing connection information
typedef struct communication_info {
    bool path_was_given;
    string mode = MODE_OCTET;
    string file_path_source;
    string file_path_dest;
} communication_info_t;


/**
 * @brief Creates new server UDP socket
 *
 * @return file descriptor for the new UDP socket
 */
int create_socket();


/**
 * @brief Closes stream and then removes a file
 *
 * @param file_stream file stream to be closed
 * @param file_to_be_remove path to the file that should be removed
 */
void close_remove_file(ofstream &file_stream, string file_to_be_remove);


/**
 * @brief Creates a temporary file filled with data from standard input a return it's size
 *
 * @param temp_path path to the temporary file
 *
 * @return size of the temporary file in Bytes
 */
unsigned int get_cin_size(string temp_path);


/**
 * @brief Negotiates options and thier values, that will be used in the transfer on client site.
 *
 * @param client_options options suggested by the client
 * @param server_options options suggested by the server
 * @param error_message address of string, where error message will be stored if an error occurs
 *
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int negotiate_option_client(option_info_t *client_options, option_info_t *server_options, string* error_message);


/**
 * @brief Negotiates options and thier values, that will be used in the transfer on server site.
 *
 * @param client_options options suggested by the client
 * @param server_options structure where will be stored accepted options offered by client and their values
 * @param error_message address of string, where error message will be stored if an error occurs
 *
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int negotiate_option_server(option_info_t *client_options, option_info_t *server_options, string* error_message);


/**
 * @brief Recieves a packet or detects timeout
 *
 * @param connection_information connection information
 * @param option_information options associated to the current transfer
 * @param buffer address, where will be received data stored
 *
 * @return received number of bytes or -4 on timeout
 */
int recvfrom_timeout(connection_info_t *connection_information, option_info_t *option_information, char *buffer, int times_retransmitted);


/**
 * @brief Retransmits packet on timeout and eventually checks if the packet came from the expected source
 *
 * @param connection_information connection information
 * @param option_information options associated to the current transfer
 * @param buffer address, where will be received data stored
 * @param packet packet, that is going to be retransmit on timeout
 * @param tid_expected expected TID
 *
 * @return received number of bytes or -1 when an error occurs
 */
int recvfrom_retransmit(connection_info_t *connection_information, option_info_t *option_information, char *buffer, string packet, int tid_expected);


/**
 * @brief Creates and then sends an initialization packet RRQ or WRQ
 *
 * @param connection_information connection information
 * @param communication_information information needed to properly execute a transfer (mode, file paths)
 * @param init_communication_packet structure of WRQ or RRQ packet to be filled with options information
 * @param option_information transfer options suggested by the client
 * @param is_rrq is RRQ packet going to be send
 * @param temp_path path to the temporary file
 *
 * @return stream of bytes representing sent RRQ or WRQ packet
 */
string send_wrq_rrq(connection_info_t *connection_information, communication_info_t *communication_information, tftp_rrq_wrq_packet_t *init_communication_packet, option_info_t *option_information, bool is_rrq, string temp_path);


/**
 * @brief Creates and then sends an Ack packet
 *
 * @param connection_information connection information
 * @param block_number block number of data packet that is being acked
 * @return stream of bytes representing sent Ack packet
 */
string send_ack(connection_info_t *connection_information, int block_number);


/**
 * @brief Creates and then sends an Data packet
 *
 * @param connection_information connection information
 * @param block_number block number of data packet
 * @param data_block data to be sent
 * @param loaded_actual size of data in Bytes
 * @return stream of bytes representing sent Data packet
 */
string send_data(connection_info_t *connection_information, int block_number, char *data_block, int loaded_actual);


/**
 * @brief Creates and then sends an Oack packet
 *
 * @param connection_information connection information
 * @param init_options transfer options suggested by the client
 * @param server_options transfer options suggested by the server
 * @param path path to a file on server that is part of the transfer
 * @param is_rrq is transfer initiated by the RRQ packet
 * @return stream of bytes representing sent Oack packet
 */
string send_oack(connection_info_t *connection_information, option_info_t *init_options, option_info_t *server_options, string path, bool is_rrq);


/**
 * @brief Creates, sends an Error packet and than wait in case other host did not receive this error packet
 *
 * @param connection_information connection information
 * @param error_code error code to include to the packet
 * @param error_message error message to include to the packet
 * @param error_timeout time to wait
 * @param timeout_enable is timeout enabled
 * @return stream of bytes representing sent Error packet
 */
string send_error_packet(connection_info_t *connection_information, int error_code, string error_message, unsigned int error_timeout = DEFAULT_TIMEOUT, bool timeout_enable = true);


/**
 * @brief Processes the received RRQ or WRQ packet (deserializes and checks content)
 *
 * @param connection_information connection information
 * @param init_communication_packet structure of WRQ or RRQ packet to be filled with options information
 * @param buffer received packet data
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int receive_wrq_rrq(connection_info_t *connection_information, tftp_rrq_wrq_packet_t *init_communication_packet, char *buffer);


/**
 * @brief Processes the received Ack packet (deserializes and checks content)
 *
 * @param connection_information connection information
 * @param buffer received packet data
 * @param expected_block_number expected data block number that should be acked
 * @param timeout time to wait on error packet sent
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int receive_ack(connection_info_t *connection_information, char *buffer, int expected_block_number, unsigned int timeout);


/**
 * @brief Processes the received Oack packet (deserializes and checks content)
 *
 * @param connection_information connection information
 * @param init_options transfer options suggested by the client
 * @param buffer received packet data
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int receive_oack(connection_info_t *connection_information, option_info_t *init_options, char *buffer);


/**
 * @brief Processes the received Data packet (deserializes and checks content). Format NETASCII data (to linux notation).
 *
 * @param connection_information connection information
 * @param buffer received packet data
 * @param bytes_read size of received Data packet
 * @param file_write file stream, that data should be write to
 * @param mode tranfer mode (netascii or octet)
 * @param timeout time to wait on error packet sent
 * @param expected_block_number expected data block number
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int receive_data(connection_info_t *connection_information, char *buffer, int bytes_read, ofstream &file_write, string mode, unsigned int timeout, int expected_block_number);


/**
 * @brief Processes the received Error packet
 *
 * @param connection_information connection information
 * @param buffer received packet data
 */
void receive_error(connection_info_t *connection_information, char *buffer);


/**
 * @brief Handles whole part of data receiving of the transfer. Receives data, sends acks and writing into file.
 *
 * @param connection_information connection information
 * @param options options associated to the current transfer
 * @param file_write file stream, that data should be write to
 * @param packet_to_be_send stream of bytes representing sent Ack/Oack packet
 * @param mode tranfer mode (netascii or octet)
 * @param tid_expected expected TID
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int write_to_file(connection_info_t *connection_information, option_info_t *options, ofstream &file_write, string packet_to_be_send, string mode, int tid_expected, int expected_block_number);


/**
 * @brief Handles whole part of data sending of the transfer. Sends data, receives acks and reading from file.
 *
 * @param connection_information connection information
 * @param filename file name, that data should be read from
 * @param options options associated to the current transfer
 * @param mode tranfer mode (netascii or octet)
 * @param tid_expected expected TID
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int read_from_file(connection_info_t *connection_information, string filename, option_info_t *options, string mode, int tid_expected);


/**
 * @brief Gets the destination port from given socket
 *
 * @param socket socket
 * @return destination port
 */
int get_destination_port(int socket);


/**
 * @brief Writes log of received RRQ or WRQ packet on standard error stream
 *
 * @param connection_information connection information
 * @param packet RRQ or WRQ packet structure
 */
void log_wrq_rrq(connection_info_t *connection_information, tftp_rrq_wrq_packet_t *packet);


/**
 * @brief Writes log of received Data packet on standard error stream
 *
 * @param connection_information connection information
 * @param packet Data packet structure
 */
void log_data(connection_info_t *connection_information, tftp_data_packet_t *packet);


/**
 * @brief Writes log of received Ack packet on standard error stream
 *
 * @param connection_information connection information
 * @param packet Ack packet structure
 */
void log_ack(connection_info_t *connection_information, tftp_ack_packet_t *packet);


/**
 * @brief Writes log of received Error packet on standard error stream
 *
 * @param connection_information connection information
 * @param packet Error packet structure
 */
void log_error(connection_info_t *connection_information, tftp_error_packet_t *packet);


/**
 * @brief Writes log of received Oack packet on standard error stream
 *
 * @param connection_information connection information
 * @param packet Oack packet structure
 */
void log_oack(connection_info_t *connection_information, tftp_oack_packet_t *packet);


/**
 * @brief Writes log of option structure on standard error stream
 *
 * @param options option structure
 */
void log_options(option_info_t *options);


/**
 * @brief
 *
 * @param connection_information connection information
 * @param buffer received packet data
 */
void log_stranger_packet(connection_info_t *connection_information, char* buffer);