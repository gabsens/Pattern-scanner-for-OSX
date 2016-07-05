//Not finished yet
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mach/mach_traps.h>
#include <mach/mach_init.h>
#include <mach/mach_error.h>
#include <mach/mach.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <libproc.h>
#include <sys/stat.h>
#include <string>
#include <sstream>

using namespace std;

mach_port_t task;
uint64_t Client, SizeClient;
uint8_t * buffer;
////
// define global patterns
////

string LocalPlayer = "4839DF74??4885FF74??4889DEE8????????EB??";
string EntityList = "554889E54156534889FBBE??000000E8????????8B??????????83F8FF";
string Glow = "48C7????E?2E05????????488D??????????488D??????????E8";
////
// copy client.dylib to our program memory
////
int GetCSpid() {
    int csgopid ;
    int numberOfProcesses = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    pid_t pids[numberOfProcesses];
    bzero(pids, sizeof(pids));
    proc_listpids(PROC_ALL_PIDS, 0, pids, sizeof(pids));
    for (int i = 0; i < numberOfProcesses; ++i) {
        if (pids[i] == 0) { continue; }
        char pathBuffer[PROC_PIDPATHINFO_MAXSIZE];
        bzero(pathBuffer, PROC_PIDPATHINFO_MAXSIZE);
        proc_pidpath(pids[i], pathBuffer, sizeof(pathBuffer));
        char nameBuffer[256];
        int position = strlen(pathBuffer);
        while(position >= 0 && pathBuffer[position] != '/')
        {
            position--;
        }
        strcpy(nameBuffer, pathBuffer + position + 1);
        if (strcmp(nameBuffer, "csgo_osx64")==0) {
            return pids[i];
        }
    }
    
    return 0;
    
}



void ClientToBuffer() {
    kern_return_t kret;
    mach_vm_address_t address;
    int csgopid = GetCSpid();
    kret = task_for_pid(mach_task_self(), csgopid, &task);
    if (kret!=KERN_SUCCESS)
    {
        printf("task_for_pid() failed with message %s!\n",mach_error_string(kret));
        exit(0);
    }
    struct task_dyld_info dyld_info;
    mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;
    if (task_info(task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count) == KERN_SUCCESS)
    {
        address = dyld_info.all_image_info_addr;
    }
    mach_msg_type_number_t size = sizeof(struct dyld_all_image_infos);
    vm_offset_t readMem;
    vm_read(task,address,size,&readMem,&size);
    struct dyld_all_image_infos* infos = (struct dyld_all_image_infos *) readMem;
    size = sizeof(struct dyld_image_info) * infos->infoArrayCount;
    vm_read(task,(mach_vm_address_t) infos->infoArray,size,&readMem,&size);
    struct dyld_image_info* info = (struct dyld_image_info*) readMem;
    mach_msg_type_number_t sizeMax=512;
    for (int i=0; i < infos->infoArrayCount; i++) {
        vm_read(task,(mach_vm_address_t) info[i].imageFilePath,sizeMax,&readMem,&sizeMax);
        char *path = (char *) readMem ;
        if(strstr(path, "/client.dylib") != NULL){
            Client = (mach_vm_address_t)info[i].imageLoadAddress ;
            struct stat st;
            stat(path, &st);
            printf("client: 0x%llx %lld\n", Client, st.st_size);
            uint8_t * m_data;
            m_data = new uint8_t[st.st_size];
            sizeMax = st.st_size;
            vm_read(task,(vm_address_t)Client,sizeMax,&readMem,&sizeMax);
            uint64_t address = (uint64_t)readMem;
            buffer = (uint8_t *)address;
            SizeClient = st.st_size;

        }
    }
}

uint64_t Scan(string s){
    stringstream ss;
    bool flag = true;
    for (int j=0;j<(SizeClient-(s.size())/2 + 1);j++){
        flag = true;
        for (int i=0; i<(s.size())/2; i++){
            string bytechar = s.substr(2*i,2);
            if (bytechar.compare("??") != 0){
                uint8_t byte = std::stoi(bytechar, NULL,16);
                if (byte != buffer[j+i]){
                    flag = false;
                    break;
                }
            }
        }
        if (flag){
            return (Client+j);
        }
    }
    return 0;
}






int main(int argc, const char * argv[]) {
    // insert code here...
    ClientToBuffer();
    printf("%x\n",buffer[0]);
    printf("%llx\n",Scan(LocalPlayer));
    printf("%llx\n",Scan(EntityList));
    printf("%llx\n",Scan(Glow));
    uint8_t test = 0x1e ;
    string conv = std::to_string(test);
    cout << conv << endl;
    return 0;
}
