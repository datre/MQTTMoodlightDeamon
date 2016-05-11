#include "durchreiche.h"
#include "rs232.h"

unsigned crc8(unsigned crc, unsigned char *data, size_t len)
{
    unsigned char *end;

    if (len == 0)
        return crc;
    crc ^= 0xff;
    end = data + len;
    do {
        crc = crc8_table[crc ^ *data++];
    } while (data < end);
    return crc ^ 0xff;
}

bool Send(char* filename, bool* file, struct Packet* packet)
{
    // Null Pointer check
    if(filename == NULL || file == NULL || packet == NULL)
    {
        return false;
    }
    unsigned char tosend[35] = {0};
    tosend[0] = 0x40;
    tosend[1] = packet->Source;
    tosend[2] = packet->Destination;
    tosend[3] = packet->Length;
    for(int i=0; i<30; i++)
    {
        tosend[i+4] = packet->Payload[i];
    }
    unsigned checksum = crc8(0, &tosend, 34);
    tosend[34] = checksum;
    unsigned PortNumber = 38;
    for(int i=0; i<38; i++) //Determine Portnumber
    {
        if(strcmp(filename, comports[i]) == 0)
        {
            PortNumber = i;
            break;
        }
    }
    if(PortNumber == 38) //No
    {
        return false;
    }
    if(*file)
    {
        if(RS232_SendBuf(PortNumber, tosend, 35))
        {
            return true;
        }
    }
    else
    {
        char mode[]={'8','N','1',0};
        if(RS232_OpenComport(1, 115200, mode))
        {
            *file = true;
            if(RS232_SendBuf(PortNumber, tosend, 35)) //TODO Correct PortNumber
            {
                return true;
            }
        }
    }
    return false;

}
