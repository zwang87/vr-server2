/*
BlackBoxServer 4.0
Includes some code from OptiTrack.
*/

#include "stdafx.h"

using namespace std;

int frameModificationVersion = 0;

bool is_b1_active = false;
bool is_b2_active = false;

#define VRSERVER_PORT 1611
class MulticastStream {
	SOCKET s;
	struct sockaddr_in addr;
	struct sockaddr_in bind_addr;
	public:
	MulticastStream() {
		WSADATA wd;
		WSAStartup(0x02, &wd);
		int err;
		if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
			printf("[VR] Could not create socket : %d", WSAGetLastError());
		}

		int opt_val = 1;
		err = setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&opt_val, sizeof(opt_val));
		if (err == SOCKET_ERROR) {
			exit(0);
		}

		opt_val = 1;
		err = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt_val, sizeof(opt_val));
		if (err == SOCKET_ERROR) {
			exit(0);
		}

		// Bind to correct NIC
		bind_addr.sin_family = AF_INET;
		// bind_addr.sin_addr.s_addr = inet_addr(INADDR_ANY);
		err = inet_pton(AF_INET, "192.168.1.4", &bind_addr.sin_addr); // S_ADDR of our IP for the WiFi interface
		if (err == SOCKET_ERROR) {
			exit(0);
		}
		bind_addr.sin_port = 0;
		err = bind(s, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
		if (err == SOCKET_ERROR) {
			exit(0);
		}

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(VRSERVER_PORT);
		// Proper mutlicast group
		err = inet_pton(AF_INET, "224.1.1.1", &addr.sin_addr);
		if (err == SOCKET_ERROR) {
			exit(0);
		}
	}
	void send(char* packet, int length) {
		sendto(s, packet, length, 0, (struct sockaddr*) &addr, sizeof(addr));
	}
	~MulticastStream() {
		closesocket(s);
		WSACleanup();
	}

};

char packet[65507];

#pragma warning( disable : 4996 )

void __cdecl DataHandler(sFrameOfMocapData* data, void* pUserData);
// For NatNet error mesages
void __cdecl MessageHandler(int msgType, char* msg);
void resetClient();
int CreateClient(int iConnectionType);

unsigned int MyServersDataPort = 1511;
unsigned int MyServersCommandPort = 1510;

NatNetClient* theClient;
FILE* fp;

sDataDescriptions* pDataDefs = NULL;
int monotonicDataPacketId = 0;

char szMyIPAddress[128] = "127.0.0.1";
char szServerIPAddress[128] = "127.0.0.1";

// Rigid body labels, etc.
map<int, string> idToLabel;
void GetDataDescriptions() {
	printf("\n\n[SampleClient] Requesting Data Descriptions...");
	int nBodies = theClient->GetDataDescriptions(&pDataDefs);
	if (!pDataDefs)
	{
		printf("[SampleClient] Unable to retrieve Data Descriptions.");
		return;
	}
	printf("[SampleClient] Received %d Data Descriptions:\n", pDataDefs->nDataDescriptions);
	for (int i = 0; i < pDataDefs->nDataDescriptions; i++)
	{
		printf("Data Description # %d (type=%d)\n", i, pDataDefs->arrDataDescriptions[i].type);
		// TODO: Process descriptions for MarkerSets, Skeletons
		if (pDataDefs->arrDataDescriptions[i].type == Descriptor_RigidBody)
		{
			sRigidBodyDescription* pRB = pDataDefs->arrDataDescriptions[i].Data.RigidBodyDescription;
			idToLabel[pRB->ID] = pRB->szName;
		} else {
			printf("Unknown data type.");
			continue;
		}
	}
}

