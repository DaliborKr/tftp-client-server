/**
 * ISA - Projekt: TFTP Klient + Server
 * @file tftp-communication.cpp
 * @brief Handling the communication (receiving, sending packets and logging)
 * @author Dalibor Kříčka (xkrick01)
 */


#include "tftp-communication.hpp"

int create_socket()
{
    int new_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (new_socket <= 0)
    {
        cout << "ERR: socket completion has failed\n";
        exit(1);
    }

    return new_socket;
}


void close_remove_file(ofstream &file_stream, string file_to_be_remove){
    file_stream.close();
    remove(file_to_be_remove.c_str());
}


unsigned int get_cin_size(string temp_path){
    namespace fs = std::filesystem;

    char data_block[CLIENT_READ_FILE_SIZE];
    int loaded_actual;
    ofstream file_write(temp_path);

    //writing standard input into temporary file
    while ((loaded_actual = cin.read(data_block, CLIENT_READ_FILE_SIZE).gcount()) != 0){
        file_write.write(data_block, loaded_actual);
    }

    file_write.close();
    return (unsigned int) fs::file_size(temp_path);
}

int negotiate_option_client(option_info_t *client_options, option_info_t *server_options, string* error_message){
    if ((server_options->option_blocksize && !client_options->option_blocksize) ||
        (server_options->option_timeout_interval && !client_options->option_timeout_interval) ||
        (server_options->option_transfer_size && !client_options->option_transfer_size)){
            return ERR_CODE_OPTIONS_FAILED;     //server must not send an option which client didnt requested
        }

    if (client_options->option_blocksize && server_options->option_blocksize){      //negotiate block size option
        if (client_options->blocksize < server_options->blocksize ||
         server_options->blocksize < MIN_BLKSIZE_VALUE ||
         server_options->blocksize > MAX_BLKSIZE_VALUE){
            *(error_message) = "Block size - offered value was not accepted";
            return ERR_CODE_OPTIONS_FAILED;
        }
        else{
            client_options->blocksize = server_options->blocksize;
        }
    }
    else{
        client_options->blocksize = DEFAULT_BLOCK_SIZE;
    }
    if (client_options->option_timeout_interval && server_options->option_timeout_interval){    //negotiate timeout interval option
        if (client_options->timeout_interval != server_options->timeout_interval ||
         server_options->timeout_interval < MIN_TIMEOUT_VALUE ||
         server_options->timeout_interval > MAX_TIMEOUT_VALUE){
            *(error_message) = "Timeout interval - offered value was not accepted";
            return ERR_CODE_OPTIONS_FAILED;
        }
    }
    else{
        client_options->timeout_interval = DEFAULT_TIMEOUT;
    }

    return PACKET_OK_CODE;
}

int negotiate_option_server(option_info_t *client_options, option_info_t *server_options, string* error_message){
    //set server block size option
    if (client_options->option_blocksize && server_options->option_blocksize){
        if (client_options->blocksize < MIN_BLKSIZE_VALUE || client_options->blocksize > MAX_BLKSIZE_VALUE){
            *(error_message) = "Block size - offered value is outside of range of alloved values <8, 65464>";
            return ERR_CODE_OPTIONS_FAILED;
        }
        else{
            server_options->blocksize = client_options->blocksize;
        }
    }
    else{
        server_options->blocksize = DEFAULT_BLOCK_SIZE;
    }

    //set server timeout interval option
    if (client_options->option_timeout_interval && server_options->option_timeout_interval){
        if (client_options->timeout_interval < MIN_TIMEOUT_VALUE || client_options->timeout_interval > MAX_TIMEOUT_VALUE){
            *(error_message) = "Timeout interval - offered value is outside of range of alloved values <1, 255>";
            return ERR_CODE_OPTIONS_FAILED;
        }
        else{
            server_options->timeout_interval = client_options->timeout_interval;
        }
    }
    else{
        server_options->timeout_interval = DEFAULT_TIMEOUT;
    }

    return PACKET_OK_CODE;
}

