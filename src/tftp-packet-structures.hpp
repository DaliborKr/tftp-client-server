/**
 * ISA - Projekt: TFTP Klient + Server
 * @file tftp-packet-structures.hpp
 * @brief Definition of the packet structures a operations on them
 * @author Dalibor Kříčka (xkrick01)
 */


#include <iostream>

using namespace std;

#define RRQ_OPCODE   1
#define WRQ_OPCODE   2
#define DATA_OPCODE  3
#define ACK_OPCODE   4
#define ERROR_OPCODE 5
#define OACK_OPCODE  6

#define DATA_PACKET_OFFSET 4

#define DUPLICATED_PACKET          -2
#define PACKET_OK_CODE             -1
#define ERR_CODE_NOT_DEF            0
#define ERR_CODE_FILE_NOT_FOUND     1
#define ERR_CODE_ACCESS_VIOLATION   2
#define ERR_CODE_DISK_FULL          3
#define ERR_CODE_ILLEGAL_OPERATION  4
#define ERR_CODE_UNKNOWN_TID        5
#define ERR_CODE_FILE_EXISTS        6
#define ERR_CODE_NO_USER            7
#define ERR_CODE_OPTIONS_FAILED     8

#define PROG_RET_CODE_ERR  1
#define PROG_RET_CODE_OK   0

#define MODE_OCTET      "octet"
#define MODE_NETASCII   "netascii"

#define DEFAULT_TFTP_PORT 69

#define DEFAULT_BLOCK_SIZE 512
#define DEFAULT_TIMEOUT    5
#define SUPPORTED_OPTIONS_NUMBER 3


typedef unsigned short int ushort;


enum options{
   NONE = -1,
   BLOCKSIZE,
   TRANSFER_SIZE,
   TIMEOUT
};


//Structure containing transfer option information
typedef struct option_info {
   unsigned int blocksize = DEFAULT_BLOCK_SIZE;    //block size value
   unsigned int transfer_size;                     //transfer size value
   unsigned int timeout_interval;                  //timeout value

   bool option_blocksize = false;                  //block size option enabled
   bool option_transfer_size = false;              //transfer size option enabled
   bool option_timeout_interval = false;           //timeout option enabled

    options option_order[SUPPORTED_OPTIONS_NUMBER] = {NONE, NONE, NONE};   //array defining order of incoming options
} option_info_t;


//Structure containing data of TFTP Write or Read request packet
typedef struct tftp_rrq_wrq_packet {
   ushort opcode;
   string filename;
   string mode;
   option_info_t options;
} tftp_rrq_wrq_packet_t;


//Structure containing data of TFTP Data packet
typedef struct tftp_data_packet {
   ushort opcode = DATA_OPCODE;
   ushort block_number;
   char *data;
} tftp_data_packet_t;


//Structure containing data of TFTP Ack packet
typedef struct tftp_ack_packet {
   ushort opcode = ACK_OPCODE;
   ushort block_number;
} tftp_ack_packet_t;


//Structure containing data of TFTP Error packet
typedef struct tftp_error_packet {
   ushort opcode = ERROR_OPCODE;
   ushort error_code;
   string error_message;
} tftp_error_packet_t;


//Structure containing data of TFTP Oack packet
typedef struct tftp_oack_packet {
   ushort opcode = OACK_OPCODE;
   option_info_t options;
} tftp_oack_packet_t;


/**
 * @brief Converts an unsigned short number to an array of chars
 *
 * @param short_number short number to be convert
 * @param number_chars address of the array of chars, that the result should be stored in
 */
void short_to_chars(ushort short_number, char *number_chars);


/**
 * @brief Converts an array of chars to unsigned short number
 *
 * @param short_number array of chars to be convert
 *
 * @return converted unsigned short number
 */
ushort chars_to_short(char *number_chars);


/**
 * @brief Serialize a Write or Read request packet structure to a stream of bytes
 *
 * @param packet_struct RRQ or WRQ packet structure to be serialized
 *
 * @return stream of bytes representing Write or Read request packet
 */
string serialize_packet_struct(tftp_rrq_wrq_packet_t *packet_struct);


