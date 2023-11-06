// Created by Ӧ��� on 2023/10/24.
/*HUST-CSE 2023����ʵ��
 *����UDPʵ��һ��TFTP�ͻ��ˣ�ʵ��������������ļ����ϴ������ع���
 *ʹ��clumsy���ߣ�����������¡�
�ӳ�(Lag)�������ݰ�����һ��ʱ����ٷ����������ܹ�ģ�������ӳٵ�״����
����(Drop)���������һЩ���ݡ�
����(Throttle)����һС��ʱ���ڵ�������������������֮���ͬһʱ��һͬ����ȥ��
�ط�(Duplicate)���������һЩ���ݲ����䱾��һͬ���͡�
����(Out of order)���������ݰ����͵�˳��
�۸�(Tamper)������޸�С���ֵİ������ݡ�
 */
//

#include <winsock2.h>
#include "iostream"
#include "stdio.h"
#include <cstdlib>
#include "time.h"
#include <string.h>
#include <stddef.h>
#include <chrono>

SOCKET getUdpSocket(){
    /*��ʼ��socket��*/
    WSADATA wsadata;
    int err = WSAStartup(0x1010, &wsadata);
    if(err != 0){
        return -1;
    }

    /*����socket*/
    SOCKET udpsocket = socket(AF_INET, SOCK_DGRAM, 0);     /*������Э����ipv4�����ݱ���ʽ*/
    if(udpsocket == INVALID_SOCKET){
        printf("set up socket failed\n");
        WSACleanup();
        return -1;
    }
    return udpsocket;
}

/*����sockaddr_in���ݽṹ*/
sockaddr_in getaddr(char* ip, int port){
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);        /*���˿ںŵ�ַת���������ֽ�˳��*/
    addr.sin_addr.S_un.S_addr = inet_addr(ip);      /*���ʮ����IP��ַ�ַ���ת��Ϊ32λ������ʾ��IPv4��ַ*/
    return addr;
}

/*�����ϴ��������ݰ�*/
char* RequestUploadPack(char* name, int& datalen, int type){
    int name_len = strlen(name);
    /*buffer��WRQ*/
    char* buffer = new char [2+name_len+type+2];
    buffer[0] = 0x0;
    buffer[1] = 0x02;       /*�ϴ��ļ���д����*/
    memcpy(buffer+2, name, name_len);
    memcpy(buffer+2+name_len, "\0", 1);
    if(type == 8)
        memcpy(buffer+2+name_len+1, "netascii", 8);
    else
        memcpy(buffer+2+name_len+1, "octet", 5);
    memcpy(buffer+2+name_len+1+type, "\0", 1);
    datalen = 2+name_len+1+1+type;       /*д��datalen����*/
    return buffer;
}

/*д�뵱ǰʱ��*/
void PrintTime(FILE* fp){
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

    // ��ʱ���ת��Ϊʱ�����������
    std::time_t timestamp = std::chrono::system_clock::to_time_t(now);

    // ʹ�ñ�׼���ctime������ʱ���ת��Ϊ�ɶ������ں�ʱ���ַ���
    char timeString[100];
    std::strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", std::localtime(&timestamp));

    // ��ӡ��ǰʱ���ַ���
    fprintf(fp, "current time:%s\n", timeString);
    return ;
}

char* MakeData(FILE* fp, short& block_number, int &datalen){
    char tmp[512];
    int sum = fread(tmp, 1, 512, fp);      /*���귵��sumΪ512*/
    if(!ferror(fp)){
        char* buf = new char[4+sum];
        buf[0] = 0x0;
        buf[1] = 0x3;
        block_number = htons(block_number);
        memcpy(buf+2, (short*)&block_number, 2);
        block_number = ntohs(block_number);
        memcpy(buf+4, tmp, sum);
        datalen = sum+4;
        return buf;
    }
    return NULL;
}

char* RequestDownloadPack(char* name, int& datalen, int type){
    int name_len = strlen(name);
    char* buff = new char[name_len+2+2+type];
    buff[0] = 0x00;
    buff[1] = 0x01;     /*RRQ����op��1*/
    memcpy(buff+2, name, name_len);
    memcpy(buff+2+name_len, "\0", 1);
    if(type == 8)
        memcpy(buff+name_len+3, "netascii", type);
    else
        memcpy(buff+name_len+3, "octet", type);
    memcpy(buff+2+name_len+1+type, "\0", 1);
    datalen = 2+name_len+1+type+1;
    return buff;
}

char* AckPack(short& number){
    char* buff = new char[4];
    buff[0] = 0x00;
    buff[1] = 0x04;
    number = htons(number);
    memcpy(buff+2, &number, 2);
    number = ntohs(number);
    return buff;
}