int recvfrom_timeout(connection_info_t *connection_information, option_info_t *option_information, char *buffer, int times_retransmitted){
    fd_set read_sockets;
    FD_ZERO(&read_sockets);
    FD_SET(connection_information->socket, &read_sockets);

    int current_timeout_interval = option_information->timeout_interval;
    if (times_retransmitted != 0){
        current_timeout_interval *= (EXPONENTIAL_BACKOFF_MULTIPLIER * times_retransmitted);
    }

    //Exponenitial backoff
    struct timeval timeout = {current_timeout_interval, 0};

    int selected = select(connection_information->socket + 1 , &read_sockets , NULL , NULL , &timeout);
    if(selected == -1){
        cout << "ERROR: select - error\n";
        return ERR_CODE_SELECT;
    }
    else if (selected == 0){
        cout << "recvfrom - timeout\n";
        return ERR_CODE_TIMEOUT;
    }

    return recvfrom(connection_information->socket, buffer, option_information->blocksize + 4, 0,
                    connection_information->address, &connection_information->address_size);
}

int recvfrom_retransmit(connection_info_t *connection_information, option_info_t *option_information, char *buffer, string packet, int tid_expected){
    int return_value = -1;
    for (int i = 0; i <= MAX_RETRANSMIT_ATTEMPTS; i++){
        if (i == MAX_RETRANSMIT_ATTEMPTS){
            return -1;
        }

        return_value = recvfrom_timeout(connection_information, option_information, buffer, i);

        if (return_value == ERR_CODE_TIMEOUT){
            //retransmit packet
            int bytes_tx = sendto(connection_information->socket, packet.c_str(), packet.size(), 0,
                            connection_information->address, connection_information->address_size);
            if (bytes_tx < 0) cout << "ERROR: sendto - client WRQ/RRQ packet\n";
        }
        else if (return_value < 0){
            return -1;
        }
        else{
            //checking if the TID of host is valid
            if((htons(((struct sockaddr_in*)connection_information->address)->sin_port) != tid_expected) && tid_expected != TID_NOT_SET_YET){
                i--;
                log_stranger_packet(connection_information, buffer);
                string error_message = "Invalid TID - Transfer ID doesn't match established communication";
                send_error_packet(connection_information, ERR_CODE_UNKNOWN_TID, error_message, DEFAULT_TIMEOUT, false);
                continue;
            }
            else{
                //packet successfully received
                break;
            }
        }
    }
    return return_value;
}

string send_wrq_rrq(connection_info_t *connection_information, communication_info_t *communication_information, tftp_rrq_wrq_packet_t *init_communication_packet, option_info_t *option_information, bool is_rrq, string temp_path){
    init_communication_packet->mode = communication_information->mode;
    init_communication_packet->options = *option_information;

    if (is_rrq){
        //setting RRQ packet
        init_communication_packet->options.transfer_size = 0;
        init_communication_packet->filename = communication_information->file_path_source;
        init_communication_packet->opcode = RRQ_OPCODE;
    }
    else{
        //setting WRQ packet
        init_communication_packet->options.transfer_size = get_cin_size(temp_path);
        init_communication_packet->filename = communication_information->file_path_dest;
        init_communication_packet->opcode = WRQ_OPCODE;
    }

    string packet = serialize_packet_struct(init_communication_packet);

    int bytes_tx = sendto(connection_information->socket, packet.c_str(), packet.size(), 0,
                            connection_information->address, connection_information->address_size);
    if (bytes_tx < 0) cout << "ERROR: sendto - client WRQ/RRQ packet\n";

    return packet;
}

string send_ack(connection_info_t *connection_information, int block_number){
    tftp_ack_packet_t ack_packet_struct;
    ack_packet_struct.block_number = block_number;

    string ack_packet = serialize_packet_struct(&ack_packet_struct);

    int bytes_tx = sendto(connection_information->socket, ack_packet.c_str(), ack_packet.size(), 0,
                            connection_information->address, connection_information->address_size);
    if (bytes_tx < 0) cout << "ERROR: sendto - sending acknowledgment\n";

    return ack_packet;
}

