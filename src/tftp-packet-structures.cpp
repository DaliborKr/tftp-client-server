/**
 * ISA - Projekt: TFTP Klient + Server
 * @file tftp-packet-structures.cpp
 * @brief Definition of the packet structures a operations on them
 * @author Dalibor Kříčka (xkrick01)
 */


#include "tftp-packet-structures.hpp"


void short_to_chars(ushort short_number, char *number_chars){
    number_chars[0] = short_number >> 8;
    number_chars[1] = short_number & 0xFF;
}

ushort chars_to_short(char *number_chars){
    ushort short_number = number_chars[0] << 8;
    short_number |= number_chars[1] & 0xFF;
    return short_number;
}


string serialize_packet_struct(tftp_rrq_wrq_packet_t *packet_struct){
    char opcode_char[2];
    short_to_chars(packet_struct->opcode, opcode_char);

    string sequence_build = opcode_char[1] + packet_struct->filename + '\x00' + packet_struct->mode + '\x00';
    sequence_build = opcode_char[0] + sequence_build;

    string sequence_options = serialize_option_info(&packet_struct->options);
    if (sequence_options != ""){
        sequence_build.append(sequence_options);
    }

    return sequence_build;
}


string serialize_packet_struct(tftp_data_packet_t *packet_struct, int loaded_data_number){
    char opcode_char[2];
    short_to_chars(packet_struct->opcode, opcode_char);
    char block_number_char[2];
    short_to_chars(packet_struct->block_number, block_number_char);

    string sequence_build = "";
    sequence_build = block_number_char[1] + sequence_build;
    sequence_build = block_number_char[0] + sequence_build;
    sequence_build = opcode_char[1] + sequence_build;
    sequence_build = opcode_char[0] + sequence_build;
    sequence_build.append(packet_struct->data, loaded_data_number);
    
    return sequence_build;
}


string serialize_packet_struct(tftp_ack_packet_t *packet_struct){
    char opcode_char[2];
    short_to_chars(packet_struct->opcode, opcode_char);
    char block_number_char[2];
    short_to_chars(packet_struct->block_number, block_number_char);

    string sequence_build = "";
    sequence_build = block_number_char[1] + sequence_build;
    sequence_build = block_number_char[0] + sequence_build;
    sequence_build = opcode_char[1] + sequence_build;
    sequence_build = opcode_char[0] + sequence_build;

    return sequence_build;
}


string serialize_packet_struct(tftp_error_packet_t *packet_struct){
    char opcode_char[2];
    short_to_chars(packet_struct->opcode, opcode_char);
    char error_code_char[2];
    short_to_chars(packet_struct->error_code, error_code_char);

    string sequence_build = packet_struct->error_message + '\x00';
    sequence_build = error_code_char[1] + sequence_build;
    sequence_build = error_code_char[0] + sequence_build;
    sequence_build = opcode_char[1] + sequence_build;
    sequence_build = opcode_char[0] + sequence_build;

    return sequence_build;
}


string serialize_packet_struct(tftp_oack_packet_t *packet_struct){
    char opcode_char[2];
    short_to_chars(packet_struct->opcode, opcode_char);

    string sequence_build = opcode_char[1] + serialize_option_info(&packet_struct->options);
    sequence_build = opcode_char[0] + sequence_build;

    return sequence_build;
}


string serialize_option_info(option_info_t *option_information){
    string sequence = "";
    if (option_information->option_transfer_size){
        sequence += "tsize";
        sequence += '\x00' + to_string(option_information->transfer_size) + '\x00';
    }
    if (option_information->option_timeout_interval){
        sequence += "timeout";
        sequence += '\x00' + to_string(option_information->timeout_interval) + '\x00';
    }
    if (option_information->option_blocksize){
        sequence += "blksize";
        sequence += '\x00' + to_string(option_information->blocksize) + '\x00';
    }
    return sequence;
}


void deserialize_packet_struct(tftp_rrq_wrq_packet_t *packet_struct, char *sequence){
    char opcode_char[2] = {sequence[0], sequence[1]};
    
    int i = 2;
    string filename = "";
    string mode = "";

    //deserializing file name
    while (sequence[i] != '\0'){
        filename += sequence[i++];
    }

    i++;

    //deserializing mode
    while (sequence[i] != '\0'){
        mode += tolower(sequence[i++]);
    }

    packet_struct->opcode = chars_to_short(opcode_char);
    packet_struct->filename = filename;
    packet_struct->mode = mode;
    deserialize_option_info(&packet_struct->options, sequence, ++i);
}