int main(){
    FILE* fp = fopen("ClientLog.txt", "a");
    SOCKET sock = getUdpSocket();
    sockaddr_in addr;     /*���ip�Ͷ˿ڵĽṹ��*/
    char resend_buffer[2048];       /*�ط�������*/
    int sendTimes;
    clock_t start, end;     /*��¼ʱ��*/
    int buflen;             /*����������*/
    int Timekill, recvTimes;       /*��¼recv_from�����ط�����*/
    /*��ʱ����*/
    int recvtimeout = 1000;     /*�ֱ�Ϊ���ա������ļ��ĳ�ʱ�޶�*/
    int sendtimeout = 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&recvtimeout, sizeof(int));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*)&sendtimeout, sizeof(int));

    while(1){
        /*�ͻ��˽�Ҫ�ϴ��ļ�*/
        printf("====================Loading========================\n");
        printf("=====            ������TFTP�ͻ���               ===\n");
        printf("=====Function:                                  ===\n");
        printf("=====1.�ϴ��ļ�    2.�����ļ�     3.�˳�        ===\n");
        printf("===================================================\n"); 
        int choice;
        scanf("%d", &choice);
        if(choice == 1){
            /*��Ҫ������ļ������ļ����ݽ���Ԥ����*/
            char target[64] = "127.0.0.1";
            addr = getaddr(target, 69);        /*client���͵�ip�Ͷ˿ںŷ���addr��*/
            printf("����Ҫ�ϴ����ļ�����\n");
            char name[20];
            scanf("%s", &name);
            int type;
            printf("ѡ���ϴ��ķ�ʽ��1��netascii 2��octet\n");
            scanf("%d", &type);
            if(type == 1)
                type = 8;
            else if(type == 2)
                type = 5;
            int datalen;

            /*������WRQ����*/
            char* sendData = RequestUploadPack(name, datalen, type);
            memcpy(resend_buffer, sendData, datalen);

            /*��ʼ�������ݰ�*/
            int ans = sendto(sock, sendData, datalen, 0, (sockaddr*)&addr, sizeof(addr));
            buflen = datalen;
            start = clock();        /*��ʱ��ʼ*/
            sendTimes = 1;  /*���ʹ���*/

            while(ans != datalen){
                std::cout << "send WRQ failed, file name: " << name << std::endl;
                if(sendTimes <= 10){
                    ans = sendto(sock, sendData, datalen, 0, (sockaddr*)&addr, sizeof(addr));
                    sendTimes++;
                }
                else
                    break;
            }
            if(sendTimes > 10){
                fprintf(fp, "Upload file failed. Error: sendto function timeout.\n");
                continue;   /*�˴δ���ʧ��*/
            }

            PrintTime(fp);         /*����־��д�뵱ǰʱ��*/
            fprintf(fp, "send WRQ to file : %s\n�ϴ���ʽ��netascii\n", name);
			printf("send WRQ successfully.\n");

            delete[] sendData;
            FILE* uploadFile = fopen(name, "rb");

            if(uploadFile == NULL){
                std::cout << "file" << name << "opne failed or file doesn't exit." << std::endl;
                continue;
            }

            short block = 0;
            datalen = 0;
            int RST = 0;        /*��¼�ط�*/
            int FullSize = 0;
            while(1){
                /*server�˿����µ�socket*/
                char recv_buf[1024];
                sockaddr_in server;     /*�������¿��˿ڹ��������Ĵ���*/
                int len = sizeof(server);
                /*�����ans���ܵľ���ACK����*/
                ans = recvfrom(sock, recv_buf, 1024, 0, (sockaddr*)&server, &len);
                int recv_len = strlen(recv_buf);        //�������ֶεĳ��ȴ�С
                recvTimes = 1;

                /*�������·���ֱ�����յ���ȷ��ACK����*/
                while(ans < 0){
                    printf("%d", recvTimes);
                    if(recvTimes > 10){        /*����10��δ���ܵ�ACK������*/
                        printf("no receive datagram get. transmission failed.\n");
                        PrintTime(fp);
                        fprintf(fp, "Upload file %s failed\n", name);
                        break;
                    }

                    int ans = sendto(sock, resend_buffer, buflen, 0, (sockaddr*)&addr, len);     /*�ط�*/
                    RST++;
                    std::cout << "resend last block" << std::endl;

                    sendTimes = 1;       /*����sendto���*/
                    while(ans != buflen){
                        std::cout << "resend last block failed :" << sendTimes << "times" << std::endl;
                        if(sendTimes <= 10){
                            ans = sendto(sock, resend_buffer, buflen, 0, (sockaddr*)&addr, len);
                            sendTimes++;
                        }
                        else
                            break;
                    }

                    ans = recvfrom(sock, recv_buf, 1024, 0, (sockaddr*)&server, &len);
                    if(ans > 0 )
                        break;
                    recvTimes++;       /*�ط�����++*/
                }

                if(ans > 0){
                    short operation;
                    memcpy(&operation, recv_buf, 2);
                    operation = ntohs(operation);
                    if(operation == 4){      /*ȷ��ack���ݰ�*/
                        short blockNumber;
                        memcpy(&blockNumber, recv_buf+2, 2);
                        blockNumber = ntohs(blockNumber);

                        if(blockNumber == block){        /*�յ���ȷ��blockӦ��*/
                            addr = server;
                            if(feof(uploadFile) && datalen != 512){
                                std::cout << "Congratulations!\nUpload finished." << std::endl;
                                end = clock();
                                double runningtime = static_cast<double>(end-start);
                                PrintTime(fp);                   
                                printf("Send block number:%d File size:%d bytes Resend times:%d Average transmission rate: %.2lf kb/s\n", blockNumber, FullSize, RST, FullSize/runningtime);
                                fprintf(fp, "Upload file %s finished, sent time:%.2lfms, size:%d Average transmission rate: %.2lf kb/s\n", name, runningtime, FullSize, FullSize/runningtime);
                                break;
                            }

                            block++;        /*������һ���ϴ���data��*/
                            sendData = MakeData(uploadFile, block, datalen);
                            if(sendData == NULL){
                                std::cout << "file read mistake." << std::endl;
                                fprintf(fp, "file %s read mistake.", name);
                                break;
                            }
                            buflen = datalen;
                            FullSize += datalen-4;      /*��¼�������ݵ��ܴ�С*/
                            memcpy(resend_buffer, sendData, datalen);       /*���ε�block�Ž��ط���*/

                            int ans = sendto(sock, sendData, datalen, 0, (sockaddr*)&addr, sizeof(addr));
                            sendTimes = 1;
                            while(ans != datalen){
                                std::cout << "send block " << block << "failed" << std::endl;
                                if(sendTimes <= 10){
                                    ans = sendto(sock, sendData, datalen, 0, (sockaddr*)&addr, sizeof(addr));
                                    sendTimes++;
                                }
                                else
                                    break;
                            }
                            if(sendTimes > 10){
                                fprintf(fp, "Upload block %d failed.\n", blockNumber); 
                           }
                            std::cout << "pack = " << block << "sent successfully!" << std::endl;
                        }
                    }else if(operation == 5){                        
                      char errorcode[3], errorContent[512];
                      memcpy(errorcode, recv_buf+2, 2);
                      errorcode[2] = '\0';
                      for(int i = 0; *(recv_buf+4+i); i++)
                        memcpy(errorContent+i, recv_buf+4+i, 1);
                    printf("errorcode %s: %s\n", errorcode, errorContent);
                    fprintf(fp, "Upload block failed.\n");
                    }
                }
        
            }
            
        }
        if(choice == 2){        /*�û���������*/
            char target[64] = "127.0.0.1";
            addr = getaddr(target, 69);
            printf("������Ҫ���ص��ļ����ƣ�\n");
            char name[100];
            int type;
            scanf("%s", &name);
            printf("��ѡ��Ҫ���صķ�ʽ��1��netascii 2��octet\n");
            scanf("%d", &type);
            if(type == 1)
                type = 8;
            else if(type == 2)
                type = 5;

            int datalen = 0;
            char* senddata = RequestDownloadPack(name, datalen, type);      /*����RRQ��*/
            buflen = datalen;
            memcpy(resend_buffer, senddata, datalen);
            recvTimes = 1;     /*�����recv���õļ���*/

            int ans = sendto(sock, senddata, datalen, 0, (sockaddr*)&addr, sizeof(addr));
            start = clock();
            PrintTime(fp);
            fprintf(fp, "send RRQ to file %s\n", name);
            printf("send RRQ successfully.\n");
            sendTimes = 1;

            while(ans != datalen){
                std::cout << "send RRQ failed time:" << sendTimes <<std::endl;
                if(sendTimes <= 10){
                    ans = sendto(sock, senddata, datalen, 0, (sockaddr*)&addr, sizeof(addr));/*�ط�RRQ*/
                    sendTimes++;
                }
                else
                    break;
            }
            if(sendTimes > 10)
                continue;
		
            /*����ɹ�������ϴεķ��ͻ�����*/
            delete []senddata;

            FILE* downloadFile = fopen(name, "wb");
            if(downloadFile == NULL){
                std::cout << "Download file" << name << "open failed" << std::endl;
                continue;
            }
            int want_recv = 1;
            int RST = 0;
            int Fullsize = 0;
            while(1){
                /*���Ͻ���data����*/
                char buf[1024];
                sockaddr_in server;
                int len = sizeof(server);
                /*�����ans���ܵľ���data����*/
                ans = recvfrom(sock, buf, 1024, 0, (sockaddr*)&server, &len);
				
                if(ans < 0){
                	int backInfo;
                    printf("Lost-resend times: %d\n", recvTimes);
                    if(recvTimes > 10){        /*����10��δ���ܵ�ACK������*/
                        printf("File %s download failed\n", name);
                        PrintTime(fp);
                        fprintf(fp, "Download file %s failed\n", name);
                        break;
                    }
                    backInfo = sendto(sock, resend_buffer, buflen, 0, (sockaddr*)&addr, len);     /*�ط�*/
                    RST++;
                    std::cout << "Data block lost, resend last ACK." << std::endl;

                    sendTimes = 1;       /*����send���*/
                    while(backInfo != buflen){
                        std::cout << "resend last ACK failed :" << sendTimes << "times" << std::endl;
                        if(sendTimes <= 10){
                            backInfo = sendto(sock, resend_buffer, buflen, 0, (sockaddr*)&addr, len);
                            sendTimes++;
                        }
                        else
                            break;
                    }
                    if(sendTimes > 10)
                        break;
                    recvTimes++;       /*�ط�����++*/
                }
                else if(ans > 0){
                    short flag;
                    memcpy(&flag, buf, 2);
                    flag = ntohs(flag);
                    if(flag == 3){      /*�������˷���data��*/
                        /*�ȴ�����ACK��*/
                        addr = server;
                        short number = 0;
                        memcpy(&number, buf+2, 2);
                        number = ntohs(number);
                        std::cout << "Package number: " << number << std::endl;
						
                        char* ack = AckPack(number);
                        int sendAck = sendto(sock, ack, 4, 0, (sockaddr*)&addr, sizeof(addr));
                        sendTimes = 1;
                        while(sendAck != 4){   /*����ʧ��*/
                            std::cout << "sent last ACK failed" << sendTimes << std::endl;
                            if(sendTimes <= 10){
                                sendAck = sendto(sock, ack, 4, 0, (sockaddr*)&addr, sizeof(addr));
                                sendTimes++;
                            }
                            else
                                break;
                        }
                        if(sendTimes > 10)
                            break; 
						std::cout << "Send package " << number << " ACK successfully." << std::endl;
						
                        /*������ȷ��data��*/
                        if(want_recv == number){
                            buflen = 4;
                            recvTimes = 1;
                            memcpy(resend_buffer, &ack, 4);      /*��ʱ����ʱ��Ҫ�����ش���ǰ�յ�����һ������ack*/
                            fwrite(buf+4, ans-4, 1, downloadFile);        /*���յ�������д��ȥ*/
                            Fullsize += ans-4;

                            if(ans-4 >=0 && ans-4 < 512){       /*�������*/
                                std::cout << "Congratulations!\nDownload finished." << std::endl;
                                end = clock();
                                double runningtime =static_cast<double>(end-start);
                                PrintTime(fp);
                                printf("transmission rate is: %.2lf kb/s\n", Fullsize/runningtime);
                                printf("File %s download finished. \nHere are details:\nsent blocks:%d running time:%.2lf resend times: %d. Fullsize: %d\n", name, number,runningtime, RST, Fullsize);
                                goto finish;
                            }
                            want_recv++;
                        }
                    }

                   	else if(flag == 5){      /*�ظ�����error��*/
                        short errorcode;
                        memcpy(&errorcode, buf+2, 2);
                        errorcode = ntohs(errorcode);
                        char str_error[1024];
                        int i =0;
                        for(i = 0; *(buf+4+i); i++)
                            memcpy(str_error+i, buf+4+i, 1);
                        str_error[i] = '\0';
                        std::cout << "error package:" << str_error << std::endl;
                        PrintTime(fp);
                        fprintf(fp, "error code: %d, error content:%s\n", errorcode, str_error);
                        break;
                    }
                }
            }
        finish:
            fclose(downloadFile);
        }
        if(choice == 0)
            break;
    }
    fclose(fp);
    int err = closesocket(sock);
    if(err){
        printf("socket close failed.\n");
        fprintf(fp, "socket �ر�ʧ��\n");
    }
    return 0;
}