string send_data(connection_info_t *connection_information, int block_number, char *data_block, int loaded_actual){
    tftp_data_packet_t data_packet_struct;
    data_packet_struct.block_number = block_number;
    data_packet_struct.data = data_block;

    string data_packet = serialize_packet_struct(&data_packet_struct, loaded_actual);

    int bytes_tx = sendto(connection_information->socket, data_packet.c_str(), data_packet.size(), 0,
                            connection_information->address, connection_information->address_size);
    if (bytes_tx < 0) cout << "ERROR: sendto - sending data\n";

    return data_packet;
}

string send_oack(connection_info_t *connection_information, option_info_t *init_options, option_info_t *server_options, string path, bool is_rrq){
    namespace fs = std::filesystem;

    tftp_oack_packet_t oack_packet_struct;

    //selecting options that will be sent
    if (!init_options->option_blocksize){
        server_options->option_blocksize = false;
    }
    if (!init_options->option_timeout_interval){
        server_options->option_timeout_interval = false;
    }
    if (!init_options->option_transfer_size){
        server_options->option_transfer_size = false;
    }
    else{
        server_options->transfer_size = is_rrq ? (unsigned int) fs::file_size(path) : init_options->transfer_size;
    }

    oack_packet_struct.options = *server_options;

    string oack_packet = serialize_packet_struct(&oack_packet_struct);
    int bytes_tx = sendto(connection_information->socket, oack_packet.c_str(), oack_packet.size(), 0,
                            connection_information->address, connection_information->address_size);
    if (bytes_tx < 0) cout << "ERROR: sendto - server initialization communication acknowledgment\n";

    return oack_packet;
}

string send_error_packet(connection_info_t *connection_information, int error_code, string error_message, unsigned int error_timeout, bool timeout_enable){
    tftp_error_packet_t error_packet_struct;
    error_packet_struct.error_code = error_code;
    error_packet_struct.error_message = error_message;

    option_info_t option_information_err;
    option_information_err.timeout_interval = error_timeout;
    char buffer[option_information_err.blocksize + DATA_PACKET_OFFSET];

    string error_packet = serialize_packet_struct(&error_packet_struct);

    for(int i = 0; i < MAX_RETRANSMIT_ATTEMPTS; i++){
        int bytes_tx = sendto(connection_information->socket, error_packet.c_str(), error_packet.size(), 0,
                                connection_information->address, connection_information->address_size);
        if (bytes_tx < 0) cout << ("ERROR: sendto - sending error\n");

        if (!timeout_enable){
            break;
        }

        int recv_timeout_ret_code = recvfrom_timeout(connection_information, &option_information_err, buffer, i);
        if (recv_timeout_ret_code == ERR_CODE_TIMEOUT){
            //error message was most probably successfully delivered
            break;
        }

        //retransmit
    }


    return error_packet;
}

int receive_wrq_rrq(connection_info_t *connection_information, tftp_rrq_wrq_packet_t *init_communication_packet, char *buffer){
    string error_message;
    deserialize_packet_struct(init_communication_packet, buffer);

    //log
    log_wrq_rrq(connection_information, init_communication_packet);

    int return_code = check_packet_content(init_communication_packet, &error_message);
    if (return_code != PACKET_OK_CODE){
        send_error_packet(connection_information, return_code, error_message);
        return return_code;
    }
    return PACKET_OK_CODE;
}

int receive_ack(connection_info_t *connection_information, char *buffer, int expected_block_number, unsigned int timeout){
    string error_message;

    tftp_ack_packet_t ack_packet_init;
    deserialize_packet_struct(&ack_packet_init, buffer);

    //log
    log_ack(connection_information, &ack_packet_init);

    int return_code = check_packet_content(&ack_packet_init, expected_block_number, &error_message);
    if (return_code == ERR_CODE_ILLEGAL_OPERATION){
        send_error_packet(connection_information, return_code, error_message, timeout);
    }
    return return_code;
}

