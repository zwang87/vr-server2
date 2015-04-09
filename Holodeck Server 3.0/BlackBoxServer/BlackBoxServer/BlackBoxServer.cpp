/*
BlackBoxServer 3.0
Includes some code from OptiTrack.
*/

#include "stdafx.h"

using namespace std;
using namespace tinyxml2;

#define RELAY_SERVER_PORT 1609

int frameModificationVersion = 0;

bool is_b1_active = false;
bool is_b2_active = false;

class UDPClient {
	SOCKET s;
	struct sockaddr_in clientAddress;
	char IpStr[128];
	int port;
	int addrLen;
	public:
	UDPClient(const char *clientIPAsStr, int clientPort) {
		strcpy_s(IpStr, 128, clientIPAsStr);
		port = clientPort;
		if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
			printf("Could not create socket : %d", WSAGetLastError());
		}
		memset(&clientAddress, 0, sizeof(clientAddress));
		clientAddress.sin_family = AF_INET;
		clientAddress.sin_port = htons(port);
		int r = inet_pton(AF_INET, IpStr, &clientAddress.sin_addr.s_addr);
		addrLen = sizeof(clientAddress);
	}
	void send(char* packet, int length) {
		sendto(s, packet, length, 0, (struct sockaddr*) &clientAddress, addrLen);
	}
	~UDPClient() {
		closesocket(s);
	}

};

std::map<std::string, UDPClient*> clients;

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

void SendXmlToClients(tinyxml2::XMLDocument *d) {
	XMLPrinter printer;
	d->Print(&printer);
	for (auto const &it1 : clients) {
		it1.second->send((char*)printer.CStr(), printer.CStrSize() - 1);
	}
}

tinyxml2::XMLDocument doc;
tinyxml2::XMLElement *frameChild = NULL;
tinyxml2::XMLElement *dataDescChild = NULL;