void deserialize_packet_struct(tftp_data_packet_t *packet_struct, char *sequence){
    char opcode_char[2] = {sequence[0], sequence[1]};
    char block_number_char[2] = {sequence[2], sequence[3]};

    packet_struct->opcode = chars_to_short(opcode_char);
    packet_struct->block_number = chars_to_short(block_number_char);
    packet_struct->data = sequence + DATA_PACKET_OFFSET;
}


void deserialize_packet_struct(tftp_ack_packet_t *packet_struct, char *sequence){
    char opcode_char[2] = {sequence[0], sequence[1]};
    char block_number_char[2] = {sequence[2], sequence[3]};

    packet_struct->opcode = chars_to_short(opcode_char);
    packet_struct->block_number = chars_to_short(block_number_char);
}


void deserialize_packet_struct(tftp_error_packet_t *packet_struct, char *sequence){
    char opcode_char[2] = {sequence[0], sequence[1]};
    char error_code_char[2] = {sequence[2], sequence[3]};

    string error_message = "";

    //deserializing error message
    for (int i = 4; sequence[i] != '\0'; i++){
        error_message += sequence[i];
    }

    packet_struct->opcode = chars_to_short(opcode_char);
    packet_struct->error_code = chars_to_short(error_code_char);
    packet_struct->error_message = error_message;
}


void deserialize_packet_struct(tftp_oack_packet_t *packet_struct, char *sequence){
    char opcode_char[2] = {sequence[0], sequence[1]};

    packet_struct->opcode = chars_to_short(opcode_char);
    deserialize_option_info(&packet_struct->options, sequence, 2);
}


void deserialize_option_info(option_info_t *option_information, char *sequence, int options_start_index){
    string option;
    string value;
    int order_number = 0;

    //deserializing options
    for (int i = options_start_index; sequence[i] != '\x00';){
        option = "";
        value = "";

        //deserializing option name
        while (sequence[i] != '\x00'){
            option += tolower(sequence[i++]);
        }
        i++;

        //deserializing option value
        while (sequence[i] != '\x00'){
            value += sequence[i++];
        }
        i++;

        int value_int;

        try{
            value_int = stoi(value);
        }
        catch(exception &err){
            continue;
        }
        if (option == "blksize"){
            option_information->option_blocksize = true;
            option_information->blocksize = value_int;
            option_information->option_order[order_number++] = BLOCKSIZE;
        }
        else if (option == "timeout"){
            option_information->option_timeout_interval = true;
            option_information->timeout_interval = value_int;
            option_information->option_order[order_number++] = TIMEOUT;
        }
        else if (option == "tsize"){
            option_information->option_transfer_size = true;
            option_information->transfer_size = value_int;
            option_information->option_order[order_number++] = TRANSFER_SIZE;
        }
    }
}


int check_packet_content(tftp_rrq_wrq_packet_t *packet_struct, string *error_message){
    if (packet_struct->opcode != RRQ_OPCODE && packet_struct->opcode != WRQ_OPCODE){
        *(error_message) = "Expected RRQ or WRQ packet";
        return ERR_CODE_ILLEGAL_OPERATION;
    }
    
    if (packet_struct->mode != "octet" && packet_struct->mode != "netascii"){
        *(error_message) = "Expected 'octed' or 'netascii'";
        return ERR_CODE_ILLEGAL_OPERATION;
    }

    return PACKET_OK_CODE;
}


int check_packet_content(tftp_ack_packet_t *packet_struct, ushort expected_block_number, string *error_message){
    if (packet_struct->opcode != ACK_OPCODE){
        *(error_message) = "Expected ACK packet";
        return ERR_CODE_ILLEGAL_OPERATION;
    }

    if (packet_struct->block_number > expected_block_number){
        *(error_message) = "Inconsistent acknowledgement - Expected block number is bigger than recieved.";
        return ERR_CODE_ILLEGAL_OPERATION;
    }
    else if (packet_struct->block_number < expected_block_number){
        return DUPLICATED_PACKET;
    }

    return PACKET_OK_CODE;
}


int check_packet_content(tftp_data_packet_t *packet_struct, ushort expected_block_number, string *error_message){
    if (packet_struct->opcode != DATA_OPCODE){
        *(error_message) = "Expected DATA packet";
        return ERR_CODE_ILLEGAL_OPERATION;
    }

    if (packet_struct->block_number < 1){
        string received_str = to_string(packet_struct->block_number);
        *(error_message) = "DATA packet block number has to be greater than 0 ";
        return ERR_CODE_ILLEGAL_OPERATION;
    }
    else if (packet_struct->block_number > expected_block_number){
        string received_str = to_string(packet_struct->block_number);
        *(error_message) = "DATA packet block number cannot be higher than the expected block number";
        return ERR_CODE_ILLEGAL_OPERATION;
    }
    else if (packet_struct->block_number < expected_block_number){
        return DUPLICATED_PACKET;
    }

    return PACKET_OK_CODE;
}