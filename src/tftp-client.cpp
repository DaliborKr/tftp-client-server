/**
 * ISA - Projekt: TFTP Klient + Server
 * @file tftp-client.cpp
 * @brief TFTP client
 * @author Dalibor Kříčka (xkrick01)
 */


#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <regex>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include "tftp-communication.hpp"


#define MIN_NUM_ARGS 5
#define MAX_NUM_ARGS 9


//Global variables
int socket_client;

/**
 * @brief Prints help for the program
 */
void print_help(){
    cout << "NAME:\n"
         << "  tftp-client - TFTP client\n"
         << "\n"
         << "USAGE:\n"
         << "  Run client:\ttftp-client -h hostname [-p port] [-f filepath] -t dest_filepath\n"
         << "  Show help:\ttftp-client --help\n"
         << "\n"
         << "OPTIONS:\n"
         << "  -h <VALUE>\thostname or IPv4 address to connect to\n"
         << "  -p <MODE>\thost port number to connect to (if not set, then 69)\n"
         << "  -f <PATH>\tpath to the server file to download (if not set, then upload from stdin)\n"
         << "  -t <PATH>\tpath to the file to save data in\n"
         << "\n"
         << "AUTHOR:\n"
         << "  Dalibor Kříčka (xkrick01), 2023\n\n";

    exit(0);
}


/**
 * @brief Handles interrupt signal (ctrl+c)
 *
 * @param signum type of the signal
 */
void interrupt_signal_handler(int signum){
    cout << "Client process closed by the interrupt signal\n";
    close(socket_client);
    exit(1);
}


/**
 * @brief Validates and parses given program arguments
 *
 * @param argc number of arguments
 * @param argv array of given arguments
 * @param host address where a host name or IPv4 will be stored in
 * @param port_host address where a host port will be stored in
 * @param file_path_source address where a source file path will be stored in
 * @param file_path_dest address where a destination file path will be stored in
 */
void check_program_args(int argc, char *argv[], string *host, int *port_host, string *file_path_source, string *file_path_dest){
    if (argc == 2 && !strcmp(argv[1],"--help")){
        print_help();
    }

    if (argc < MIN_NUM_ARGS || argc > MAX_NUM_ARGS){
        cout << "ERR: invalid number of program arguments\n";
        exit(PROG_RET_CODE_ERR);
    }

    bool host_name_checked = false;
    bool dest_filepath_checked = false;
    bool port_checked = false;
    bool filepath_checked = false;

    for (int i = 1; i < argc; i++){
    //check -h argument
        if ((strcmp(argv[i],"-h") == 0) && !host_name_checked){
            host_name_checked = true;
            i++;

            //check host name or IPv4 address format
            if (!(regex_match(argv[i], regex("^.*$")))){
                cout << "ERR: invalid format of IPv4 address\n";
                exit(PROG_RET_CODE_ERR);
            }
            *(host) = argv[i];
        }
        //check -p argument
        else if ((strcmp(argv[i],"-p") == 0) && !port_checked){
            port_checked = true;
            i++;

            //check port format
            if (!(regex_match(argv[i], regex("^\\d+$")))){
                cout << "ERR: invalid format of port\n";
                exit(PROG_RET_CODE_ERR);
            }
            *(port_host) = atoi(argv[i]);
        }
        //check -f argument
        else if ((strcmp(argv[i],"-f") == 0) && !filepath_checked){
            filepath_checked = true;
            i++;

            //check file path format
            if (!(regex_match(argv[i], regex("^.*$")))){
                cout << "ERR: invalid source filepath (argument -f)\n";
                exit(PROG_RET_CODE_ERR);
            }
            *(file_path_source) = argv[i];
        }
        //check -t argument
        else if ((strcmp(argv[i],"-t") == 0) && !dest_filepath_checked){
            dest_filepath_checked = true;
            i++;

            //check destination file path format
            if (!(regex_match(argv[i], regex("^.*$")))){
                cout << "ERR: invalid destination filepath (argument -t)\n";
                exit(PROG_RET_CODE_ERR);
            }
            *(file_path_dest) = argv[i];
        }
        else{
            cout << "ERR: invalid argument (the client is started using: 'tftp-client -h hostname [-p port] [-f filepath] -t dest_filepath')\n";
            exit(PROG_RET_CODE_ERR);
        }
    }

    if (!port_checked){
        *(port_host) = DEFAULT_TFTP_PORT;
    }

    if (!filepath_checked){
        *(file_path_source) = "";
    }

    if (!host_name_checked || !dest_filepath_checked){
        cout << "ERR: missing required argument (-h hostname or -t dest_filepath)\n";
        exit(PROG_RET_CODE_ERR);
    }
}


/**
 * @brief Sets informations about host to communicate with
 *
 * @param ip_address IPv4 address of host
 * @param port port of host
 *
 * @return structure describing internet socket address
 */
struct sockaddr_in set_host_informations(string ip_address, int port){
    struct hostent *server = gethostbyname(ip_address.c_str());
    if (server == NULL){
        cout << "ERR: no such a host " << ip_address << "\n";
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));     //initialization of the memory
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    memcpy(&server_address.sin_addr.s_addr, server->h_addr, server->h_length);

    return server_address;
}


/**
 * @brief Handles TFTP communication with server
 *
 * @param connection_information connection information (socket, address)
 * @param communication_information information needed to properly execute a transfer (mode, file paths)
 * @param option_information information determining transfer options and their values
 */