// Rigid body labels, etc.
void GetDataDescriptions() {
	printf("\n\n[SampleClient] Requesting Data Descriptions...");
	int nBodies = theClient->GetDataDescriptions(&pDataDefs);
	if (!pDataDefs)
	{
		printf("[SampleClient] Unable to retrieve Data Descriptions.");
		return;
	}
	// form XML document
	if (dataDescChild != NULL) {
		frameChild->DeleteChild(frameChild->FirstChild());
		dataDescChild = NULL;
	}
	XMLElement * root = doc.NewElement("DataDescriptions");
	frameChild->InsertFirstChild(root);
	dataDescChild = root;
	root->SetAttribute("id", monotonicDataPacketId++);

	XMLElement *e = NULL;
	XMLElement *d = NULL;
	printf("[SampleClient] Received %d Data Descriptions:\n", pDataDefs->nDataDescriptions);
	for (int i = 0; i < pDataDefs->nDataDescriptions; i++)
	{
		printf("Data Description # %d (type=%d)\n", i, pDataDefs->arrDataDescriptions[i].type);

		if (pDataDefs->arrDataDescriptions[i].type == Descriptor_MarkerSet)
		{
			sMarkerSetDescription* pMS = pDataDefs->arrDataDescriptions[i].Data.MarkerSetDescription;
			// MarkerSet
			d = doc.NewElement("MarkerSet");
			d->SetAttribute("name", pMS->szName);
			printf("MarkerSet Name : %s\n", pMS->szName);

			for (int i = 0; i < pMS->nMarkers; i++) {
				printf("%s\n", pMS->szMarkerNames[i]);
				e = doc.NewElement("Marker");
				d->InsertEndChild(e);
				e->SetAttribute("name", pMS->szMarkerNames[i]);
				e->SetAttribute("id", i);
			}
		}
		else if (pDataDefs->arrDataDescriptions[i].type == Descriptor_RigidBody)
		{
			sRigidBodyDescription* pRB = pDataDefs->arrDataDescriptions[i].Data.RigidBodyDescription;
			// RigidBody
			d = doc.NewElement("RigidBody");
			d->SetAttribute("name", pRB->szName);
			d->SetAttribute("id", pRB->ID);
			d->SetAttribute("parentId", pRB->parentID);
			d->SetAttribute("parentOffsetX", pRB->offsetx);
			d->SetAttribute("parentOffsetY", pRB->offsety);
			d->SetAttribute("parentOffsetZ", pRB->offsetz);

			printf("RigidBody Name : %s\n", pRB->szName);
			printf("RigidBody ID : %d\n", pRB->ID);
			printf("RigidBody Parent ID : %d\n", pRB->parentID);
			printf("Parent Offset : %3.2f,%3.2f,%3.2f\n", pRB->offsetx, pRB->offsety, pRB->offsetz);
		}
		else if (pDataDefs->arrDataDescriptions[i].type == Descriptor_Skeleton)
		{
			sSkeletonDescription* pSK = pDataDefs->arrDataDescriptions[i].Data.SkeletonDescription;
			// Skeleton
			d = doc.NewElement("Skeleton");
			d->SetAttribute("name", pSK->szName);
			d->SetAttribute("id", pSK->skeletonID);

			printf("Skeleton Name : %s\n", pSK->szName);
			printf("Skeleton ID : %d\n", pSK->skeletonID);
			printf("RigidBody (Bone) Count : %d\n", pSK->nRigidBodies);
			for (int j = 0; j < pSK->nRigidBodies; j++)
			{
				e = doc.NewElement("RigidBody");
				d->InsertEndChild(e);
				sRigidBodyDescription* pRB = &pSK->RigidBodies[j];

				e->SetAttribute("type", "RigidBody");
				e->SetAttribute("name", pRB->szName);
				e->SetAttribute("id", pRB->ID);
				e->SetAttribute("parentId", pRB->parentID);
				e->SetAttribute("parentOffsetX", pRB->offsetx);
				e->SetAttribute("parentOffsetY", pRB->offsety);
				e->SetAttribute("parentOffsetZ", pRB->offsetz);

				printf("  RigidBody Name : %s\n", pRB->szName);
				printf("  RigidBody ID : %d\n", pRB->ID);
				printf("  RigidBody Parent ID : %d\n", pRB->parentID);
				printf("  Parent Offset : %3.2f,%3.2f,%3.2f\n", pRB->offsetx, pRB->offsety, pRB->offsetz);
			}
		}
		else
		{
			printf("Unknown data type.");
			// Unknown
			continue;
		}
		d->SetAttribute("descriptor_id", i);
		root->InsertEndChild(d);
		frameChild->SetAttribute("modification_version", ++frameModificationVersion);
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	frameChild = doc.NewElement("Update");
	frameChild->SetAttribute("modification_version", ++frameModificationVersion);
	frameChild->SetAttribute("id", "motive");
	doc.InsertFirstChild(frameChild);

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

	clients.insert(std::pair<std::string, UDPClient*>("127.0.0.1", new UDPClient("127.0.0.1", RELAY_SERVER_PORT)));

	while (1)
	{
		system("CLS");
		printf("(press the 'h' key for help)\n");
		c = _getch();
		std::map<std::string, UDPClient*>::iterator ipIt;
		switch (c)
		{
		case 'c':
			clientsI = 0;
			printf("\nData is currently being streamed to the following clients:");
			for (auto const &it1 : clients) {
				++clientsI;
				cout << "\n" << it1.first;
			}
			cout << "\n\n-----\n* Enter an IP to start/stop streaming\n* Press the enter key after entry or to return to the main menu\n> ";
			getline(cin, in_str, '\n');
			if (in_str == "") {
				continue;
			}
			ipIt = clients.find(in_str);
			if (ipIt == clients.end()) {
				clients.insert(std::pair<std::string, UDPClient*>(in_str, new UDPClient(in_str.c_str(), RELAY_SERVER_PORT)));
			}
			else {
				free(ipIt->second);
				clients.erase(ipIt);
			}
			break;
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

	// Clear all previous data except for the DataDescriptions
	if (dataDescChild == NULL) {
		return;
	}
	if (!frameChild->NoChildren()) {
		while (frameChild->FirstChild() != frameChild->LastChild())
		{
			frameChild->DeleteChild(dataDescChild->NextSibling());
		}
	}
	XMLElement * root = frameChild;
	root->SetAttribute("duringRecording", bIsRecording);
	root->SetAttribute("trackedModelsChanged", bTrackedModelsChanged);
	root->SetAttribute("timestamp", data->fTimestamp);
	root->SetAttribute("timecode", szTimecode);
	root->SetAttribute("button1", is_b1_active);
	root->SetAttribute("button2", is_b2_active);

	XMLElement *d, *e, *f;
	int i = 0;

	// Other Markers
	d = doc.NewElement("OtherMarkers");
	for (i = 0; i < data->nOtherMarkers; i++)
	{
		e = doc.NewElement("OtherMarker");
		d->InsertEndChild(e);
		e->SetAttribute("id", i);
		e->SetAttribute("x", data->OtherMarkers[i][0]);
		e->SetAttribute("y", data->OtherMarkers[i][1]);
		e->SetAttribute("z", data->OtherMarkers[i][2]);
	}
	root->InsertEndChild(d);

	// Rigid Bodies
	d = doc.NewElement("RigidBodies");
	for (i = 0; i < data->nRigidBodies; i++)
	{
		e = doc.NewElement("RigidBody");
		d->InsertEndChild(e);

		// params
		// 0x01 : bool, rigid body was successfully tracked in this frame
		bool bTrackingValid = data->RigidBodies[i].params & 0x01;
		e->SetAttribute("trackingValid", bTrackingValid);
		e->SetAttribute("meanError", data->RigidBodies[i].MeanError);
		e->SetAttribute("id", data->RigidBodies[i].ID);

		e->SetAttribute("x", data->RigidBodies[i].x);
		e->SetAttribute("y", data->RigidBodies[i].y);
		e->SetAttribute("z", data->RigidBodies[i].z);

		e->SetAttribute("qx", data->RigidBodies[i].qx);
		e->SetAttribute("qy", data->RigidBodies[i].qy);
		e->SetAttribute("qz", data->RigidBodies[i].qz);
		e->SetAttribute("qw", data->RigidBodies[i].qw);

		for (int iMarker = 0; iMarker < data->RigidBodies[i].nMarkers; iMarker++)
		{
			f = doc.NewElement("Marker");
			e->InsertEndChild(f);

			if (data->RigidBodies[i].MarkerIDs) {
				f->SetAttribute("id", data->RigidBodies[i].MarkerIDs[iMarker]);
			}

			if (data->RigidBodies[i].MarkerSizes) {
				f->SetAttribute("size", data->RigidBodies[i].MarkerSizes[iMarker]);
			}

			if (data->RigidBodies[i].Markers) {
				f->SetAttribute("x", data->RigidBodies[i].Markers[iMarker][0]);
				f->SetAttribute("y", data->RigidBodies[i].Markers[iMarker][1]);
				f->SetAttribute("z", data->RigidBodies[i].Markers[iMarker][2]);
			}
		}
	}
	root->InsertEndChild(d);
	frameChild->SetAttribute("modification_version", ++frameModificationVersion);

	// TODO: Skeletons and Labeled Markers //
	/*
	// skeletons
	printf("Skeletons [Count=%d]\n", data->nSkeletons);
	for (i = 0; i < data->nSkeletons; i++)
	{
	sSkeletonData skData = data->Skeletons[i];
	printf("Skeleton [ID=%d  Bone count=%d]\n", skData.skeletonID, skData.nRigidBodies);
	for (int j = 0; j< skData.nRigidBodies; j++)
	{
	sRigidBodyData rbData = skData.RigidBodyData[j];
	printf("Bone %d\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\t%3.2f\n",
	rbData.ID, rbData.x, rbData.y, rbData.z, rbData.qx, rbData.qy, rbData.qz, rbData.qw);

	printf("\tRigid body markers [Count=%d]\n", rbData.nMarkers);
	for (int iMarker = 0; iMarker < rbData.nMarkers; iMarker++)
	{
	printf("\t\t");
	if (rbData.MarkerIDs)
	printf("MarkerID:%d", rbData.MarkerIDs[iMarker]);
	if (rbData.MarkerSizes)
	printf("\tMarkerSize:%3.2f", rbData.MarkerSizes[iMarker]);
	if (rbData.Markers)
	printf("\tMarkerPos:%3.2f,%3.2f,%3.2f\n",
	data->RigidBodies[i].Markers[iMarker][0],
	data->RigidBodies[i].Markers[iMarker][1],
	data->RigidBodies[i].Markers[iMarker][2]);
	}
	}
	}

	// labeled markers
	bool bOccluded;     // marker was not visible (occluded) in this frame
	bool bPCSolved;     // reported position provided by point cloud solve
	bool bModelSolved;  // reported position provided by model solve
	printf("Labeled Markers [Count=%d]\n", data->nLabeledMarkers);
	for (i = 0; i < data->nLabeledMarkers; i++)
	{
	bOccluded = data->LabeledMarkers[i].params & 0x01;
	bPCSolved = data->LabeledMarkers[i].params & 0x02;
	bModelSolved = data->LabeledMarkers[i].params & 0x04;
	sMarker marker = data->LabeledMarkers[i];
	printf("Labeled Marker [ID=%d, Occluded=%d, PCSolved=%d, ModelSolved=%d] [size=%3.2f] [pos=%3.2f,%3.2f,%3.2f]\n",
	marker.ID, bOccluded, bPCSolved, bModelSolved, marker.size, marker.x, marker.y, marker.z);
	}
	*/
	SendXmlToClients(&doc);
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