MulticastStream *mcast;
int _tmain(int argc, _TCHAR* argv[])
{
	mcast = new MulticastStream();
	// Protobuf setup
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	Update update;

	int iResult;
	int iConnectionType = ConnectionType_Multicast; // ConnectionType_Unicast;

	// Create NatNet Client
	iResult = CreateClient(iConnectionType);
	if (iResult != ErrorCode_OK)
	{
		printf("Error initializing client.  See log for details.  Exiting");
		return 1;
	}
	else
	{
		printf("Client initialized and ready.\n");
	}

	// Send/receive test request
	printf("[SampleClient] Sending Test Request\n");
	void* response;
	int nBytes;
	iResult = theClient->SendMessageAndWait("TestRequest", &response, &nBytes);
	if (iResult == ErrorCode_OK)
	{
		printf("[SampleClient] Received: %s", (char*)response);
	}
	GetDataDescriptions();


	printf("\nClient is connected to server and listening for data...\n");
	int c;
	bool bExit = false;
	int clientsI = 0;
	std::string in_str;

	while (1)
	{
		system("CLS");
		printf("(press the 'h' key for help)\n");
		c = _getch();
		switch (c)
		{
		case 'h':
			printf("\nc: client connections\nr: reset\nq: quit\np: print server info\nd: refresh data descriptions\nf: print out most recent mocap frame ID\nm: multicast\nu: unicast");
			break;
		case 'q':
			bExit = true;
			break;
		case 'r':
			resetClient();
			break;
		case 'p':
			sServerDescription ServerDescription;
			memset(&ServerDescription, 0, sizeof(ServerDescription));
			theClient->GetServerDescription(&ServerDescription);
			if (!ServerDescription.HostPresent)
			{
				printf("Unable to connect to server. Host not present. Exiting.");
				return 1;
			}
			break;
		case 'd':
			GetDataDescriptions();
			continue;
			break;
		case 'f':
		{
			sFrameOfMocapData* pData = theClient->GetLastFrameOfData();
			printf("Most Recent Frame: %d", pData->iFrame);
		}
		break;
		case 'm':	                        // change to multicast
			iResult = CreateClient(ConnectionType_Multicast);
			if (iResult == ErrorCode_OK)
				printf("Client connection type changed to Multicast.\n\n");
			else
				printf("Error changing client connection type to Multicast.\n\n");
			break;
		case 'u':	                        // change to unicast
			iResult = CreateClient(ConnectionType_Unicast);
			if (iResult == ErrorCode_OK)
				printf("Client connection type changed to Unicast.\n\n");
			else
				printf("Error changing client connection type to Unicast.\n\n");
			break;
			// (HACK) page up: toggle button 1
		case 'I':
			is_b1_active ^= 1;
			if (is_b1_active) printf("button 1 active\n");
			else printf("button 1 inactive\n");
			break;
		case 'Q':
			is_b2_active ^= 1;
			if (is_b2_active) printf("button 2 active\n");
			else printf("button 2 inactive\n");
			break;
		default:
			printf("unrecognized keycode: %c", c);
			break;
		}
		if (bExit) {
			break;
		}
		printf("\nPress any key to continue...");
		c = _getch();
	}

	// Done - clean up.
	theClient->Uninitialize();

	return ErrorCode_OK;
}

// Establish a NatNet Client connection
int CreateClient(int iConnectionType)
{
	// release previous server
	if (theClient)
	{
		theClient->Uninitialize();
		delete theClient;
	}

	// create NatNet client
	theClient = new NatNetClient(iConnectionType);
	unsigned char ver[4];
	theClient->NatNetVersion(ver);
	printf("(NatNet ver. %d.%d.%d.%d)\n", ver[0], ver[1], ver[2], ver[3]);

	theClient->SetMessageCallback(MessageHandler);
	theClient->SetVerbosityLevel(Verbosity_Error);
	theClient->SetDataCallback(DataHandler, theClient);

	// Init Client and connect to NatNet server
	// to use NatNet default port assigments
	int retCode = theClient->Initialize(szMyIPAddress, szServerIPAddress);
	// to use a different port for commands and/or data:
	//int retCode = theClient->Initialize(szMyIPAddress, szServerIPAddress, MyServersCommandPort, MyServersDataPort);
	if (retCode != ErrorCode_OK)
	{
		printf("Unable to connect to server.  Error code: %d. Exiting", retCode);
		return ErrorCode_Internal;
	}
	else
	{
		// Print server info
		sServerDescription ServerDescription;
		memset(&ServerDescription, 0, sizeof(ServerDescription));
		theClient->GetServerDescription(&ServerDescription);

		printf("[Client] Server application info:\n");
		printf("Application: %s (ver. %d.%d.%d.%d)\n", ServerDescription.szHostApp, ServerDescription.HostAppVersion[0],
			ServerDescription.HostAppVersion[1], ServerDescription.HostAppVersion[2], ServerDescription.HostAppVersion[3]);
		printf("NatNet Version: %d.%d.%d.%d\n", ServerDescription.NatNetVersion[0], ServerDescription.NatNetVersion[1],
			ServerDescription.NatNetVersion[2], ServerDescription.NatNetVersion[3]);
		printf("Client IP:%s\n", szMyIPAddress);
		printf("Server IP:%s\n", szServerIPAddress);
		printf("Server Name:%s\n\n", ServerDescription.szHostComputerName);
	}

	return ErrorCode_OK;

}