void execute_transfer(connection_info_t *connection_information, communication_info_t *communication_information, option_info_t *option_information){
    //setting default options
    option_info_t default_options;
    default_options.blocksize = DEFAULT_BLOCK_SIZE;
    default_options.timeout_interval = DEFAULT_TIMEOUT;

    string error_message = "";
    string packet_to_be_send;

    char buffer[DEFAULT_BLOCK_SIZE + DATA_PACKET_OFFSET];
    bzero(buffer, DEFAULT_BLOCK_SIZE + DATA_PACKET_OFFSET);

    tftp_rrq_wrq_packet_t init_communication_packet;

    if (communication_information->path_was_given){     //RRQ
        //testing if the file we want to write in doesn't exist
        ifstream file_existence_test(communication_information->file_path_dest);
        if (file_existence_test.is_open()){
            cout << "ERR: File - file to write to already exists\n";
            file_existence_test.close();
            return;
        }

        packet_to_be_send = send_wrq_rrq(connection_information, communication_information, &init_communication_packet, option_information, true, "");

        int bytes_rx = recvfrom_retransmit(connection_information, option_information, buffer, packet_to_be_send, TID_NOT_SET_YET);
        if (bytes_rx < 0){
            return;
        }

        int tid_server = htons(((struct sockaddr_in*)connection_information->address)->sin_port);   //server TID

        ofstream file_write(communication_information->file_path_dest);
        int write_to_file_ret_code = 0;
        char opcode_char[2] = {buffer[0], buffer[1]};

        if (chars_to_short(opcode_char) == ERROR_OPCODE){
            receive_error(connection_information, buffer);
            close_remove_file(file_write, communication_information->file_path_dest);
            return;
        }
        else if (chars_to_short(opcode_char) == OACK_OPCODE){
            if (receive_oack(connection_information, &init_communication_packet.options, buffer) != PACKET_OK_CODE){
                close_remove_file(file_write, communication_information->file_path_dest);
                return;
            }

            packet_to_be_send = send_ack(connection_information, 0);

            //continue receiving data
            write_to_file_ret_code = write_to_file(connection_information, &init_communication_packet.options, file_write, packet_to_be_send, communication_information->mode, tid_server, 1);
        }
        else{
            int expected_block_number = 1;
            if (receive_data(connection_information, buffer, bytes_rx, file_write, communication_information->mode, default_options.timeout_interval, expected_block_number) != PACKET_OK_CODE){
                close_remove_file(file_write, communication_information->file_path_dest);
                return;
            }

            packet_to_be_send = send_ack(connection_information, expected_block_number);

            if (bytes_rx < (DEFAULT_BLOCK_SIZE + DATA_PACKET_OFFSET)){
                return;     //end of the transition
            }

            //continue receiving data
            write_to_file_ret_code = write_to_file(connection_information, &default_options, file_write, packet_to_be_send, communication_information->mode, tid_server, ++expected_block_number);
        }

        file_write.close();

        //removing invalid file, when an error occurs
        if (write_to_file_ret_code == PROG_RET_CODE_ERR){
            remove(communication_information->file_path_dest.c_str());
        }
    }
    else{       //WRQ
        //creating a path to the temporary file with stdin content
        srand((unsigned int)time(NULL));
        string temp_file_path = TEMP_FILE_PATH + to_string(rand()) + ".tmp";

        packet_to_be_send = send_wrq_rrq(connection_information, communication_information, &init_communication_packet, option_information, false, temp_file_path);

        int bytes_rx = recvfrom_retransmit(connection_information, &default_options, buffer, packet_to_be_send, TID_NOT_SET_YET);
        if (bytes_rx < 0){
            remove(temp_file_path.c_str());
            return;
        }

        int tid_server = htons(((struct sockaddr_in*)connection_information->address)->sin_port);   //server TID

        char opcode_char[2] = {buffer[0], buffer[1]};
        ushort received_opcode = chars_to_short(opcode_char);

        if (received_opcode== ERROR_OPCODE){
            receive_error(connection_information, buffer);
            remove(temp_file_path.c_str());
            return;
        }
        else if (chars_to_short(opcode_char) == OACK_OPCODE){
            if (receive_oack(connection_information, &init_communication_packet.options, buffer) != PACKET_OK_CODE){
                remove(temp_file_path.c_str());
                return;
            }

            //continue sending data
            read_from_file(connection_information, temp_file_path, &init_communication_packet.options, communication_information->mode, tid_server);
        }
        else{           //ACK packet
            if (receive_ack(connection_information, buffer, 0, default_options.timeout_interval) != PACKET_OK_CODE){
                remove(temp_file_path.c_str());
                return;
            }

            //continue sending data
            read_from_file(connection_information, temp_file_path, &default_options, communication_information->mode, tid_server);
        }

        remove(temp_file_path.c_str());
    }
}


int main(int argc, char *argv[]) {
    string file_path_source;
    string file_path_dest;
    string host;
    int port_host;

    check_program_args(argc, argv, &host, &port_host, &file_path_source, &file_path_dest);

    socket_client = create_socket();

    signal(SIGINT, interrupt_signal_handler);

    struct sockaddr_in server_address = set_host_informations(host, port_host);

    //defining transfer connection information
    connection_info_t connection_information;
    connection_information.socket = socket_client;
    connection_information.address = (struct sockaddr *) &server_address;
    connection_information.address_size = sizeof(server_address);

    //defining transfer communication information
    communication_info_t communication_information;
    communication_information.mode = MODE_OCTET;
    communication_information.path_was_given = file_path_source == "" ? false : true;
    communication_information.file_path_source = file_path_source;
    communication_information.file_path_dest = file_path_dest;

    //defining transfer option information
    option_info_t option_information;
    option_information.option_blocksize = false;
    option_information.blocksize = 512;
    option_information.option_transfer_size = false;
    option_information.option_timeout_interval = false;
    option_information.timeout_interval = 2;

    execute_transfer(&connection_information, &communication_information, &option_information);

    close(socket_client);

    return 0;
}