int receive_oack(connection_info_t *connection_information, option_info_t *init_options, char *buffer){
    namespace fs = std::filesystem;
    string error_message;

    tftp_oack_packet_t oack_packet_struct;
    deserialize_packet_struct(&oack_packet_struct, buffer);

    //log
    log_oack(connection_information, &oack_packet_struct);

    int return_code = negotiate_option_client(init_options, &oack_packet_struct.options, &error_message);
    if (return_code != PACKET_OK_CODE){
        send_error_packet(connection_information, return_code, error_message);
        return return_code;
    }

    if (init_options->option_transfer_size && init_options->transfer_size == 0){
        if (fs::space("./").available < oack_packet_struct.options.transfer_size){
            error_message = "Transfer size - not enough space on disk to download the file";
            send_error_packet(connection_information, ERR_CODE_DISK_FULL, error_message);
            return ERR_CODE_DISK_FULL;
        }
    }
    return PACKET_OK_CODE;
}

int receive_data(connection_info_t *connection_information, char *buffer, int bytes_read, ofstream &file_write, string mode, unsigned int timeout, int expected_block_number){
    string error_message;

    tftp_data_packet_t data_packet;
    deserialize_packet_struct(&data_packet, buffer);

    log_data(connection_information, &data_packet);

    int return_code = check_packet_content(&data_packet, expected_block_number, &error_message);
    if (return_code == ERR_CODE_ILLEGAL_OPERATION){
        send_error_packet(connection_information, return_code, error_message, timeout);
        return return_code;
    }
    else if (return_code == DUPLICATED_PACKET){
        return return_code;
    }

    //formating NETASCII data (to linux notation).
    if (mode == MODE_NETASCII){
        for (int i = 0; i < bytes_read - DATA_PACKET_OFFSET; i++){
            if (data_packet.data[i] == CR_VALUE){
                if (data_packet.data[i + 1] == '\n'){
                    memmove(&data_packet.data[i], &data_packet.data[i + 1], bytes_read - i - 1);
                    bytes_read--;
                    i--;
                }
                else if(data_packet.data[i + 1] == '\x00'){
                    memmove(&data_packet.data[i + 1], &data_packet.data[i + 2], bytes_read - i - 2);
                    bytes_read--;
                }
            }
        }
    }

    //writing data into the file
    file_write.write(data_packet.data, bytes_read - DATA_PACKET_OFFSET);

    return PACKET_OK_CODE;
}

void receive_error(connection_info_t *connection_information, char *buffer){
    tftp_error_packet_t error_packet_struct;
    deserialize_packet_struct(&error_packet_struct, buffer);
    log_error(connection_information, &error_packet_struct);
}

int write_to_file(connection_info_t *connection_information, option_info_t *options, ofstream &file_write, string packet_to_be_send, string mode, int tid_expected, int expected_block_number){
    string error_message = "";
    int datagram_size = options->blocksize + DATA_PACKET_OFFSET;
    char buffer[datagram_size];

    while (true){

        //handlig duplicated recieved Data
        int bytes_rx;
        int receive_data_ret_code;
        do{
            bzero(buffer, datagram_size);

            bytes_rx = recvfrom_retransmit(connection_information, options, buffer, packet_to_be_send, tid_expected);
            if (bytes_rx < 0){
                return PROG_RET_CODE_ERR;
            }

            char opcode_char[2] = {buffer[0], buffer[1]};
            if (chars_to_short(opcode_char) == ERROR_OPCODE){
                receive_error(connection_information, buffer);
                return PROG_RET_CODE_ERR;
            }

            receive_data_ret_code = receive_data(connection_information, buffer, bytes_rx, file_write, mode, options->timeout_interval, expected_block_number);

            if (receive_data_ret_code == ERR_CODE_ILLEGAL_OPERATION){
                return PROG_RET_CODE_ERR;
            }
            else if(receive_data_ret_code == PACKET_OK_CODE){
                break;
            }

            int bytes_tx = sendto(connection_information->socket, packet_to_be_send.c_str(), packet_to_be_send.size(), 0,
                            connection_information->address, connection_information->address_size);
            if (bytes_tx < 0) cout << "ERROR: sendto - sending data\n";
        }
        while(receive_data_ret_code);

        packet_to_be_send = send_ack(connection_information, expected_block_number);

        expected_block_number++;

        //end transfer if number of received Bytes is lovwer than datagram size
        if (bytes_rx < (datagram_size)){
            for(int i = 0; i < MAX_RETRANSMIT_ATTEMPTS; i++){
                int recv_timeout_ret_code = recvfrom_timeout(connection_information, options, buffer, 0);
                if (recv_timeout_ret_code == ERR_CODE_TIMEOUT){
                    //error message was most probably successfully delivered
                    break;
                }

                int bytes_tx = sendto(connection_information->socket, packet_to_be_send.c_str(), packet_to_be_send.size(), i,
                                        connection_information->address, connection_information->address_size);
                if (bytes_tx < 0) cout << ("ERROR: sendto - sending error\n");

                //retransmit
            }
            break;
        }
    }
    return PROG_RET_CODE_OK;
}