void SendFrameToClients(sFrameOfMocapData *data, void *pUserData)
{
	NatNetClient* pClient = (NatNetClient*)pUserData;

	// FrameOfMocapData params
	bool bIsRecording = data->params & 0x01;
	bool bTrackedModelsChanged = data->params & 0x02;

	// timecode - for systems with an eSync and SMPTE timecode generator - decode to values
	int hour, minute, second, frame, subframe;
	bool bValid = pClient->DecodeTimecode(data->Timecode, data->TimecodeSubframe, &hour, &minute, &second, &frame, &subframe);
	// decode to friendly string
	char szTimecode[128] = "";
	pClient->TimecodeStringify(data->Timecode, data->TimecodeSubframe, szTimecode, 128);

	++frameModificationVersion;
	Updates* updates = new Updates();
	updates->set_mod_version(frameModificationVersion);
	Update* mocap_update = updates->add_updates();
	mocap_update->set_id("mocap");
	mocap_update->set_mod_version(frameModificationVersion);
	mocap_update->set_time(data->fTimestamp);
	int body_id = 0;
	Mocap* mocap = new Mocap();
	mocap->set_duringrecording(bIsRecording);
	mocap->set_trackedmodelschanged(bTrackedModelsChanged);
	
	int i = 0;

	// Rigid Bodies
	for (i = 0; i < data->nRigidBodies; i++)
	{
		TrackedBody* body = mocap->add_tracked_bodies();
		// params
		// 0x01 : bool, rigid body was successfully tracked in this frame
		bool bTrackingValid = data->RigidBodies[i].params & 0x01;
		body->set_id(data->RigidBodies[i].ID);
		body->set_trackingvalid(bTrackingValid);
		//e->SetAttribute("meanError", data->RigidBodies[i].MeanError);
		body->set_label(idToLabel[data->RigidBodies[i].ID]);

		Position* pos = new Position();
		pos->set_x(data->RigidBodies[i].x);
		pos->set_y(data->RigidBodies[i].y);
		pos->set_z(data->RigidBodies[i].z);

		Rotation* rot = new Rotation();
		rot->set_x(data->RigidBodies[i].qx);
		rot->set_y(data->RigidBodies[i].qy);
		rot->set_z(data->RigidBodies[i].qz);
		rot->set_w(data->RigidBodies[i].qw);

		body->set_allocated_position(pos);
		body->set_allocated_rotation(rot);

		// TODO: Rigid body markers
	}

	mocap_update->set_allocated_mocap(mocap);
	updates->SerializeToArray(packet, sizeof(packet));
	// TODO: Skeletons and Labeled  (non-body) Markers

	mcast->send(packet, updates->ByteSize());
}

void __cdecl DataHandler(sFrameOfMocapData* data, void* pUserData)
{
	NatNetClient* pClient = (NatNetClient*)pUserData;
	SendFrameToClients(data, pUserData);
}
void __cdecl MessageHandler(int msgType, char* msg)
{
	printf("\n%s\n", msg);
}
void resetClient()
{
	int iSuccess;
	printf("\n\nre-setting Client\n\n.");
	iSuccess = theClient->Uninitialize();
	if (iSuccess != 0) {
		printf("error un-initting Client\n");
	}
	iSuccess = theClient->Initialize(szMyIPAddress, szServerIPAddress);
	if (iSuccess != 0) {
		printf("error re-initting Client\n");
	}
}

