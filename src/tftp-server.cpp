/**
 * ISA - Projekt: TFTP Klient + Server
 * @file tftp-server.cpp
 * @brief TFTP server
 * @author Dalibor Kříčka (xkrick01)
 */


#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fstream>
#include <regex>
#include <filesystem>
#include <netdb.h>
#include <signal.h>
#include "tftp-communication.hpp"

#define MIN_NUM_ARGS 2
#define MAX_NUM_ARGS 4


namespace fs = std::filesystem;

//Global variables
int socket_server;
int socket_transfer;
bool is_child_process = false;


/**
 * @brief Prints help for the program
 */
void print_help(){
    cout << "NAME:\n"
        << "  tftp-server - TFTP server\n"
        << "\n"
        << "USAGE:\n"
        << "  Run server:\ttftp-server [-p port] root_dirpath\n"
        << "  Show help:\ttftp-server --help\n"
        << "\n"
        << "OPTIONS:\n"
        << "  -p <MODE>\thost port number to connect to (if not set, then 69)\n"
        << "  root_dirpath\tpath to the server directory to upload files to and download files from\n"
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
    if (is_child_process){
        cout << "Child process closed by the interrupt signal\n";
        close(socket_transfer);
        exit(0);
    }
    else{
        cout << "Main server process closed by the interrupt signal\n";
        close(socket_server);
        exit(1);
    }
}

/**
 * @brief Validates and parses given program arguments
 *
 * @param argc number of arguments
 * @param argv array of given arguments
 * @param root_dirpath address where the root directory path will be stored in
 * @param port_host address where a host port will be stored in
 */
void check_program_args(int argc, char *argv[], string *root_dirpath, int *port_host){
    if (argc == 2 && !strcmp(argv[1],"--help")){
        print_help();
    }

    if (argc < MIN_NUM_ARGS || argc > MAX_NUM_ARGS){
        cout << "ERR: invalid number of program arguments\n";
        exit(PROG_RET_CODE_ERR);
    }

    bool port_checked = false;
    bool root_dirpath_checked = false;

    for (int i = 1; i < argc; i++){
        //check -p argument
        if ((strcmp(argv[i],"-p") == 0) && !port_checked){
            port_checked = true;
            i++;

            //check port format
            if (!(regex_match(argv[i], regex("^\\d+$")))){
                cout << "ERR: invalid format of port\n";
                exit(PROG_RET_CODE_ERR);
            }
            *(port_host) = atoi(argv[i]);
        }
        else if (!root_dirpath_checked){
            //check root directory path format
            root_dirpath_checked = true;
            if (!(regex_match(argv[i], regex("^.*$")))){
                cout << "ERR: invalid format of root dirpath\n";
                exit(PROG_RET_CODE_ERR);
            }
            *(root_dirpath) = argv[i];
        }
        else{
            cout << "ERR: invalid argument (the server is started using: 'tftp-server [-p port] root_dirpath')\n";
            exit(PROG_RET_CODE_ERR);
        }
    }

    if (!port_checked){
        *(port_host) = DEFAULT_TFTP_PORT;
    }

    if (!root_dirpath_checked){
        cout << "ERR: missing required argument (root_dirpath)\n";
        exit(PROG_RET_CODE_ERR);
    }

}


/**
 * @brief Sets informations about connection
 *
 * @param port port, that server is listening on
 * @return structure describing internet socket address
 */
struct sockaddr_in set_server_informations(int port)
{
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));     //initialization of the memory
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_server, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        close(socket_server);
        cout << "ERR: bind has failed\n";
        exit(EXIT_FAILURE);
    }

    return server_address;
}


/**
 * @brief Checks if there is at least one option required
 *
 * @param options structure containing options information
 * @return true, if at least one option is required, else false
 */
bool are_options_used(option_info_t *options){
    if (options->option_blocksize ||
        options->option_timeout_interval ||
        options->option_transfer_size){
        return true;
    }
    else{
        return false;
    }
}

/**
 * @brief Handles TFTP communication with multiple clients
 *
 * @param connection_information connection information (socket, address)
 * @param root_dirpath server root directory path
 * @param option_information information determining transfer options and their values
 */