int read_from_file(connection_info_t *connection_information, string filename, option_info_t *options, string mode, int tid_expected){
    string error_message = "";
    string packet_to_be_send;

    int datagram_size = options->blocksize + DATA_PACKET_OFFSET;

    char buffer[datagram_size];
    char data_block[options->blocksize];

    unsigned int loaded_actual = 0;
    bool lf_on_new = false;
    bool null_on_new = false;

    ushort current_block_number = 1;

    ifstream file_read(filename);

    //reading data from file (with format to NETASCII mode)
    do{
        bzero(data_block, options->blocksize);
        loaded_actual = 0;

        if (lf_on_new){
            data_block[loaded_actual++] = '\n';
            lf_on_new = false;
        }
        else if (null_on_new){
            data_block[loaded_actual++] = '\x00';
            null_on_new = false;
        }
        while (loaded_actual < options->blocksize){
            char c;
            file_read.get(c);
            if (file_read.eof()){
                break;
            }
            else if (c == '\n' && mode == MODE_NETASCII){
                data_block[loaded_actual++] = CR_VALUE;
                if (loaded_actual == options->blocksize){
                    lf_on_new = true;
                }
                else{
                    data_block[loaded_actual++] = c;
                }
            }
            else if (c == CR_VALUE && mode == MODE_NETASCII){
                data_block[loaded_actual++] = CR_VALUE;
                if (loaded_actual == options->blocksize){
                    null_on_new = true;
                }
                else{
                    data_block[loaded_actual++] = '\x00';
                }
            }
            else{
                data_block[loaded_actual++] = c;
            }
        }

        packet_to_be_send = send_data(connection_information, current_block_number, data_block, loaded_actual);


        //handlig duplicated recieved Acks
        int receive_ack_ret_code;
        do {
            bzero(buffer, datagram_size);

            int bytes_rx = recvfrom_retransmit(connection_information, options, buffer, packet_to_be_send, tid_expected);
            if (bytes_rx < 0){
                file_read.close();
                return 1;
            }

            char opcode_char[2] = {buffer[0], buffer[1]};
            if (chars_to_short(opcode_char) == ERROR_OPCODE){
                receive_error(connection_information, buffer);
                file_read.close();
                return 1;
            }

            receive_ack_ret_code = receive_ack(connection_information, buffer, current_block_number, options->timeout_interval);

            if (receive_ack_ret_code == ERR_CODE_ILLEGAL_OPERATION){
                file_read.close();
                return 1;
            }
            else if(receive_ack_ret_code == PACKET_OK_CODE){
                break;
            }

            //Sorcerer's Apprentice Syndrome - Data should be never send from sender again on duplicate ACK
            // int bytes_tx = sendto(connection_information->socket, packet_to_be_send.c_str(), packet_to_be_send.size(), 0,
            //                 connection_information->address, connection_information->address_size);
            // if (bytes_tx < 0) cout << "ERROR: sendto - sending data\n";
        }
        while(receive_ack_ret_code == DUPLICATED_PACKET);

        current_block_number++;

        bzero(data_block, options->blocksize );
    }
    while(loaded_actual >= options->blocksize); //end transfer if number of sent data Bytes is lovwer than block size

    file_read.close();
    return 0;
}

int get_destination_port(int socket){
    struct sockaddr client_server_addr;
    socklen_t client_server_addr_len = sizeof(client_server_addr);
    getsockname(socket, &client_server_addr, &client_server_addr_len);
    return htons(((struct sockaddr_in*)&client_server_addr)->sin_port);
}

