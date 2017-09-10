#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>  /* File Control Definitions          */
#include <termios.h>/* POSIX Terminal Control Definitions*/
#include <unistd.h> /* UNIX Standard Definitions         */
#include <errno.h>  /* ERROR Number Definitions          */
#include <unistd.h>
#include <sys/ioctl.h> //ioctl() call defenitions

struct termios SerialPortSettings;
uint8_t END_OF_FILE = 0;
int fd;

uint8_t charToHex(char* c) {
    uint8_t tmp[2] = { 0, 0 };
    uint8_t result = 0, i = 0;
    for (i = 0; i < 2; i++) {
        if (c[i] >= 'a') tmp[i] = c[i] - 'a' + 10;
        else if (c[i] >= 'A') tmp[i] = c[i] - 'A' + 10;
        else if (c[i] >= '0') tmp[i] = c[i] - '0';
        //printf("tmp value %d for %d is %d, ", i, c[i], tmp[i]);
    }
    result = (tmp[0] << 4) + tmp[1];
    //printf("the result is %d\r\n", result);
    return result;
}

char stringCompaire(char * str1, char * str2, char bits, int sizeofstr1) {
    int i = 0, j = 0;
    while (str1[i++] != str2[j]) {
        if (i >= sizeofstr1) return 1;
    }
    for (j = 1; j < bits; j++) {
        if (str1[i++] != str2[j] || i >= sizeofstr1) return 1;
    }
    return 0;
}