void start_listen(connection_info_t *connection_information, string root_dirpath, option_info_t *option_information)
{
    //setting default options
    option_info_t default_options;
    default_options.blocksize = DEFAULT_BLOCK_SIZE;
    default_options.timeout_interval = DEFAULT_TIMEOUT;

    string error_message = "";
    string packet_to_be_send;

    char buffer[option_information->blocksize + DATA_PACKET_OFFSET];
    bzero(buffer, option_information->blocksize + DATA_PACKET_OFFSET);


    while (true)
    {
        //waiting for an inital RRQ or WRQ packet
        int bytes_rx = recvfrom(connection_information->socket, buffer, DEFAULT_BLOCK_SIZE + DATA_PACKET_OFFSET, 0,
                                connection_information->address, &connection_information->address_size);
        if (bytes_rx < 0){
            cout << "ERROR: recvfrom - server initialization communication (RRQ or WRQ)\n";
            close(socket_server);
            exit(1);
        }

        //creating child process that will handle communication with client
        pid_t pid = fork();

        if (pid == 0){
            int tid_client = htons(((struct sockaddr_in*)connection_information->address)->sin_port);

            char opcode_char[2] = {buffer[0], buffer[1]};
            if (chars_to_short(opcode_char) == ERROR_OPCODE){
                receive_error(connection_information, buffer);
                break;
            }

            tftp_rrq_wrq_packet_t init_communication_packet;
            if (receive_wrq_rrq(connection_information, &init_communication_packet, buffer) != PACKET_OK_CODE){
                break;
            }

            string full_path_file = root_dirpath + "/" + init_communication_packet.filename;

            socket_transfer = create_socket();      //new socket that maintain communication with certain user
            is_child_process = true;

            connection_information->socket = socket_transfer;

            if (init_communication_packet.opcode == RRQ_OPCODE){    //RRQ
                //testing if the file we want to read from exists
                ifstream file_existence_test(full_path_file);
                if (!file_existence_test.is_open()){
                    error_message = "File - file to read from doesn't exists";
                    send_error_packet(connection_information, ERR_CODE_FILE_NOT_FOUND, error_message);
                    file_existence_test.close();
                    break;
                }

                if (are_options_used(&init_communication_packet.options)){
                    //RRQ communication with options (OACK response)
                    int return_code = negotiate_option_server(&init_communication_packet.options, option_information, &error_message);
                    if (return_code != PACKET_OK_CODE){
                        send_error_packet(connection_information, return_code, error_message);
                        break;
                    }
                    packet_to_be_send = send_oack(connection_information, &init_communication_packet.options, option_information, full_path_file, true);

                    int bytes_rx = recvfrom_retransmit(connection_information, option_information, buffer, packet_to_be_send, tid_client);
                    if (bytes_rx < 0){
                        break;
                    }

                    char opcode_char[2] = {buffer[0], buffer[1]};
                    if (chars_to_short(opcode_char) == ERROR_OPCODE){
                        receive_error(connection_information, buffer);
                        break;
                    }

                    if (receive_ack(connection_information, buffer, 0, option_information->timeout_interval) != PACKET_OK_CODE){
                        break;
                    }

                    //continue sending data
                    read_from_file(connection_information, full_path_file, option_information, init_communication_packet.mode, tid_client);

                }
                else{
                    //RRQ communication without options (Data response)
                    //continue sending data
                    read_from_file(connection_information, full_path_file, &default_options, init_communication_packet.mode, tid_client);
                }

            }
            else if (init_communication_packet.opcode == WRQ_OPCODE){   //WRQ
                //testing if the file we want to write in doesn't exist
                ifstream file_existence_test(full_path_file);
                if (file_existence_test.is_open()){
                    error_message = "File - file to write to already exists";
                    send_error_packet(connection_information, ERR_CODE_FILE_EXISTS, error_message);
                    file_existence_test.close();
                    break;
                }

                if (init_communication_packet.options.option_transfer_size){
                    //testing if there is enough free space on the server to receive a file
                    if (fs::space("./").available < init_communication_packet.options.transfer_size){
                        error_message = "Transfer size - not enough space on disk to download the file";
                        send_error_packet(connection_information, ERR_CODE_DISK_FULL, error_message);
                        break;
                    }
                }

                ofstream file_write(full_path_file);
                int write_to_file_ret_code;
                if (are_options_used(&init_communication_packet.options)){
                    //WRQ communication with options (OACK response)
                    int return_code = negotiate_option_server(&init_communication_packet.options, option_information, &error_message);
                    if (return_code != PACKET_OK_CODE){
                        send_error_packet(connection_information, return_code, error_message);
                        close_remove_file(file_write, full_path_file.c_str());
                        break;
                    }
                    packet_to_be_send = send_oack(connection_information, &init_communication_packet.options, option_information, full_path_file, false);

                    //continue receiving data
                    write_to_file_ret_code = write_to_file(connection_information, option_information, file_write, packet_to_be_send, init_communication_packet.mode, tid_client, 1);

                }
                else{
                    //WRQ communication without options (ACK response)
                    packet_to_be_send = send_ack(connection_information, 0);

                    //continue receiving data
                    write_to_file_ret_code = write_to_file(connection_information, &default_options, file_write, packet_to_be_send, init_communication_packet.mode, tid_client, 1);
                }

                file_write.close();

                //removing invalid file, when an error occurs
                if (write_to_file_ret_code == PROG_RET_CODE_ERR){
                    remove(full_path_file.c_str());
                }
            }
            break;
            close(socket_transfer);
        }
        else if (pid > 0){
            //Continue to listen for other clients
            continue;
        }
        else{
            cout << "Error: fork() - Error while creating a child process";
            close(socket_server);
            exit(1);
            break;
        }
    }
}


int main(int argc, char *argv[]) {
    string root_dirpath;
    int port_server;

    cout << root_dirpath;

    check_program_args(argc, argv, &root_dirpath, &port_server);

    socket_server = create_socket();    //stored into the global variable due to interrupt signal

    signal(SIGINT, interrupt_signal_handler);

    set_server_informations(port_server);

    struct sockaddr_in client_addr;

    //defining transfer connection information
    connection_info_t connection_information;
    connection_information.socket = socket_server;
    connection_information.address = (struct sockaddr *)&client_addr;
    connection_information.address_size = sizeof(client_addr);

    //defining transfer option information
    option_info_t option_information;
    option_information.option_blocksize = true;
    option_information.option_transfer_size = true;
    option_information.option_timeout_interval = true;


    start_listen(&connection_information, root_dirpath, &option_information);

    cout << "End of the transfer\n";

    return 0;
}