/**
 * @brief Serialize a Data packet structure to a stream of bytes
 *
 * @param packet_struct Data packet structure to be serialized
 * @param loaded_data_number size of data block, that should be serialized
 *
 * @return stream of bytes representing Data packet
 */
string serialize_packet_struct(tftp_data_packet_t *packet_struct, int loaded_data_number);


/**
 * @brief Serialize an Ack packet structure to a stream of bytes
 *
 * @param packet_struct Ack packet structure to be serialized
 *
 * @return stream of bytes representing Ack packet
 */
string serialize_packet_struct(tftp_ack_packet_t *packet_struct);


/**
 * @brief Serialize an Error packet structure to a stream of bytes
 *
 * @param packet_struct Error packet structure to be serialized
 *
 * @return stream of bytes representing Error packet
 */
string serialize_packet_struct(tftp_error_packet_t *packet_struct);


/**
 * @brief Serialize an Oack packet structure to a stream of bytes
 *
 * @param packet_struct Oack packet structure to be serialized
 *
 * @return stream of bytes representing Oack packet
 */
string serialize_packet_struct(tftp_oack_packet_t *packet_struct);


/**
 * @brief Serialize an options structure to a stream of bytes
 *
 * @param option_information options structure to be serialized
 *
 * @return stream of bytes representing transfer options
 */
string serialize_option_info(option_info_t *option_information);


/**
 * @brief Deserialize a stream of bytes into a Write or Read request packet structure
 *
 * @param packet_struct Write or Read request packet structure, that the result should be stored in
 * @param sequence stream of bytes that should be deserialized
 */
void deserialize_packet_struct(tftp_rrq_wrq_packet_t *packet_struct, char *sequence);


/**
 * @brief Deserialize a stream of bytes into a Data packet structure
 *
 * @param packet_struct Data packet structure, that the result should be stored in
 * @param sequence stream of bytes that should be deserialized
 */
void deserialize_packet_struct(tftp_data_packet_t *packet_struct, char *sequence);


/**
 * @brief Deserialize a stream of bytes into a Ack packet structure
 *
 * @param packet_struct Ack packet structure, that the result should be stored in
 * @param sequence stream of bytes that should be deserialized
 */
void deserialize_packet_struct(tftp_ack_packet_t *packet_struct, char *sequence);


/**
 * @brief Deserialize a stream of bytes into a Error packet structure
 *
 * @param packet_struct Error packet structure, that the result should be stored in
 * @param sequence stream of bytes that should be deserialized
 */
void deserialize_packet_struct(tftp_error_packet_t *packet_struct, char *sequence);


/**
 * @brief Deserialize a stream of bytes into a Oack packet structure
 *
 * @param packet_struct Oack packet structure, that the result should be stored in
 * @param sequence stream of bytes that should be deserialized
 */
void deserialize_packet_struct(tftp_oack_packet_t *packet_struct, char *sequence);


/**
 * @brief Deserialize a stream of bytes into a Data packet structure
 *
 * @param option_information option structure, that the result should be stored in
 * @param sequence stream of packet bytes that should be deserialized
 * @param options_start_index index of the byte from the stream, that options information starts on
 */
void deserialize_option_info(option_info_t *option_information, char *sequence, int options_start_index);


/**
 * @brief Checks if the content of Write or Read request packet is valid
 *
 * @param packet_struct Write or Read request packet structure, that should be checked
 * @param error_message address of string, where error message will be stored if an error occurs
 *
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int check_packet_content(tftp_rrq_wrq_packet_t *packet_struct, string *error_message);


/**
 * @brief Checks if the content of Ack packet is valid
 *
 * @param packet_struct Ack packet structure, that should be checked
 * @param expected_block_number expected data block number that should be acked
 * @param error_message address of string, where error message will be stored if an error occurs
 *
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int check_packet_content(tftp_ack_packet_t *packet_struct, ushort expected_block_number, string *error_message);


/**
 * @brief Checks if the content of Data packet is valid
 *
 * @param packet_struct Data structure, that should be checked
 * @param expected_block_number expected data block number
 * @param error_message address of string, where error message will be stored if an error occurs
 *
 * @return -1 if OK, else return code according to a possible TFTP error codes
 */
int check_packet_content(tftp_data_packet_t *packet_struct, ushort expected_block_number, string *error_message);