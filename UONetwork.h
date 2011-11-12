#ifndef UONETWORK_INCLUDED
#define UONETWORK_INCLUDED

/*
	UOAI - Network related stuff

	-- Wim Decelle
*/

#pragma pack(push, 1)

typedef struct NetworkObjectClassStruct NetworkObjectClass;

//IMPORTANT: _stdcall SHOULD BE _thiscall !!!
// <- all prototypes of the functions in the virtual function tables are thus wrong (incomplete)!!!!
// <- this means you CAN NOT call any of those functions directly
// <- what's wrong is that these are C++ class member functions
// <- they use the _thiscall calling convention, which is not the _stdcall calling convention specified here
// <- BUT note that _thiscall and _stdcall differ only in one way : _thiscall needs a this-pointer in the ecx-registry
// <- so if you can ensure (using inline assembly?) that ecx=pNetworkObject when calling any of this functions, everything should work fine
typedef struct NetworkObjectClassVtblStruct
{
	void (_stdcall * destructor_0)();//<- ignore this entry
	NetworkObjectClass * (_stdcall * destructor_1)(int bDestroy);//<- ignore this entry
	void (_stdcall * disconnect_and_destroy_1)();//<- ignore this entry

	void (_stdcall * onConnectionLoss)();//<- we hook this to get disconnection notifications
	
	void (_stdcall * disconnect_and_destroy_2)();//<- ignore this entry
	
	void (_stdcall * ReceiveAndHandlePackets)();//<- calls recv on the socket; decompresses and decrypts (if needed) the packet; then calls the packethandler for each completely received packet
	
	int (_stdcall * SendBufferedPackets)();//<- packets are written to an internal buffer and send later on, this function sends all buffered packets
	int (_stdcall * HasBufferedPackets)();//<- checks if there's anything to send buffered
	
	void (_stdcall * HandlePacket)(unsigned char * packet_to_handle);//<- packethandler, called for every received packet and can be called with your packetbuffer to propage changes to the client as if it had received the packet you passed from the server (sendpacket2client functionality)
} NetworkObjectClassVtbl;

typedef struct NetworkObjectClassStruct
{
	NetworkObjectClassVtbl * lpVtbl;//network object's virtual function table pointer
	int socket;//winsock socket used by the client
} NetworkObjectClass;//not all info is here (the structure is larger containing send/recv buffers, a bConnected boolean, buffer-usage-variables, ... but none of those are essential

typedef struct PacketInfoStruct
{
	unsigned int size;//packet size or 0x8000 when dynamically sized
	unsigned int number;//packet number or CMD
	char * description;//always set to a pointer to an empty string ("\0") in the client, but the pointers are clearly treated as pointers to strings though, so probably these description-strings are only available in some 'debugging version' of the client and not the deployment version.
} PacketInfo;

#pragma pack(pop)


#endif