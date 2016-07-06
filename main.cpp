/*
	This file is part of the Pattern scanner for OSX
    Copyright (C) 2016 Gabriel Romon <mariemromon@yahoo.fr> 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License version 3
    as published by the Free Software Foundation. You may not use, modify
    or distribute this program under any other version of the
    GNU Affero General Public License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  
*/


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
string Glow = "48C7????e?2E05????????488D??????????488D??????????E8";
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
    bool flag = true;
    for (int j=0;j<(SizeClient-(s.size())/2 + 1);j++){
        flag = true;
        for (int i=0; i<(s.size())/2; i++){
            string bytechar = s.substr(2*i,2);
            if (bytechar.compare("??") != 0){
                if (bytechar.substr(1,1).compare("?") == 0){
                    char byte[2];
                    int out;
                    out = sprintf(byte, "%x",buffer[i+j]);
                    string bytebuffer (byte);
                    if (bytebuffer.length()==1){
                        bytebuffer = "0"+bytebuffer;
                    }
                    if (bytechar.substr(0,1).compare(bytebuffer.substr(0,1))!=0){
                        flag=false;
                        break;
                    }
                    continue;
                }
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
    ClientToBuffer();
    uint64_t LocalPlayerArr=Scan(LocalPlayer);
    uint64_t EntityArr=Scan(EntityList);
    uint64_t GlowArr=Scan(Glow);
    uint32_t int1 = (int)*((int*)(LocalPlayerArr-Client+buffer + 0x17));
    LocalPlayerArr = LocalPlayerArr + 0x1F + int1 - Client;
    printf("%llx\n",LocalPlayerArr);
    uint32_t int2 = (int)*((int*)(EntityArr-Client+buffer + 0x22));
    uint64_t int3 = (uint64_t) *(uint64_t *)(EntityArr-Client+buffer + 0x26 + int2);
    EntityArr = int3 + 0x8 + 0x20 -Client;
    printf("%llx\n",EntityArr);
    uint32_t int4 = (int)*((int*)(GlowArr-Client+buffer + 0x2D));
    GlowArr = GlowArr + 0x31 + int4 - Client;
    printf("%llx\n",GlowArr);
    return 0;
}