void log_wrq_rrq(connection_info_t *connection_information, tftp_rrq_wrq_packet_t *packet){
    string packet_type;
    if (packet->opcode == RRQ_OPCODE){
        packet_type = "RRQ ";
    }
    else{
        packet_type = "WRQ ";
    }

    cerr << packet_type
        << inet_ntoa((((struct sockaddr_in*)connection_information->address)->sin_addr))
        << ":"
        << htons(((struct sockaddr_in*)connection_information->address)->sin_port)
        << " \"" << packet->filename << "\" "
        << packet->mode;

    log_options(&packet->options);
    cerr << "\n";
}

void log_data(connection_info_t *connection_information, tftp_data_packet_t *packet){
    cerr << "DATA "
        << inet_ntoa((((struct sockaddr_in*)connection_information->address)->sin_addr))
        << ":"
        << htons(((struct sockaddr_in*)connection_information->address)->sin_port)
        << ":"
        << get_destination_port(connection_information->socket)
        << " " << packet->block_number << "\n";
}

void log_ack(connection_info_t *connection_information, tftp_ack_packet_t *packet){
    cerr << "ACK "
        << inet_ntoa((((struct sockaddr_in*)connection_information->address)->sin_addr))
        << ":"
        << htons(((struct sockaddr_in*)connection_information->address)->sin_port)
        << " " << packet->block_number << "\n";
}

void log_error(connection_info_t *connection_information, tftp_error_packet_t *packet){
    cerr << "ERROR "
        << inet_ntoa((((struct sockaddr_in*)connection_information->address)->sin_addr))
        << ":"
        << htons(((struct sockaddr_in*)connection_information->address)->sin_port)
        << ":"
        << get_destination_port(connection_information->socket)
        << " " << packet->error_code
        << " \"" << packet->error_message << "\" \n";
}

void log_oack(connection_info_t *connection_information, tftp_oack_packet_t *packet){
    cerr << "OACK "
        << inet_ntoa((((struct sockaddr_in*)connection_information->address)->sin_addr))
        << ":"
        << htons(((struct sockaddr_in*)connection_information->address)->sin_port);
    log_options(&packet->options);
    cerr << "\n";
}

void log_options(option_info_t *options){
    for (int i = 0; i < SUPPORTED_OPTIONS_NUMBER; i++){
        if (options->option_order[i] == TRANSFER_SIZE){
            cerr << " " << "tsize" << "=" << options->transfer_size;
        }
        else if (options->option_order[i] == TIMEOUT){
            cerr << " " << "timeout" << "=" << options->timeout_interval;
        }
        else if (options->option_order[i] == BLOCKSIZE){
            cerr << " " << "blksize" << "=" << options->blocksize;
        }
        else{
            break;
        }
    }

}

void log_stranger_packet(connection_info_t *connection_information, char* buffer){
    char opcode_char[2] = {buffer[0], buffer[1]};
    ushort opcode = chars_to_short(opcode_char);
    if (opcode == RRQ_OPCODE || opcode == WRQ_OPCODE){
        tftp_rrq_wrq_packet_t packet_struct_wr;
        deserialize_packet_struct(&packet_struct_wr, buffer);
        log_wrq_rrq(connection_information, &packet_struct_wr);
    }
    else if (opcode == DATA_OPCODE){
        tftp_data_packet_t packet_struct_d;
        deserialize_packet_struct(&packet_struct_d, buffer);
        log_data(connection_information, &packet_struct_d);
    }
    else if (opcode == ACK_OPCODE){
        tftp_ack_packet_t packet_struct_a;
        deserialize_packet_struct(&packet_struct_a, buffer);
        log_ack(connection_information, &packet_struct_a);
    }
    else if (opcode == ERROR_OPCODE){
        tftp_error_packet_t packet_struct_e;
        deserialize_packet_struct(&packet_struct_e, buffer);
        log_error(connection_information, &packet_struct_e);
    }
    else if (opcode == OACK_OPCODE){
        tftp_oack_packet_t packet_struct_o;
        deserialize_packet_struct(&packet_struct_o, buffer);
        log_oack(connection_information, &packet_struct_o);
    }
}