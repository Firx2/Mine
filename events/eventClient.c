#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "eventClient.h"
#include "eventServer.h"
#include "../enet/include/enet.h"
#include "../httpService.h"
#include "../utils/utils.h"
#include "../mainVar.h"
#include "../packet/packet.h"
#include "../proxyStruct.h"

void clientConnect() {
    if (isSendToServer) {
        printf("[CLIENT -> SERVER] Client connected into proxy\n[CLIENT -> SERVER] Connecting to subserver...\n");
        enet_host_destroy(realServer);

        realServer = enet_host_create(NULL, 1, 2, 0, 0);
        realServer->checksum = enet_crc32;
        realServer->usingNewPacket = userConfig.usingNewPacket;
        enet_host_compress_with_range_coder(realServer);
        enet_address_set_host(&realAddress, OnSendToServer.serverAddress);
        realAddress.port = OnSendToServer.port;
        realPeer = enet_host_connect(realServer, &realAddress, 2, 0);

        free(OnSendToServer.serverAddress);
        free(OnSendToServer.UUIDToken);

        isSendToServer = 0;
    } else {
        printf("[CLIENT -> SERVER] Client connected into proxy\n[CLIENT -> SERVER] Connecting to Growtopia Server...\n");

        memset(&realAddress, 0, sizeof(ENetAddress));
        if (userConfig.usingServerData) {
            info = HTTPSClient(userConfig.serverDataIP);

            char** arr = strsplit(info.buffer + (findStr(info.buffer, "server|") - 7), "\n", 0);

            enet_address_set_host(&realAddress, arr[findArray(arr, "server|")] + 7);
            realAddress.port = atoi(arr[findArray(arr, "port|")] + 5);
            realPeer = enet_host_connect(realServer, &realAddress, 2, 0);
            if (currentInfo.isMetaMalloc) free(currentInfo.meta);
            asprintf(&currentInfo.meta, "%s", arr[findArray(arr, "meta|")] + 5);
            currentInfo.isMetaMalloc = 1;

            free(arr);
        }
        else {
            printf("[CLIENT -> SERVER] Client connected into proxy\n[CLIENT -> SERVER] Connecting to Custom Growtopia Server...\n");
            enet_address_set_host(&realAddress, userConfig.manualIP);
            realAddress.port = userConfig.manualPort;
            realPeer = enet_host_connect(realServer, &realAddress, 2, 0);
        }

    }
}