void hexReadLine(FILE *readFile) {
    uint8_t numChars = 0, i = 0;
    char* buf = (char *)malloc(sizeof(char) * 256);
    char c = fgetc(readFile);
    char reset_buf[] = "-";
    int bytes_written  = 0;

    while (c != EOF && c != '\n') {
        buf[numChars++] = c;
        c = fgetc(readFile);
    }
    if (c == EOF) {
        END_OF_FILE = 1;
    } else {
        numChars--;
        if (buf[numChars] == '\r') buf[numChars] = '\0';
        else buf[++numChars] = '\0';
    }

    if (buf[0] != ':') {
        printf("ERROR: The file formate is not right\r\n");
        // uart to chip '-' to clean code
        bytes_written = write(fd, reset_buf, sizeof(reset_buf));
        exit(1);
    }

    uint8_t* d = (uint8_t *)malloc(sizeof(uint8_t) * (numChars / 2));
    uint8_t numData = 0;
    for (i = 1; i < numChars - 1; i += 2) {
        char tmp[2];
        tmp[0] = buf[i];
        tmp[1] = buf[i + 1];
        d[numData++] = charToHex(tmp);
    }

    // for (i = 0; i < numData; i++) {
    //     printf("%02X ", d[i]);
    // }
    // printf("\r\n");

    uint8_t null_data_check = 0;
    if (d[3] == 0) {
        for (i = 4; i < numData; i++) {
            if (d[i] == 0xFF) null_data_check++;
        }
        if (null_data_check == d[0]) END_OF_FILE = 1;
    }
    if (d[3] == 1) END_OF_FILE = 1;
    if (numData != (d[0] + 5)) {
        printf("ERROR: The data is not right\r\n");
        // uart to chip '-' to clean code
        bytes_written = write(fd, reset_buf, sizeof(reset_buf));
        exit(1);
    }

    uint8_t checksum = 0;
    for (i = 0; i < numData - 1; i++) {
        checksum += d[i];
    }
    checksum = ~checksum + 1;
    if (d[numData - 1] != checksum) {
        printf("ERROR: Checksum %02X is not right, expectate %02X\r\n", d[numData - 1], checksum);
        // uart to chip '-' to clean code
        bytes_written = write(fd, reset_buf, sizeof(reset_buf));
        exit(1);
    }

    if (d[3] == 0 && END_OF_FILE != 1) {
        uint8_t* buf_data = (uint8_t *)malloc(sizeof(uint8_t) * (d[0] + 5));
        buf_data[0] = '+';
        buf_data[1] = d[1];
        buf_data[2] = d[2];
        buf_data[3] = d[0];
        for (i = 4; i < (d[0] + 4); i ++) {
            buf_data[i] = d[i];
        }
        // buf_data[d[0] + 4] = '\r';

        // printf("size of buf %lu, %d\r\n", sizeof(buf_data), d[0] + 5);
        // uart to chip
        // bytes_written = write(fd, buf_data, d[0] + 5);
        bytes_written = write(fd, buf_data, d[0] + 4);

        printf(" %c", buf_data[0]);
        buf_data[0] = 0xFF;
        for (i = 1; i < (d[0] + 4); i++) {
            printf(" %02X", buf_data[i]);
            buf_data[i] = 0xFF;
        }
        printf(" -- %d\r\n", bytes_written);
    }
}

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        printf("ERROR: Please use formate ./serial <serial port> <program hex file>\r\n");
        exit(1);
    }

    FILE *hexFile;
    hexFile = fopen(argv[2], "r");
    if (hexFile == NULL) {
        exit(1);
    }

    fd = open(argv[1], O_RDWR | O_NOCTTY);
    if(fd == -1)
        printf("\r\n  Error! in Opening %s\r\n", argv[1]);
    else
        printf("\r\n  %s Opened Successfully\r\n", argv[1]);

    tcgetattr(fd, &SerialPortSettings);
    cfsetispeed(&SerialPortSettings, B115200);
    cfsetospeed(&SerialPortSettings, B115200);

    SerialPortSettings.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */
    SerialPortSettings.c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits,here it is cleared so 1 Stop bit */
    SerialPortSettings.c_cflag &= ~CSIZE;	 /* Clears the mask for setting the data size             */
    SerialPortSettings.c_cflag |=  CS8;      /* Set the data bits = 8                                 */

    SerialPortSettings.c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
    SerialPortSettings.c_cflag |= CREAD | CLOCAL; /* Enable receiver,Ignore Modem Control lines       */

    SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);          /* Disable XON/XOFF flow control both i/p and o/p */
    SerialPortSettings.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);  /* Non Cannonical mode                            */

    SerialPortSettings.c_oflag &= ~OPOST;/*No Output Processing*/

    /* Setting Time outs */
    SerialPortSettings.c_cc[VMIN] = 1; /* Read at least 10 characters */
    SerialPortSettings.c_cc[VTIME] = 0; /* Wait indefinetly   */

    if((tcsetattr(fd,TCSANOW,&SerialPortSettings)) != 0) /* Set the attributes to the termios structure*/
	    printf("\r\n  ERROR ! in Setting attributes");
	else
        printf("\r\n  BaudRate = 115200 \r\n  StopBits = 1 \r\n  Parity   = none \r\n");

    /*------------------------------- Read data from serial port -----------------------------*/

	char read_buffer[20];   /* Buffer to store the data received              */
	int bytes_read = 0;    /* Number of bytes read by the read() system call */
    int bytes_written  = 0;  	/* Value for storing the number of bytes written to the port */
	int i = 0;
    char start_bootloader_buf[] = "b";
    char error = 0;

    int DTR_flag;
    DTR_flag = TIOCM_DTR;

	tcflush(fd, TCIFLUSH);   /* Discards old data in the rx buffer            */

    printf("Reset the Chip ... \r\n");
    ioctl(fd, TIOCMBIS, &DTR_flag);//Set RTS pin
    usleep(10000);
    ioctl(fd, TIOCMBIC, &DTR_flag);//Clear RTS pin

    while (stringCompaire(read_buffer, "System Booting", 14, bytes_read) != 0) {
        bytes_read = read(fd, &read_buffer, 20); /* Read the data */
    }

    for(i = 0; i < bytes_read; i++) printf("%c", read_buffer[i]);

    bytes_written = write(fd, start_bootloader_buf, sizeof(start_bootloader_buf));

    while (stringCompaire(read_buffer, "Run Bootloader", 14, bytes_read) != 0) {
        bytes_read = read(fd, &read_buffer, 20); /* Read the data */
    }

    for(i = 0; i < bytes_read; i++) printf("%c", read_buffer[i]);

    while (stringCompaire(read_buffer, "Programming", 11, bytes_read) != 0) {
        bytes_read = read(fd, &read_buffer, 20); /* Read the data */
        for(i = 0; i < bytes_read; i++) printf("%c", read_buffer[i]);
    }

    while (END_OF_FILE != 1) {
        hexReadLine(hexFile);
        while (stringCompaire(read_buffer, ">", 1, bytes_read) != 0) {
            bytes_read = read(fd, &read_buffer, 1); /* Read the data */
            if (stringCompaire(read_buffer, "!", 1, bytes_read) == 0) {
                error = 1;
                break;
            }
        }
        if (error == 1) {
            printf("ERROR: \r\n");
            break;
        }

        usleep(10000);
    }

    printf("Done programming! Restart the system ... \r\n");
    ioctl(fd, TIOCMBIS, &DTR_flag);//Set RTS pin
    usleep(10000);
    ioctl(fd, TIOCMBIC, &DTR_flag);//Clear RTS pin

    close(fd);
    fclose(hexFile);

    return 0;
}