void clientReceive(ENetEvent event, ENetPeer* clientPeer, ENetPeer* serverPeer) {
    switch(GetMessageTypeFromPacket(event.packet)) {
        case 2: {
            char* packetText = GetTextPointerFromPacket(event.packet);

            if (!currentInfo.isLogin) {
                char** loginInfo = strsplit(packetText, "\n", 0);
                if (userConfig.usingServerData) loginInfo[findArray(loginInfo, "meta|")] = CatchMessage("meta|%s", currentInfo.meta);
                else loginInfo[findArray(loginInfo, "meta|")] = CatchMessage("meta|%s", userConfig.manualMeta);

                if (userConfig.isSpoofed) {
                    char* klvGen;

                    loginInfo[findArray(loginInfo, "wk|")] = CatchMessage("wk|%s", currentInfo.wk);
                    loginInfo[findArray(loginInfo, "rid|")] = CatchMessage("rid|%s", currentInfo.rid);
                    loginInfo[findArray(loginInfo, "mac|")] = CatchMessage("mac|%s", currentInfo.mac);
                    loginInfo[findArray(loginInfo, "hash|")] = CatchMessage("hash|%d", protonHash(CatchMessage("%sRT", currentInfo.mac)));
                    loginInfo[findArray(loginInfo, "hash2|")] = CatchMessage("hash2|%d", protonHash(CatchMessage("%sRT", currentInfo.deviceID)));

                    if (findArray(loginInfo, "gid|") == -1) klvGen = generateKlv(loginInfo[findArray(loginInfo, "game_version|")] + 13, loginInfo[findArray(loginInfo, "hash|")] + 5, currentInfo.rid, loginInfo[findArray(loginInfo, "protocol|")] + 9, 0);
                    else klvGen = generateKlv(loginInfo[findArray(loginInfo, "game_version|")] + 13, loginInfo[findArray(loginInfo, "hash|")] + 5, currentInfo.rid, loginInfo[findArray(loginInfo, "protocol|")] + 9, 1);

                    loginInfo[findArray(loginInfo, "klv|")] = CatchMessage("klv|%s", klvGen);

                    free(klvGen);
                }

                char* resultSpoofed = arrayJoin(loginInfo, "\n", 1);
                printf("[CLIENT -> SERVER] Spoofed Login info: %s\n", resultSpoofed);
                sendPacket(2, resultSpoofed, serverPeer);

                free(resultSpoofed);
                currentInfo.isLogin = 1;

                break;
            }

            printf("[CLIENT -> SERVER] Packet 2: received packet text: %s\n", packetText);

            if ((packetText + 19)[0] == '/') {
                // command here
                char** command = strsplit(packetText + 19, " ", 0);
                if (isStr(command[0], "/proxy", 1)) {
                    sendPacket(3, "action|log\nmsg|>> Commands: /helloworld /testarg <your arg> /testdialog /warp <name world> /netid /changename <Name> /fastroulette", clientPeer);
                }
                else if (isStr(command[0], "/helloworld", 1)) {
                    sendPacket(3, "action|log\nmsg|`2Hello World", clientPeer);
                }
                else if (isStr(command[0], "/netid", 1)) {
                    enet_peerSend(onPacketCreate(0, 0, "ss", "OnConsoleMessage", CatchMessage("My netID is %d", OnSpawn.LocalNetid)), clientPeer);
                }
                else if (isStr(command[0], "/testarg", 1)) {
                    if (!command[1]) {
                        sendPacket(3, "action|log\nmsg|Please input argument", clientPeer);
                        free(command); // prevent memleak
                        break;
                    }
                    sendPacket(3, CatchMessage("action|log\nmsg|%s", command[1]), clientPeer);
                }
                else if (isStr(command[0], "/testdialog", 1)) {
                    enet_peerSend(onPacketCreate(0, 0, "ss", "OnDialogRequest","set_default_color|`o\nadd_label_with_icon|big|`wTest Dialog!``|left|758|\nadd_textbox|Is It Working?|left|\nadd_text_input|yesno||yes|5|\nembed_data|testembed|4\nadd_textbox|`4Warning:``Dont Forget To Star Repo!|left|\nend_dialog|test_dialog|Cancel|OK|"), clientPeer);
                }
                else if (isStr(command[0], "/changename", 1)) {
                    if (!command[1]) {
                        sendPacket(3, "action|log\nmsg|Please input the name", clientPeer);
                        free(command); // prevent memleak
                        break;
                    }
                    enet_peerSend(onPacketCreate(OnSpawn.LocalNetid, 0, "ss", "OnNameChanged", command[1]), clientPeer);
                }
                else if (isStr(command[0], "/warp", 1)) {
                    if (!command[1]) {
                        sendPacket(3, "action|log\nmsg|Please input world name", clientPeer);
                        free(command); // prevent memleak
                        break;
                    }
                    sendPacket(3, CatchMessage("action|join_request\nname|%s\ninvitedWorld|0", command[1]), serverPeer);
                }
                else if (isStr(command[0], "/fastroulette", 1)) {
                    if (userOpt.isFastRoulette) {
                        userOpt.isFastRoulette = 0;
                        sendPacket(3, "action|log\nmsg|`wFast roulette is `4turning off`w, type /fastroulette to `2turning on", clientPeer);
                    }
                    else {
                        userOpt.isFastRoulette = 1;
                        sendPacket(3, "action|log\nmsg|`wFast roulette is `2turning on`w, type /fastroulette to `4turning off", clientPeer);
                    }
                }
                    // add new commands here
                else if (isStr(command[0], "/rw", 1)) {
                    srand(time(NULL));
                    if (!command[1]) {
                        sendPacket(3, "action|log\nmsg|Please input world name", clientPeer);
                        free(command); // prevent memleak
                        break;
                    }
                    for (int i = 0; i < command[1]; ++i) {
        int randomChar = rand() % 36; // 26 letters + 10 numbers

        if (randomChar < 26) {
            // Print a random letter
            printf("%c", 'A' + randomChar);
        } else {
            // Print a random number
            printf("%c", '0' + (randomChar - 26));
        }
    }
                    sendPacket(3, CatchMessage("action|join_request\nname|%s\ninvitedWorld|0", randomChar), serverPeer);
}
            enet_peerSend(event.packet, serverPeer);
            break;
        }
        case 4: {
            switch(event.packet->data[4]) {
                case 26: {
                    enet_peerSend(event.packet, serverPeer);
                    enet_peer_disconnect_now(clientPeer, 0);
                    enet_peer_disconnect_now(serverPeer, 0);
                    break;
                }
                default: {
                    printf("[CLIENT -> SERVER] TankUpdatePacket: Unknown packet tank type: %d\n", event.packet->data[4]);
                    enet_peerSend(event.packet, serverPeer);
                    break;
                }
            }
            break;
        }
        default: {
            printf("[CLIENT -> SERVER] Unknown message type: %d\n", GetMessageTypeFromPacket(event.packet));
            enet_peerSend(event.packet, serverPeer);
            break;
        }
    }
}

void clientDisconnect() {
    printf("[CLIENT -> SERVER] Client just disconnected from Proxy\n");
    isLoop = 0;
    doLoop = 1;
}
